// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "App.h"
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>

#include "App.g.cpp"
#include "TerminalPage.h"

using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::System;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace ::TerminalApp;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::TerminalApp::implementation
{
    App::App() :
        _settings{},
        _tabs{},
        _loadedInitialSettings{ false },
        _settingsLoadedResult{ S_OK },
        _dialogLock{},
        _resourceLoader{ L"TerminalApp/Resources" }
    {
        // For your own sanity, it's better to do setup outside the ctor.
        // If you do any setup in the ctor that ends up throwing an exception,
        // then it might look like App just failed to activate, which will
        // cause you to chase down the rabbit hole of "why is App not
        // registered?" when it definitely is.

        // Initialize will become protected or be deleted when GH#1339 (workaround for MSFT:22116519) are fixed.
        Initialize();
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

        /* !!! TODO
           This is not the correct way to host a XAML page. This exists today because we valued
           getting a .xaml over tearing out all of the terminal logic and splitting it across App
           and Page.
           The work to clarify the boundary between app global state and "terminal page" state
           is tracked in GH#1878.
        */
        auto terminalPage = winrt::make_self<TerminalPage>();
        _root = terminalPage.as<winrt::Windows::UI::Xaml::Controls::Control>();
        _tabContent = terminalPage->TabContent();
        _tabRow = terminalPage->TabRow();
        _tabView = _tabRow.TabView();
        _newTabButton = _tabRow.NewTabButton();

        if (_settings->GlobalSettings().GetShowTabsInTitlebar())
        {
            // Remove the TabView from the page. We'll hang on to it, we need to
            // put it in the titlebar.
            uint32_t index = 0;
            if (terminalPage->Root().Children().IndexOf(_tabRow, index))
            {
                terminalPage->Root().Children().RemoveAt(index);
            }

            // Inform the host that our titlebar content has changed.
            _setTitleBarContentHandlers(*this, _tabRow);
        }

        // Event Bindings (Early)
        _newTabButton.Click([this](auto&&, auto&&) {
            this->_OpenNewTab(std::nullopt);
        });
        _tabView.SelectionChanged({ this, &App::_OnTabSelectionChanged });
        _tabView.TabClosing({ this, &App::_OnTabClosing });
        _tabView.Items().VectorChanged({ this, &App::_OnTabItemsChanged });
        _root.Loaded({ this, &App::_OnLoaded });

        _CreateNewTabFlyout();
        _OpenNewTab(std::nullopt);

        _tabContent.SizeChanged({ this, &App::_OnContentSizeChanged });

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
    // - titleElement: the element to use as the title of this ContentDialog
    // - contentElement: the element to use as the content of this ContentDialog
    // - closeButtonText: The string to use on the close button
    fire_and_forget App::_ShowDialog(const IInspectable& titleElement,
                                     const IInspectable& contentElement,
                                     const winrt::hstring& closeButtonText)
    {
        // DON'T release this lock in a wil::scope_exit. The scope_exit will get
        // called when we await, which is not what we want.
        std::unique_lock lock{ _dialogLock, std::try_to_lock };
        if (!lock)
        {
            // Another dialog is visible.
            return;
        }

        Controls::ContentDialog dialog;
        dialog.Title(titleElement);
        dialog.Content(contentElement);
        dialog.CloseButtonText(closeButtonText);

        // IMPORTANT: This is necessary as documented in the ContentDialog MSDN docs.
        // Since we're hosting the dialog in a Xaml island, we need to connect it to the
        // xaml tree somehow.
        dialog.XamlRoot(_root.XamlRoot());

        // IMPORTANT: Set the requested theme of the dialog, because the
        // PopupRoot isn't directly in the Xaml tree of our root. So the dialog
        // won't inherit our RequestedTheme automagically.
        dialog.RequestedTheme(_settings->GlobalSettings().GetRequestedTheme());

        // Display the dialog.
        Controls::ContentDialogResult result = co_await dialog.ShowAsync(Controls::ContentDialogPlacement::Popup);

        // After the dialog is dismissed, the dialog lock (held by `lock`) will
        // be released so another can be shown.
    }

    // Method Description:
    // - Show a ContentDialog with a single "Ok" button to dismiss. Looks up the
    //   the title and text from our Resources using the provided keys.
    // - Only one dialog can be visible at a time. If another dialog is visible
    //   when this is called, nothing happens. See _ShowDialog for details
    // Arguments:
    // - titleKey: The key to use to lookup the title text from our resources.
    // - contentKey: The key to use to lookup the content text from our resources.
    void App::_ShowOkDialog(const winrt::hstring& titleKey,
                            const winrt::hstring& contentKey)
    {
        auto title = _resourceLoader.GetLocalizedString(titleKey);
        auto message = _resourceLoader.GetLocalizedString(contentKey);
        auto buttonText = _resourceLoader.GetLocalizedString(L"Ok");

        _ShowDialog(winrt::box_value(title), winrt::box_value(message), buttonText);
    }

    // Method Description:
    // - Show a dialog with "About" information. Displays the app's Display
    //   Name, version, getting started link, documentation link, and release
    //   Notes link.
    void App::_ShowAboutDialog()
    {
        const auto title = _resourceLoader.GetLocalizedString(L"AboutTitleText");
        const auto versionLabel = _resourceLoader.GetLocalizedString(L"VersionLabelText");
        const auto gettingStartedLabel = _resourceLoader.GetLocalizedString(L"GettingStartedLabelText");
        const auto documentationLabel = _resourceLoader.GetLocalizedString(L"DocumentationLabelText");
        const auto releaseNotesLabel = _resourceLoader.GetLocalizedString(L"ReleaseNotesLabelText");
        const auto gettingStartedUriValue = _resourceLoader.GetLocalizedString(L"GettingStartedUriValue");
        const auto documentationUriValue = _resourceLoader.GetLocalizedString(L"DocumentationUriValue");
        const auto releaseNotesUriValue = _resourceLoader.GetLocalizedString(L"ReleaseNotesUriValue");
        const auto package = winrt::Windows::ApplicationModel::Package::Current();
        const auto packageName = package.DisplayName();
        const auto version = package.Id().Version();
        winrt::Windows::UI::Xaml::Documents::Run about;
        winrt::Windows::UI::Xaml::Documents::Run gettingStarted;
        winrt::Windows::UI::Xaml::Documents::Run documentation;
        winrt::Windows::UI::Xaml::Documents::Run releaseNotes;
        winrt::Windows::UI::Xaml::Documents::Hyperlink gettingStartedLink;
        winrt::Windows::UI::Xaml::Documents::Hyperlink documentationLink;
        winrt::Windows::UI::Xaml::Documents::Hyperlink releaseNotesLink;
        std::wstringstream aboutTextStream;

        gettingStarted.Text(gettingStartedLabel);
        documentation.Text(documentationLabel);
        releaseNotes.Text(releaseNotesLabel);

        winrt::Windows::Foundation::Uri gettingStartedUri{ gettingStartedUriValue };
        winrt::Windows::Foundation::Uri documentationUri{ documentationUriValue };
        winrt::Windows::Foundation::Uri releaseNotesUri{ releaseNotesUriValue };

        gettingStartedLink.NavigateUri(gettingStartedUri);
        documentationLink.NavigateUri(documentationUri);
        releaseNotesLink.NavigateUri(releaseNotesUri);

        gettingStartedLink.Inlines().Append(gettingStarted);
        documentationLink.Inlines().Append(documentation);
        releaseNotesLink.Inlines().Append(releaseNotes);

        // Format our about text. It will look like the following:
        // <Display Name>
        // Version: <Major>.<Minor>.<Build>.<Revision>
        // Getting Started
        // Documentation
        // Release Notes

        aboutTextStream << packageName.c_str() << L"\n";

        aboutTextStream << versionLabel.c_str() << L" ";
        aboutTextStream << version.Major << L"." << version.Minor << L"." << version.Build << L"." << version.Revision << L"\n";

        winrt::hstring aboutText{ aboutTextStream.str() };
        about.Text(aboutText);

        const auto buttonText = _resourceLoader.GetLocalizedString(L"Ok");

        Controls::TextBlock aboutTextBlock;
        aboutTextBlock.Inlines().Append(about);
        aboutTextBlock.Inlines().Append(gettingStartedLink);
        aboutTextBlock.Inlines().Append(documentationLink);
        aboutTextBlock.Inlines().Append(releaseNotesLink);
        aboutTextBlock.IsTextSelectionEnabled(true);

        _ShowDialog(winrt::box_value(title), aboutTextBlock, buttonText);
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
            _ShowOkDialog(titleKey, textKey);
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
    // - Builds the flyout (dropdown) attached to the new tab button, and
    //   attaches it to the button. Populates the flyout with one entry per
    //   Profile, displaying the profile's name. Clicking each flyout item will
    //   open a new tab with that profile.
    //   Below the profiles are the static menu items: settings, feedback
    void App::_CreateNewTabFlyout()
    {
        auto newTabFlyout = Controls::MenuFlyout{};
        auto keyBindings = _settings->GetKeybindings();

        const GUID defaultProfileGuid = _settings->GlobalSettings().GetDefaultProfile();
        for (int profileIndex = 0; profileIndex < _settings->GetProfiles().size(); profileIndex++)
        {
            const auto& profile = _settings->GetProfiles()[profileIndex];
            auto profileMenuItem = Controls::MenuFlyoutItem{};

            // add the keyboard shortcuts for the first 9 profiles
            if (profileIndex < 9)
            {
                // enum value for ShortcutAction::NewTabProfileX; 0==NewTabProfile0
                auto profileKeyChord = keyBindings.GetKeyBinding(static_cast<ShortcutAction>(profileIndex + static_cast<int>(ShortcutAction::NewTabProfile0)));

                // make sure we find one to display
                if (profileKeyChord)
                {
                    _SetAcceleratorForMenuItem(profileMenuItem, profileKeyChord);
                }
            }

            auto profileName = profile.GetName();
            winrt::hstring hName{ profileName };
            profileMenuItem.Text(hName);

            // If there's an icon set for this profile, set it as the icon for
            // this flyout item.
            if (profile.HasIcon())
            {
                profileMenuItem.Icon(_GetIconFromProfile(profile));
            }

            if (profile.GetGuid() == defaultProfileGuid)
            {
                // Contrast the default profile with others in font weight.
                profileMenuItem.FontWeight(FontWeights::Bold());
            }

            profileMenuItem.Click([this, profileIndex](auto&&, auto&&) {
                this->_OpenNewTab({ profileIndex });
            });
            newTabFlyout.Items().Append(profileMenuItem);
        }

        // add menu separator
        auto separatorItem = Controls::MenuFlyoutSeparator{};
        newTabFlyout.Items().Append(separatorItem);

        // add static items
        {
            // Create the settings button.
            auto settingsItem = Controls::MenuFlyoutItem{};
            settingsItem.Text(_resourceLoader.GetLocalizedString(L"SettingsMenuItem"));

            Controls::SymbolIcon ico{};
            ico.Symbol(Controls::Symbol::Setting);
            settingsItem.Icon(ico);

            settingsItem.Click({ this, &App::_SettingsButtonOnClick });
            newTabFlyout.Items().Append(settingsItem);

            auto settingsKeyChord = keyBindings.GetKeyBinding(ShortcutAction::OpenSettings);
            if (settingsKeyChord)
            {
                _SetAcceleratorForMenuItem(settingsItem, settingsKeyChord);
            }

            // Create the feedback button.
            auto feedbackFlyout = Controls::MenuFlyoutItem{};
            feedbackFlyout.Text(_resourceLoader.GetLocalizedString(L"FeedbackMenuItem"));

            Controls::FontIcon feedbackIco{};
            feedbackIco.Glyph(L"\xE939");
            feedbackIco.FontFamily(Media::FontFamily{ L"Segoe MDL2 Assets" });
            feedbackFlyout.Icon(feedbackIco);

            feedbackFlyout.Click({ this, &App::_FeedbackButtonOnClick });
            newTabFlyout.Items().Append(feedbackFlyout);

            // Create the about button.
            auto aboutFlyout = Controls::MenuFlyoutItem{};
            aboutFlyout.Text(_resourceLoader.GetLocalizedString(L"AboutMenuItem"));

            Controls::SymbolIcon aboutIco{};
            aboutIco.Symbol(Controls::Symbol::Help);
            aboutFlyout.Icon(aboutIco);

            aboutFlyout.Click({ this, &App::_AboutButtonOnClick });
            newTabFlyout.Items().Append(aboutFlyout);
        }

        _newTabButton.Flyout(newTabFlyout);
    }

    // Function Description:
    // - Called when the settings button is clicked. ShellExecutes the settings
    //   file, as to open it in the default editor for .json files. Does this in
    //   a background thread, as to not hang/crash the UI thread.
    fire_and_forget LaunchSettings()
    {
        // This will switch the execution of the function to a background (not
        // UI) thread. This is IMPORTANT, because the Windows.Storage API's
        // (used for retrieving the path to the file) will crash on the UI
        // thread, because the main thread is a STA.
        co_await winrt::resume_background();

        const auto settingsPath = CascadiaSettings::GetSettingsPath();

        HINSTANCE res = ShellExecute(nullptr, nullptr, settingsPath.c_str(), nullptr, nullptr, SW_SHOW);
        if (static_cast<int>(reinterpret_cast<uintptr_t>(res)) <= 32)
        {
            ShellExecute(nullptr, nullptr, L"notepad", settingsPath.c_str(), nullptr, SW_SHOW);
        }
    }

    // Method Description:
    // - Called when the settings button is clicked. Launches a background
    //   thread to open the settings file in the default JSON editor.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void App::_SettingsButtonOnClick(const IInspectable&,
                                     const RoutedEventArgs&)
    {
        LaunchSettings();
    }

    // Method Description:
    // - Called when the feedback button is clicked. Launches github in your
    //   default browser, navigated to the "issues" page of the Terminal repo.
    void App::_FeedbackButtonOnClick(const IInspectable&,
                                     const RoutedEventArgs&)
    {
        const auto feedbackUriValue = _resourceLoader.GetLocalizedString(L"FeedbackUriValue");

        winrt::Windows::System::Launcher::LaunchUriAsync({ feedbackUriValue });
    }

    // Method Description:
    // - Called when the about button is clicked. See _ShowAboutDialog for more info.
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void App::_AboutButtonOnClick(const IInspectable&,
                                  const RoutedEventArgs&)
    {
        _ShowAboutDialog();
    }

    // Method Description:
    // - Register our event handlers with the given keybindings object. This
    //   should be done regardless of what the events are actually bound to -
    //   this simply ensures the AppKeyBindings object will call us correctly
    //   for each event.
    // Arguments:
    // - bindings: A AppKeyBindings object to wire up with our event handlers
    void App::_HookupKeyBindings(TerminalApp::AppKeyBindings bindings) noexcept
    {
        // Hook up the KeyBinding object's events to our handlers.
        // They should all be hooked up here, regardless of whether or not
        //      there's an actual keychord for them.
        bindings.NewTab([this]() { _OpenNewTab(std::nullopt); });
        bindings.DuplicateTab([this]() { _DuplicateTabViewItem(); });
        bindings.CloseTab([this]() { _CloseFocusedTab(); });
        bindings.ClosePane([this]() { _CloseFocusedPane(); });
        bindings.NewTabWithProfile([this](const auto index) { _OpenNewTab({ index }); });
        bindings.ScrollUp([this]() { _Scroll(-1); });
        bindings.ScrollDown([this]() { _Scroll(1); });
        bindings.NextTab([this]() { _SelectNextTab(true); });
        bindings.PrevTab([this]() { _SelectNextTab(false); });
        bindings.SplitVertical([this]() { _SplitVertical(std::nullopt); });
        bindings.SplitHorizontal([this]() { _SplitHorizontal(std::nullopt); });
        bindings.ScrollUpPage([this]() { _ScrollPage(-1); });
        bindings.ScrollDownPage([this]() { _ScrollPage(1); });
        bindings.SwitchToTab([this](const auto index) { _SelectTab({ index }); });
        bindings.OpenSettings([this]() { _OpenSettings(); });
        bindings.ResizePane([this](const auto direction) { _ResizePane(direction); });
        bindings.MoveFocus([this](const auto direction) { _MoveFocus(direction); });
        bindings.CopyText([this](const auto trimWhitespace) { _CopyText(trimWhitespace); });
        bindings.PasteText([this]() { _PasteText(); });
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
            hr = S_OK;
        }
        catch (const winrt::hresult_error& e)
        {
            hr = e.code();
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

        _HookupKeyBindings(_settings->GetKeybindings());

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
            _root.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [this]() {
                const winrt::hstring titleKey = L"ReloadJsonParseErrorTitle";
                const winrt::hstring textKey = L"ReloadJsonParseErrorText";
                _ShowOkDialog(titleKey, textKey);
            });

            return;
        }

        // Here, we successfully reloaded the settings, and created a new
        // TerminalSettings object.

        // Re-wire the keybindings to their handlers, as we'll have created a
        // new AppKeyBindings object.
        _HookupKeyBindings(_settings->GetKeybindings());

        // Refresh UI elements

        auto profiles = _settings->GetProfiles();
        for (auto& profile : profiles)
        {
            const GUID profileGuid = profile.GetGuid();
            TerminalSettings settings = _settings->MakeSettings(profileGuid);

            for (auto& tab : _tabs)
            {
                // Attempt to reload the settings of any panes with this profile
                tab->UpdateSettings(settings, profileGuid);
            }
        }

        // Update the icon of the tab for the currently focused profile in that tab.
        for (auto& tab : _tabs)
        {
            _UpdateTabIcon(tab);
            _UpdateTitle(tab);
        }

        _root.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [this]() {
            // Refresh the UI theme
            _ApplyTheme(_settings->GlobalSettings().GetRequestedTheme());

            // repopulate the new tab button's flyout with entries for each
            // profile, which might have changed
            _CreateNewTabFlyout();
        });
    }

    // Method Description:
    // - Get the icon of the currently focused terminal control, and set its
    //   tab's icon to that icon.
    // Arguments:
    // - tab: the Tab to update the title for.
    void App::_UpdateTabIcon(std::shared_ptr<Tab> tab)
    {
        const auto lastFocusedProfileOpt = tab->GetFocusedProfile();
        if (lastFocusedProfileOpt.has_value())
        {
            const auto lastFocusedProfile = lastFocusedProfileOpt.value();

            auto tabViewItem = tab->GetTabViewItem();
            tabViewItem.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [this, lastFocusedProfile, tabViewItem]() {
                // _GetIconFromProfile has to run on the main thread
                const auto* const matchingProfile = _settings->FindProfile(lastFocusedProfile);
                if (matchingProfile)
                {
                    tabViewItem.Icon(App::_GetIconFromProfile(*matchingProfile));
                }
            });
        }
    }

    // Method Description:
    // - Get the title of the currently focused terminal control, and set it's
    //   tab's text to that text. If this tab is the focused tab, then also
    //   bubble this title to any listeners of our TitleChanged event.
    // Arguments:
    // - tab: the Tab to update the title for.
    void App::_UpdateTitle(std::shared_ptr<Tab> tab)
    {
        auto newTabTitle = tab->GetFocusedTitle();
        const auto lastFocusedProfileOpt = tab->GetFocusedProfile();
        if (lastFocusedProfileOpt.has_value())
        {
            const auto lastFocusedProfile = lastFocusedProfileOpt.value();
            const auto* const matchingProfile = _settings->FindProfile(lastFocusedProfile);

            const auto tabTitle = matchingProfile->GetTabTitle();

            // Checks if tab title has been set in the profile settings and
            // updates accordingly.

            const auto newActualTitle = tabTitle.empty() ? newTabTitle : tabTitle;

            tab->SetTabText(winrt::to_hstring(newActualTitle.data()));
            if (_settings->GlobalSettings().GetShowTitleInTitlebar() &&
                tab->IsFocused())
            {
                _titleChangeHandlers(newActualTitle);
            }
        }
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
        return _root;
    }

    void App::_SetFocusedTabIndex(int tabIndex)
    {
        // GH#1117: This is a workaround because _tabView.SelectedIndex(tabIndex)
        //          sometimes set focus to an incorrect tab after removing some tabs
        auto tab = _tabs.at(tabIndex);
        _tabView.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [tab, this]() {
            auto tabViewItem = tab->GetTabViewItem();
            _tabView.SelectedItem(tabViewItem);
        });
    }

    // Method Description:
    // - Handle changes in tab layout.
    void App::_UpdateTabView()
    {
        // Show tabs when there's more than 1, or the user has chosen to always
        // show the tab bar.
        const bool isVisible = _settings->GlobalSettings().GetShowTabsInTitlebar() ||
                               (_tabs.size() > 1) ||
                               _settings->GlobalSettings().GetAlwaysShowTabs();

        // collapse/show the tabs themselves
        _tabView.Visibility(isVisible ? Visibility::Visible : Visibility::Collapsed);

        // collapse/show the row that the tabs are in.
        // NaN is the special value XAML uses for "Auto" sizing.
        _tabRow.Height(isVisible ? NAN : 0);
    }

    // Method Description:
    // - Open a new tab. This will create the TerminalControl hosting the
    //      terminal, and add a new Tab to our list of tabs. The method can
    //      optionally be provided a profile index, which will be used to create
    //      a tab using the profile in that index.
    //      If no index is provided, the default profile will be used.
    // Arguments:
    // - profileIndex: an optional index into the list of profiles to use to
    //      initialize this tab up with.
    void App::_OpenNewTab(std::optional<int> profileIndex)
    {
        GUID profileGuid;

        if (profileIndex)
        {
            const auto realIndex = profileIndex.value();
            const auto profiles = _settings->GetProfiles();

            // If we don't have that many profiles, then do nothing.
            if (realIndex >= profiles.size())
            {
                return;
            }

            const auto& selectedProfile = profiles[realIndex];
            profileGuid = selectedProfile.GetGuid();
        }
        else
        {
            // Getting Guid for default profile
            const auto globalSettings = _settings->GlobalSettings();
            profileGuid = globalSettings.GetDefaultProfile();
        }

        TerminalSettings settings = _settings->MakeSettings(profileGuid);
        _CreateNewTabFromSettings(profileGuid, settings);

        const int tabCount = static_cast<int>(_tabs.size());
        TraceLoggingWrite(
            g_hTerminalAppProvider, // handle to TerminalApp tracelogging provider
            "TabInformation",
            TraceLoggingDescription("Event emitted upon new tab creation in TerminalApp"),
            TraceLoggingInt32(tabCount, "TabCount", "Count of tabs curently opened in TerminalApp"),
            TraceLoggingBool(profileIndex.has_value(), "ProfileSpecified", "Whether the new tab specified a profile explicitly"),
            TraceLoggingGuid(profileGuid, "ProfileGuid", "The GUID of the profile spawned in the new tab"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));
    }

    // Function Description:
    // - Copies and processes the text data from the Windows Clipboard.
    //   Does some of this in a background thread, as to not hang/crash the UI thread.
    // Arguments:
    // - eventArgs: the PasteFromClipboard event sent from the TermControl
    fire_and_forget PasteFromClipboard(PasteFromClipboardEventArgs eventArgs)
    {
        const DataPackageView data = Clipboard::GetContent();

        // This will switch the execution of the function to a background (not
        // UI) thread. This is IMPORTANT, because the getting the clipboard data
        // will crash on the UI thread, because the main thread is a STA.
        co_await winrt::resume_background();

        hstring text = L"";
        if (data.Contains(StandardDataFormats::Text()))
        {
            text = co_await data.GetTextAsync();
        }
        eventArgs.HandleClipboardData(text);
    }

    // Method Description:
    // - Connects event handlers to the TermControl for events that we want to
    //   handle. This includes:
    //    * the Copy and Paste events, for setting and retrieving clipboard data
    //      on the right thread
    //    * the TitleChanged event, for changing the text of the tab
    //    * the GotFocus event, for changing the title/icon in the tab when a new
    //      control is focused
    // Arguments:
    // - term: The newly created TermControl to connect the events for
    // - hostingTab: The Tab that's hosting this TermControl instance
    void App::_RegisterTerminalEvents(TermControl term, std::shared_ptr<Tab> hostingTab)
    {
        // Add an event handler when the terminal's selection wants to be copied.
        // When the text buffer data is retrieved, we'll copy the data into the Clipboard
        term.CopyToClipboard({ this, &App::_CopyToClipboardHandler });

        // Add an event handler when the terminal wants to paste data from the Clipboard.
        term.PasteFromClipboard({ this, &App::_PasteFromClipboardHandler });

        // Don't capture a strong ref to the tab. If the tab is removed as this
        // is called, we don't really care anymore about handling the event.
        std::weak_ptr<Tab> weakTabPtr = hostingTab;
        term.TitleChanged([this, weakTabPtr](auto newTitle) {
            auto tab = weakTabPtr.lock();
            if (!tab)
            {
                return;
            }
            // The title of the control changed, but not necessarily the title
            // of the tab. Get the title of the focused pane of the tab, and set
            // the tab's text to the focused panes' text.
            _UpdateTitle(tab);
        });

        term.GotFocus([this, weakTabPtr](auto&&, auto&&) {
            auto tab = weakTabPtr.lock();
            if (!tab)
            {
                return;
            }
            // Update the focus of the tab's panes
            tab->UpdateFocus();

            // Possibly update the title of the tab, window to match the newly
            // focused pane.
            _UpdateTitle(tab);

            // Possibly update the icon of the tab.
            _UpdateTabIcon(tab);
        });
    }

    // Method Description:
    // - Creates a new tab with the given settings. If the tab bar is not being
    //      currently displayed, it will be shown.
    // Arguments:
    // - settings: the TerminalSettings object to use to create the TerminalControl with.
    void App::_CreateNewTabFromSettings(GUID profileGuid, TerminalSettings settings)
    {
        // Initialize the new tab

        // Create a connection based on the values in our settings object.
        const auto connection = _CreateConnectionFromSettings(profileGuid, settings);

        TermControl term{ settings, connection };

        // Add the new tab to the list of our tabs.
        auto newTab = _tabs.emplace_back(std::make_shared<Tab>(profileGuid, term));

        const auto* const profile = _settings->FindProfile(profileGuid);

        // Hookup our event handlers to the new terminal
        _RegisterTerminalEvents(term, newTab);

        auto tabViewItem = newTab->GetTabViewItem();
        _tabView.Items().Append(tabViewItem);

        // Set this profile's tab to the icon the user specified
        if (profile != nullptr && profile->HasIcon())
        {
            tabViewItem.Icon(_GetIconFromProfile(*profile));
        }

        tabViewItem.PointerPressed({ this, &App::_OnTabClick });

        // When the tab is closed, remove it from our list of tabs.
        newTab->Closed([tabViewItem, this]() {
            _tabView.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [tabViewItem, this]() {
                _RemoveTabViewItem(tabViewItem);
            });
        });

        // This is one way to set the tab's selected background color.
        //   tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderBackgroundSelected"), a Brush?);

        // This kicks off TabView::SelectionChanged, in response to which we'll attach the terminal's
        // Xaml control to the Xaml root.
        _tabView.SelectedItem(tabViewItem);
    }

    // Method Description:
    // - Returns the index in our list of tabs of the currently focused tab. If
    //      no tab is currently selected, returns -1.
    // Return Value:
    // - the index of the currently focused tab if there is one, else -1
    int App::_GetFocusedTabIndex() const
    {
        // GH#1117: This is a workaround because _tabView.SelectedIndex()
        //          sometimes return incorrect result after removing some tabs
        uint32_t focusedIndex;
        if (_tabView.Items().IndexOf(_tabView.SelectedItem(), focusedIndex))
        {
            return focusedIndex;
        }
        return -1;
    }

    void App::_OpenSettings()
    {
        LaunchSettings();
    }

    // Method Description:
    // - Close the currently focused tab. Focus will move to the left, if possible.
    void App::_CloseFocusedTab()
    {
        int focusedTabIndex = _GetFocusedTabIndex();
        std::shared_ptr<Tab> focusedTab{ _tabs[focusedTabIndex] };
        _RemoveTabViewItem(focusedTab->GetTabViewItem());
    }

    // Method Description:
    // - Close the currently focused pane. If the pane is the last pane in the
    //   tab, the tab will also be closed. This will happen when we handle the
    //   tab's Closed event.
    void App::_CloseFocusedPane()
    {
        int focusedTabIndex = _GetFocusedTabIndex();
        std::shared_ptr<Tab> focusedTab{ _tabs[focusedTabIndex] };
        focusedTab->ClosePane();
    }

    // Method Description:
    // - Move the viewport of the terminal of the currently focused tab up or
    //      down a number of lines. Negative values of `delta` will move the
    //      view up, and positive values will move the viewport down.
    // Arguments:
    // - delta: a number of lines to move the viewport relative to the current viewport.
    void App::_Scroll(int delta)
    {
        int focusedTabIndex = _GetFocusedTabIndex();
        _tabs[focusedTabIndex]->Scroll(delta);
    }

    // Method Description:
    // - Move the viewport of the terminal of the currently focused tab up or
    //      down a page. The page length will be dependent on the terminal view height.
    //      Negative values of `delta` will move the view up by one page, and positive values
    //      will move the viewport down by one page.
    // Arguments:
    // - delta: The direction to move the view relative to the current viewport(it
    //      is clamped between -1 and 1)
    void App::_ScrollPage(int delta)
    {
        delta = std::clamp(delta, -1, 1);
        const auto focusedTabIndex = _GetFocusedTabIndex();
        const auto control = _GetFocusedControl();
        const auto termHeight = control.GetViewHeight();
        _tabs[focusedTabIndex]->Scroll(termHeight * delta);
    }

    // Method Description:
    // - Attempt to move a separator between panes, as to resize each child on
    //   either size of the separator. See Pane::ResizePane for details.
    // - Moves a separator on the currently focused tab.
    // Arguments:
    // - direction: The direction to move the separator in.
    // Return Value:
    // - <none>
    void App::_ResizePane(const Direction& direction)
    {
        const auto focusedTabIndex = _GetFocusedTabIndex();
        _tabs[focusedTabIndex]->ResizePane(direction);
    }

    // Method Description:
    // - Attempt to move focus between panes, as to focus the child on
    //   the other side of the separator. See Pane::NavigateFocus for details.
    // - Moves the focus of the currently focused tab.
    // Arguments:
    // - direction: The direction to move the focus in.
    // Return Value:
    // - <none>
    void App::_MoveFocus(const Direction& direction)
    {
        const auto focusedTabIndex = _GetFocusedTabIndex();
        _tabs[focusedTabIndex]->NavigateFocus(direction);
    }

    // Method Description:
    // - Copy text from the focused terminal to the Windows Clipboard
    // Arguments:
    // - trimTrailingWhitespace: enable removing any whitespace from copied selection
    //    and get text to appear on separate lines.
    void App::_CopyText(const bool trimTrailingWhitespace)
    {
        const auto control = _GetFocusedControl();
        control.CopySelectionToClipboard(trimTrailingWhitespace);
    }

    // Method Description:
    // - Paste text from the Windows Clipboard to the focused terminal
    void App::_PasteText()
    {
        const auto control = _GetFocusedControl();
        control.PasteTextFromClipboard();
    }

    // Method Description:
    // - Sets focus to the tab to the right or left the currently selected tab.
    void App::_SelectNextTab(const bool bMoveRight)
    {
        int focusedTabIndex = _GetFocusedTabIndex();
        auto tabCount = _tabs.size();
        // Wraparound math. By adding tabCount and then calculating modulo tabCount,
        // we clamp the values to the range [0, tabCount) while still supporting moving
        // leftward from 0 to tabCount - 1.
        _SetFocusedTabIndex(
            static_cast<int>((tabCount + focusedTabIndex + (bMoveRight ? 1 : -1)) % tabCount));
    }

    // Method Description:
    // - Sets focus to the desired tab.
    void App::_SelectTab(const int tabIndex)
    {
        if (tabIndex >= 0 && tabIndex < _tabs.size())
        {
            _SetFocusedTabIndex(tabIndex);
        }
    }

    // Method Description:
    // - Responds to the TabView control's Selection Changed event (to move a
    //      new terminal control into focus.)
    // Arguments:
    // - sender: the control that originated this event
    // - eventArgs: the event's constituent arguments
    void App::_OnTabSelectionChanged(const IInspectable& sender, const Controls::SelectionChangedEventArgs& eventArgs)
    {
        auto tabView = sender.as<MUX::Controls::TabView>();
        auto selectedIndex = tabView.SelectedIndex();

        // Unfocus all the tabs.
        for (auto tab : _tabs)
        {
            tab->SetFocused(false);
        }

        if (selectedIndex >= 0)
        {
            try
            {
                auto tab = _tabs.at(selectedIndex);

                _tabContent.Children().Clear();
                _tabContent.Children().Append(tab->GetRootElement());

                tab->SetFocused(true);
                _titleChangeHandlers(GetTitle());
            }
            CATCH_LOG();
        }
    }

    // Method Description:
    // - Responds to the TabView control's Tab Closing event by removing
    //      the indicated tab from the set and focusing another one.
    //      The event is cancelled so App maintains control over the
    //      items in the tabview.
    // Arguments:
    // - sender: the control that originated this event
    // - eventArgs: the event's constituent arguments
    void App::_OnTabClosing(const IInspectable& sender, const MUX::Controls::TabViewTabClosingEventArgs& eventArgs)
    {
        const auto tabViewItem = eventArgs.Item();
        _RemoveTabViewItem(tabViewItem);

        // If we don't cancel the event, the TabView will remove the item itself.
        eventArgs.Cancel(true);
    }

    // Method Description:
    // - Responds to changes in the TabView's item list by changing the tabview's
    //      visibility.
    // Arguments:
    // - sender: the control that originated this event
    // - eventArgs: the event's constituent arguments
    void App::_OnTabItemsChanged(const IInspectable& sender, const Windows::Foundation::Collections::IVectorChangedEventArgs& eventArgs)
    {
        _UpdateTabView();
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
        if (_settings->GlobalSettings().GetShowTitleInTitlebar())
        {
            auto selectedIndex = _tabView.SelectedIndex();
            if (selectedIndex >= 0)
            {
                try
                {
                    if (auto focusedControl{ _GetFocusedControl() })
                    {
                        return focusedControl.Title();
                    }
                }
                CATCH_LOG();
            }
        }
        return { L"Windows Terminal" };
    }

    // Method Description:
    // - Additional responses to clicking on a TabView's item. Currently, just remove tab with middle click
    // Arguments:
    // - sender: the control that originated this event (TabViewItem)
    // - eventArgs: the event's constituent arguments
    void App::_OnTabClick(const IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& eventArgs)
    {
        if (eventArgs.GetCurrentPoint(_root).Properties().IsMiddleButtonPressed())
        {
            _RemoveTabViewItem(sender);
            eventArgs.Handled(true);
        }
    }

    // Method Description:
    // - Duplicates the current focused tab
    void App::_DuplicateTabViewItem()
    {
        const int& focusedTabIndex = _GetFocusedTabIndex();
        const auto& _tab = _tabs.at(focusedTabIndex);

        const auto& profileGuid = _tab->GetFocusedProfile();
        const auto& settings = _settings->MakeSettings(profileGuid);

        _CreateNewTabFromSettings(profileGuid.value(), settings);
    }

    // Method Description:
    // - Removes the tab (both TerminalControl and XAML)
    // Arguments:
    // - tabViewItem: the TabViewItem in the TabView that is being removed.
    void App::_RemoveTabViewItem(const IInspectable& tabViewItem)
    {
        // To close the window here, we need to close the hosting window.
        if (_tabs.size() == 1)
        {
            _lastTabClosedHandlers();
        }
        uint32_t tabIndexFromControl = 0;
        _tabView.Items().IndexOf(tabViewItem, tabIndexFromControl);
        auto focusedTabIndex = _GetFocusedTabIndex();

        // Removing the tab from the collection will destroy its control and disconnect its connection.
        _tabs.erase(_tabs.begin() + tabIndexFromControl);
        _tabView.Items().RemoveAt(tabIndexFromControl);

        if (tabIndexFromControl == focusedTabIndex)
        {
            if (focusedTabIndex >= _tabs.size())
            {
                focusedTabIndex = static_cast<int>(_tabs.size()) - 1;
            }

            if (focusedTabIndex < 0)
            {
                focusedTabIndex = 0;
            }

            _SelectTab(focusedTabIndex);
        }
    }

    // Method Description:
    // - Gets a colored IconElement for the profile in question. If the profile
    //   has an `icon` set in the settings, this will return an icon with that
    //   image in it. Otherwise it will return a nullptr-initialized
    //   IconElement.
    // Arguments:
    // - profile: the profile to get the icon from
    // Return Value:
    // - an IconElement for the profile's icon, if it has one.
    Controls::IconElement App::_GetIconFromProfile(const Profile& profile)
    {
        if (profile.HasIcon())
        {
            std::wstring path{ profile.GetIconPath() };
            const auto envExpandedPath{ wil::ExpandEnvironmentStringsW<std::wstring>(path.data()) };
            winrt::hstring iconPath{ envExpandedPath };
            winrt::Windows::Foundation::Uri iconUri{ iconPath };
            Controls::BitmapIconSource iconSource;
            // Make sure to set this to false, so we keep the RGB data of the
            // image. Otherwise, the icon will be white for all the
            // non-transparent pixels in the image.
            iconSource.ShowAsMonochrome(false);
            iconSource.UriSource(iconUri);
            Controls::IconSourceElement elem;
            elem.IconSource(iconSource);
            return elem;
        }
        else
        {
            return { nullptr };
        }
    }

    winrt::Microsoft::Terminal::TerminalControl::TermControl App::_GetFocusedControl()
    {
        int focusedTabIndex = _GetFocusedTabIndex();
        auto focusedTab = _tabs[focusedTabIndex];
        return focusedTab->GetFocusedTerminalControl();
    }

    // Method Description:
    // - Vertically split the focused pane, and place the given TermControl into
    //   the newly created pane.
    // Arguments:
    // - profile: The profile GUID to associate with the newly created pane. If
    //   this is nullopt, use the default profile.
    void App::_SplitVertical(const std::optional<GUID>& profileGuid)
    {
        _SplitPane(Pane::SplitState::Vertical, profileGuid);
    }

    // Method Description:
    // - Horizontally split the focused pane and place the given TermControl
    //   into the newly created pane.
    // Arguments:
    // - profile: The profile GUID to associate with the newly created pane. If
    //   this is nullopt, use the default profile.
    void App::_SplitHorizontal(const std::optional<GUID>& profileGuid)
    {
        _SplitPane(Pane::SplitState::Horizontal, profileGuid);
    }

    // Method Description:
    // - Split the focused pane either horizontally or vertically, and place the
    //   given TermControl into the newly created pane.
    // - If splitType == SplitState::None, this method does nothing.
    // Arguments:
    // - splitType: one value from the Pane::SplitState enum, indicating how the
    //   new pane should be split from its parent.
    // - profile: The profile GUID to associate with the newly created pane. If
    //   this is nullopt, use the default profile.
    void App::_SplitPane(const Pane::SplitState splitType, const std::optional<GUID>& profileGuid)
    {
        // Do nothing if we're requesting no split.
        if (splitType == Pane::SplitState::None)
        {
            return;
        }

        const auto realGuid = profileGuid ? profileGuid.value() :
                                            _settings->GlobalSettings().GetDefaultProfile();
        const auto controlSettings = _settings->MakeSettings(realGuid);

        const auto controlConnection = _CreateConnectionFromSettings(realGuid, controlSettings);

        TermControl newControl{ controlSettings, controlConnection };

        const int focusedTabIndex = _GetFocusedTabIndex();
        auto focusedTab = _tabs[focusedTabIndex];

        // Hookup our event handlers to the new terminal
        _RegisterTerminalEvents(newControl, focusedTab);

        return splitType == Pane::SplitState::Horizontal ? focusedTab->AddHorizontalSplit(realGuid, newControl) :
                                                           focusedTab->AddVerticalSplit(realGuid, newControl);
    }

    // Method Description:
    // - Called when our tab content size changes. This updates each tab with
    //   the new size, so they have a chance to update each of their panes with
    //   the new size.
    // Arguments:
    // - e: the SizeChangedEventArgs with the new size of the tab content area.
    // Return Value:
    // - <none>
    void App::_OnContentSizeChanged(const IInspectable& /*sender*/, Windows::UI::Xaml::SizeChangedEventArgs const& e)
    {
        const auto newSize = e.NewSize();
        for (auto& tab : _tabs)
        {
            tab->ResizeContent(newSize);
        }
    }

    // Method Description:
    // - Place `copiedData` into the clipboard as text. Triggered when a
    //   terminal control raises it's CopyToClipboard event.
    // Arguments:
    // - copiedData: the new string content to place on the clipboard.
    void App::_CopyToClipboardHandler(const winrt::hstring& copiedData)
    {
        _root.Dispatcher().RunAsync(CoreDispatcherPriority::High, [copiedData]() {
            DataPackage dataPack = DataPackage();
            dataPack.RequestedOperation(DataPackageOperation::Copy);
            dataPack.SetText(copiedData);
            Clipboard::SetContent(dataPack);

            // TODO: MSFT 20642290 and 20642291
            // rtf copy and html copy
        });
    }

    // Method Description:
    // - Fires an async event to get data from the clipboard, and paste it to
    //   the terminal. Triggered when the Terminal Control requests clipboard
    //   data with it's PasteFromClipboard event.
    // Arguments:
    // - eventArgs: the PasteFromClipboard event sent from the TermControl
    void App::_PasteFromClipboardHandler(const IInspectable& /*sender*/,
                                         const PasteFromClipboardEventArgs& eventArgs)
    {
        _root.Dispatcher().RunAsync(CoreDispatcherPriority::High, [eventArgs]() {
            PasteFromClipboard(eventArgs);
        });
    }

    // Method Description:
    // - Handles the special case of providing a text override for the UI shortcut due to VK_OEM issue.
    //      Looks at the flags from the KeyChord modifiers and provides a concatenated string value of all
    //      in the same order that XAML would put them as well.
    // Return Value:
    // - a string representation of the key modifiers for the shortcut
    //NOTE: This needs to be localized with https://github.com/microsoft/terminal/issues/794 if XAML framework issue not resolved before then
    static std::wstring _FormatOverrideShortcutText(Settings::KeyModifiers modifiers)
    {
        std::wstring buffer{ L"" };

        if (WI_IsFlagSet(modifiers, Settings::KeyModifiers::Ctrl))
        {
            buffer += L"Ctrl+";
        }
        if (WI_IsFlagSet(modifiers, Settings::KeyModifiers::Shift))
        {
            buffer += L"Shift+";
        }
        if (WI_IsFlagSet(modifiers, Settings::KeyModifiers::Alt))
        {
            buffer += L"Alt+";
        }

        return buffer;
    }

    // Method Description:
    // - Takes a MenuFlyoutItem and a corresponding KeyChord value and creates the accelerator for UI display.
    //   Takes into account a special case for an error condition for a comma
    // Arguments:
    // - MenuFlyoutItem that will be displayed, and a KeyChord to map an accelerator
    void App::_SetAcceleratorForMenuItem(Windows::UI::Xaml::Controls::MenuFlyoutItem& menuItem, const winrt::Microsoft::Terminal::Settings::KeyChord& keyChord)
    {
#ifdef DEP_MICROSOFT_UI_XAML_708_FIXED
        // work around https://github.com/microsoft/microsoft-ui-xaml/issues/708 in case of VK_OEM_COMMA
        if (keyChord.Vkey() != VK_OEM_COMMA)
        {
            // use the XAML shortcut to give us the automatic capabilities
            auto menuShortcut = Windows::UI::Xaml::Input::KeyboardAccelerator{};

            // TODO: Modify this when https://github.com/microsoft/terminal/issues/877 is resolved
            menuShortcut.Key(static_cast<Windows::System::VirtualKey>(keyChord.Vkey()));

            // inspect the modifiers from the KeyChord and set the flags int he XAML value
            auto modifiers = AppKeyBindings::ConvertVKModifiers(keyChord.Modifiers());

            // add the modifiers to the shortcut
            menuShortcut.Modifiers(modifiers);

            // add to the menu
            menuItem.KeyboardAccelerators().Append(menuShortcut);
        }
        else // we've got a comma, so need to just use the alternate method
#endif
        {
            // extract the modifier and key to a nice format
            auto overrideString = _FormatOverrideShortcutText(keyChord.Modifiers());
            auto mappedCh = MapVirtualKeyW(keyChord.Vkey(), MAPVK_VK_TO_CHAR);
            if (mappedCh != 0)
            {
                menuItem.KeyboardAcceleratorTextOverride(overrideString + gsl::narrow_cast<wchar_t>(mappedCh));
            }
        }
    }

    // Method Description:
    // - Creates a new connection based on the profile settings
    // Arguments:
    // - the profile GUID we want the settings from
    // - the terminal settings
    // Return value:
    // - the desired connection
    TerminalConnection::ITerminalConnection App::_CreateConnectionFromSettings(GUID profileGuid, winrt::Microsoft::Terminal::Settings::TerminalSettings settings)
    {
        const auto* const profile = _settings->FindProfile(profileGuid);
        TerminalConnection::ITerminalConnection connection{ nullptr };
        // The Azure connection has a boost dependency, and boost does not support ARM64
        // so we make sure that we do not try to compile the Azure connection code if we are in ARM64 (we would get build errors otherwise)
        GUID connectionType{ 0 };
        if (profile->HasConnectionType())
        {
            connectionType = profile->GetConnectionType();
        }
#ifndef _M_ARM64
        if (connectionType == AzureConnectionType)
        {
            connection = TerminalConnection::AzureConnection(settings.InitialRows(), settings.InitialCols());
        }
        else
#endif
        {
            connection = TerminalConnection::ConhostConnection(settings.Commandline(), settings.StartingDirectory(), settings.InitialRows(), settings.InitialCols(), winrt::guid());
        }

        TraceLoggingWrite(
            g_hTerminalAppProvider,
            "ConnectionCreated",
            TraceLoggingDescription("Event emitted upon the creation of a connection"),
            TraceLoggingGuid(connectionType, "ConnectionTypeGuid", "The type of the connection"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));

        return connection;
    }

    // -------------------------------- WinRT Events ---------------------------------
    // Winrt events need a method for adding a callback to the event and removing the callback.
    // These macros will define them both for you.
    DEFINE_EVENT(App, TitleChanged, _titleChangeHandlers, TerminalControl::TitleChangedEventArgs);
    DEFINE_EVENT(App, LastTabClosed, _lastTabClosedHandlers, winrt::TerminalApp::LastTabClosedEventArgs);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(App, SetTitleBarContent, _setTitleBarContentHandlers, TerminalApp::App, winrt::Windows::UI::Xaml::UIElement);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(App, RequestedThemeChanged, _requestedThemeChangedHandlers, TerminalApp::App, winrt::Windows::UI::Xaml::ElementTheme);
}
