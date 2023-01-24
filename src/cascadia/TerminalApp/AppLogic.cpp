// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppLogic.h"
#include "../inc/WindowingBehavior.h"
#include "AppLogic.g.cpp"
#include "FindTargetWindowResult.g.cpp"
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>

#include <LibraryResources.h>
#include <WtExeUtils.h>
#include <wil/token_helpers.h>

#include "../../types/inc/utils.hpp"

using namespace winrt::Windows::ApplicationModel;
using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::System;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace ::TerminalApp;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

static constexpr std::wstring_view StartupTaskName = L"StartTerminalOnLoginTask";
// clang-format off
// !!! IMPORTANT !!!
// Make sure that these keys are in the same order as the
// SettingsLoadWarnings/Errors enum is!
static const std::array settingsLoadWarningsLabels {
    USES_RESOURCE(L"MissingDefaultProfileText"),
    USES_RESOURCE(L"DuplicateProfileText"),
    USES_RESOURCE(L"UnknownColorSchemeText"),
    USES_RESOURCE(L"InvalidBackgroundImage"),
    USES_RESOURCE(L"InvalidIcon"),
    USES_RESOURCE(L"AtLeastOneKeybindingWarning"),
    USES_RESOURCE(L"TooManyKeysForChord"),
    USES_RESOURCE(L"MissingRequiredParameter"),
    USES_RESOURCE(L"FailedToParseCommandJson"),
    USES_RESOURCE(L"FailedToWriteToSettings"),
    USES_RESOURCE(L"InvalidColorSchemeInCmd"),
    USES_RESOURCE(L"InvalidSplitSize"),
    USES_RESOURCE(L"FailedToParseStartupActions"),
    USES_RESOURCE(L"FailedToParseSubCommands"),
    USES_RESOURCE(L"UnknownTheme"),
    USES_RESOURCE(L"DuplicateRemainingProfilesEntry"),
};
static const std::array settingsLoadErrorsLabels {
    USES_RESOURCE(L"NoProfilesText"),
    USES_RESOURCE(L"AllProfilesHiddenText")
};
// clang-format on

static_assert(settingsLoadWarningsLabels.size() == static_cast<size_t>(SettingsLoadWarnings::WARNINGS_SIZE));
static_assert(settingsLoadErrorsLabels.size() == static_cast<size_t>(SettingsLoadErrors::ERRORS_SIZE));

// Function Description:
// - General-purpose helper for looking up a localized string for a
//   warning/error. First will look for the given key in the provided map of
//   keys->strings, where the values in the map are ResourceKeys. If it finds
//   one, it will lookup the localized string from that ResourceKey.
// - If it does not find a key, it'll return an empty string
// Arguments:
// - key: the value to use to look for a resource key in the given map
// - map: A map of keys->Resource keys.
// Return Value:
// - the localized string for the given type, if it exists.
template<typename T>
winrt::hstring _GetMessageText(uint32_t index, const T& keys)
{
    if (index < keys.size())
    {
        return GetLibraryResourceString(til::at(keys, index));
    }
    return {};
}

// Function Description:
// - Gets the text from our ResourceDictionary for the given
//   SettingsLoadWarning. If there is no such text, we'll return nullptr.
// - The warning should have an entry in settingsLoadWarningsLabels.
// Arguments:
// - warning: the SettingsLoadWarnings value to get the localized text for.
// Return Value:
// - localized text for the given warning
static winrt::hstring _GetWarningText(SettingsLoadWarnings warning)
{
    return _GetMessageText(static_cast<uint32_t>(warning), settingsLoadWarningsLabels);
}

// Function Description:
// - Gets the text from our ResourceDictionary for the given
//   SettingsLoadError. If there is no such text, we'll return nullptr.
// - The warning should have an entry in settingsLoadErrorsLabels.
// Arguments:
// - error: the SettingsLoadErrors value to get the localized text for.
// Return Value:
// - localized text for the given error
static winrt::hstring _GetErrorText(SettingsLoadErrors error)
{
    return _GetMessageText(static_cast<uint32_t>(error), settingsLoadErrorsLabels);
}

// Function Description:
// - Creates a Run of text to display an error message. The text is yellow or
//   red for dark/light theme, respectively.
// Arguments:
// - text: The text of the error message.
// - resources: The application's resource loader.
// Return Value:
// - The fully styled text run.
static Documents::Run _BuildErrorRun(const winrt::hstring& text, const ResourceDictionary& resources)
{
    Documents::Run textRun;
    textRun.Text(text);

    // Color the text red (light theme) or yellow (dark theme) based on the system theme
    auto key = winrt::box_value(L"ErrorTextBrush");
    if (resources.HasKey(key))
    {
        auto g = resources.Lookup(key);
        auto brush = g.try_as<winrt::Windows::UI::Xaml::Media::Brush>();
        textRun.Foreground(brush);
    }

    return textRun;
}

namespace winrt::TerminalApp::implementation
{
    // Function Description:
    // - Get the AppLogic for the current active Xaml application, or null if there isn't one.
    // Return value:
    // - A pointer (bare) to the AppLogic, or nullptr. The app logic outlives all other objects,
    //   unless the application is in a terrible way, so this is "safe."
    AppLogic* AppLogic::Current() noexcept
    try
    {
        if (auto currentXamlApp{ winrt::Windows::UI::Xaml::Application::Current().try_as<winrt::TerminalApp::App>() })
        {
            if (auto appLogicPointer{ winrt::get_self<AppLogic>(currentXamlApp.Logic()) })
            {
                return appLogicPointer;
            }
        }
        return nullptr;
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
        return nullptr;
    }

    // Method Description:
    // - Returns the settings currently in use by the entire Terminal application.
    // - IMPORTANT! This can throw! Make sure to try/catch this, so that the
    //   LocalTests don't crash (because their Application::Current() won't be a
    //   AppLogic)
    // Throws:
    // - HR E_INVALIDARG if the app isn't up and running.
    const CascadiaSettings AppLogic::CurrentAppSettings()
    {
        auto appLogic{ ::winrt::TerminalApp::implementation::AppLogic::Current() };
        THROW_HR_IF_NULL(E_INVALIDARG, appLogic);
        return appLogic->GetSettings();
    }

    AppLogic::AppLogic() :
        _reloadState{ std::chrono::milliseconds(100), []() { ApplicationState::SharedInstance().Reload(); } }
    {
        // For your own sanity, it's better to do setup outside the ctor.
        // If you do any setup in the ctor that ends up throwing an exception,
        // then it might look like App just failed to activate, which will
        // cause you to chase down the rabbit hole of "why is App not
        // registered?" when it definitely is.

        // The TerminalPage has to be constructed during our construction, to
        // make sure that there's a terminal page for callers of
        // SetTitleBarContent
        _isElevated = ::Microsoft::Console::Utils::IsElevated();
        _root = winrt::make_self<TerminalPage>();

        _reloadSettings = std::make_shared<ThrottledFuncTrailing<>>(winrt::Windows::System::DispatcherQueue::GetForCurrentThread(), std::chrono::milliseconds(100), [weakSelf = get_weak()]() {
            if (auto self{ weakSelf.get() })
            {
                self->ReloadSettings();
            }
        });

        _languageProfileNotifier = winrt::make_self<LanguageProfileNotifier>([this]() {
            _reloadSettings->Run();
        });
    }

    // Method Description:
    // - Implements the IInitializeWithWindow interface from shobjidl_core.
    HRESULT AppLogic::Initialize(HWND hwnd)
    {
        return _root->Initialize(hwnd);
    }

    // Method Description:
    // - Called around the codebase to discover if this is a UWP where we need to turn off specific settings.
    // Arguments:
    // - <none> - reports internal state
    // Return Value:
    // - True if UWP, false otherwise.
    bool AppLogic::IsUwp() const noexcept
    {
        return _isUwp;
    }

    // Method Description:
    // - Called around the codebase to discover if Terminal is running elevated
    // Arguments:
    // - <none> - reports internal state
    // Return Value:
    // - True if elevated, false otherwise.
    bool AppLogic::IsElevated() const noexcept
    {
        return _isElevated;
    }

    // Method Description:
    // - Called by UWP context invoker to let us know that we may have to change some of our behaviors
    //   for being a UWP
    // Arguments:
    // - <none> (sets to UWP = true, one way change)
    // Return Value:
    // - <none>
    void AppLogic::RunAsUwp()
    {
        _isUwp = true;
    }

    // Method Description:
    // - Build the UI for the terminal app. Before this method is called, it
    //   should not be assumed that the TerminalApp is usable. The Settings
    //   should be loaded before this is called, either with LoadSettings or
    //   GetLaunchDimensions (which will call LoadSettings)
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void AppLogic::Create()
    {
        // Assert that we've already loaded our settings. We have to do
        // this as a MTA, before the app is Create()'d
        WINRT_ASSERT(_loadedInitialSettings);

        _root->DialogPresenter(*this);

        // In UWP mode, we cannot handle taking over the title bar for tabs,
        // so this setting is overridden to false no matter what the preference is.
        if (_isUwp)
        {
            _settings.GlobalSettings().ShowTabsInTitlebar(false);
        }

        // Pay attention, that even if some command line arguments were parsed (like launch mode),
        // we will not use the startup actions from settings.
        // While this simplifies the logic, we might want to reconsider this behavior in the future.
        if (!_hasCommandLineArguments && _hasSettingsStartupActions)
        {
            _root->SetStartupActions(_settingsAppArgs.GetStartupActions());
        }

        _root->SetSettings(_settings, false);
        _root->Loaded({ this, &AppLogic::_OnLoaded });
        _root->Initialized([this](auto&&, auto&&) {
            // GH#288 - When we finish initialization, if the user wanted us
            // launched _fullscreen_, toggle fullscreen mode. This will make sure
            // that the window size is _first_ set up as something sensible, so
            // leaving fullscreen returns to a reasonable size.
            const auto launchMode = this->GetLaunchMode();
            if (IsQuakeWindow() || WI_IsFlagSet(launchMode, LaunchMode::FocusMode))
            {
                _root->SetFocusMode(true);
            }

            // The IslandWindow handles (creating) the maximized state
            // we just want to record it here on the page as well.
            if (WI_IsFlagSet(launchMode, LaunchMode::MaximizedMode))
            {
                _root->Maximized(true);
            }

            if (WI_IsFlagSet(launchMode, LaunchMode::FullscreenMode) && !IsQuakeWindow())
            {
                _root->SetFullscreen(true);
            }

            // Both LoadSettings and ReloadSettings are supposed to call this function,
            // but LoadSettings skips it, so that the UI starts up faster.
            // Now that the UI is present we can do them with a less significant UX impact.
            _ProcessLazySettingsChanges();

            FILETIME creationTime, exitTime, kernelTime, userTime, now;
            if (GetThreadTimes(GetCurrentThread(), &creationTime, &exitTime, &kernelTime, &userTime))
            {
                static constexpr auto asInteger = [](const FILETIME& f) {
                    ULARGE_INTEGER i;
                    i.LowPart = f.dwLowDateTime;
                    i.HighPart = f.dwHighDateTime;
                    return i.QuadPart;
                };
                static constexpr auto asSeconds = [](uint64_t v) {
                    return v * 1e-7f;
                };

                GetSystemTimeAsFileTime(&now);

                const auto latency = asSeconds(asInteger(now) - asInteger(creationTime));

                TraceLoggingWrite(
                    g_hTerminalAppProvider,
                    "AppInitialized",
                    TraceLoggingDescription("Event emitted once the app is initialized"),
                    TraceLoggingFloat32(latency, "latency"),
                    TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                    TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
            }
        });
        _root->Create();

        _ApplyLanguageSettingChange();
        _RefreshThemeRoutine();
        _ApplyStartupTaskStateChange();

        auto args = winrt::make_self<SystemMenuChangeArgs>(RS_(L"SettingsMenuItem"), SystemMenuChangeAction::Add, SystemMenuItemHandler(this, &AppLogic::_OpenSettingsUI));
        _SystemMenuChangeRequestedHandlers(*this, *args);

        TraceLoggingWrite(
            g_hTerminalAppProvider,
            "AppCreated",
            TraceLoggingDescription("Event emitted when the application is started"),
            TraceLoggingBool(_settings.GlobalSettings().ShowTabsInTitlebar(), "TabsInTitlebar"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }

    void AppLogic::Quit()
    {
        if (_root)
        {
            _root->CloseWindow(true);
        }
    }

    // Method Description:
    // - Show a ContentDialog with buttons to take further action. Uses the
    //   FrameworkElements provided as the title and content of this dialog, and
    //   displays buttons (or a single button). Two buttons (primary and secondary)
    //   will be displayed if this is an warning dialog for closing the terminal,
    //   this allows the users to abandon the closing action. Otherwise, a single
    //   close button will be displayed.
    // - Only one dialog can be visible at a time. If another dialog is visible
    //   when this is called, nothing happens.
    // Arguments:
    // - dialog: the dialog object that is going to show up
    // Return value:
    // - an IAsyncOperation with the dialog result
    winrt::Windows::Foundation::IAsyncOperation<ContentDialogResult> AppLogic::ShowDialog(winrt::Windows::UI::Xaml::Controls::ContentDialog dialog)
    {
        // DON'T release this lock in a wil::scope_exit. The scope_exit will get
        // called when we await, which is not what we want.
        std::unique_lock lock{ _dialogLock, std::try_to_lock };
        if (!lock)
        {
            // Another dialog is visible.
            co_return ContentDialogResult::None;
        }

        _dialog = dialog;

        // IMPORTANT: This is necessary as documented in the ContentDialog MSDN docs.
        // Since we're hosting the dialog in a Xaml island, we need to connect it to the
        // xaml tree somehow.
        dialog.XamlRoot(_root->XamlRoot());

        // IMPORTANT: Set the requested theme of the dialog, because the
        // PopupRoot isn't directly in the Xaml tree of our root. So the dialog
        // won't inherit our RequestedTheme automagically.
        // GH#5195, GH#3654 Because we cannot set RequestedTheme at the application level,
        // we occasionally run into issues where parts of our UI end up themed incorrectly.
        // Dialogs, for example, live under a different Xaml root element than the rest of
        // our application. This makes our popup menus and buttons "disappear" when the
        // user wants Terminal to be in a different theme than the rest of the system.
        // This hack---and it _is_ a hack--walks up a dialog's ancestry and forces the
        // theme on each element up to the root. We're relying a bit on Xaml's implementation
        // details here, but it does have the desired effect.
        // It's not enough to set the theme on the dialog alone.
        auto themingLambda{ [this](const Windows::Foundation::IInspectable& sender, const RoutedEventArgs&) {
            auto theme{ _settings.GlobalSettings().CurrentTheme() };
            auto requestedTheme{ theme.RequestedTheme() };
            auto element{ sender.try_as<winrt::Windows::UI::Xaml::FrameworkElement>() };
            while (element)
            {
                element.RequestedTheme(requestedTheme);
                element = element.Parent().try_as<winrt::Windows::UI::Xaml::FrameworkElement>();
            }
        } };

        themingLambda(dialog, nullptr); // if it's already in the tree
        auto loadedRevoker{ dialog.Loaded(winrt::auto_revoke, themingLambda) }; // if it's not yet in the tree

        // Display the dialog.
        co_return co_await dialog.ShowAsync(Controls::ContentDialogPlacement::Popup);

        // After the dialog is dismissed, the dialog lock (held by `lock`) will
        // be released so another can be shown
    }

    // Method Description:
    // - Dismiss the (only) visible ContentDialog
    void AppLogic::DismissDialog()
    {
        if (auto localDialog = std::exchange(_dialog, nullptr))
        {
            localDialog.Hide();
        }
    }

    // Method Description:
    // - Displays a dialog for errors found while loading or validating the
    //   settings. Uses the resources under the provided  title and content keys
    //   as the title and first content of the dialog, then also displays a
    //   message for whatever exception was found while validating the settings.
    // - Only one dialog can be visible at a time. If another dialog is visible
    //   when this is called, nothing happens. See ShowDialog for details
    // Arguments:
    // - titleKey: The key to use to lookup the title text from our resources.
    // - contentKey: The key to use to lookup the content text from our resources.
    void AppLogic::_ShowLoadErrorsDialog(const winrt::hstring& titleKey,
                                         const winrt::hstring& contentKey,
                                         HRESULT settingsLoadedResult)
    {
        auto title = GetLibraryResourceString(titleKey);
        auto buttonText = RS_(L"Ok");

        Controls::TextBlock warningsTextBlock;
        // Make sure you can copy-paste
        warningsTextBlock.IsTextSelectionEnabled(true);
        // Make sure the lines of text wrap
        warningsTextBlock.TextWrapping(TextWrapping::Wrap);

        winrt::Windows::UI::Xaml::Documents::Run errorRun;
        const auto errorLabel = GetLibraryResourceString(contentKey);
        errorRun.Text(errorLabel);
        warningsTextBlock.Inlines().Append(errorRun);
        warningsTextBlock.Inlines().Append(Documents::LineBreak{});

        if (FAILED(settingsLoadedResult))
        {
            if (!_settingsLoadExceptionText.empty())
            {
                warningsTextBlock.Inlines().Append(_BuildErrorRun(_settingsLoadExceptionText, ::winrt::Windows::UI::Xaml::Application::Current().as<::winrt::TerminalApp::App>().Resources()));
                warningsTextBlock.Inlines().Append(Documents::LineBreak{});
            }
        }

        // Add a note that we're using the default settings in this case.
        winrt::Windows::UI::Xaml::Documents::Run usingDefaultsRun;
        const auto usingDefaultsText = RS_(L"UsingDefaultSettingsText");
        usingDefaultsRun.Text(usingDefaultsText);
        warningsTextBlock.Inlines().Append(Documents::LineBreak{});
        warningsTextBlock.Inlines().Append(usingDefaultsRun);

        Controls::ContentDialog dialog;
        dialog.Title(winrt::box_value(title));
        dialog.Content(winrt::box_value(warningsTextBlock));
        dialog.CloseButtonText(buttonText);
        dialog.DefaultButton(Controls::ContentDialogButton::Close);

        ShowDialog(dialog);
    }

    // Method Description:
    // - Displays a dialog for warnings found while loading or validating the
    //   settings. Displays messages for whatever warnings were found while
    //   validating the settings.
    // - Only one dialog can be visible at a time. If another dialog is visible
    //   when this is called, nothing happens. See ShowDialog for details
    void AppLogic::_ShowLoadWarningsDialog()
    {
        auto title = RS_(L"SettingsValidateErrorTitle");
        auto buttonText = RS_(L"Ok");

        Controls::TextBlock warningsTextBlock;
        // Make sure you can copy-paste
        warningsTextBlock.IsTextSelectionEnabled(true);
        // Make sure the lines of text wrap
        warningsTextBlock.TextWrapping(TextWrapping::Wrap);

        for (const auto& warning : _warnings)
        {
            // Try looking up the warning message key for each warning.
            const auto warningText = _GetWarningText(warning);
            if (!warningText.empty())
            {
                warningsTextBlock.Inlines().Append(_BuildErrorRun(warningText, ::winrt::Windows::UI::Xaml::Application::Current().as<::winrt::TerminalApp::App>().Resources()));
                warningsTextBlock.Inlines().Append(Documents::LineBreak{});
            }
        }

        Controls::ContentDialog dialog;
        dialog.Title(winrt::box_value(title));
        dialog.Content(winrt::box_value(warningsTextBlock));
        dialog.CloseButtonText(buttonText);
        dialog.DefaultButton(Controls::ContentDialogButton::Close);

        ShowDialog(dialog);
    }

    // Method Description:
    // - Triggered when the application is finished loading. If we failed to load
    //   the settings, then this will display the error dialog. This is done
    //   here instead of when loading the settings, because we need our UI to be
    //   visible to display the dialog, and when we're loading the settings,
    //   the UI might not be visible yet.
    // Arguments:
    // - <unused>
    void AppLogic::_OnLoaded(const IInspectable& /*sender*/,
                             const RoutedEventArgs& /*eventArgs*/)
    {
        if (_settings.GlobalSettings().InputServiceWarning())
        {
            const auto keyboardServiceIsDisabled = !_IsKeyboardServiceEnabled();
            if (keyboardServiceIsDisabled)
            {
                _root->ShowKeyboardServiceWarning();
            }
        }

        if (FAILED(_settingsLoadedResult))
        {
            const winrt::hstring titleKey = USES_RESOURCE(L"InitialJsonParseErrorTitle");
            const winrt::hstring textKey = USES_RESOURCE(L"InitialJsonParseErrorText");
            _ShowLoadErrorsDialog(titleKey, textKey, _settingsLoadedResult);
        }
        else if (_settingsLoadedResult == S_FALSE)
        {
            _ShowLoadWarningsDialog();
        }
    }

    // Method Description:
    // - Helper for determining if the "Touch Keyboard and Handwriting Panel
    //   Service" is enabled. If it isn't, we want to be able to display a
    //   warning to the user, because they won't be able to type in the
    //   Terminal.
    // Return Value:
    // - true if the service is enabled, or if we fail to query the service. We
    //   return true in that case, to be less noisy (though, that is unexpected)
    bool AppLogic::_IsKeyboardServiceEnabled()
    {
        if (IsUwp())
        {
            return true;
        }

        // If at any point we fail to open the service manager, the service,
        // etc, then just quick return true to disable the dialog. We'd rather
        // not be noisy with this dialog if we failed for some reason.

        // Open the service manager. This will return 0 if it failed.
        wil::unique_schandle hManager{ OpenSCManagerW(nullptr, nullptr, 0) };

        if (LOG_LAST_ERROR_IF(!hManager.is_valid()))
        {
            return true;
        }

        // Get a handle to the keyboard service
        wil::unique_schandle hService{ OpenServiceW(hManager.get(), TabletInputServiceKey.data(), SERVICE_QUERY_STATUS) };

        // Windows 11 doesn't have a TabletInputService.
        // (It was renamed to TextInputManagementService, because people kept thinking that a
        // service called "tablet-something" is system-irrelevant on PCs and can be disabled.)
        if (!hService.is_valid())
        {
            return true;
        }

        // Get the current state of the service
        SERVICE_STATUS status{ 0 };
        if (!LOG_IF_WIN32_BOOL_FALSE(QueryServiceStatus(hService.get(), &status)))
        {
            return true;
        }

        const auto state = status.dwCurrentState;
        return (state == SERVICE_RUNNING || state == SERVICE_START_PENDING);
    }

    // Method Description:
    // - Get the size in pixels of the client area we'll need to launch this
    //   terminal app. This method will use the default profile's settings to do
    //   this calculation, as well as the _system_ dpi scaling. See also
    //   TermControl::GetProposedDimensions.
    // Arguments:
    // - <none>
    // Return Value:
    // - a point containing the requested dimensions in pixels.
    winrt::Windows::Foundation::Size AppLogic::GetLaunchDimensions(uint32_t dpi)
    {
        if (!_loadedInitialSettings)
        {
            // Load settings if we haven't already
            ReloadSettings();
        }

        winrt::Windows::Foundation::Size proposedSize{};

        const auto scale = static_cast<float>(dpi) / static_cast<float>(USER_DEFAULT_SCREEN_DPI);
        if (const auto layout = _root->LoadPersistedLayout(_settings))
        {
            if (layout.InitialSize())
            {
                proposedSize = layout.InitialSize().Value();
                // The size is saved as a non-scaled real pixel size,
                // so we need to scale it appropriately.
                proposedSize.Height = proposedSize.Height * scale;
                proposedSize.Width = proposedSize.Width * scale;
            }
        }

        if (_appArgs.GetSize().has_value() || (proposedSize.Width == 0 && proposedSize.Height == 0))
        {
            // Use the default profile to determine how big of a window we need.
            const auto settings{ TerminalSettings::CreateWithNewTerminalArgs(_settings, nullptr, nullptr) };

            const til::size emptySize{};
            const auto commandlineSize = _appArgs.GetSize().value_or(emptySize);
            proposedSize = TermControl::GetProposedDimensions(settings.DefaultSettings(),
                                                              dpi,
                                                              commandlineSize.width,
                                                              commandlineSize.height);
        }

        // GH#2061 - If the global setting "Always show tab bar" is
        // set or if "Show tabs in title bar" is set, then we'll need to add
        // the height of the tab bar here.
        if (_settings.GlobalSettings().ShowTabsInTitlebar())
        {
            // If we're showing the tabs in the titlebar, we need to use a
            // TitlebarControl here to calculate how much space to reserve.
            //
            // We'll create a fake TitlebarControl, and we'll propose an
            // available size to it with Measure(). After Measure() is called,
            // the TitlebarControl's DesiredSize will contain the _unscaled_
            // size that the titlebar would like to use. We'll use that as part
            // of the height calculation here.
            auto titlebar = TitlebarControl{ static_cast<uint64_t>(0) };
            titlebar.Measure({ SHRT_MAX, SHRT_MAX });
            proposedSize.Height += (titlebar.DesiredSize().Height) * scale;
        }
        else if (_settings.GlobalSettings().AlwaysShowTabs())
        {
            // Otherwise, let's use a TabRowControl to calculate how much extra
            // space we'll need.
            //
            // Similarly to above, we'll measure it with an arbitrarily large
            // available space, to make sure we get all the space it wants.
            auto tabControl = TabRowControl();
            tabControl.Measure({ SHRT_MAX, SHRT_MAX });

            // For whatever reason, there's about 10px of unaccounted-for space
            // in the application. I couldn't tell you where these 10px are
            // coming from, but they need to be included in this math.
            proposedSize.Height += (tabControl.DesiredSize().Height + 10) * scale;
        }

        return proposedSize;
    }

    // Method Description:
    // - Get the launch mode in json settings file. Now there
    //   two launch mode: default, maximized. Default means the window
    //   will launch according to the launch dimensions provided. Maximized
    //   means the window will launch as a maximized window
    // Arguments:
    // - <none>
    // Return Value:
    // - LaunchMode enum that indicates the launch mode
    LaunchMode AppLogic::GetLaunchMode()
    {
        if (!_loadedInitialSettings)
        {
            // Load settings if we haven't already
            ReloadSettings();
        }

        // GH#4620/#5801 - If the user passed --maximized or --fullscreen on the
        // commandline, then use that to override the value from the settings.
        const auto valueFromSettings = _settings.GlobalSettings().LaunchMode();
        const auto valueFromCommandlineArgs = _appArgs.GetLaunchMode();
        if (const auto layout = _root->LoadPersistedLayout(_settings))
        {
            if (layout.LaunchMode())
            {
                return layout.LaunchMode().Value();
            }
        }
        return valueFromCommandlineArgs.has_value() ?
                   valueFromCommandlineArgs.value() :
                   valueFromSettings;
    }

    // Method Description:
    // - Get the user defined initial position from Json settings file.
    //   This position represents the top left corner of the Terminal window.
    //   This setting is optional, if not provided, we will use the system
    //   default size, which is provided in IslandWindow::MakeWindow.
    // Arguments:
    // - defaultInitialX: the system default x coordinate value
    // - defaultInitialY: the system default y coordinate value
    // Return Value:
    // - a point containing the requested initial position in pixels.
    TerminalApp::InitialPosition AppLogic::GetInitialPosition(int64_t defaultInitialX, int64_t defaultInitialY)
    {
        if (!_loadedInitialSettings)
        {
            // Load settings if we haven't already
            ReloadSettings();
        }

        auto initialPosition{ _settings.GlobalSettings().InitialPosition() };

        if (const auto layout = _root->LoadPersistedLayout(_settings))
        {
            if (layout.InitialPosition())
            {
                initialPosition = layout.InitialPosition().Value();
            }
        }

        // Commandline args trump everything else
        if (_appArgs.GetPosition().has_value())
        {
            initialPosition = _appArgs.GetPosition().value();
        }

        return {
            initialPosition.X ? initialPosition.X.Value() : defaultInitialX,
            initialPosition.Y ? initialPosition.Y.Value() : defaultInitialY
        };
    }

    bool AppLogic::CenterOnLaunch()
    {
        if (!_loadedInitialSettings)
        {
            // Load settings if we haven't already
            ReloadSettings();
        }
        // If the position has been specified on the commandline, don't center on launch
        return _settings.GlobalSettings().CenterOnLaunch() && !_appArgs.GetPosition().has_value();
    }

    winrt::Windows::UI::Xaml::ElementTheme AppLogic::GetRequestedTheme()
    {
        return Theme().RequestedTheme();
    }

    bool AppLogic::GetShowTabsInTitlebar()
    {
        if (!_loadedInitialSettings)
        {
            // Load settings if we haven't already
            ReloadSettings();
        }

        return _settings.GlobalSettings().ShowTabsInTitlebar();
    }

    bool AppLogic::GetInitialAlwaysOnTop()
    {
        if (!_loadedInitialSettings)
        {
            // Load settings if we haven't already
            ReloadSettings();
        }

        return _settings.GlobalSettings().AlwaysOnTop();
    }

    // Method Description:
    // - See Pane::CalcSnappedDimension
    float AppLogic::CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
    {
        return _root->CalcSnappedDimension(widthOrHeight, dimension);
    }

    // Method Description:
    // - Attempt to load the settings. If we fail for any reason, returns an error.
    // Return Value:
    // - S_OK if we successfully parsed the settings, otherwise an appropriate HRESULT.
    [[nodiscard]] HRESULT AppLogic::_TryLoadSettings() noexcept
    {
        auto hr = E_FAIL;

        try
        {
            auto newSettings = _isUwp ? CascadiaSettings::LoadUniversal() : CascadiaSettings::LoadAll();

            if (newSettings.GetLoadingError())
            {
                _settingsLoadExceptionText = _GetErrorText(newSettings.GetLoadingError().Value());
                return E_INVALIDARG;
            }
            else if (!newSettings.GetSerializationErrorMessage().empty())
            {
                _settingsLoadExceptionText = newSettings.GetSerializationErrorMessage();
                return E_INVALIDARG;
            }

            _warnings.clear();
            for (uint32_t i = 0; i < newSettings.Warnings().Size(); i++)
            {
                _warnings.push_back(newSettings.Warnings().GetAt(i));
            }

            _hasSettingsStartupActions = false;
            const auto startupActions = newSettings.GlobalSettings().StartupActions();
            if (!startupActions.empty())
            {
                _settingsAppArgs.FullResetState();

                ExecuteCommandlineArgs args{ newSettings.GlobalSettings().StartupActions() };
                auto result = _settingsAppArgs.ParseArgs(args);
                if (result == 0)
                {
                    _hasSettingsStartupActions = true;

                    // Validation also injects new-tab command if implicit new-tab was provided.
                    _settingsAppArgs.ValidateStartupCommands();
                }
                else
                {
                    _warnings.push_back(SettingsLoadWarnings::FailedToParseStartupActions);
                }
            }

            _settings = std::move(newSettings);
            hr = _warnings.empty() ? S_OK : S_FALSE;
        }
        catch (const winrt::hresult_error& e)
        {
            hr = e.code();
            _settingsLoadExceptionText = e.message();
            LOG_HR(hr);
        }
        catch (...)
        {
            hr = wil::ResultFromCaughtException();
            LOG_HR(hr);
        }
        return hr;
    }

    // Call this function after loading your _settings.
    // It handles any CPU intensive settings updates (like updating the Jumplist)
    // which should thus only occur if the settings file actually changed.
    void AppLogic::_ProcessLazySettingsChanges()
    {
        const auto hash = _settings.Hash();
        const auto applicationState = ApplicationState::SharedInstance();
        const auto cachedHash = applicationState.SettingsHash();

        // The hash might be empty if LoadAll() failed and we're dealing with the defaults settings object.
        // In that case we can just wait until the user fixed their settings or CascadiaSettings fixed
        // itself and either will soon trigger a settings reload.
        if (hash.empty() || hash == cachedHash)
        {
            return;
        }

        Jumplist::UpdateJumplist(_settings);
        applicationState.SettingsHash(hash);
    }

    // Method Description:
    // - Registers for changes to the settings folder and upon a updated settings
    //      profile calls ReloadSettings().
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void AppLogic::_RegisterSettingsChange()
    {
        const std::filesystem::path settingsPath{ std::wstring_view{ CascadiaSettings::SettingsPath() } };
        _reader.create(
            settingsPath.parent_path().c_str(),
            false,
            // We want file modifications, AND when files are renamed to be
            // settings.json. This second case will oftentimes happen with text
            // editors, who will write a temp file, then rename it to be the
            // actual file you wrote. So listen for that too.
            wil::FolderChangeEvents::FileName | wil::FolderChangeEvents::LastWriteTime,
            [this, settingsBasename = settingsPath.filename()](wil::FolderChangeEvent, PCWSTR fileModified) {
                // DO NOT create a static reference to ApplicationState::SharedInstance here.
                //
                // ApplicationState::SharedInstance already caches its own
                // static ref. If _we_ keep a static ref to the member in
                // AppState, then our reference will keep ApplicationState alive
                // after the `ActionToStringMap` gets cleaned up. Then, when we
                // try to persist the actions in the window state, we won't be
                // able to. We'll try to look up the action and the map just
                // won't exist. We'll explode, even though the Terminal is
                // tearing down anyways. So we'll just die, but still invoke
                // WinDBG's post-mortem debugger, who won't be able to attach to
                // the process that's already exiting.
                //
                // So DON'T ~give a mouse a cookie~ take a static ref here.

                const winrt::hstring modifiedBasename{ std::filesystem::path{ fileModified }.filename().c_str() };

                if (modifiedBasename == settingsBasename)
                {
                    _reloadSettings->Run();
                }
                else if (ApplicationState::SharedInstance().IsStatePath(modifiedBasename))
                {
                    _reloadState();
                }
            });
    }

    void AppLogic::_ApplyLanguageSettingChange() noexcept
    try
    {
        if (!IsPackaged())
        {
            return;
        }

        using ApplicationLanguages = winrt::Windows::Globalization::ApplicationLanguages;

        // NOTE: PrimaryLanguageOverride throws if this instance is unpackaged.
        const auto primaryLanguageOverride = ApplicationLanguages::PrimaryLanguageOverride();
        const auto language = _settings.GlobalSettings().Language();

        if (primaryLanguageOverride != language)
        {
            ApplicationLanguages::PrimaryLanguageOverride(language);
        }
    }
    CATCH_LOG()

    // Method Description:
    // - Update the current theme of the application. This will trigger our
    //   RequestedThemeChanged event, to have our host change the theme of the
    //   root of the application.
    // Arguments:
    // - newTheme: The ElementTheme to apply to our elements.
    void AppLogic::_RefreshThemeRoutine()
    {
        // Propagate the event to the host layer, so it can update its own UI
        _RequestedThemeChangedHandlers(*this, Theme());
    }

    // Function Description:
    // Returns the current app package or nullptr.
    // TRANSITIONAL
    // Exists to work around a compiler bug. This function encapsulates the
    // exception handling that we used to keep around calls to Package::Current,
    // so that when it's called inside a coroutine and fails it doesn't explode
    // terribly.
    static winrt::Windows::ApplicationModel::Package GetCurrentPackageNoThrow() noexcept
    {
        try
        {
            return winrt::Windows::ApplicationModel::Package::Current();
        }
        catch (...)
        {
            // discard any exception -- literally pretend we're not in a package
        }
        return nullptr;
    }

    fire_and_forget AppLogic::_ApplyStartupTaskStateChange()
    try
    {
        // First, make sure we're running in a packaged context. This method
        // won't work, and will crash mysteriously if we're running unpackaged.
        const auto package{ GetCurrentPackageNoThrow() };
        if (package == nullptr)
        {
            co_return;
        }

        const auto tryEnableStartupTask = _settings.GlobalSettings().StartOnUserLogin();
        const auto task = co_await StartupTask::GetAsync(StartupTaskName);

        switch (task.State())
        {
        case StartupTaskState::Disabled:
            if (tryEnableStartupTask)
            {
                co_await task.RequestEnableAsync();
            }
            break;
        case StartupTaskState::DisabledByUser:
            // TODO: GH#6254: define UX for other StartupTaskStates
            break;
        case StartupTaskState::Enabled:
            if (!tryEnableStartupTask)
            {
                task.Disable();
            }
            break;
        }
    }
    CATCH_LOG();

    // Method Description:
    // - Reloads the settings from the settings.json file.
    // - When this is called the first time, this initializes our settings. See
    //   CascadiaSettings for more details. Additionally hooks up our callbacks
    //   for keybinding events to the keybindings object.
    //   - NOTE: when called initially, this must be called from a MTA if we're
    //     running as a packaged application. The Windows.Storage APIs require a
    //     MTA. If this isn't happening during startup, it'll need to happen on
    //     a background thread.
    void AppLogic::ReloadSettings()
    {
        // Attempt to load our settings.
        // If it fails,
        //  - don't change the settings (and don't actually apply the new settings)
        //  - don't persist them.
        //  - display a loading error
        _settingsLoadedResult = _TryLoadSettings();

        const auto initialLoad = !_loadedInitialSettings;
        _loadedInitialSettings = true;

        if (FAILED(_settingsLoadedResult))
        {
            if (initialLoad)
            {
                _settings = CascadiaSettings::LoadDefaults();
            }
            else
            {
                const winrt::hstring titleKey = USES_RESOURCE(L"ReloadJsonParseErrorTitle");
                const winrt::hstring textKey = USES_RESOURCE(L"ReloadJsonParseErrorText");
                _ShowLoadErrorsDialog(titleKey, textKey, _settingsLoadedResult);
                return;
            }
        }

        if (initialLoad)
        {
            // Register for directory change notification.
            _RegisterSettingsChange();
            return;
        }

        if (_settingsLoadedResult == S_FALSE)
        {
            _ShowLoadWarningsDialog();
        }

        // Here, we successfully reloaded the settings, and created a new
        // TerminalSettings object.

        // Update the settings in TerminalPage
        _root->SetSettings(_settings, true);

        _ApplyLanguageSettingChange();
        _RefreshThemeRoutine();
        _ApplyStartupTaskStateChange();
        _ProcessLazySettingsChanges();

        _SettingsChangedHandlers(*this, nullptr);
    }

    void AppLogic::_OpenSettingsUI()
    {
        _root->OpenSettingsUI();
    }

    // Method Description:
    // - Returns a pointer to the global shared settings.
    [[nodiscard]] CascadiaSettings AppLogic::GetSettings() const noexcept
    {
        return _settings;
    }

    UIElement AppLogic::GetRoot() noexcept
    {
        return _root.as<winrt::Windows::UI::Xaml::Controls::Control>();
    }

    // Method Description:
    // - Gets the title of the currently focused terminal control. If there
    //   isn't a control selected for any reason, returns "Terminal"
    // Arguments:
    // - <none>
    // Return Value:
    // - the title of the focused control if there is one, else "Terminal"
    hstring AppLogic::Title()
    {
        if (_root)
        {
            return _root->Title();
        }
        return { L"Terminal" };
    }

    // Method Description:
    // - Used to tell the app that the titlebar has been clicked. The App won't
    //   actually receive any clicks in the titlebar area, so this is a helper
    //   to clue the app in that a click has happened. The App will use this as
    //   a indicator that it needs to dismiss any open flyouts.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void AppLogic::TitlebarClicked()
    {
        if (_root)
        {
            _root->TitlebarClicked();
        }
    }

    // Method Description:
    // - Used to tell the PTY connection that the window visibility has changed.
    //   The underlying PTY might need to expose window visibility status to the
    //   client application for the `::GetConsoleWindow()` API.
    // Arguments:
    // - showOrHide - True is show; false is hide.
    // Return Value:
    // - <none>
    void AppLogic::WindowVisibilityChanged(const bool showOrHide)
    {
        if (_root)
        {
            _root->WindowVisibilityChanged(showOrHide);
        }
    }

    // Method Description:
    // - Implements the F7 handler (per GH#638)
    // - Implements the Alt handler (per GH#6421)
    // Return value:
    // - whether the key was handled
    bool AppLogic::OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down)
    {
        if (_root)
        {
            // Manually bubble the OnDirectKeyEvent event up through the focus tree.
            auto xamlRoot{ _root->XamlRoot() };
            auto focusedObject{ Windows::UI::Xaml::Input::FocusManager::GetFocusedElement(xamlRoot) };
            do
            {
                if (auto keyListener{ focusedObject.try_as<IDirectKeyListener>() })
                {
                    if (keyListener.OnDirectKeyEvent(vkey, scanCode, down))
                    {
                        return true;
                    }
                    // otherwise, keep walking. bubble the event manually.
                }

                if (auto focusedElement{ focusedObject.try_as<Windows::UI::Xaml::FrameworkElement>() })
                {
                    focusedObject = focusedElement.Parent();

                    // Parent() seems to return null when the focusedElement is created from an ItemTemplate.
                    // Use the VisualTreeHelper's GetParent as a fallback.
                    if (!focusedObject)
                    {
                        focusedObject = winrt::Windows::UI::Xaml::Media::VisualTreeHelper::GetParent(focusedElement);
                    }
                }
                else
                {
                    break; // we hit a non-FE object, stop bubbling.
                }
            } while (focusedObject);
        }
        return false;
    }

    // Method Description:
    // - Used to tell the app that the 'X' button has been clicked and
    //   the user wants to close the app. We kick off the close warning
    //   experience.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void AppLogic::CloseWindow(LaunchPosition pos)
    {
        if (_root)
        {
            // If persisted layout is enabled and we are the last window closing
            // we should save our state.
            if (_root->ShouldUsePersistedLayout(_settings) && _numOpenWindows == 1)
            {
                if (const auto layout = _root->GetWindowLayout())
                {
                    layout.InitialPosition(pos);
                    const auto state = ApplicationState::SharedInstance();
                    state.PersistedWindowLayouts(winrt::single_threaded_vector<WindowLayout>({ layout }));
                }
            }

            _root->CloseWindow(false);
        }
    }

    winrt::TerminalApp::TaskbarState AppLogic::TaskbarState()
    {
        if (_root)
        {
            return _root->TaskbarState();
        }
        return {};
    }

    winrt::Windows::UI::Xaml::Media::Brush AppLogic::TitlebarBrush()
    {
        if (_root)
        {
            return _root->TitlebarBrush();
        }
        return { nullptr };
    }
    void AppLogic::WindowActivated(const bool activated)
    {
        _root->WindowActivated(activated);
    }

    bool AppLogic::HasCommandlineArguments() const noexcept
    {
        return _hasCommandLineArguments;
    }

    bool AppLogic::HasSettingsStartupActions() const noexcept
    {
        return _hasSettingsStartupActions;
    }

    // Method Description:
    // - Sets the initial commandline to process on startup, and attempts to
    //   parse it. Commands will be parsed into a list of ShortcutActions that
    //   will be processed on TerminalPage::Create().
    // - This function will have no effective result after Create() is called.
    // - This function returns 0, unless a there was a non-zero result from
    //   trying to parse one of the commands provided. In that case, no commands
    //   after the failing command will be parsed, and the non-zero code
    //   returned.
    // Arguments:
    // - args: an array of strings to process as a commandline. These args can contain spaces
    // Return Value:
    // - the result of the first command who's parsing returned a non-zero code,
    //   or 0. (see AppLogic::_ParseArgs)
    int32_t AppLogic::SetStartupCommandline(array_view<const winrt::hstring> args)
    {
        const auto result = _appArgs.ParseArgs(args);
        if (result == 0)
        {
            // If the size of the arguments list is 1,
            // then it contains only the executable name and no other arguments.
            _hasCommandLineArguments = args.size() > 1;
            _appArgs.ValidateStartupCommands();
            if (const auto idx = _appArgs.GetPersistedLayoutIdx())
            {
                _root->SetPersistedLayoutIdx(idx.value());
            }
            _root->SetStartupActions(_appArgs.GetStartupActions());

            // Check if we were started as a COM server for inbound connections of console sessions
            // coming out of the operating system default application feature. If so,
            // tell TerminalPage to start the listener as we have to make sure it has the chance
            // to register a handler to hear about the requests first and is all ready to receive
            // them before the COM server registers itself. Otherwise, the request might come
            // in and be routed to an event with no handlers or a non-ready Page.
            if (_appArgs.IsHandoffListener())
            {
                _root->SetInboundListener(true);
            }
        }

        return result;
    }

    // Method Description:
    // - Triggers the setup of the listener for incoming console connections
    //   from the operating system.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void AppLogic::SetInboundListener()
    {
        _root->SetInboundListener(false);
    }

    // Method Description:
    // - Parse the provided commandline arguments into actions, and try to
    //   perform them immediately.
    // - This function returns 0, unless a there was a non-zero result from
    //   trying to parse one of the commands provided. In that case, no commands
    //   after the failing command will be parsed, and the non-zero code
    //   returned.
    // - If a non-empty cwd is provided, the entire terminal exe will switch to
    //   that CWD while we handle these actions, then return to the original
    //   CWD.
    // Arguments:
    // - args: an array of strings to process as a commandline. These args can contain spaces
    // - cwd: The directory to use as the CWD while performing these actions.
    // Return Value:
    // - the result of the first command who's parsing returned a non-zero code,
    //   or 0. (see AppLogic::_ParseArgs)
    int32_t AppLogic::ExecuteCommandline(array_view<const winrt::hstring> args,
                                         const winrt::hstring& cwd)
    {
        ::TerminalApp::AppCommandlineArgs appArgs;
        auto result = appArgs.ParseArgs(args);
        if (result == 0)
        {
            auto actions = winrt::single_threaded_vector<ActionAndArgs>(std::move(appArgs.GetStartupActions()));

            _root->ProcessStartupActions(actions, false, cwd);

            if (appArgs.IsHandoffListener())
            {
                _root->SetInboundListener(true);
            }
        }
        // Return the result of parsing with commandline, though it may or may not be used.
        return result;
    }

    // Method Description:
    // - Parse the given commandline args in an attempt to find the specified
    //   window. The rest of the args are ignored for now (they'll be handled
    //   whenever the commandline gets to the window it was intended for).
    // - Note that this function will only ever be called by the monarch. A
    //   return value of `0` in this case does not mean "run the commandline in
    //   _this_ process", rather it means "run the commandline in the current
    //   process", whoever that may be.
    // Arguments:
    // - args: an array of strings to process as a commandline. These args can contain spaces
    // Return Value:
    // - 0: We should handle the args "in the current window".
    // - WindowingBehaviorUseNew: We should handle the args in a new window
    // - WindowingBehaviorUseExisting: We should handle the args "in
    //   the current window ON THIS DESKTOP"
    // - WindowingBehaviorUseAnyExisting: We should handle the args "in the current
    //   window ON ANY DESKTOP"
    // - anything else: We should handle the commandline in the window with the given ID.
    TerminalApp::FindTargetWindowResult AppLogic::FindTargetWindow(array_view<const winrt::hstring> args)
    {
        if (!_loadedInitialSettings)
        {
            // Load settings if we haven't already
            ReloadSettings();
        }

        return AppLogic::_doFindTargetWindow(args, _settings.GlobalSettings().WindowingBehavior());
    }

    // The main body of this function is a static helper, to facilitate unit-testing
    TerminalApp::FindTargetWindowResult AppLogic::_doFindTargetWindow(array_view<const winrt::hstring> args,
                                                                      const Microsoft::Terminal::Settings::Model::WindowingMode& windowingBehavior)
    {
        ::TerminalApp::AppCommandlineArgs appArgs;
        const auto result = appArgs.ParseArgs(args);
        if (result == 0)
        {
            if (!appArgs.GetExitMessage().empty())
            {
                return winrt::make<FindTargetWindowResult>(WindowingBehaviorUseNew);
            }

            const std::string parsedTarget{ appArgs.GetTargetWindow() };

            // If the user did not provide any value on the commandline,
            // then lookup our windowing behavior to determine what to do
            // now.
            if (parsedTarget.empty())
            {
                auto windowId = WindowingBehaviorUseNew;
                switch (windowingBehavior)
                {
                case WindowingMode::UseNew:
                    windowId = WindowingBehaviorUseNew;
                    break;
                case WindowingMode::UseExisting:
                    windowId = WindowingBehaviorUseExisting;
                    break;
                case WindowingMode::UseAnyExisting:
                    windowId = WindowingBehaviorUseAnyExisting;
                    break;
                }
                return winrt::make<FindTargetWindowResult>(windowId);
            }

            // Here, the user _has_ provided a window-id on the commandline.
            // What is it? Let's start by checking if it's an int, for the
            // window's ID:
            try
            {
                auto windowId = ::base::saturated_cast<int32_t>(std::stoi(parsedTarget));

                // If the user provides _any_ negative number, then treat it as
                // -1, for "use a new window".
                if (windowId < 0)
                {
                    windowId = -1;
                }

                // Hooray! This is a valid integer. The set of possible values
                // here is {-1, 0, ℤ+}. Let's return that window ID.
                return winrt::make<FindTargetWindowResult>(windowId);
            }
            catch (...)
            {
                // Value was not a valid int. It could be any other string to
                // use as a title though!
                //
                // First, check the reserved keywords:
                if (parsedTarget == "new")
                {
                    return winrt::make<FindTargetWindowResult>(WindowingBehaviorUseNew);
                }
                else if (parsedTarget == "last")
                {
                    return winrt::make<FindTargetWindowResult>(WindowingBehaviorUseExisting);
                }
                else
                {
                    // The string they provided wasn't an int, it wasn't "new"
                    // or "last", so whatever it is, that's the name they get.
                    winrt::hstring winrtName{ til::u8u16(parsedTarget) };
                    return winrt::make<FindTargetWindowResult>(WindowingBehaviorUseName, winrtName);
                }
            }
        }

        // Any unsuccessful parse will be a new window. That new window will try
        // to handle the commandline itself, and find that the commandline
        // failed to parse. When that happens, the new window will display the
        // message box.
        //
        // This will also work for the case where the user specifies an invalid
        // commandline in conjunction with `-w 0`. This function will determine
        // that the commandline has a  parse error, and indicate that we should
        // create a new window. Then, in that new window, we'll try to  set the
        // StartupActions, which will again fail, returning the correct error
        // message.
        return winrt::make<FindTargetWindowResult>(WindowingBehaviorUseNew);
    }

    // Method Description:
    // - If there were any errors parsing the commandline that was used to
    //   initialize the terminal, this will return a string containing that
    //   message. If there were no errors, this message will be blank.
    // - If the user requested help on any command (using --help), this will
    //   contain the help message.
    // - If the user requested the version number (using --version), this will
    //   contain the version string.
    // Arguments:
    // - <none>
    // Return Value:
    // - the help text or error message for the provided commandline, if one
    //   exists, otherwise the empty string.
    winrt::hstring AppLogic::ParseCommandlineMessage()
    {
        return winrt::to_hstring(_appArgs.GetExitMessage());
    }

    // Method Description:
    // - Returns true if we should exit the application before even starting the
    //   window. We might want to do this if we're displaying an error message or
    //   the version string, or if we want to open the settings file.
    // Arguments:
    // - <none>
    // Return Value:
    // - true iff we should exit the application before even starting the window
    bool AppLogic::ShouldExitEarly()
    {
        return _appArgs.ShouldExitEarly();
    }

    bool AppLogic::FocusMode() const
    {
        return _root ? _root->FocusMode() : false;
    }

    bool AppLogic::Fullscreen() const
    {
        return _root ? _root->Fullscreen() : false;
    }

    void AppLogic::Maximized(bool newMaximized)
    {
        if (_root)
        {
            _root->Maximized(newMaximized);
        }
    }

    bool AppLogic::AlwaysOnTop() const
    {
        return _root ? _root->AlwaysOnTop() : false;
    }

    bool AppLogic::AutoHideWindow()
    {
        if (!_loadedInitialSettings)
        {
            // Load settings if we haven't already
            ReloadSettings();
        }

        return _settings.GlobalSettings().AutoHideWindow();
    }

    Windows::Foundation::Collections::IMapView<Microsoft::Terminal::Control::KeyChord, Microsoft::Terminal::Settings::Model::Command> AppLogic::GlobalHotkeys()
    {
        return _settings.GlobalSettings().ActionMap().GlobalHotkeys();
    }

    bool AppLogic::ShouldUsePersistedLayout()
    {
        return _root != nullptr ? _root->ShouldUsePersistedLayout(_settings) : false;
    }

    bool AppLogic::ShouldImmediatelyHandoffToElevated()
    {
        if (!_loadedInitialSettings)
        {
            // Load settings if we haven't already
            ReloadSettings();
        }

        return _root != nullptr ? _root->ShouldImmediatelyHandoffToElevated(_settings) : false;
    }
    void AppLogic::HandoffToElevated()
    {
        if (_root)
        {
            _root->HandoffToElevated(_settings);
        }
    }

    void AppLogic::SaveWindowLayoutJsons(const Windows::Foundation::Collections::IVector<hstring>& layouts)
    {
        std::vector<WindowLayout> converted;
        converted.reserve(layouts.Size());

        for (const auto& json : layouts)
        {
            if (json != L"")
            {
                converted.emplace_back(WindowLayout::FromJson(json));
            }
        }

        ApplicationState::SharedInstance().PersistedWindowLayouts(winrt::single_threaded_vector(std::move(converted)));
    }

    hstring AppLogic::GetWindowLayoutJson(LaunchPosition position)
    {
        if (_root != nullptr)
        {
            if (const auto layout = _root->GetWindowLayout())
            {
                layout.InitialPosition(position);
                return WindowLayout::ToJson(layout);
            }
        }
        return L"";
    }

    void AppLogic::IdentifyWindow()
    {
        if (_root)
        {
            _root->IdentifyWindow();
        }
    }

    winrt::hstring AppLogic::WindowName()
    {
        return _root ? _root->WindowName() : L"";
    }
    void AppLogic::WindowName(const winrt::hstring& name)
    {
        if (_root)
        {
            _root->WindowName(name);
        }
    }
    uint64_t AppLogic::WindowId()
    {
        return _root ? _root->WindowId() : 0;
    }
    void AppLogic::WindowId(const uint64_t& id)
    {
        if (_root)
        {
            _root->WindowId(id);
        }
    }

    void AppLogic::SetPersistedLayoutIdx(const uint32_t idx)
    {
        if (_root)
        {
            _root->SetPersistedLayoutIdx(idx);
        }
    }

    void AppLogic::SetNumberOfOpenWindows(const uint64_t num)
    {
        _numOpenWindows = num;
        if (_root)
        {
            _root->SetNumberOfOpenWindows(num);
        }
    }

    void AppLogic::RenameFailed()
    {
        if (_root)
        {
            _root->RenameFailed();
        }
    }

    bool AppLogic::IsQuakeWindow() const noexcept
    {
        return _root->IsQuakeWindow();
    }

    void AppLogic::RequestExitFullscreen()
    {
        _root->SetFullscreen(false);
    }

    bool AppLogic::GetMinimizeToNotificationArea()
    {
        if (!_loadedInitialSettings)
        {
            // Load settings if we haven't already
            ReloadSettings();
        }

        return _settings.GlobalSettings().MinimizeToNotificationArea();
    }

    bool AppLogic::GetAlwaysShowNotificationIcon()
    {
        if (!_loadedInitialSettings)
        {
            // Load settings if we haven't already
            ReloadSettings();
        }

        return _settings.GlobalSettings().AlwaysShowNotificationIcon();
    }

    bool AppLogic::GetShowTitleInTitlebar()
    {
        return _settings.GlobalSettings().ShowTitleInTitlebar();
    }

    Microsoft::Terminal::Settings::Model::Theme AppLogic::Theme()
    {
        if (!_loadedInitialSettings)
        {
            // Load settings if we haven't already
            ReloadSettings();
        }
        return _settings.GlobalSettings().CurrentTheme();
    }

}
