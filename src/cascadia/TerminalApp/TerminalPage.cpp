// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalPage.h"
#include "Utils.h"

#include <LibraryResources.h>

#include "TerminalPage.g.cpp"
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>

#include "AzureCloudShellGenerator.h" // For AzureConnectionType
#include "TabRowControl.h"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::System;
using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::UI::Text;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace ::TerminalApp;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::TerminalApp::implementation
{
    TerminalPage::TerminalPage() :
        _tabs{}
    {
        InitializeComponent();
    }

    void TerminalPage::SetSettings(std::shared_ptr<::TerminalApp::CascadiaSettings> settings, bool needRefreshUI)
    {
        _settings = settings;
        if (needRefreshUI)
        {
            _RefreshUIForSettingsReload();
        }
    }

    void TerminalPage::Create()
    {
        // Hookup the key bindings
        _HookupKeyBindings(_settings->GetKeybindings());

        _tabContent = this->TabContent();
        _tabRow = this->TabRow();
        _tabView = _tabRow.TabView();

        auto tabRowImpl = winrt::get_self<implementation::TabRowControl>(_tabRow);
        _newTabButton = tabRowImpl->NewTabButton();

        if (_settings->GlobalSettings().GetShowTabsInTitlebar())
        {
            // Remove the TabView from the page. We'll hang on to it, we need to
            // put it in the titlebar.
            uint32_t index = 0;
            if (this->Root().Children().IndexOf(_tabRow, index))
            {
                this->Root().Children().RemoveAt(index);
            }

            // Inform the host that our titlebar content has changed.
            _setTitleBarContentHandlers(*this, _tabRow);
        }

        //Event Bindings (Early)
        _newTabButton.Click([this](auto&&, auto&&) {
            this->_OpenNewTab(std::nullopt);
        });
        _tabView.SelectionChanged({ this, &TerminalPage::_OnTabSelectionChanged });
        _tabView.TabCloseRequested({ this, &TerminalPage::_OnTabCloseRequested });
        _tabView.TabItemsChanged({ this, &TerminalPage::_OnTabItemsChanged });

        _CreateNewTabFlyout();
        _OpenNewTab(std::nullopt);

        _tabContent.SizeChanged({ this, &TerminalPage::_OnContentSizeChanged });
    }

    // Method Description:
    // - Show a ContentDialog with a single "Ok" button to dismiss. Looks up the
    //   the title and text from our Resources using the provided keys.
    // - Only one dialog can be visible at a time. If another dialog is visible
    //   when this is called, nothing happens. See _ShowDialog for details
    // Arguments:
    // - titleKey: The key to use to lookup the title text from our resources.
    // - contentKey: The key to use to lookup the content text from our resources.
    void TerminalPage::ShowOkDialog(const winrt::hstring& titleKey,
                                    const winrt::hstring& contentKey)
    {
        auto title = GetLibraryResourceString(titleKey);
        auto message = GetLibraryResourceString(contentKey);
        auto buttonText = RS_(L"Ok");

        WUX::Controls::ContentDialog dialog;
        dialog.Title(winrt::box_value(title));
        dialog.Content(winrt::box_value(message));
        dialog.CloseButtonText(buttonText);

        _showDialogHandlers(*this, dialog);
    }

    // Method Description:
    // - Show a dialog with "About" information. Displays the app's Display
    //   Name, version, getting started link, documentation link, and release
    //   Notes link.
    void TerminalPage::_ShowAboutDialog()
    {
        const auto title = RS_(L"AboutTitleText");
        const auto versionLabel = RS_(L"VersionLabelText");
        const auto gettingStartedLabel = RS_(L"GettingStartedLabelText");
        const auto documentationLabel = RS_(L"DocumentationLabelText");
        const auto releaseNotesLabel = RS_(L"ReleaseNotesLabelText");
        const auto gettingStartedUriValue = RS_(L"GettingStartedUriValue");
        const auto documentationUriValue = RS_(L"DocumentationUriValue");
        const auto releaseNotesUriValue = RS_(L"ReleaseNotesUriValue");
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

        const auto buttonText = RS_(L"Ok");

        WUX::Controls::TextBlock aboutTextBlock;
        aboutTextBlock.Inlines().Append(about);
        aboutTextBlock.Inlines().Append(gettingStartedLink);
        aboutTextBlock.Inlines().Append(documentationLink);
        aboutTextBlock.Inlines().Append(releaseNotesLink);
        aboutTextBlock.IsTextSelectionEnabled(true);

        WUX::Controls::ContentDialog dialog;
        dialog.Title(winrt::box_value(title));
        dialog.Content(aboutTextBlock);
        dialog.CloseButtonText(buttonText);

        _showDialogHandlers(*this, dialog);
    }

    // Method Description:
    // - Displays a dialog for warnings found while closing the terminal app using
    //   key binding with multiple tabs opened. Display messages to warn user
    //   that more than 1 tab is opend, and once the user clicks the OK button, remove
    //   all the tabs and shut down and app. If cancel is clicked, the dialog will close
    // - Only one dialog can be visible at a time. If another dialog is visible
    //   when this is called, nothing happens. See _ShowDialog for details
    void TerminalPage::_ShowCloseWarningDialog()
    {
        auto title = RS_(L"CloseWindowWarningTitle");
        auto primaryButtonText = RS_(L"CloseAll");
        auto secondaryButtonText = RS_(L"Cancel");

        WUX::Controls::ContentDialog dialog;
        dialog.Title(winrt::box_value(title));

        dialog.PrimaryButtonText(primaryButtonText);
        dialog.SecondaryButtonText(secondaryButtonText);
        auto token = dialog.PrimaryButtonClick({ this, &TerminalPage::_CloseWarningPrimaryButtonOnClick });

        _showDialogHandlers(*this, dialog);
    }

    // Method Description:
    // - Builds the flyout (dropdown) attached to the new tab button, and
    //   attaches it to the button. Populates the flyout with one entry per
    //   Profile, displaying the profile's name. Clicking each flyout item will
    //   open a new tab with that profile.
    //   Below the profiles are the static menu items: settings, feedback
    void TerminalPage::_CreateNewTabFlyout()
    {
        auto newTabFlyout = WUX::Controls::MenuFlyout{};
        auto keyBindings = _settings->GetKeybindings();

        const GUID defaultProfileGuid = _settings->GlobalSettings().GetDefaultProfile();
        // the number of profiles should not change in the loop for this to work
        auto const profileCount = gsl::narrow_cast<int>(_settings->GetProfiles().size());
        for (int profileIndex = 0; profileIndex < profileCount; profileIndex++)
        {
            const auto& profile = _settings->GetProfiles()[profileIndex];
            auto profileMenuItem = WUX::Controls::MenuFlyoutItem{};

            // add the keyboard shortcuts for the first 9 profiles
            if (profileIndex < 9)
            {
                // enum value for ShortcutAction::NewTabProfileX; 0==NewTabProfile0
                const auto action = static_cast<ShortcutAction>(profileIndex + static_cast<int>(ShortcutAction::NewTabProfile0));
                auto profileKeyChord = keyBindings.GetKeyBinding(action);

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
                auto iconSource = GetColoredIcon<WUX::Controls::IconSource>(profile.GetExpandedIconPath());

                WUX::Controls::IconSourceElement iconElement;
                iconElement.IconSource(iconSource);
                profileMenuItem.Icon(iconElement);
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
        auto separatorItem = WUX::Controls::MenuFlyoutSeparator{};
        newTabFlyout.Items().Append(separatorItem);

        // add static items
        {
            // Create the settings button.
            auto settingsItem = WUX::Controls::MenuFlyoutItem{};
            settingsItem.Text(RS_(L"SettingsMenuItem"));

            WUX::Controls::SymbolIcon ico{};
            ico.Symbol(WUX::Controls::Symbol::Setting);
            settingsItem.Icon(ico);

            settingsItem.Click({ this, &TerminalPage::_SettingsButtonOnClick });
            newTabFlyout.Items().Append(settingsItem);

            auto settingsKeyChord = keyBindings.GetKeyBinding(ShortcutAction::OpenSettings);
            if (settingsKeyChord)
            {
                _SetAcceleratorForMenuItem(settingsItem, settingsKeyChord);
            }

            // Create the feedback button.
            auto feedbackFlyout = WUX::Controls::MenuFlyoutItem{};
            feedbackFlyout.Text(RS_(L"FeedbackMenuItem"));

            WUX::Controls::FontIcon feedbackIcon{};
            feedbackIcon.Glyph(L"\xE939");
            feedbackIcon.FontFamily(Media::FontFamily{ L"Segoe MDL2 Assets" });
            feedbackFlyout.Icon(feedbackIcon);

            feedbackFlyout.Click({ this, &TerminalPage::_FeedbackButtonOnClick });
            newTabFlyout.Items().Append(feedbackFlyout);

            // Create the about button.
            auto aboutFlyout = WUX::Controls::MenuFlyoutItem{};
            aboutFlyout.Text(RS_(L"AboutMenuItem"));

            WUX::Controls::SymbolIcon aboutIcon{};
            aboutIcon.Symbol(WUX::Controls::Symbol::Help);
            aboutFlyout.Icon(aboutIcon);

            aboutFlyout.Click({ this, &TerminalPage::_AboutButtonOnClick });
            newTabFlyout.Items().Append(aboutFlyout);
        }

        _newTabButton.Flyout(newTabFlyout);
    }

    // Function Description:
    // Called when the openNewTabDropdown keybinding is used.
    // Adds the flyout show option to left-align the dropdown with the split button.
    // Shows the dropdown flyout.
    void TerminalPage::_OpenNewTabDropdown()
    {
        WUX::Controls::Primitives::FlyoutShowOptions options{};
        options.Placement(WUX::Controls::Primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);
        _newTabButton.Flyout().ShowAt(_newTabButton, options);
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
    void TerminalPage::_OpenNewTab(std::optional<int> profileIndex)
    {
        GUID profileGuid;

        if (profileIndex)
        {
            const auto realIndex = profileIndex.value();
            const auto profiles = _settings->GetProfiles();

            // If we don't have that many profiles, then do nothing.
            if (realIndex >= gsl::narrow<decltype(realIndex)>(profiles.size()))
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

    // Method Description:
    // - Creates a new tab with the given settings. If the tab bar is not being
    //      currently displayed, it will be shown.
    // Arguments:
    // - settings: the TerminalSettings object to use to create the TerminalControl with.
    void TerminalPage::_CreateNewTabFromSettings(GUID profileGuid, TerminalSettings settings)
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
        _tabView.TabItems().Append(tabViewItem);

        // Set this profile's tab to the icon the user specified
        if (profile != nullptr && profile->HasIcon())
        {
            newTab->UpdateIcon(profile->GetExpandedIconPath());
        }

        tabViewItem.PointerPressed({ this, &TerminalPage::_OnTabClick });

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
    // - Creates a new connection based on the profile settings
    // Arguments:
    // - the profile GUID we want the settings from
    // - the terminal settings
    // Return value:
    // - the desired connection
    TerminalConnection::ITerminalConnection TerminalPage::_CreateConnectionFromSettings(GUID profileGuid,
                                                                                        winrt::Microsoft::Terminal::Settings::TerminalSettings settings)
    {
        const auto* const profile = _settings->FindProfile(profileGuid);

        TerminalConnection::ITerminalConnection connection{ nullptr };

        GUID connectionType{ 0 };
        GUID sessionGuid{ 0 };

        if (profile->HasConnectionType())
        {
            connectionType = profile->GetConnectionType();
        }

        if (profile->HasConnectionType() &&
            profile->GetConnectionType() == AzureConnectionType &&
            TerminalConnection::AzureConnection::IsAzureConnectionAvailable())
        {
            connection = TerminalConnection::AzureConnection(settings.InitialRows(),
                                                             settings.InitialCols());
        }

        else
        {
            auto conhostConn = TerminalConnection::ConhostConnection(settings.Commandline(),
                                                                     settings.StartingDirectory(),
                                                                     settings.StartingTitle(),
                                                                     settings.InitialRows(),
                                                                     settings.InitialCols(),
                                                                     winrt::guid());
            sessionGuid = conhostConn.Guid();
            connection = conhostConn;
        }

        TraceLoggingWrite(
            g_hTerminalAppProvider,
            "ConnectionCreated",
            TraceLoggingDescription("Event emitted upon the creation of a connection"),
            TraceLoggingGuid(connectionType, "ConnectionTypeGuid", "The type of the connection"),
            TraceLoggingGuid(profileGuid, "ProfileGuid", "The profile's GUID"),
            TraceLoggingGuid(sessionGuid, "SessionGuid", "The WT_SESSION's GUID"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));

        return connection;
    }

    // Method Description:
    // - Called when the settings button is clicked. Launches a background
    //   thread to open the settings file in the default JSON editor.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_SettingsButtonOnClick(const IInspectable&,
                                              const RoutedEventArgs&)
    {
        const CoreWindow window = CoreWindow::GetForCurrentThread();
        const auto rAltState = window.GetKeyState(VirtualKey::RightMenu);
        const auto lAltState = window.GetKeyState(VirtualKey::LeftMenu);
        const bool altPressed = WI_IsFlagSet(lAltState, CoreVirtualKeyStates::Down) ||
                                WI_IsFlagSet(rAltState, CoreVirtualKeyStates::Down);

        _LaunchSettings(altPressed);
    }

    // Method Description:
    // - Called when the feedback button is clicked. Launches github in your
    //   default browser, navigated to the "issues" page of the Terminal repo.
    void TerminalPage::_FeedbackButtonOnClick(const IInspectable&,
                                              const RoutedEventArgs&)
    {
        const auto feedbackUriValue = RS_(L"FeedbackUriValue");

        winrt::Windows::System::Launcher::LaunchUriAsync({ feedbackUriValue });
    }

    // Method Description:
    // - Called when the about button is clicked. See _ShowAboutDialog for more info.
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void TerminalPage::_AboutButtonOnClick(const IInspectable&,
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
    void TerminalPage::_HookupKeyBindings(TerminalApp::AppKeyBindings bindings) noexcept
    {
        // Hook up the KeyBinding object's events to our handlers.
        // They should all be hooked up here, regardless of whether or not
        // there's an actual keychord for them.

        bindings.NewTab({ this, &TerminalPage::_HandleNewTab });
        bindings.OpenNewTabDropdown({ this, &TerminalPage::_HandleOpenNewTabDropdown });
        bindings.DuplicateTab({ this, &TerminalPage::_HandleDuplicateTab });
        bindings.CloseTab({ this, &TerminalPage::_HandleCloseTab });
        bindings.ClosePane({ this, &TerminalPage::_HandleClosePane });
        bindings.CloseWindow({ this, &TerminalPage::_HandleCloseWindow });
        bindings.ScrollUp({ this, &TerminalPage::_HandleScrollUp });
        bindings.ScrollDown({ this, &TerminalPage::_HandleScrollDown });
        bindings.NextTab({ this, &TerminalPage::_HandleNextTab });
        bindings.PrevTab({ this, &TerminalPage::_HandlePrevTab });
        bindings.SplitVertical({ this, &TerminalPage::_HandleSplitVertical });
        bindings.SplitHorizontal({ this, &TerminalPage::_HandleSplitHorizontal });
        bindings.ScrollUpPage({ this, &TerminalPage::_HandleScrollUpPage });
        bindings.ScrollDownPage({ this, &TerminalPage::_HandleScrollDownPage });
        bindings.OpenSettings({ this, &TerminalPage::_HandleOpenSettings });
        bindings.PasteText({ this, &TerminalPage::_HandlePasteText });
        bindings.NewTabWithProfile({ this, &TerminalPage::_HandleNewTabWithProfile });
        bindings.SwitchToTab({ this, &TerminalPage::_HandleSwitchToTab });
        bindings.ResizePane({ this, &TerminalPage::_HandleResizePane });
        bindings.MoveFocus({ this, &TerminalPage::_HandleMoveFocus });
        bindings.CopyText({ this, &TerminalPage::_HandleCopyText });
        bindings.AdjustFontSize({ this, &TerminalPage::_HandleAdjustFontSize });
    }

    // Method Description:
    // - Get the title of the currently focused terminal control, and set it's
    //   tab's text to that text. If this tab is the focused tab, then also
    //   bubble this title to any listeners of our TitleChanged event.
    // Arguments:
    // - tab: the Tab to update the title for.
    void TerminalPage::_UpdateTitle(std::shared_ptr<Tab> tab)
    {
        auto newTabTitle = tab->GetFocusedTitle();
        tab->SetTabText(newTabTitle);

        if (_settings->GlobalSettings().GetShowTitleInTitlebar() &&
            tab->IsFocused())
        {
            _titleChangeHandlers(*this, newTabTitle);
        }
    }

    // Method Description:
    // - Get the icon of the currently focused terminal control, and set its
    //   tab's icon to that icon.
    // Arguments:
    // - tab: the Tab to update the title for.
    void TerminalPage::_UpdateTabIcon(std::shared_ptr<Tab> tab)
    {
        const auto lastFocusedProfileOpt = tab->GetFocusedProfile();
        if (lastFocusedProfileOpt.has_value())
        {
            const auto lastFocusedProfile = lastFocusedProfileOpt.value();
            const auto* const matchingProfile = _settings->FindProfile(lastFocusedProfile);
            if (matchingProfile)
            {
                tab->UpdateIcon(matchingProfile->GetExpandedIconPath());
            }
            else
            {
                tab->UpdateIcon({});
            }
        }
    }

    // Method Description:
    // - Handle changes in tab layout.
    void TerminalPage::_UpdateTabView()
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
    // - Duplicates the current focused tab
    void TerminalPage::_DuplicateTabViewItem()
    {
        const int& focusedTabIndex = _GetFocusedTabIndex();
        const auto& _tab = _tabs.at(focusedTabIndex);

        const auto& profileGuid = _tab->GetFocusedProfile();
        const auto& settings = _settings->MakeSettings(profileGuid);

        _CreateNewTabFromSettings(profileGuid.value(), settings);
    }

    // Method Description:
    // - Look for the index of the input tabView in the tabs vector,
    //   and call _RemoveTabViewItemByIndex
    // Arguments:
    // - tabViewItem: the TabViewItem in the TabView that is being removed.
    void TerminalPage::_RemoveTabViewItem(const MUX::Controls::TabViewItem& tabViewItem)
    {
        uint32_t tabIndexFromControl = 0;
        _tabView.TabItems().IndexOf(tabViewItem, tabIndexFromControl);

        _RemoveTabViewItemByIndex(tabIndexFromControl);
    }

    // Method Description:
    // - Removes the tab (both TerminalControl and XAML)
    // Arguments:
    // - tabIndex: the index of the tab to be removed
    void TerminalPage::_RemoveTabViewItemByIndex(uint32_t tabIndex)
    {
        // To close the window here, we need to close the hosting window.
        if (_tabs.size() == 1)
        {
            _lastTabClosedHandlers(*this, nullptr);
        }

        // Removing the tab from the collection will destroy its control and disconnect its connection.
        _tabs.erase(_tabs.begin() + tabIndex);
        _tabView.TabItems().RemoveAt(tabIndex);

        auto focusedTabIndex = _GetFocusedTabIndex();
        if (tabIndex == focusedTabIndex)
        {
            auto const tabCount = gsl::narrow_cast<decltype(focusedTabIndex)>(_tabs.size());
            if (focusedTabIndex >= tabCount)
            {
                focusedTabIndex = tabCount - 1;
            }
            else if (focusedTabIndex < 0)
            {
                focusedTabIndex = 0;
            }

            _SelectTab(focusedTabIndex);
        }
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
    void TerminalPage::_RegisterTerminalEvents(TermControl term, std::shared_ptr<Tab> hostingTab)
    {
        // Add an event handler when the terminal's selection wants to be copied.
        // When the text buffer data is retrieved, we'll copy the data into the Clipboard
        term.CopyToClipboard({ this, &TerminalPage::_CopyToClipboardHandler });

        // Add an event handler when the terminal wants to paste data from the Clipboard.
        term.PasteFromClipboard({ this, &TerminalPage::_PasteFromClipboardHandler });

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
    // - Sets focus to the tab to the right or left the currently selected tab.
    void TerminalPage::_SelectNextTab(const bool bMoveRight)
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
    // - Sets focus to the desired tab. Returns false if the provided tabIndex
    //   is greater than the number of tabs we have.
    // Return Value:
    // true iff we were able to select that tab index, false otherwise
    bool TerminalPage::_SelectTab(const int tabIndex)
    {
        if (tabIndex >= 0 && tabIndex < gsl::narrow_cast<decltype(tabIndex)>(_tabs.size()))
        {
            _SetFocusedTabIndex(tabIndex);
            return true;
        }
        return false;
    }

    // Method Description:
    // - Attempt to move focus between panes, as to focus the child on
    //   the other side of the separator. See Pane::NavigateFocus for details.
    // - Moves the focus of the currently focused tab.
    // Arguments:
    // - direction: The direction to move the focus in.
    // Return Value:
    // - <none>
    void TerminalPage::_MoveFocus(const Direction& direction)
    {
        const auto focusedTabIndex = _GetFocusedTabIndex();
        _tabs[focusedTabIndex]->NavigateFocus(direction);
    }

    winrt::Microsoft::Terminal::TerminalControl::TermControl TerminalPage::_GetFocusedControl()
    {
        int focusedTabIndex = _GetFocusedTabIndex();
        auto focusedTab = _tabs[focusedTabIndex];
        return focusedTab->GetFocusedTerminalControl();
    }

    // Method Description:
    // - Returns the index in our list of tabs of the currently focused tab. If
    //      no tab is currently selected, returns -1.
    // Return Value:
    // - the index of the currently focused tab if there is one, else -1
    int TerminalPage::_GetFocusedTabIndex() const
    {
        // GH#1117: This is a workaround because _tabView.SelectedIndex()
        //          sometimes return incorrect result after removing some tabs
        uint32_t focusedIndex;
        if (_tabView.TabItems().IndexOf(_tabView.SelectedItem(), focusedIndex))
        {
            return focusedIndex;
        }
        return -1;
    }

    void TerminalPage::_SetFocusedTabIndex(int tabIndex)
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
    // - Close the currently focused tab. Focus will move to the left, if possible.
    void TerminalPage::_CloseFocusedTab()
    {
        uint32_t focusedTabIndex = _GetFocusedTabIndex();
        _RemoveTabViewItemByIndex(focusedTabIndex);
    }

    // Method Description:
    // - Close the currently focused pane. If the pane is the last pane in the
    //   tab, the tab will also be closed. This will happen when we handle the
    //   tab's Closed event.
    void TerminalPage::_CloseFocusedPane()
    {
        int focusedTabIndex = _GetFocusedTabIndex();
        std::shared_ptr<Tab> focusedTab{ _tabs[focusedTabIndex] };
        focusedTab->ClosePane();
    }

    // Method Description:
    // - Close the terminal app. If there is more
    //   than one tab opened, show a warning dialog.
    void TerminalPage::CloseWindow()
    {
        if (_tabs.size() > 1)
        {
            _ShowCloseWarningDialog();
        }
        else
        {
            _CloseAllTabs();
        }
    }

    // Method Description:
    // - Remove all the tabs opened and the terminal will terminate
    //   on its own when the last tab is closed.
    void TerminalPage::_CloseAllTabs()
    {
        while (!_tabs.empty())
        {
            _RemoveTabViewItemByIndex(0);
        }
    }

    // Method Description:
    // - Move the viewport of the terminal of the currently focused tab up or
    //      down a number of lines. Negative values of `delta` will move the
    //      view up, and positive values will move the viewport down.
    // Arguments:
    // - delta: a number of lines to move the viewport relative to the current viewport.
    void TerminalPage::_Scroll(int delta)
    {
        int focusedTabIndex = _GetFocusedTabIndex();
        _tabs[focusedTabIndex]->Scroll(delta);
    }

    // Method Description:
    // - Vertically split the focused pane, and place the given TermControl into
    //   the newly created pane.
    // Arguments:
    // - profile: The profile GUID to associate with the newly created pane. If
    //   this is nullopt, use the default profile.
    void TerminalPage::_SplitVertical(const std::optional<GUID>& profileGuid)
    {
        _SplitPane(Pane::SplitState::Vertical, profileGuid);
    }

    // Method Description:
    // - Horizontally split the focused pane and place the given TermControl
    //   into the newly created pane.
    // Arguments:
    // - profile: The profile GUID to associate with the newly created pane. If
    //   this is nullopt, use the default profile.
    void TerminalPage::_SplitHorizontal(const std::optional<GUID>& profileGuid)
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
    void TerminalPage::_SplitPane(const Pane::SplitState splitType, const std::optional<GUID>& profileGuid)
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

        const int focusedTabIndex = _GetFocusedTabIndex();
        auto focusedTab = _tabs[focusedTabIndex];

        const auto canSplit = focusedTab->CanSplitPane(splitType);

        if (!canSplit)
        {
            return;
        }

        TermControl newControl{ controlSettings, controlConnection };

        // Hookup our event handlers to the new terminal
        _RegisterTerminalEvents(newControl, focusedTab);

        focusedTab->SplitPane(splitType, realGuid, newControl);
    }

    // Method Description:
    // - Attempt to move a separator between panes, as to resize each child on
    //   either size of the separator. See Pane::ResizePane for details.
    // - Moves a separator on the currently focused tab.
    // Arguments:
    // - direction: The direction to move the separator in.
    // Return Value:
    // - <none>
    void TerminalPage::_ResizePane(const Direction& direction)
    {
        const auto focusedTabIndex = _GetFocusedTabIndex();
        _tabs[focusedTabIndex]->ResizePane(direction);
    }

    // Method Description:
    // - Move the viewport of the terminal of the currently focused tab up or
    //      down a page. The page length will be dependent on the terminal view height.
    //      Negative values of `delta` will move the view up by one page, and positive values
    //      will move the viewport down by one page.
    // Arguments:
    // - delta: The direction to move the view relative to the current viewport(it
    //      is clamped between -1 and 1)
    void TerminalPage::_ScrollPage(int delta)
    {
        delta = std::clamp(delta, -1, 1);
        const auto focusedTabIndex = _GetFocusedTabIndex();
        const auto control = _GetFocusedControl();
        const auto termHeight = control.GetViewHeight();
        _tabs[focusedTabIndex]->Scroll(termHeight * delta);
    }

    // Method Description:
    // - Gets the title of the currently focused terminal control. If there
    //   isn't a control selected for any reason, returns "Windows Terminal"
    // Arguments:
    // - <none>
    // Return Value:
    // - the title of the focused control if there is one, else "Windows Terminal"
    hstring TerminalPage::Title()
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
    void TerminalPage::_SetAcceleratorForMenuItem(WUX::Controls::MenuFlyoutItem& menuItem,
                                                  const winrt::Microsoft::Terminal::Settings::KeyChord& keyChord)
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
    // - Place `copiedData` into the clipboard as text. Triggered when a
    //   terminal control raises it's CopyToClipboard event.
    // Arguments:
    // - copiedData: the new string content to place on the clipboard.
    void TerminalPage::_CopyToClipboardHandler(const IInspectable& /*sender*/,
                                               const winrt::Microsoft::Terminal::TerminalControl::CopyToClipboardEventArgs& copiedData)
    {
        this->Dispatcher().RunAsync(CoreDispatcherPriority::High, [copiedData]() {
            DataPackage dataPack = DataPackage();
            dataPack.RequestedOperation(DataPackageOperation::Copy);

            // copy text to dataPack
            dataPack.SetText(copiedData.Text());

            // copy html to dataPack
            const auto htmlData = copiedData.Html();
            if (!htmlData.empty())
            {
                dataPack.SetHtmlFormat(htmlData);
            }

            try
            {
                Clipboard::SetContent(dataPack);
                Clipboard::Flush();
            }
            CATCH_LOG();
        });
    }

    // Method Description:
    // - Fires an async event to get data from the clipboard, and paste it to
    //   the terminal. Triggered when the Terminal Control requests clipboard
    //   data with it's PasteFromClipboard event.
    // Arguments:
    // - eventArgs: the PasteFromClipboard event sent from the TermControl
    void TerminalPage::_PasteFromClipboardHandler(const IInspectable& /*sender*/,
                                                  const PasteFromClipboardEventArgs& eventArgs)
    {
        this->Dispatcher().RunAsync(CoreDispatcherPriority::High, [eventArgs]() {
            TerminalPage::PasteFromClipboard(eventArgs);
        });
    }

    // Function Description:
    // - Copies and processes the text data from the Windows Clipboard.
    //   Does some of this in a background thread, as to not hang/crash the UI thread.
    // Arguments:
    // - eventArgs: the PasteFromClipboard event sent from the TermControl
    fire_and_forget TerminalPage::PasteFromClipboard(PasteFromClipboardEventArgs eventArgs)
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
    // - Copy text from the focused terminal to the Windows Clipboard
    // Arguments:
    // - trimTrailingWhitespace: enable removing any whitespace from copied selection
    //    and get text to appear on separate lines.
    // Return Value:
    // - true iff we we able to copy text (if a selection was active)
    bool TerminalPage::_CopyText(const bool trimTrailingWhitespace)
    {
        const auto control = _GetFocusedControl();
        return control.CopySelectionToClipboard(trimTrailingWhitespace);
    }

    // Method Description:
    // - Paste text from the Windows Clipboard to the focused terminal
    void TerminalPage::_PasteText()
    {
        const auto control = _GetFocusedControl();
        control.PasteTextFromClipboard();
    }

    // Function Description:
    // - Called when the settings button is clicked. ShellExecutes the settings
    //   file, as to open it in the default editor for .json files. Does this in
    //   a background thread, as to not hang/crash the UI thread.
    fire_and_forget TerminalPage::_LaunchSettings(const bool openDefaults)
    {
        // This will switch the execution of the function to a background (not
        // UI) thread. This is IMPORTANT, because the Windows.Storage API's
        // (used for retrieving the path to the file) will crash on the UI
        // thread, because the main thread is a STA.
        co_await winrt::resume_background();

        const auto settingsPath = openDefaults ? CascadiaSettings::GetDefaultSettingsPath() :
                                                 CascadiaSettings::GetSettingsPath();

        HINSTANCE res = ShellExecute(nullptr, nullptr, settingsPath.c_str(), nullptr, nullptr, SW_SHOW);
        if (static_cast<int>(reinterpret_cast<uintptr_t>(res)) <= 32)
        {
            ShellExecute(nullptr, nullptr, L"notepad", settingsPath.c_str(), nullptr, SW_SHOW);
        }
    }

    // Method Description:
    // - Responds to changes in the TabView's item list by changing the tabview's
    //      visibility.
    // Arguments:
    // - sender: the control that originated this event
    // - eventArgs: the event's constituent arguments
    void TerminalPage::_OnTabItemsChanged(const IInspectable& sender, const Windows::Foundation::Collections::IVectorChangedEventArgs& eventArgs)
    {
        _UpdateTabView();
    }

    // Method Description:
    // - Additional responses to clicking on a TabView's item. Currently, just remove tab with middle click
    // Arguments:
    // - sender: the control that originated this event (TabViewItem)
    // - eventArgs: the event's constituent arguments
    void TerminalPage::_OnTabClick(const IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& eventArgs)
    {
        if (eventArgs.GetCurrentPoint(*this).Properties().IsMiddleButtonPressed())
        {
            _RemoveTabViewItem(sender.as<MUX::Controls::TabViewItem>());
            eventArgs.Handled(true);
        }
    }

    // Method Description:
    // - Responds to the TabView control's Selection Changed event (to move a
    //      new terminal control into focus.)
    // Arguments:
    // - sender: the control that originated this event
    // - eventArgs: the event's constituent arguments
    void TerminalPage::_OnTabSelectionChanged(const IInspectable& sender, const WUX::Controls::SelectionChangedEventArgs& eventArgs)
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
                _titleChangeHandlers(*this, Title());
            }
            CATCH_LOG();
        }
    }

    // Method Description:
    // - Called when our tab content size changes. This updates each tab with
    //   the new size, so they have a chance to update each of their panes with
    //   the new size.
    // Arguments:
    // - e: the SizeChangedEventArgs with the new size of the tab content area.
    // Return Value:
    // - <none>
    void TerminalPage::_OnContentSizeChanged(const IInspectable& /*sender*/, Windows::UI::Xaml::SizeChangedEventArgs const& e)
    {
        const auto newSize = e.NewSize();
        for (auto& tab : _tabs)
        {
            tab->ResizeContent(newSize);
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
    void TerminalPage::_OnTabCloseRequested(const IInspectable& sender, const MUX::Controls::TabViewTabCloseRequestedEventArgs& eventArgs)
    {
        const auto tabViewItem = eventArgs.Tab();
        _RemoveTabViewItem(tabViewItem);
    }

    // Method Description:
    // - Called when the primary button of the content dialog is clicked.
    //   This calls _CloseAllTabs(), which closes all the tabs currently
    //   opened and then the Terminal app. This method will be called if
    //   the user confirms to close all the tabs.
    // Arguments:
    // - sender: unused
    // - ContentDialogButtonClickEventArgs: unused
    void TerminalPage::_CloseWarningPrimaryButtonOnClick(WUX::Controls::ContentDialog /* sender */,
                                                         WUX::Controls::ContentDialogButtonClickEventArgs /* eventArgs*/)
    {
        _CloseAllTabs();
    }

    // Method Description:
    // - Hook up keybindings, and refresh the UI of the terminal.
    //   This includes update the settings of all the tabs according
    //   to their profiles, update the title and icon of each tab, and
    //   finally create the tab flyout
    void TerminalPage::_RefreshUIForSettingsReload()
    {
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

        this->Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [this]() {
            // repopulate the new tab button's flyout with entries for each
            // profile, which might have changed
            _CreateNewTabFlyout();
        });
    }

    // Method Description:
    // - This is the method that App will call when the titlebar
    //   has been clicked. It dismisses any open flyouts.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::TitlebarClicked()
    {
        if (_newTabButton && _newTabButton.Flyout())
        {
            _newTabButton.Flyout().Hide();
        }
    }

    // -------------------------------- WinRT Events ---------------------------------
    // Winrt events need a method for adding a callback to the event and removing the callback.
    // These macros will define them both for you.
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TerminalPage, TitleChanged, _titleChangeHandlers, winrt::Windows::Foundation::IInspectable, winrt::hstring);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TerminalPage, LastTabClosed, _lastTabClosedHandlers, winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::LastTabClosedEventArgs);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TerminalPage, SetTitleBarContent, _setTitleBarContentHandlers, winrt::Windows::Foundation::IInspectable, UIElement);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TerminalPage, ShowDialog, _showDialogHandlers, winrt::Windows::Foundation::IInspectable, WUX::Controls::ContentDialog);
}
