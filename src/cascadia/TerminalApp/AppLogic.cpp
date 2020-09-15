// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppLogic.h"
#include "AppLogic.g.cpp"
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>

#include <LibraryResources.h>

using namespace winrt::Windows::ApplicationModel;
using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::System;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace ::TerminalApp;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

static const winrt::hstring StartupTaskName = L"StartTerminalOnLoginTask";
// clang-format off
// !!! IMPORTANT !!!
// Make sure that these keys are in the same order as the
// SettingsLoadWarnings/Errors enum is!
static const std::array<std::wstring_view, static_cast<uint32_t>(SettingsLoadWarnings::WARNINGS_SIZE)> settingsLoadWarningsLabels {
    USES_RESOURCE(L"MissingDefaultProfileText"),
    USES_RESOURCE(L"DuplicateProfileText"),
    USES_RESOURCE(L"UnknownColorSchemeText"),
    USES_RESOURCE(L"InvalidBackgroundImage"),
    USES_RESOURCE(L"InvalidIcon"),
    USES_RESOURCE(L"AtLeastOneKeybindingWarning"),
    USES_RESOURCE(L"TooManyKeysForChord"),
    USES_RESOURCE(L"MissingRequiredParameter"),
    USES_RESOURCE(L"LegacyGlobalsProperty"),
    USES_RESOURCE(L"FailedToParseCommandJson")
};
static const std::array<std::wstring_view, static_cast<uint32_t>(SettingsLoadErrors::ERRORS_SIZE)> settingsLoadErrorsLabels {
    USES_RESOURCE(L"NoProfilesText"),
    USES_RESOURCE(L"AllProfilesHiddenText")
};
// clang-format on

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
template<std::size_t N>
static winrt::hstring _GetMessageText(uint32_t index, std::array<std::wstring_view, N> keys)
{
    if (index < keys.size())
    {
        return GetLibraryResourceString(keys.at(index));
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
    winrt::IInspectable key = winrt::box_value(L"ErrorTextBrush");
    if (resources.HasKey(key))
    {
        winrt::IInspectable g = resources.Lookup(key);
        auto brush = g.try_as<winrt::Windows::UI::Xaml::Media::Brush>();
        textRun.Foreground(brush);
    }

    return textRun;
}

// Method Description:
// - Returns whether the user is either a member of the Administrators group or
//   is currently elevated.
// Return Value:
// - true if the user is an administrator
static bool _isUserAdmin() noexcept
try
{
    SID_IDENTIFIER_AUTHORITY ntAuthority{ SECURITY_NT_AUTHORITY };
    wil::unique_sid adminGroupSid{};
    THROW_IF_WIN32_BOOL_FALSE(AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroupSid));
    BOOL b;
    THROW_IF_WIN32_BOOL_FALSE(CheckTokenMembership(NULL, adminGroupSid.get(), &b));
    return !!b;
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return false;
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
    // Throws:
    // - HR E_INVALIDARG if the app isn't up and running.
    const CascadiaSettings AppLogic::CurrentAppSettings()
    {
        auto appLogic{ ::winrt::TerminalApp::implementation::AppLogic::Current() };
        THROW_HR_IF_NULL(E_INVALIDARG, appLogic);
        return appLogic->GetSettings();
    }

    AppLogic::AppLogic() :
        _dialogLock{},
        _loadedInitialSettings{ false },
        _settingsLoadedResult{ S_OK }
    {
        // For your own sanity, it's better to do setup outside the ctor.
        // If you do any setup in the ctor that ends up throwing an exception,
        // then it might look like App just failed to activate, which will
        // cause you to chase down the rabbit hole of "why is App not
        // registered?" when it definitely is.

        // The TerminalPage has to be constructed during our construction, to
        // make sure that there's a terminal page for callers of
        // SetTitleBarContent
        _isElevated = _isUserAdmin();
        _root = winrt::make_self<TerminalPage>();
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

        _root->SetSettings(_settings, false);
        _root->Loaded({ this, &AppLogic::_OnLoaded });
        _root->Initialized([this](auto&&, auto&&) {
            // GH#288 - When we finish initialization, if the user wanted us
            // launched _fullscreen_, toggle fullscreen mode. This will make sure
            // that the window size is _first_ set up as something sensible, so
            // leaving fullscreen returns to a reasonable size.
            const auto launchMode = this->GetLaunchMode();
            if (launchMode == LaunchMode::FullscreenMode)
            {
                _root->ToggleFullscreen();
            }
        });
        _root->Create();

        _ApplyTheme(_settings.GlobalSettings().Theme());
        _ApplyStartupTaskStateChange();

        TraceLoggingWrite(
            g_hTerminalAppProvider,
            "AppCreated",
            TraceLoggingDescription("Event emitted when the application is started"),
            TraceLoggingBool(_settings.GlobalSettings().ShowTabsInTitlebar(), "TabsInTitlebar"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));
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
            auto theme{ _settings.GlobalSettings().Theme() };
            auto element{ sender.try_as<winrt::Windows::UI::Xaml::FrameworkElement>() };
            while (element)
            {
                element.RequestedTheme(theme);
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

        const auto warnings = _settings.Warnings();
        for (const auto& warning : warnings)
        {
            // Try looking up the warning message key for each warning.
            const auto warningText = _GetWarningText(warning);
            if (!warningText.empty())
            {
                warningsTextBlock.Inlines().Append(_BuildErrorRun(warningText, ::winrt::Windows::UI::Xaml::Application::Current().as<::winrt::TerminalApp::App>().Resources()));

                // The "LegacyGlobalsProperty" warning is special - it has a URL
                // that goes with it. So we need to manually construct a
                // Hyperlink and insert it along with the warning text.
                if (warning == SettingsLoadWarnings::LegacyGlobalsProperty)
                {
                    // Add the URL here too
                    const auto legacyGlobalsLinkLabel = RS_(L"LegacyGlobalsPropertyHrefLabel");
                    const auto legacyGlobalsLinkUriValue = RS_(L"LegacyGlobalsPropertyHrefUrl");

                    winrt::Windows::UI::Xaml::Documents::Run legacyGlobalsLinkText;
                    winrt::Windows::UI::Xaml::Documents::Hyperlink legacyGlobalsLink;
                    winrt::Windows::Foundation::Uri legacyGlobalsLinkUri{ legacyGlobalsLinkUriValue };

                    legacyGlobalsLinkText.Text(legacyGlobalsLinkLabel);
                    legacyGlobalsLink.NavigateUri(legacyGlobalsLinkUri);
                    legacyGlobalsLink.Inlines().Append(legacyGlobalsLinkText);

                    warningsTextBlock.Inlines().Append(legacyGlobalsLink);
                }

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
            LoadSettings();
        }

        // Use the default profile to determine how big of a window we need.
        const auto [_, settings] = TerminalSettings::BuildSettings(_settings, nullptr, nullptr);

        auto proposedSize = TermControl::GetProposedDimensions(settings, dpi);

        const float scale = static_cast<float>(dpi) / static_cast<float>(USER_DEFAULT_SCREEN_DPI);

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

            // For whatever reason, there's about 6px of unaccounted-for space
            // in the application. I couldn't tell you where these 6px are
            // coming from, but they need to be included in this math.
            proposedSize.Width += (tabControl.DesiredSize().Height + 6) * scale;
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
            LoadSettings();
        }

        // GH#4620/#5801 - If the user passed --maximized or --fullscreen on the
        // commandline, then use that to override the value from the settings.
        const auto valueFromSettings = _settings.GlobalSettings().LaunchMode();
        const auto valueFromCommandlineArgs = _appArgs.GetLaunchMode();
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
            LoadSettings();
        }

        const auto initialPosition{ _settings.GlobalSettings().InitialPosition() };
        return {
            initialPosition.X ? initialPosition.X.Value() : defaultInitialX,
            initialPosition.Y ? initialPosition.Y.Value() : defaultInitialY
        };
    }

    winrt::Windows::UI::Xaml::ElementTheme AppLogic::GetRequestedTheme()
    {
        if (!_loadedInitialSettings)
        {
            // Load settings if we haven't already
            LoadSettings();
        }

        return _settings.GlobalSettings().Theme();
    }

    bool AppLogic::GetShowTabsInTitlebar()
    {
        if (!_loadedInitialSettings)
        {
            // Load settings if we haven't already
            LoadSettings();
        }

        return _settings.GlobalSettings().ShowTabsInTitlebar();
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
        HRESULT hr = E_FAIL;

        try
        {
            auto newSettings = _isUwp ? CascadiaSettings::LoadUniversal() : CascadiaSettings::LoadAll();
            _settings = newSettings;

            if (_settings.GetLoadingError())
            {
                _settingsLoadExceptionText = _GetErrorText(_settings.GetLoadingError().Value());
                return E_INVALIDARG;
            }
            else if (!_settings.GetSerializationErrorMessage().empty())
            {
                _settingsLoadExceptionText = _settings.GetSerializationErrorMessage();
                return E_INVALIDARG;
            }

            hr = _settings.Warnings().Size() == 0 ? S_OK : S_FALSE;
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

    // Method Description:
    // - Initialized our settings. See CascadiaSettings for more details.
    //      Additionally hooks up our callbacks for keybinding events to the
    //      keybindings object.
    // NOTE: This must be called from a MTA if we're running as a packaged
    //      application. The Windows.Storage APIs require a MTA. If this isn't
    //      happening during startup, it'll need to happen on a background thread.
    void AppLogic::LoadSettings()
    {
        auto start = std::chrono::high_resolution_clock::now();

        TraceLoggingWrite(
            g_hTerminalAppProvider,
            "SettingsLoadStarted",
            TraceLoggingDescription("Event emitted before loading the settings"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));

        // Attempt to load the settings.
        // If it fails,
        //  - use Default settings,
        //  - don't persist them (LoadAll won't save them in this case).
        //  - _settingsLoadedResult will be set to an error, indicating that
        //    we should display the loading error.
        //    * We can't display the error now, because we might not have a
        //      UI yet. We'll display the error in _OnLoaded.
        _settingsLoadedResult = _TryLoadSettings();

        if (FAILED(_settingsLoadedResult))
        {
            _settings = CascadiaSettings::LoadDefaults();
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> delta = end - start;

        TraceLoggingWrite(
            g_hTerminalAppProvider,
            "SettingsLoadComplete",
            TraceLoggingDescription("Event emitted when loading the settings is finished"),
            TraceLoggingFloat64(delta.count(), "Duration"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));

        _loadedInitialSettings = true;

        // Register for directory change notification.
        _RegisterSettingsChange();

        Jumplist::UpdateJumplist(_settings);
    }

    // Method Description:
    // - Registers for changes to the settings folder and upon a updated settings
    //      profile calls _ReloadSettings().
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void AppLogic::_RegisterSettingsChange()
    {
        // Get the containing folder.
        const std::filesystem::path settingsPath{ CascadiaSettings::SettingsPath().c_str() };
        const auto folder = settingsPath.parent_path();

        _reader.create(folder.c_str(),
                       false,
                       wil::FolderChangeEvents::All,
                       [this, settingsPath](wil::FolderChangeEvent event, PCWSTR fileModified) {
                           // We want file modifications, AND when files are renamed to be
                           // settings.json. This second case will oftentimes happen with text
                           // editors, who will write a temp file, then rename it to be the
                           // actual file you wrote. So listen for that too.
                           if (!(event == wil::FolderChangeEvent::Modified ||
                                 event == wil::FolderChangeEvent::RenameNewName))
                           {
                               return;
                           }

                           std::filesystem::path modifiedFilePath = fileModified;

                           // Getting basename (filename.ext)
                           const auto settingsBasename = settingsPath.filename();
                           const auto modifiedBasename = modifiedFilePath.filename();

                           if (settingsBasename == modifiedBasename)
                           {
                               this->_DispatchReloadSettings();
                           }
                       });
    }

    // Method Description:
    // - Dispatches a settings reload with debounce.
    //   Text editors implement Save in a bunch of different ways, so
    //   this stops us from reloading too many times or too quickly.
    fire_and_forget AppLogic::_DispatchReloadSettings()
    {
        static constexpr auto FileActivityQuiesceTime{ std::chrono::milliseconds(50) };
        if (!_settingsReloadQueued.exchange(true))
        {
            co_await winrt::resume_after(FileActivityQuiesceTime);
            _ReloadSettings();
            _settingsReloadQueued.store(false);
        }
    }

    fire_and_forget AppLogic::_LoadErrorsDialogRoutine()
    {
        co_await winrt::resume_foreground(_root->Dispatcher());

        const winrt::hstring titleKey = USES_RESOURCE(L"ReloadJsonParseErrorTitle");
        const winrt::hstring textKey = USES_RESOURCE(L"ReloadJsonParseErrorText");
        _ShowLoadErrorsDialog(titleKey, textKey, _settingsLoadedResult);
    }

    fire_and_forget AppLogic::_ShowLoadWarningsDialogRoutine()
    {
        co_await winrt::resume_foreground(_root->Dispatcher());

        _ShowLoadWarningsDialog();
    }

    fire_and_forget AppLogic::_RefreshThemeRoutine()
    {
        co_await winrt::resume_foreground(_root->Dispatcher());

        // Refresh the UI theme
        _ApplyTheme(_settings.GlobalSettings().Theme());
    }

    fire_and_forget AppLogic::_ApplyStartupTaskStateChange()
    try
    {
        // First, make sure we're running in a packaged context. This method
        // won't work, and will crash mysteriously if we're running unpackaged.
        const auto package{ winrt::Windows::ApplicationModel::Package::Current() };
        if (package == nullptr)
        {
            return;
        }

        auto weakThis{ get_weak() };
        co_await winrt::resume_foreground(_root->Dispatcher(), CoreDispatcherPriority::Normal);
        if (auto page{ weakThis.get() })
        {
            StartupTaskState state;
            bool tryEnableStartupTask = _settings.GlobalSettings().StartOnUserLogin();
            StartupTask task = co_await StartupTask::GetAsync(StartupTaskName);

            state = task.State();
            switch (state)
            {
            case StartupTaskState::Disabled:
            {
                if (tryEnableStartupTask)
                {
                    co_await task.RequestEnableAsync();
                }
                break;
            }
            case StartupTaskState::DisabledByUser:
            {
                // TODO: GH#6254: define UX for other StartupTaskStates
                break;
            }
            case StartupTaskState::Enabled:
            {
                if (!tryEnableStartupTask)
                {
                    task.Disable();
                }
                break;
            }
            }
        }
    }
    CATCH_LOG();

    // Method Description:
    // - Reloads the settings from the profile.json.
    void AppLogic::_ReloadSettings()
    {
        // Attempt to load our settings.
        // If it fails,
        //  - don't change the settings (and don't actually apply the new settings)
        //  - don't persist them.
        //  - display a loading error
        _settingsLoadedResult = _TryLoadSettings();

        if (FAILED(_settingsLoadedResult))
        {
            _LoadErrorsDialogRoutine();
            return;
        }
        else if (_settingsLoadedResult == S_FALSE)
        {
            _ShowLoadWarningsDialogRoutine();
        }

        // Here, we successfully reloaded the settings, and created a new
        // TerminalSettings object.

        // Update the settings in TerminalPage
        _root->SetSettings(_settings, true);

        _RefreshThemeRoutine();
        _ApplyStartupTaskStateChange();

        Jumplist::UpdateJumplist(_settings);
    }

    // Method Description:
    // - Returns a pointer to the global shared settings.
    [[nodiscard]] CascadiaSettings AppLogic::GetSettings() const noexcept
    {
        return _settings;
    }

    // Method Description:
    // - Update the current theme of the application. This will trigger our
    //   RequestedThemeChanged event, to have our host change the theme of the
    //   root of the application.
    // Arguments:
    // - newTheme: The ElementTheme to apply to our elements.
    void AppLogic::_ApplyTheme(const Windows::UI::Xaml::ElementTheme& newTheme)
    {
        // Propagate the event to the host layer, so it can update its own UI
        _requestedThemeChangedHandlers(*this, newTheme);
    }

    UIElement AppLogic::GetRoot() noexcept
    {
        return _root.as<winrt::Windows::UI::Xaml::Controls::Control>();
    }

    // Method Description:
    // - Gets the title of the currently focused terminal control. If there
    //   isn't a control selected for any reason, returns "Windows Terminal"
    // Arguments:
    // - <none>
    // Return Value:
    // - the title of the focused control if there is one, else "Windows Terminal"
    hstring AppLogic::Title()
    {
        if (_root)
        {
            return _root->Title();
        }
        return { L"Windows Terminal" };
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
    void AppLogic::WindowCloseButtonClicked()
    {
        if (_root)
        {
            _root->CloseWindow();
        }
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
            _appArgs.ValidateStartupCommands();
            _root->SetStartupActions(_appArgs.GetStartupActions());
        }

        return result;
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

    bool AppLogic::AlwaysOnTop() const
    {
        return _root ? _root->AlwaysOnTop() : false;
    }

    // -------------------------------- WinRT Events ---------------------------------
    // Winrt events need a method for adding a callback to the event and removing the callback.
    // These macros will define them both for you.
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(AppLogic, RequestedThemeChanged, _requestedThemeChangedHandlers, winrt::Windows::Foundation::IInspectable, winrt::Windows::UI::Xaml::ElementTheme);
}
