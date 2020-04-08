// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalPage.h"
#include "ActionAndArgs.h"
#include "Utils.h"
#include "AppLogic.h"
#include "../../types/inc/utils.hpp"

#include <LibraryResources.h>

#include "TerminalPage.g.cpp"
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>

#include "AzureCloudShellGenerator.h" // For AzureConnectionType
#include "TelnetGenerator.h" // For TelnetConnectionType
#include "TabRowControl.h"
#include "DebugTapConnection.h"

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
using namespace ::Microsoft::Console;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::TerminalApp::implementation
{
    TerminalPage::TerminalPage() :
        _tabs{ winrt::single_threaded_observable_vector<TerminalApp::Tab>() }
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
        _rearranging = false;

        // GH#2455 - Make sure to try/catch calls to Application::Current,
        // because that _won't_ be an instance of TerminalApp::App in the
        // LocalTests
        auto isElevated = false;
        try
        {
            // GH#3581 - There's a platform limitation that causes us to crash when we rearrange tabs.
            // Xaml tries to send a drag visual (to wit: a screenshot) to the drag hosting process,
            // but that process is running at a different IL than us.
            // For now, we're disabling elevated drag.
            isElevated = ::winrt::Windows::UI::Xaml::Application::Current().as<::winrt::TerminalApp::App>().Logic().IsUwp();
        }
        CATCH_LOG();

        _tabView.CanReorderTabs(!isElevated);
        _tabView.CanDragTabs(!isElevated);

        _tabView.TabDragStarting([weakThis{ get_weak() }](auto&& /*o*/, auto&& /*a*/) {
            if (auto page{ weakThis.get() })
            {
                page->_rearranging = true;
                page->_rearrangeFrom = std::nullopt;
                page->_rearrangeTo = std::nullopt;
            }
        });

        _tabView.TabDragCompleted([weakThis{ get_weak() }](auto&& /*o*/, auto&& /*a*/) {
            if (auto page{ weakThis.get() })
            {
                auto& from{ page->_rearrangeFrom };
                auto& to{ page->_rearrangeTo };

                if (from.has_value() && to.has_value() && to != from)
                {
                    auto& tabs{ page->_tabs };
                    auto tab = tabs.GetAt(from.value());
                    tabs.RemoveAt(from.value());
                    tabs.InsertAt(to.value(), tab);
                }

                page->_rearranging = false;
                from = std::nullopt;
                to = std::nullopt;
            }
        });

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

        // Hookup our event handlers to the ShortcutActionDispatch
        _RegisterActionCallbacks();

        //Event Bindings (Early)
        _newTabButton.Click([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto page{ weakThis.get() })
            {
                page->_OpenNewTab(nullptr);
            }
        });
        _tabView.SelectionChanged({ this, &TerminalPage::_OnTabSelectionChanged });
        _tabView.TabCloseRequested({ this, &TerminalPage::_OnTabCloseRequested });
        _tabView.TabItemsChanged({ this, &TerminalPage::_OnTabItemsChanged });

        _CreateNewTabFlyout();

        _UpdateTabWidthMode();

        _tabContent.SizeChanged({ this, &TerminalPage::_OnContentSizeChanged });

        // Once the page is actually laid out on the screen, trigger all our
        // startup actions. Things like Panes need to know at least how big the
        // window will be, so they can subdivide that space.
        //
        // _OnFirstLayout will remove this handler so it doesn't get called more than once.
        _layoutUpdatedRevoker = _tabContent.LayoutUpdated(winrt::auto_revoke, { this, &TerminalPage::_OnFirstLayout });
    }

    // Method Description:
    // - This method is called once on startup, on the first LayoutUpdated event.
    //   We'll use this event to know that we have an ActualWidth and
    //   ActualHeight, so we can now attempt to process our list of startup
    //   actions.
    // - We'll remove this event handler when the event is first handled.
    // - If there are no startup actions, we'll open a single tab with the
    //   default profile.
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void TerminalPage::_OnFirstLayout(const IInspectable& /*sender*/, const IInspectable& /*eventArgs*/)
    {
        // Only let this succeed once.
        _layoutUpdatedRevoker.revoke();

        // This event fires every time the layout changes, but it is always the
        // last one to fire in any layout change chain. That gives us great
        // flexibility in finding the right point at which to initialize our
        // renderer (and our terminal). Any earlier than the last layout update
        // and we may not know the terminal's starting size.
        if (_startupState == StartupState::NotInitialized)
        {
            _startupState = StartupState::InStartup;
            _appArgs.ValidateStartupCommands();
            if (_appArgs.GetStartupActions().empty())
            {
                _OpenNewTab(nullptr);
                _startupState = StartupState::Initialized;
                _InitializedHandlers(*this, nullptr);
            }
            else
            {
                _ProcessStartupActions();
            }
        }
    }

    // Method Description:
    // - Process all the startup actions in our list of startup actions. We'll
    //   do this all at once here.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    winrt::fire_and_forget TerminalPage::_ProcessStartupActions()
    {
        // If there are no actions left, do nothing.
        if (_appArgs.GetStartupActions().empty())
        {
            return;
        }
        auto weakThis{ get_weak() };

        // Handle it on a subsequent pass of the UI thread.
        co_await winrt::resume_foreground(Dispatcher(), CoreDispatcherPriority::Normal);
        if (auto page{ weakThis.get() })
        {
            for (const auto& action : _appArgs.GetStartupActions())
            {
                _actionDispatch->DoAction(action);
            }
            _startupState = StartupState::Initialized;
            _InitializedHandlers(*this, nullptr);
        }
    }

    // Method Description:
    // - Show a dialog with "About" information. Displays the app's Display
    //   Name, version, getting started link, documentation link, release
    //   Notes link, and privacy policy link.
    void TerminalPage::_ShowAboutDialog()
    {
        _showDialogHandlers(*this, FindName(L"AboutDialog").try_as<WUX::Controls::ContentDialog>());
    }

    winrt::hstring TerminalPage::ApplicationDisplayName()
    {
        if (const auto appLogic{ implementation::AppLogic::Current() })
        {
            return appLogic->ApplicationDisplayName();
        }

        return RS_(L"ApplicationDisplayNameUnpackaged");
    }

    winrt::hstring TerminalPage::ApplicationVersion()
    {
        if (const auto appLogic{ implementation::AppLogic::Current() })
        {
            return appLogic->ApplicationVersion();
        }

        return RS_(L"ApplicationVersionUnknown");
    }

    // Method Description:
    // - Displays a dialog for warnings found while closing the terminal app using
    //   key binding with multiple tabs opened. Display messages to warn user
    //   that more than 1 tab is opened, and once the user clicks the OK button, remove
    //   all the tabs and shut down and app. If cancel is clicked, the dialog will close
    // - Only one dialog can be visible at a time. If another dialog is visible
    //   when this is called, nothing happens. See _ShowDialog for details
    void TerminalPage::_ShowCloseWarningDialog()
    {
        _showDialogHandlers(*this, FindName(L"CloseAllDialog").try_as<WUX::Controls::ContentDialog>());
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
                // Look for a keychord that is bound to the equivalent
                // NewTab(ProfileIndex=N) action
                auto actionAndArgs = winrt::make_self<winrt::TerminalApp::implementation::ActionAndArgs>();
                actionAndArgs->Action(ShortcutAction::NewTab);
                auto newTabArgs = winrt::make_self<winrt::TerminalApp::implementation::NewTabArgs>();
                auto newTerminalArgs = winrt::make_self<winrt::TerminalApp::implementation::NewTerminalArgs>();
                newTerminalArgs->ProfileIndex(profileIndex);
                newTabArgs->TerminalArgs(*newTerminalArgs);
                actionAndArgs->Args(*newTabArgs);
                auto profileKeyChord{ keyBindings.GetKeyBindingForActionWithArgs(*actionAndArgs) };

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
                Automation::AutomationProperties::SetAccessibilityView(iconElement, Automation::Peers::AccessibilityView::Raw);
            }

            if (profile.GetGuid() == defaultProfileGuid)
            {
                // Contrast the default profile with others in font weight.
                profileMenuItem.FontWeight(FontWeights::Bold());
            }

            profileMenuItem.Click([profileIndex, weakThis{ get_weak() }](auto&&, auto&&) {
                if (auto page{ weakThis.get() })
                {
                    auto newTerminalArgs = winrt::make_self<winrt::TerminalApp::implementation::NewTerminalArgs>();
                    newTerminalArgs->ProfileIndex(profileIndex);
                    page->_OpenNewTab(*newTerminalArgs);
                }
            });
            newTabFlyout.Items().Append(profileMenuItem);
        }

        // add menu separator
        auto separatorItem = WUX::Controls::MenuFlyoutSeparator{};
        newTabFlyout.Items().Append(separatorItem);

        // add static items
        {
            // GH#2455 - Make sure to try/catch calls to Application::Current,
            // because that _won't_ be an instance of TerminalApp::App in the
            // LocalTests
            auto isUwp = false;
            try
            {
                isUwp = ::winrt::Windows::UI::Xaml::Application::Current().as<::winrt::TerminalApp::App>().Logic().IsUwp();
            }
            CATCH_LOG();

            if (!isUwp)
            {
                // Create the settings button.
                auto settingsItem = WUX::Controls::MenuFlyoutItem{};
                settingsItem.Text(RS_(L"SettingsMenuItem"));

                WUX::Controls::SymbolIcon ico{};
                ico.Symbol(WUX::Controls::Symbol::Setting);
                settingsItem.Icon(ico);

                settingsItem.Click({ this, &TerminalPage::_SettingsButtonOnClick });
                newTabFlyout.Items().Append(settingsItem);

                auto settingsKeyChord = keyBindings.GetKeyBindingForAction(ShortcutAction::OpenSettings);
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
            }

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
    //   terminal, and add a new Tab to our list of tabs. The method can
    //   optionally be provided a NewTerminalArgs, which will be used to create
    //   a tab using the values in that object.
    // Arguments:
    // - newTerminalArgs: An object that may contain a blob of parameters to
    //   control which profile is created and with possible other
    //   configurations. See CascadiaSettings::BuildSettings for more details.
    void TerminalPage::_OpenNewTab(const winrt::TerminalApp::NewTerminalArgs& newTerminalArgs)
    try
    {
        const auto [profileGuid, settings] = _settings->BuildSettings(newTerminalArgs);

        _CreateNewTabFromSettings(profileGuid, settings);

        const uint32_t tabCount = _tabs.Size();
        const bool usedManualProfile = (newTerminalArgs != nullptr) &&
                                       (newTerminalArgs.ProfileIndex() != nullptr ||
                                        newTerminalArgs.Profile().empty());
        TraceLoggingWrite(
            g_hTerminalAppProvider, // handle to TerminalApp tracelogging provider
            "TabInformation",
            TraceLoggingDescription("Event emitted upon new tab creation in TerminalApp"),
            TraceLoggingUInt32(1u, "EventVer", "Version of this event"),
            TraceLoggingUInt32(tabCount, "TabCount", "Count of tabs currently opened in TerminalApp"),
            TraceLoggingBool(usedManualProfile, "ProfileSpecified", "Whether the new tab specified a profile explicitly"),
            TraceLoggingGuid(profileGuid, "ProfileGuid", "The GUID of the profile spawned in the new tab"),
            TraceLoggingBool(settings.UseAcrylic(), "UseAcrylic", "The acrylic preference from the settings"),
            TraceLoggingFloat64(settings.TintOpacity(), "TintOpacity", "Opacity preference from the settings"),
            TraceLoggingWideString(settings.FontFace().c_str(), "FontFace", "Font face chosen in the settings"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));
    }
    CATCH_LOG();

    winrt::fire_and_forget TerminalPage::_RemoveOnCloseRoutine(Microsoft::UI::Xaml::Controls::TabViewItem tabViewItem, winrt::com_ptr<TerminalPage> page)
    {
        co_await winrt::resume_foreground(page->_tabView.Dispatcher());

        page->_RemoveTabViewItem(tabViewItem);
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
        auto connection = _CreateConnectionFromSettings(profileGuid, settings);

        TerminalConnection::ITerminalConnection debugConnection{ nullptr };
        if (_settings->GlobalSettings().DebugFeaturesEnabled())
        {
            const CoreWindow window = CoreWindow::GetForCurrentThread();
            const auto rAltState = window.GetKeyState(VirtualKey::RightMenu);
            const auto lAltState = window.GetKeyState(VirtualKey::LeftMenu);
            const bool bothAltsPressed = WI_IsFlagSet(lAltState, CoreVirtualKeyStates::Down) &&
                                         WI_IsFlagSet(rAltState, CoreVirtualKeyStates::Down);
            if (bothAltsPressed)
            {
                std::tie(connection, debugConnection) = OpenDebugTapConnection(connection);
            }
        }

        TermControl term{ settings, connection };

        // Add the new tab to the list of our tabs.
        auto newTabImpl = winrt::make_self<Tab>(profileGuid, term);
        _tabs.Append(*newTabImpl);

        // Hookup our event handlers to the new terminal
        _RegisterTerminalEvents(term, *newTabImpl);

        // Don't capture a strong ref to the tab. If the tab is removed as this
        // is called, we don't really care anymore about handling the event.
        auto weakTab = make_weak(newTabImpl);

        // When the tab's active pane changes, we'll want to lookup a new icon
        // for it, and possibly propagate the title up to the window.
        newTabImpl->ActivePaneChanged([weakTab, weakThis{ get_weak() }]() {
            auto page{ weakThis.get() };
            auto tab{ weakTab.get() };

            if (page && tab)
            {
                // Possibly update the icon of the tab.
                page->_UpdateTabIcon(*tab);
                // Possibly update the title of the tab, window to match the newly
                // focused pane.
                page->_UpdateTitle(*tab);
            }
        });

        auto tabViewItem = newTabImpl->GetTabViewItem();
        _tabView.TabItems().Append(tabViewItem);

        // Set this tab's icon to the icon from the user's profile
        const auto* const profile = _settings->FindProfile(profileGuid);
        if (profile != nullptr && profile->HasIcon())
        {
            newTabImpl->UpdateIcon(profile->GetExpandedIconPath());
        }

        tabViewItem.PointerPressed({ this, &TerminalPage::_OnTabClick });

        // When the tab is closed, remove it from our list of tabs.
        newTabImpl->Closed([tabViewItem, weakThis{ get_weak() }](auto&& /*s*/, auto&& /*e*/) {
            if (auto page{ weakThis.get() })
            {
                page->_RemoveOnCloseRoutine(tabViewItem, page);
            }
        });

        if (debugConnection) // this will only be set if global debugging is on and tap is active
        {
            TermControl newControl{ settings, debugConnection };
            _RegisterTerminalEvents(newControl, *newTabImpl);
            // Split (auto) with the debug tap.
            newTabImpl->SplitPane(SplitState::Automatic, profileGuid, newControl);
        }

        // This kicks off TabView::SelectionChanged, in response to which
        // we'll attach the terminal's Xaml control to the Xaml root.
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
            // TODO GH#4661: Replace this with directly using the AzCon when our VT is better
            std::filesystem::path azBridgePath{ wil::GetModuleFileNameW<std::wstring>(nullptr) };
            azBridgePath.replace_filename(L"TerminalAzBridge.exe");
            connection = TerminalConnection::ConptyConnection(azBridgePath.wstring(), L".", L"Azure", settings.InitialRows(), settings.InitialCols(), winrt::guid());
        }

        else if (profile->HasConnectionType() &&
                 profile->GetConnectionType() == TelnetConnectionType)
        {
            connection = TerminalConnection::TelnetConnection(settings.Commandline());
        }

        else
        {
            auto conhostConn = TerminalConnection::ConptyConnection(settings.Commandline(),
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
        winrt::Windows::Foundation::Uri feedbackUri{ feedbackUriValue };

        winrt::Windows::System::Launcher::LaunchUriAsync(feedbackUri);
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
    // - Configure the AppKeyBindings to use our ShortcutActionDispatch as the
    //   object to handle dispatching ShortcutAction events.
    // Arguments:
    // - bindings: A AppKeyBindings object to wire up with our event handlers
    void TerminalPage::_HookupKeyBindings(TerminalApp::AppKeyBindings bindings) noexcept
    {
        bindings.SetDispatch(*_actionDispatch);
    }

    // Method Description:
    // - Register our event handlers with our ShortcutActionDispatch. The
    //   ShortcutActionDispatch is responsible for raising the appropriate
    //   events for an ActionAndArgs. WE'll handle each possible event in our
    //   own way.
    // Arguments:
    // - <none>
    void TerminalPage::_RegisterActionCallbacks()
    {
        // Hook up the ShortcutActionDispatch object's events to our handlers.
        // They should all be hooked up here, regardless of whether or not
        // there's an actual keychord for them.
        _actionDispatch->OpenNewTabDropdown({ this, &TerminalPage::_HandleOpenNewTabDropdown });
        _actionDispatch->DuplicateTab({ this, &TerminalPage::_HandleDuplicateTab });
        _actionDispatch->CloseTab({ this, &TerminalPage::_HandleCloseTab });
        _actionDispatch->ClosePane({ this, &TerminalPage::_HandleClosePane });
        _actionDispatch->CloseWindow({ this, &TerminalPage::_HandleCloseWindow });
        _actionDispatch->ScrollUp({ this, &TerminalPage::_HandleScrollUp });
        _actionDispatch->ScrollDown({ this, &TerminalPage::_HandleScrollDown });
        _actionDispatch->NextTab({ this, &TerminalPage::_HandleNextTab });
        _actionDispatch->PrevTab({ this, &TerminalPage::_HandlePrevTab });
        _actionDispatch->SplitPane({ this, &TerminalPage::_HandleSplitPane });
        _actionDispatch->ScrollUpPage({ this, &TerminalPage::_HandleScrollUpPage });
        _actionDispatch->ScrollDownPage({ this, &TerminalPage::_HandleScrollDownPage });
        _actionDispatch->OpenSettings({ this, &TerminalPage::_HandleOpenSettings });
        _actionDispatch->PasteText({ this, &TerminalPage::_HandlePasteText });
        _actionDispatch->NewTab({ this, &TerminalPage::_HandleNewTab });
        _actionDispatch->SwitchToTab({ this, &TerminalPage::_HandleSwitchToTab });
        _actionDispatch->ResizePane({ this, &TerminalPage::_HandleResizePane });
        _actionDispatch->MoveFocus({ this, &TerminalPage::_HandleMoveFocus });
        _actionDispatch->CopyText({ this, &TerminalPage::_HandleCopyText });
        _actionDispatch->AdjustFontSize({ this, &TerminalPage::_HandleAdjustFontSize });
        _actionDispatch->Find({ this, &TerminalPage::_HandleFind });
        _actionDispatch->ResetFontSize({ this, &TerminalPage::_HandleResetFontSize });
        _actionDispatch->ToggleFullscreen({ this, &TerminalPage::_HandleToggleFullscreen });
    }

    // Method Description:
    // - Get the title of the currently focused terminal control. If this tab is
    //   the focused tab, then also bubble this title to any listeners of our
    //   TitleChanged event.
    // Arguments:
    // - tab: the Tab to update the title for.
    void TerminalPage::_UpdateTitle(const Tab& tab)
    {
        auto newTabTitle = tab.GetActiveTitle();

        if (_settings->GlobalSettings().GetShowTitleInTitlebar() &&
            tab.IsFocused())
        {
            _titleChangeHandlers(*this, newTabTitle);
        }
    }

    // Method Description:
    // - Get the icon of the currently focused terminal control, and set its
    //   tab's icon to that icon.
    // Arguments:
    // - tab: the Tab to update the title for.
    void TerminalPage::_UpdateTabIcon(Tab& tab)
    {
        const auto lastFocusedProfileOpt = tab.GetFocusedProfile();
        if (lastFocusedProfileOpt.has_value())
        {
            const auto lastFocusedProfile = lastFocusedProfileOpt.value();
            const auto* const matchingProfile = _settings->FindProfile(lastFocusedProfile);
            if (matchingProfile)
            {
                tab.UpdateIcon(matchingProfile->GetExpandedIconPath());
            }
            else
            {
                tab.UpdateIcon({});
            }
        }
    }

    // Method Description:
    // - Handle changes to the tab width set by the user
    void TerminalPage::_UpdateTabWidthMode()
    {
        _tabView.TabWidthMode(_settings->GlobalSettings().GetTabWidthMode());
    }

    // Method Description:
    // - Handle changes in tab layout.
    void TerminalPage::_UpdateTabView()
    {
        // Never show the tab row when we're fullscreen. Otherwise:
        // Show tabs when there's more than 1, or the user has chosen to always
        // show the tab bar.
        const bool isVisible = (!_isFullscreen) &&
                               (_settings->GlobalSettings().GetShowTabsInTitlebar() ||
                                (_tabs.Size() > 1) ||
                                _settings->GlobalSettings().GetAlwaysShowTabs());

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
        if (auto index{ _GetFocusedTabIndex() })
        {
            try
            {
                auto focusedTab = _GetStrongTabImpl(*index);
                // TODO: GH#5047 - In the future, we should get the Profile of
                // the focused pane, and use that to build a new instance of the
                // settings so we can duplicate this tab/pane.
                //
                // Currently, if the profile doesn't exist anymore in our
                // settings, we'll silently do nothing.
                //
                // In the future, it will be preferable to just duplicate the
                // current control's settings, but we can't do that currently,
                // because we won't be able to create a new instance of the
                // connection without keeping an instance of the original Profile
                // object around.

                const auto& profileGuid = focusedTab->GetFocusedProfile();
                if (profileGuid.has_value())
                {
                    const auto settings = _settings->BuildSettings(profileGuid.value());
                    _CreateNewTabFromSettings(profileGuid.value(), settings);
                }
            }
            CATCH_LOG();
        }
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
        // Removing the tab from the collection should destroy its control and disconnect its connection,
        // but it doesn't always do so. The UI tree may still be holding the control and preventing its destruction.
        auto tab{ _GetStrongTabImpl(tabIndex) };
        tab->Shutdown();

        _tabs.RemoveAt(tabIndex);
        _tabView.TabItems().RemoveAt(tabIndex);

        // To close the window here, we need to close the hosting window.
        if (_tabs.Size() == 0)
        {
            _lastTabClosedHandlers(*this, nullptr);
        }
    }

    // Method Description:
    // - Connects event handlers to the TermControl for events that we want to
    //   handle. This includes:
    //    * the Copy and Paste events, for setting and retrieving clipboard data
    //      on the right thread
    //    * the TitleChanged event, for changing the text of the tab
    // Arguments:
    // - term: The newly created TermControl to connect the events for
    // - hostingTab: The Tab that's hosting this TermControl instance
    void TerminalPage::_RegisterTerminalEvents(TermControl term, Tab& hostingTab)
    {
        // Add an event handler when the terminal's selection wants to be copied.
        // When the text buffer data is retrieved, we'll copy the data into the Clipboard
        term.CopyToClipboard({ this, &TerminalPage::_CopyToClipboardHandler });

        // Add an event handler when the terminal wants to paste data from the Clipboard.
        term.PasteFromClipboard({ this, &TerminalPage::_PasteFromClipboardHandler });

        // Bind Tab events to the TermControl and the Tab's Pane
        hostingTab.BindEventHandlers(term);

        // Don't capture a strong ref to the tab. If the tab is removed as this
        // is called, we don't really care anymore about handling the event.
        term.TitleChanged([weakTab{ hostingTab.get_weak() }, weakThis{ get_weak() }](auto newTitle) {
            auto page{ weakThis.get() };
            auto tab{ weakTab.get() };

            if (page && tab)
            {
                // The title of the control changed, but not necessarily the title
                // of the tab. Get the title of the focused pane of the tab, and set
                // the tab's text to the focused panes' text.
                page->_UpdateTitle(*tab);
            }
        });
    }

    // Method Description:
    // - Sets focus to the tab to the right or left the currently selected tab.
    void TerminalPage::_SelectNextTab(const bool bMoveRight)
    {
        if (auto index{ _GetFocusedTabIndex() })
        {
            uint32_t tabCount = _tabs.Size();
            // Wraparound math. By adding tabCount and then calculating modulo tabCount,
            // we clamp the values to the range [0, tabCount) while still supporting moving
            // leftward from 0 to tabCount - 1.
            const auto newTabIndex = ((tabCount + *index + (bMoveRight ? 1 : -1)) % tabCount);
            _SelectTab(newTabIndex);
        }
    }

    // Method Description:
    // - Sets focus to the desired tab. Returns false if the provided tabIndex
    //   is greater than the number of tabs we have.
    // - During startup, we'll immediately set the selected tab as focused.
    // - After startup, we'll dispatch an async method to set the the selected
    //   item of the TabView, which will then also trigger a
    //   TabView::SelectionChanged, handled in
    //   TerminalPage::_OnTabSelectionChanged
    // Return Value:
    // true iff we were able to select that tab index, false otherwise
    bool TerminalPage::_SelectTab(const uint32_t tabIndex)
    {
        if (tabIndex >= 0 && tabIndex < _tabs.Size())
        {
            if (_startupState == StartupState::InStartup)
            {
                auto tab{ _GetStrongTabImpl(tabIndex) };
                _tabView.SelectedItem(tab->GetTabViewItem());
                _UpdatedSelectedTab(tabIndex);
            }
            else
            {
                _SetFocusedTabIndex(tabIndex);
            }

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
        if (auto index{ _GetFocusedTabIndex() })
        {
            auto focusedTab{ _GetStrongTabImpl(*index) };
            focusedTab->NavigateFocus(direction);
        }
    }

    winrt::Microsoft::Terminal::TerminalControl::TermControl TerminalPage::_GetActiveControl()
    {
        if (auto index{ _GetFocusedTabIndex() })
        {
            auto focusedTab{ _GetStrongTabImpl(*index) };
            return focusedTab->GetActiveTerminalControl();
        }
        else
        {
            return nullptr;
        }
    }

    // Method Description:
    // - Returns the index in our list of tabs of the currently focused tab. If
    //      no tab is currently selected, returns -1.
    // Return Value:
    // - the index of the currently focused tab if there is one, else -1
    std::optional<uint32_t> TerminalPage::_GetFocusedTabIndex() const noexcept
    {
        // GH#1117: This is a workaround because _tabView.SelectedIndex()
        //          sometimes return incorrect result after removing some tabs
        uint32_t focusedIndex;
        if (_tabView.TabItems().IndexOf(_tabView.SelectedItem(), focusedIndex))
        {
            return focusedIndex;
        }
        return std::nullopt;
    }

    // Method Description:
    // - An async method for changing the focused tab on the UI thread. This
    //   method will _only_ set the selected item of the TabView, which will
    //   then also trigger a TabView::SelectionChanged event, which we'll handle
    //   in TerminalPage::_OnTabSelectionChanged, where we'll mark the new tab
    //   as focused.
    // Arguments:
    // - tabIndex: the index in the list of tabs to focus.
    // Return Value:
    // - <none>
    winrt::fire_and_forget TerminalPage::_SetFocusedTabIndex(const uint32_t tabIndex)
    {
        // GH#1117: This is a workaround because _tabView.SelectedIndex(tabIndex)
        //          sometimes set focus to an incorrect tab after removing some tabs
        auto weakThis{ get_weak() };

        co_await winrt::resume_foreground(_tabView.Dispatcher());

        if (auto page{ weakThis.get() })
        {
            auto tab{ _GetStrongTabImpl(tabIndex) };
            _tabView.SelectedItem(tab->GetTabViewItem());
        }
    }

    // Method Description:
    // - Close the currently focused tab. Focus will move to the left, if possible.
    void TerminalPage::_CloseFocusedTab()
    {
        if (auto index{ _GetFocusedTabIndex() })
        {
            _RemoveTabViewItemByIndex(*index);
        }
    }

    // Method Description:
    // - Close the currently focused pane. If the pane is the last pane in the
    //   tab, the tab will also be closed. This will happen when we handle the
    //   tab's Closed event.
    void TerminalPage::_CloseFocusedPane()
    {
        if (auto index{ _GetFocusedTabIndex() })
        {
            auto focusedTab{ _GetStrongTabImpl(*index) };
            focusedTab->ClosePane();
        }
    }

    // Method Description:
    // - Close the terminal app. If there is more
    //   than one tab opened, show a warning dialog.
    void TerminalPage::CloseWindow()
    {
        if (_tabs.Size() > 1 && _settings->GlobalSettings().GetConfirmCloseAllTabs())
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
        while (_tabs.Size() != 0)
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
        if (auto index{ _GetFocusedTabIndex() })
        {
            auto focusedTab{ _GetStrongTabImpl(*index) };
            focusedTab->Scroll(delta);
        }
    }

    // Method Description:
    // - Split the focused pane either horizontally or vertically, and place the
    //   given TermControl into the newly created pane.
    // - If splitType == SplitState::None, this method does nothing.
    // Arguments:
    // - splitType: one value from the TerminalApp::SplitState enum, indicating how the
    //   new pane should be split from its parent.
    // - splitMode: value from TerminalApp::SplitType enum, indicating the profile to be used in the newly split pane.
    // - newTerminalArgs: An object that may contain a blob of parameters to
    //   control which profile is created and with possible other
    //   configurations. See CascadiaSettings::BuildSettings for more details.
    void TerminalPage::_SplitPane(const TerminalApp::SplitState splitType,
                                  const TerminalApp::SplitType splitMode,
                                  const winrt::TerminalApp::NewTerminalArgs& newTerminalArgs)
    {
        // Do nothing if we're requesting no split.
        if (splitType == TerminalApp::SplitState::None)
        {
            return;
        }

        auto indexOpt = _GetFocusedTabIndex();

        // Do nothing if for some reason, there's no tab in focus. We don't want to crash.
        if (!indexOpt)
        {
            return;
        }

        try
        {
            auto focusedTab = _GetStrongTabImpl(*indexOpt);
            winrt::Microsoft::Terminal::Settings::TerminalSettings controlSettings;
            GUID realGuid;
            bool profileFound = false;

            if (splitMode == TerminalApp::SplitType::Duplicate)
            {
                std::optional<GUID> current_guid = focusedTab->GetFocusedProfile();
                if (current_guid)
                {
                    profileFound = true;
                    controlSettings = _settings->BuildSettings(current_guid.value());
                    realGuid = current_guid.value();
                }
                // TODO: GH#5047 - In the future, we should get the Profile of
                // the focused pane, and use that to build a new instance of the
                // settings so we can duplicate this tab/pane.
                //
                // Currently, if the profile doesn't exist anymore in our
                // settings, we'll silently do nothing.
                //
                // In the future, it will be preferable to just duplicate the
                // current control's settings, but we can't do that currently,
                // because we won't be able to create a new instance of the
                // connection without keeping an instance of the original Profile
                // object around.
            }
            if (!profileFound)
            {
                std::tie(realGuid, controlSettings) = _settings->BuildSettings(newTerminalArgs);
            }

            const auto controlConnection = _CreateConnectionFromSettings(realGuid, controlSettings);

            const auto canSplit = focusedTab->CanSplitPane(splitType);

            if (!canSplit && _startupState == StartupState::Initialized)
            {
                return;
            }

            auto realSplitType = splitType;
            if (realSplitType == SplitState::Automatic && _startupState < StartupState::Initialized)
            {
                float contentWidth = gsl::narrow_cast<float>(_tabContent.ActualWidth());
                float contentHeight = gsl::narrow_cast<float>(_tabContent.ActualHeight());
                realSplitType = focusedTab->PreCalculateAutoSplit({ contentWidth, contentHeight });
            }

            TermControl newControl{ controlSettings, controlConnection };

            // Hookup our event handlers to the new terminal
            _RegisterTerminalEvents(newControl, *focusedTab);

            focusedTab->SplitPane(realSplitType, realGuid, newControl);
        }
        CATCH_LOG();
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
        if (auto index{ _GetFocusedTabIndex() })
        {
            auto focusedTab{ _GetStrongTabImpl(*index) };
            focusedTab->ResizePane(direction);
        }
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
        auto indexOpt = _GetFocusedTabIndex();
        // Do nothing if for some reason, there's no tab in focus. We don't want to crash.
        if (!indexOpt)
        {
            return;
        }

        delta = std::clamp(delta, -1, 1);
        const auto control = _GetActiveControl();
        const auto termHeight = control.GetViewHeight();
        auto focusedTab{ _GetStrongTabImpl(*indexOpt) };
        focusedTab->Scroll(termHeight * delta);
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
                    if (auto focusedControl{ _GetActiveControl() })
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
    // - Calculates the appropriate size to snap to in the given direction, for
    //   the given dimension. If the global setting `snapToGridOnResize` is set
    //   to `false`, this will just immediately return the provided dimension,
    //   effectively disabling snapping.
    // - See Pane::CalcSnappedDimension
    float TerminalPage::CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
    {
        if (_settings->GlobalSettings().SnapToGridOnResize())
        {
            if (auto index{ _GetFocusedTabIndex() })
            {
                auto focusedTab{ _GetStrongTabImpl(*index) };
                return focusedTab->CalcSnappedDimension(widthOrHeight, dimension);
            }
        }
        return dimension;
    }

    // Method Description:
    // - Place `copiedData` into the clipboard as text. Triggered when a
    //   terminal control raises it's CopyToClipboard event.
    // Arguments:
    // - copiedData: the new string content to place on the clipboard.
    winrt::fire_and_forget TerminalPage::_CopyToClipboardHandler(const IInspectable /*sender*/,
                                                                 const winrt::Microsoft::Terminal::TerminalControl::CopyToClipboardEventArgs copiedData)
    {
        co_await winrt::resume_foreground(Dispatcher(), CoreDispatcherPriority::High);

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

        // copy rtf data to dataPack
        const auto rtfData = copiedData.Rtf();
        if (!rtfData.empty())
        {
            dataPack.SetRtf(rtfData);
        }

        try
        {
            Clipboard::SetContent(dataPack);
            Clipboard::Flush();
        }
        CATCH_LOG();
    }

    // Method Description:
    // - Fires an async event to get data from the clipboard, and paste it to
    //   the terminal. Triggered when the Terminal Control requests clipboard
    //   data with it's PasteFromClipboard event.
    // Arguments:
    // - eventArgs: the PasteFromClipboard event sent from the TermControl
    winrt::fire_and_forget TerminalPage::_PasteFromClipboardHandler(const IInspectable /*sender*/,
                                                                    const PasteFromClipboardEventArgs eventArgs)
    {
        co_await winrt::resume_foreground(Dispatcher(), CoreDispatcherPriority::High);

        TerminalPage::PasteFromClipboard(eventArgs);
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
    // - singleLine: if enabled, copy contents as a single line of text
    // Return Value:
    // - true iff we we able to copy text (if a selection was active)
    bool TerminalPage::_CopyText(const bool singleLine)
    {
        const auto control = _GetActiveControl();
        return control.CopySelectionToClipboard(singleLine);
    }

    // Method Description:
    // - Paste text from the Windows Clipboard to the focused terminal
    void TerminalPage::_PasteText()
    {
        const auto control = _GetActiveControl();
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
    //      visibility.  This method is also invoked when tabs are dragged / dropped as part of tab reordering
    //      and this method hands that case as well in concert with TabDragStarting and TabDragCompleted handlers
    //      that are set up in TerminalPage::Create()
    // Arguments:
    // - sender: the control that originated this event
    // - eventArgs: the event's constituent arguments
    void TerminalPage::_OnTabItemsChanged(const IInspectable& /*sender*/, const Windows::Foundation::Collections::IVectorChangedEventArgs& eventArgs)
    {
        if (_rearranging)
        {
            if (eventArgs.CollectionChange() == Windows::Foundation::Collections::CollectionChange::ItemRemoved)
            {
                _rearrangeFrom = eventArgs.Index();
            }

            if (eventArgs.CollectionChange() == Windows::Foundation::Collections::CollectionChange::ItemInserted)
            {
                _rearrangeTo = eventArgs.Index();
            }
        }

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

    void TerminalPage::_UpdatedSelectedTab(const int32_t index)
    {
        // Unfocus all the tabs.
        for (auto tab : _tabs)
        {
            auto tabImpl{ _GetStrongTabImpl(tab) };
            tabImpl->SetFocused(false);
        }

        if (index >= 0)
        {
            try
            {
                auto tab{ _GetStrongTabImpl(index) };

                _tabContent.Children().Clear();
                _tabContent.Children().Append(tab->GetRootElement());

                tab->SetFocused(true);
                _titleChangeHandlers(*this, Title());
            }
            CATCH_LOG();
        }
    }

    // Method Description:
    // - Responds to the TabView control's Selection Changed event (to move a
    //      new terminal control into focus) when not in in the middle of a tab rearrangement.
    // Arguments:
    // - sender: the control that originated this event
    // - eventArgs: the event's constituent arguments
    void TerminalPage::_OnTabSelectionChanged(const IInspectable& sender, const WUX::Controls::SelectionChangedEventArgs& /*eventArgs*/)
    {
        if (!_rearranging)
        {
            auto tabView = sender.as<MUX::Controls::TabView>();
            auto selectedIndex = tabView.SelectedIndex();
            _UpdatedSelectedTab(selectedIndex);
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
        for (auto tab : _tabs)
        {
            auto tabImpl{ _GetStrongTabImpl(tab) };
            tabImpl->ResizeContent(newSize);
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
    void TerminalPage::_OnTabCloseRequested(const IInspectable& /*sender*/, const MUX::Controls::TabViewTabCloseRequestedEventArgs& eventArgs)
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
    winrt::fire_and_forget TerminalPage::_RefreshUIForSettingsReload()
    {
        // Re-wire the keybindings to their handlers, as we'll have created a
        // new AppKeyBindings object.
        _HookupKeyBindings(_settings->GetKeybindings());

        // Refresh UI elements
        auto profiles = _settings->GetProfiles();
        for (auto& profile : profiles)
        {
            const GUID profileGuid = profile.GetGuid();

            try
            {
                // BuildSettings can throw an exception if the profileGuid does
                // not belong to an actual profile in the list of profiles.
                const auto settings = _settings->BuildSettings(profileGuid);

                for (auto tab : _tabs)
                {
                    // Attempt to reload the settings of any panes with this profile
                    auto tabImpl{ _GetStrongTabImpl(tab) };
                    tabImpl->UpdateSettings(settings, profileGuid);
                }
            }
            CATCH_LOG();
        }

        // GH#2455: If there are any panes with controls that had been
        // initialized with a Profile that no longer exists in our list of
        // profiles, we'll leave it unmodified. The profile doesn't exist
        // anymore, so we can't possibly update its settings.

        // Update the icon of the tab for the currently focused profile in that tab.
        for (auto tab : _tabs)
        {
            auto tabImpl{ _GetStrongTabImpl(tab) };
            _UpdateTabIcon(*tabImpl);
            _UpdateTitle(*tabImpl);
        }

        auto weakThis{ get_weak() };

        co_await winrt::resume_foreground(Dispatcher());

        // repopulate the new tab button's flyout with entries for each
        // profile, which might have changed
        if (auto page{ weakThis.get() })
        {
            _UpdateTabWidthMode();
            _CreateNewTabFlyout();
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
    //   or 0. (see TerminalPage::_ParseArgs)
    int32_t TerminalPage::SetStartupCommandline(winrt::array_view<const hstring> args)
    {
        return _ParseArgs(args);
    }

    // Method Description:
    // - Attempts to parse an array of commandline args into a list of
    //   commands to execute, and then parses these commands. As commands are
    //   successfully parsed, they will generate ShortcutActions for us to be
    //   able to execute. If we fail to parse any commands, we'll return the
    //   error code from the failure to parse that command, and stop processing
    //   additional commands.
    // Arguments:
    // - args: an array of strings to process as a commandline. These args can contain spaces
    // Return Value:
    // - 0 if the commandline was successfully parsed
    int TerminalPage::_ParseArgs(winrt::array_view<const hstring>& args)
    {
        auto commands = ::TerminalApp::AppCommandlineArgs::BuildCommands(args);

        for (auto& cmdBlob : commands)
        {
            // On one hand, it seems like we should be able to have one
            // AppCommandlineArgs for parsing all of them, and collect the
            // results one at a time.
            //
            // On the other hand, re-using a CLI::App seems to leave state from
            // previous parsings around, so we could get mysterious behavior
            // where one command affects the values of the next.
            //
            // From https://cliutils.github.io/CLI11/book/chapters/options.html:
            // > If that option is not given, CLI11 will not touch the initial
            // > value. This allows you to set up defaults by simply setting
            // > your value beforehand.
            //
            // So we pretty much need the to either manually reset the state
            // each command, or build new ones.
            const auto result = _appArgs.ParseCommand(cmdBlob);

            // If this succeeded, result will be 0. Otherwise, the caller should
            // exit(result), to exit the program.
            if (result != 0)
            {
                return result;
            }
        }

        // If all the args were successfully parsed, we'll have some commands
        // built in _appArgs, which we'll use when the application starts up.
        return 0;
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

    // Method Description:
    // - Called when the user tries to do a search using keybindings.
    //   This will tell the current focused terminal control to create
    //   a search box and enable find process.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_Find()
    {
        const auto termControl = _GetActiveControl();
        termControl.CreateSearchBoxControl();
    }

    // Method Description:
    // - Toggles fullscreen mode. Hides the tab row, and raises our
    //   ToggleFullscreen event.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_ToggleFullscreen()
    {
        _toggleFullscreenHandlers(*this, nullptr);

        _isFullscreen = !_isFullscreen;

        _UpdateTabView();
    }

    // Method Description:
    // - If there were any errors parsing the commandline that was used to
    //   initialize the terminal, this will return a string containing that
    //   message. If there were no errors, this message will be blank.
    // - If the user requested help on any command (using --help), this will
    //   contain the help message.
    // Arguments:
    // - <none>
    // Return Value:
    // - the help text or error message for the provided commandline, if one
    //   exists, otherwise the empty string.
    winrt::hstring TerminalPage::EarlyExitMessage()
    {
        return winrt::to_hstring(_appArgs.GetExitMessage());
    }

    // Method Description:
    // - Returns a com_ptr to the implementation type of the tab at the given index
    // Arguments:
    // - index: an unsigned integer index to a tab in _tabs
    // Return Value:
    // - a com_ptr to the implementation type of the Tab
    winrt::com_ptr<Tab> TerminalPage::_GetStrongTabImpl(const uint32_t index) const
    {
        winrt::com_ptr<Tab> tabImpl;
        tabImpl.copy_from(winrt::get_self<Tab>(_tabs.GetAt(index)));
        return tabImpl;
    }

    // Method Description:
    // - Returns a com_ptr to the implementation type of the given projected Tab
    // Arguments:
    // - tab: the projected type of a Tab
    // Return Value:
    // - a com_ptr to the implementation type of the Tab
    winrt::com_ptr<Tab> TerminalPage::_GetStrongTabImpl(const ::winrt::TerminalApp::Tab& tab) const
    {
        winrt::com_ptr<Tab> tabImpl;
        tabImpl.copy_from(winrt::get_self<Tab>(tab));
        return tabImpl;
    }

    // -------------------------------- WinRT Events ---------------------------------
    // Winrt events need a method for adding a callback to the event and removing the callback.
    // These macros will define them both for you.
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TerminalPage, TitleChanged, _titleChangeHandlers, winrt::Windows::Foundation::IInspectable, winrt::hstring);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TerminalPage, LastTabClosed, _lastTabClosedHandlers, winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::LastTabClosedEventArgs);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TerminalPage, SetTitleBarContent, _setTitleBarContentHandlers, winrt::Windows::Foundation::IInspectable, UIElement);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TerminalPage, ShowDialog, _showDialogHandlers, winrt::Windows::Foundation::IInspectable, WUX::Controls::ContentDialog);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TerminalPage, ToggleFullscreen, _toggleFullscreenHandlers, winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::ToggleFullscreenEventArgs);
}
