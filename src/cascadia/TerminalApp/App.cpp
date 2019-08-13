// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "App.h"
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>

#include "App.g.cpp"

using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::System;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace ::TerminalApp;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

// clang-format off
// !!! IMPORTANT !!!
// Make sure that these keys are in the same order as the
// SettingsLoadWarnings/Errors enum is!
static const std::array<std::wstring_view, 2> settingsLoadWarningsLabels {
   L"MissingDefaultProfileText",
   L"DuplicateProfileText"
};
static const std::array<std::wstring_view, 1> settingsLoadErrorsLabels {
    L"NoProfilesText"
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
// - loader: the ScopedResourceLoader to use to look up the localized string.
// Return Value:
// - the localized string for the given type, if it exists.
template<std::size_t N>
static winrt::hstring _GetMessageText(uint32_t index, std::array<std::wstring_view, N> keys, ScopedResourceLoader loader)
{
    if (index < keys.size())
    {
        return loader.GetLocalizedString(keys.at(index));
    }
    return {};
}

// Function Description:
// - Gets the text from our ResourceDictionary for the given
//   SettingsLoadWarning. If there is no such text, we'll return nullptr.
// - The warning should have an entry in settingsLoadWarningsLabels.
// Arguments:
// - warning: the SettingsLoadWarnings value to get the localized text for.
// - loader: the ScopedResourceLoader to use to look up the localized string.
// Return Value:
// - localized text for the given warning
static winrt::hstring _GetWarningText(::TerminalApp::SettingsLoadWarnings warning, ScopedResourceLoader loader)
{
    return _GetMessageText(static_cast<uint32_t>(warning), settingsLoadWarningsLabels, loader);
}

// Function Description:
// - Gets the text from our ResourceDictionary for the given
//   SettingsLoadError. If there is no such text, we'll return nullptr.
// - The warning should have an entry in settingsLoadErrorsLabels.
// Arguments:
// - error: the SettingsLoadErrors value to get the localized text for.
// - loader: the ScopedResourceLoader to use to look up the localized string.
// Return Value:
// - localized text for the given error
static winrt::hstring _GetErrorText(::TerminalApp::SettingsLoadErrors error, ScopedResourceLoader loader)
{
    return _GetMessageText(static_cast<uint32_t>(error), settingsLoadErrorsLabels, loader);
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

namespace winrt::TerminalApp::implementation
{
    App::App() :
        _settings{},
        _dialogLock{},
        _loadedInitialSettings{ false },
        _settingsLoadedResult{ S_OK }
    {
        // For your own sanity, it's better to do setup outside the ctor.
        // If you do any setup in the ctor that ends up throwing an exception,
        // then it might look like App just failed to activate, which will
        // cause you to chase down the rabbit hole of "why is App not
        // registered?" when it definitely is.

        // Initialize will become protected or be deleted when GH#1339 (workaround for MSFT:22116519) are fixed.
        Initialize();

        // The TerminalPage has to be constructed during our construction, to
        // make sure that there's a terminal page for callers of
        // SetTitleBarContent
        _root = winrt::make_self<TerminalPage>();
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
    void App::Create()
    {
        // Assert that we've already loaded our settings. We have to do
        // this as a MTA, before the app is Create()'d
        WINRT_ASSERT(_loadedInitialSettings);

        _root->ShowDialog({ this, &App::_ShowDialog });

        _root->SetSettings(_settings.get(), false);
        _root->Loaded({ this, &App::_OnLoaded });
        _root->Create();

        _ApplyTheme(_settings->GlobalSettings().GetRequestedTheme());

        TraceLoggingWrite(
            g_hTerminalAppProvider,
            "AppCreated",
            TraceLoggingDescription("Event emitted when the application is started"),
            TraceLoggingBool(_settings->GlobalSettings().GetShowTabsInTitlebar(), "TabsInTitlebar"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));
    }

    // Method Description:
    // - Show a ContentDialog with a single button to dismiss. Uses the
    //   FrameworkElements provided as the title and content of this dialog, and
    //   displays a single button to dismiss.
    // - Only one dialog can be visible at a time. If another dialog is visible
    //   when this is called, nothing happens.
    // Arguments:
    // sender: unused
    // dialog: the dialog object that is going to show up
    fire_and_forget App::_ShowDialog(const winrt::Windows::Foundation::IInspectable& sender, winrt::Windows::UI::Xaml::Controls::ContentDialog dialog)
    {
        // DON'T release this lock in a wil::scope_exit. The scope_exit will get
        // called when we await, which is not what we want.
        std::unique_lock lock{ _dialogLock, std::try_to_lock };
        if (!lock)
        {
            // Another dialog is visible.
            return;
        }

        // IMPORTANT: This is necessary as documented in the ContentDialog MSDN docs.
        // Since we're hosting the dialog in a Xaml island, we need to connect it to the
        // xaml tree somehow.
        dialog.XamlRoot(_root->XamlRoot());

        // IMPORTANT: Set the requested theme of the dialog, because the
        // PopupRoot isn't directly in the Xaml tree of our root. So the dialog
        // won't inherit our RequestedTheme automagically.
        dialog.RequestedTheme(_settings->GlobalSettings().GetRequestedTheme());

        // Display the dialog.
        Controls::ContentDialogResult result = co_await dialog.ShowAsync(Controls::ContentDialogPlacement::Popup);

        // After the dialog is dismissed, the dialog lock (held by `lock`) will
        // be released so another can be shown
    }

    // Method Description:
    // - Triggered when the application is fiished loading. If we failed to load
    //   the settings, then this will display the error dialog. This is done
    //   here instead of when loading the settings, because we need our UI to be
    //   visible to display the dialog, and when we're loading the settings,
    //   the UI might not be visible yet.
    // Arguments:
    // - <unused>
    void App::_OnLoaded(const IInspectable& /*sender*/,
                        const RoutedEventArgs& /*eventArgs*/)
    {
        if (FAILED(_settingsLoadedResult))
        {
            const winrt::hstring titleKey = L"InitialJsonParseErrorTitle";
            const winrt::hstring textKey = L"InitialJsonParseErrorText";
            _root->ShowOkDialog(titleKey, textKey);
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
    winrt::Windows::Foundation::Point App::GetLaunchDimensions(uint32_t dpi)
    {
        if (!_loadedInitialSettings)
        {
            // Load settings if we haven't already
            LoadSettings();
        }

        // Use the default profile to determine how big of a window we need.
        TerminalSettings settings = _settings->MakeSettings(std::nullopt);

        // TODO MSFT:21150597 - If the global setting "Always show tab bar" is
        // set, then we'll need to add the height of the tab bar here.

        return TermControl::GetProposedDimensions(settings, dpi);
    }

    bool App::GetShowTabsInTitlebar()
    {
        if (!_loadedInitialSettings)
        {
            // Load settings if we haven't already
            LoadSettings();
        }

        return _settings->GlobalSettings().GetShowTabsInTitlebar();
    }

    // Method Description:
    // - Attempt to load the settings. If we fail for any reason, returns an error.
    // Arguments:
    // - saveOnLoad: If true, after loading the settings, we should re-write
    //   them to the file, to make sure the schema is updated. See
    //   `CascadiaSettings::LoadAll` for details.
    // Return Value:
    // - S_OK if we successfully parsed the settings, otherwise an appropriate HRESULT.
    [[nodiscard]] HRESULT App::_TryLoadSettings(const bool saveOnLoad) noexcept
    {
        HRESULT hr = E_FAIL;

        try
        {
            auto newSettings = CascadiaSettings::LoadAll(saveOnLoad);
            _settings = std::move(newSettings);
            const auto& warnings = _settings->GetWarnings();
            hr = warnings.size() == 0 ? S_OK : S_FALSE;
        }
        catch (const winrt::hresult_error& e)
        {
            hr = e.code();
            _settingsLoadExceptionText = e.message();
            LOG_HR(hr);
        }
        catch (const ::TerminalApp::SettingsException& ex)
        {
            hr = E_INVALIDARG;
            _settingsLoadExceptionText = _GetErrorText(ex.Error(), _resourceLoader);
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
    void App::LoadSettings()
    {
        // Attempt to load the settings.
        // If it fails,
        //  - use Default settings,
        //  - don't persist them (LoadAll won't save them in this case).
        //  - _settingsLoadedResult will be set to an error, indicating that
        //    we should display the loading error.
        //    * We can't display the error now, because we might not have a
        //      UI yet. We'll display the error in _OnLoaded.
        _settingsLoadedResult = _TryLoadSettings(true);

        if (FAILED(_settingsLoadedResult))
        {
            _settings = std::make_unique<CascadiaSettings>();
            _settings->CreateDefaults();
        }

        _loadedInitialSettings = true;

        // Register for directory change notification.
        _RegisterSettingsChange();
    }

    // Method Description:
    // - Registers for changes to the settings folder and upon a updated settings
    //      profile calls _ReloadSettings().
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void App::_RegisterSettingsChange()
    {
        // Get the containing folder.
        std::filesystem::path settingsPath{ CascadiaSettings::GetSettingsPath() };
        const auto folder = settingsPath.parent_path();

        _reader.create(folder.c_str(),
                       false,
                       wil::FolderChangeEvents::All,
                       [this, settingsPath](wil::FolderChangeEvent event, PCWSTR fileModified) {
                           // We want file modifications, AND when files are renamed to be
                           // profiles.json. This second case will oftentimes happen with text
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
    fire_and_forget App::_DispatchReloadSettings()
    {
        static constexpr auto FileActivityQuiesceTime{ std::chrono::milliseconds(50) };
        if (!_settingsReloadQueued.exchange(true))
        {
            co_await winrt::resume_after(FileActivityQuiesceTime);
            _ReloadSettings();
            _settingsReloadQueued.store(false);
        }
    }

    // Method Description:
    // - Reloads the settings from the profile.json.
    void App::_ReloadSettings()
    {
        // Attempt to load our settings.
        // If it fails,
        //  - don't change the settings (and don't actually apply the new settings)
        //  - don't persist them.
        //  - display a loading error
        _settingsLoadedResult = _TryLoadSettings(false);

        if (FAILED(_settingsLoadedResult))
        {
            _root->Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [this]() {
                const winrt::hstring titleKey = L"ReloadJsonParseErrorTitle";
                const winrt::hstring textKey = L"ReloadJsonParseErrorText";
                _root->ShowOkDialog(titleKey, textKey);
            });

            return;
        }
        else if (_settingsLoadedResult == S_FALSE)
        {
            _root.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [this]() {
                _ShowLoadWarningsDialog();
            });
        }

        // Here, we successfully reloaded the settings, and created a new
        // TerminalSettings object.

        // Update the settings in TerminalPage
        _root->SetSettings(_settings.get(), true);

        _root->Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [this]() {
            // Refresh the UI theme
            _ApplyTheme(_settings->GlobalSettings().GetRequestedTheme());
        });
    }

    // Method Description:
    // - Update the current theme of the application. This will trigger our
    //   RequestedThemeChanged event, to have our host change the theme of the
    //   root of the application.
    // Arguments:
    // - newTheme: The ElementTheme to apply to our elements.
    void App::_ApplyTheme(const Windows::UI::Xaml::ElementTheme& newTheme)
    {
        // Propagate the event to the host layer, so it can update its own UI
        _requestedThemeChangedHandlers(*this, newTheme);
    }

    UIElement App::GetRoot() noexcept
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
    hstring App::GetTitle()
    {
        if (_root)
        {
            return _root->GetTitle();
        }
        return { L"Windows Terminal" };
    }

    // Methods that proxy typed event handlers through TerminalPage
    winrt::event_token App::SetTitleBarContent(Windows::Foundation::TypedEventHandler<winrt::Windows::Foundation::IInspectable, winrt::Windows::UI::Xaml::UIElement> const& handler)
    {
        return _root->SetTitleBarContent(handler);
    }
    void App::SetTitleBarContent(winrt::event_token const& token) noexcept
    {
        return _root->SetTitleBarContent(token);
    }

    winrt::event_token App::TitleChanged(Windows::Foundation::TypedEventHandler<winrt::Windows::Foundation::IInspectable, winrt::hstring> const& handler)
    {
        return _root->TitleChanged(handler);
    }
    void App::TitleChanged(winrt::event_token const& token) noexcept
    {
        return _root->TitleChanged(token);
    }

    winrt::event_token App::LastTabClosed(Windows::Foundation::TypedEventHandler<winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::LastTabClosedEventArgs> const& handler)
    {
        return _root->LastTabClosed(handler);
    }
    void App::LastTabClosed(winrt::event_token const& token) noexcept
    {
        return _root->LastTabClosed(token);
    }

    winrt::event_token App::ShowDialog(
        Windows::Foundation::TypedEventHandler<winrt::Windows::Foundation::IInspectable, winrt::Windows::UI::Xaml::Controls::ContentDialog> const& handler)
    {
        return _root->ShowDialog(handler);
    }
    void App::ShowDialog(winrt::event_token const& token) noexcept
    {
        return _root->ShowDialog(token);
    }

    // -------------------------------- WinRT Events ---------------------------------
    // Winrt events need a method for adding a callback to the event and removing the callback.
    // These macros will define them both for you.
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(App, RequestedThemeChanged, _requestedThemeChangedHandlers, TerminalApp::App, winrt::Windows::UI::Xaml::ElementTheme);
}
