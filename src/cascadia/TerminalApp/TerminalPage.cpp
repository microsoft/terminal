// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalPage.h"
#include "Utils.h"
#include "AppLogic.h"
#include "../../types/inc/utils.hpp"

#include <LibraryResources.h>

#include "TerminalPage.g.cpp"
#include <winrt/Windows.Storage.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>

#include "TabRowControl.h"
#include "ColorHelper.h"
#include "DebugTapConnection.h"

using namespace winrt;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::System;
using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::UI::Text;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace winrt::Microsoft::Terminal::Settings::Model;
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
        _tabs{ winrt::single_threaded_observable_vector<TerminalApp::Tab>() },
        _startupActions{ winrt::single_threaded_vector<ActionAndArgs>() }
    {
        InitializeComponent();
    }

    // Function Description:
    // - Recursively check our commands to see if there's a keybinding for
    //   exactly their action. If there is, label that command with the text
    //   corresponding to that key chord.
    // - Will recurse into nested commands as well.
    // Arguments:
    // - settings: The settings who's keybindings we should use to look up the key chords from
    // - commands: The list of commands to label.
    static void _recursiveUpdateCommandKeybindingLabels(CascadiaSettings settings,
                                                        IMapView<winrt::hstring, Command> commands)
    {
        for (const auto& nameAndCmd : commands)
        {
            const auto& command = nameAndCmd.Value();
            // If there's a keybinding that's bound to exactly this command,
            // then get the string for that keychord and display it as a
            // part of the command in the UI. Each Command's KeyChordText is
            // unset by default, so we don't need to worry about clearing it
            // if there isn't a key associated with it.
            auto keyChord{ settings.KeyMap().GetKeyBindingForActionWithArgs(command.Action()) };

            if (keyChord)
            {
                command.KeyChordText(KeyChordSerialization::ToString(keyChord));
            }
            if (command.HasNestedCommands())
            {
                _recursiveUpdateCommandKeybindingLabels(settings, command.NestedCommands());
            }
        }
    }

    static void _recursiveUpdateCommandIcons(IMapView<winrt::hstring, Command> commands)
    {
        for (const auto& nameAndCmd : commands)
        {
            const auto& command = nameAndCmd.Value();

            // !!! LOAD-BEARING !!! If this is never called, then Commands will
            // have a nullptr icon. If they do, a really weird crash can occur.
            // MAKE SURE this is called once after a settings load.
            command.RefreshIcon();

            if (command.HasNestedCommands())
            {
                _recursiveUpdateCommandIcons(command.NestedCommands());
            }
        }
    }

    winrt::fire_and_forget TerminalPage::SetSettings(CascadiaSettings settings, bool needRefreshUI)
    {
        _settings = settings;
        if (needRefreshUI)
        {
            _RefreshUIForSettingsReload();
        }

        auto weakThis{ get_weak() };
        co_await winrt::resume_foreground(Dispatcher());
        if (auto page{ weakThis.get() })
        {
            _UpdateCommandsForPalette();
            CommandPalette().SetKeyBindings(*_bindings);
        }
    }

    void TerminalPage::Create()
    {
        // Hookup the key bindings
        _HookupKeyBindings(_settings.KeyMap());

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
            isElevated = ::winrt::Windows::UI::Xaml::Application::Current().as<::winrt::TerminalApp::App>().Logic().IsElevated();
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
                    page->_UpdateTabIndices();
                }

                page->_rearranging = false;
                from = std::nullopt;
                to = std::nullopt;
            }
        });

        auto tabRowImpl = winrt::get_self<implementation::TabRowControl>(_tabRow);
        _newTabButton = tabRowImpl->NewTabButton();

        if (_settings.GlobalSettings().ShowTabsInTitlebar())
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
                // if alt is pressed, open a pane
                const CoreWindow window = CoreWindow::GetForCurrentThread();
                const auto rAltState = window.GetKeyState(VirtualKey::RightMenu);
                const auto lAltState = window.GetKeyState(VirtualKey::LeftMenu);
                const bool altPressed = WI_IsFlagSet(lAltState, CoreVirtualKeyStates::Down) ||
                                        WI_IsFlagSet(rAltState, CoreVirtualKeyStates::Down);

                // Check for DebugTap
                bool debugTap = page->_settings.GlobalSettings().DebugFeaturesEnabled() &&
                                WI_IsFlagSet(lAltState, CoreVirtualKeyStates::Down) &&
                                WI_IsFlagSet(rAltState, CoreVirtualKeyStates::Down);

                if (altPressed && !debugTap)
                {
                    page->_SplitPane(SplitState::Automatic,
                                     SplitType::Manual,
                                     nullptr);
                }
                else
                {
                    page->_OpenNewTab(nullptr);
                }
            }
        });
        _tabView.SelectionChanged({ this, &TerminalPage::_OnTabSelectionChanged });
        _tabView.TabCloseRequested({ this, &TerminalPage::_OnTabCloseRequested });
        _tabView.TabItemsChanged({ this, &TerminalPage::_OnTabItemsChanged });

        _CreateNewTabFlyout();

        _UpdateTabWidthMode();

        _tabContent.SizeChanged({ this, &TerminalPage::_OnContentSizeChanged });

        CommandPalette().SetDispatch(*_actionDispatch);
        // When the visibility of the command palette changes to "collapsed",
        // the palette has been closed. Toss focus back to the currently active
        // control.
        CommandPalette().RegisterPropertyChangedCallback(UIElement::VisibilityProperty(), [this](auto&&, auto&&) {
            if (CommandPalette().Visibility() == Visibility::Collapsed)
            {
                _CommandPaletteClosed(nullptr, nullptr);
            }
        });

        _tabs.VectorChanged([weakThis{ get_weak() }](auto&& s, auto&& e) {
            if (auto page{ weakThis.get() })
            {
                page->CommandPalette().OnTabsChanged(s, e);
            }
        });

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
            if (_startupActions.Size() == 0)
            {
                _OpenNewTab(nullptr);

                _CompleteInitialization();
            }
            else
            {
                _ProcessStartupActions(_startupActions, true);
            }
        }
    }

    // Method Description:
    // - Process all the startup actions in the provided list of startup
    //   actions. We'll do this all at once here.
    // Arguments:
    // - actions: a winrt vector of actions to process. Note that this must NOT
    //   be an IVector&, because we need the collection to be accessible on the
    //   other side of the co_await.
    // - initial: if true, we're parsing these args during startup, and we
    //   should fire an Initialized event.
    // Return Value:
    // - <none>
    winrt::fire_and_forget TerminalPage::_ProcessStartupActions(Windows::Foundation::Collections::IVector<ActionAndArgs> actions,
                                                                const bool initial)
    {
        // If there are no actions left, do nothing.
        if (actions.Size() == 0)
        {
            return;
        }
        auto weakThis{ get_weak() };

        // Handle it on a subsequent pass of the UI thread.
        co_await winrt::resume_foreground(Dispatcher(), CoreDispatcherPriority::Normal);
        if (auto page{ weakThis.get() })
        {
            for (const auto& action : actions)
            {
                if (auto page{ weakThis.get() })
                {
                    _actionDispatch->DoAction(action);
                }
                else
                {
                    co_return;
                }
            }
        }
        if (initial)
        {
            _CompleteInitialization();
        }
    }

    // Method Description:
    // - Perform and steps that need to be done once our initial state is all
    //   set up. This includes entering fullscreen mode and firing our
    //   Initialized event.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_CompleteInitialization()
    {
        _startupState = StartupState::Initialized;
        _InitializedHandlers(*this, nullptr);
    }

    // Method Description:
    // - Show a dialog with "About" information. Displays the app's Display
    //   Name, version, getting started link, documentation link, release
    //   Notes link, and privacy policy link.
    void TerminalPage::_ShowAboutDialog()
    {
        if (auto presenter{ _dialogPresenter.get() })
        {
            presenter.ShowDialog(FindName(L"AboutDialog").try_as<WUX::Controls::ContentDialog>());
        }
    }

    winrt::hstring TerminalPage::ApplicationDisplayName()
    {
        return CascadiaSettings::ApplicationDisplayName();
    }

    winrt::hstring TerminalPage::ApplicationVersion()
    {
        return CascadiaSettings::ApplicationVersion();
    }

    void TerminalPage::_ThirdPartyNoticesOnClick(const IInspectable& /*sender*/, const Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/)
    {
        std::filesystem::path currentPath{ wil::GetModuleFileNameW<std::wstring>(nullptr) };
        currentPath.replace_filename(L"NOTICE.html");
        ShellExecute(nullptr, nullptr, currentPath.c_str(), nullptr, nullptr, SW_SHOW);
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
        if (auto presenter{ _dialogPresenter.get() })
        {
            presenter.ShowDialog(FindName(L"CloseAllDialog").try_as<WUX::Controls::ContentDialog>());
        }
    }

    // Method Description:
    // - Displays a dialog to warn the user about the fact that the text that
    //   they are trying to paste contains the "new line" character which can
    //   have the effect of starting commands without the user's knowledge if
    //   it is pasted on a shell where the "new line" character marks the end
    //   of a command.
    // - Only one dialog can be visible at a time. If another dialog is visible
    //   when this is called, nothing happens. See _ShowDialog for details
    winrt::Windows::Foundation::IAsyncOperation<ContentDialogResult> TerminalPage::_ShowMultiLinePasteWarningDialog()
    {
        if (auto presenter{ _dialogPresenter.get() })
        {
            co_return co_await presenter.ShowDialog(FindName(L"MultiLinePasteDialog").try_as<WUX::Controls::ContentDialog>());
        }
        co_return ContentDialogResult::None;
    }

    // Method Description:
    // - Displays a dialog to warn the user about the fact that the text that
    //   they are trying to paste is very long, in case they did not mean to
    //   paste it but pressed the paste shortcut by accident.
    // - Only one dialog can be visible at a time. If another dialog is visible
    //   when this is called, nothing happens. See _ShowDialog for details
    winrt::Windows::Foundation::IAsyncOperation<ContentDialogResult> TerminalPage::_ShowLargePasteWarningDialog()
    {
        if (auto presenter{ _dialogPresenter.get() })
        {
            co_return co_await presenter.ShowDialog(FindName(L"LargePasteDialog").try_as<WUX::Controls::ContentDialog>());
        }
        co_return ContentDialogResult::None;
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
        auto keyBindings = _settings.KeyMap();

        const auto defaultProfileGuid = _settings.GlobalSettings().DefaultProfile();
        // the number of profiles should not change in the loop for this to work
        auto const profileCount = gsl::narrow_cast<int>(_settings.Profiles().Size());
        for (int profileIndex = 0; profileIndex < profileCount; profileIndex++)
        {
            const auto profile = _settings.Profiles().GetAt(profileIndex);
            auto profileMenuItem = WUX::Controls::MenuFlyoutItem{};

            // Add the keyboard shortcuts based on the number of profiles defined
            // Look for a keychord that is bound to the equivalent
            // NewTab(ProfileIndex=N) action
            NewTerminalArgs newTerminalArgs{ profileIndex };
            NewTabArgs newTabArgs{ newTerminalArgs };
            ActionAndArgs actionAndArgs{ ShortcutAction::NewTab, newTabArgs };
            auto profileKeyChord{ keyBindings.GetKeyBindingForActionWithArgs(actionAndArgs) };

            // make sure we find one to display
            if (profileKeyChord)
            {
                _SetAcceleratorForMenuItem(profileMenuItem, profileKeyChord);
            }

            auto profileName = profile.Name();
            profileMenuItem.Text(profileName);

            // If there's an icon set for this profile, set it as the icon for
            // this flyout item.
            if (!profile.IconPath().empty())
            {
                auto iconSource = GetColoredIcon<WUX::Controls::IconSource>(profile.ExpandedIconPath());

                WUX::Controls::IconSourceElement iconElement;
                iconElement.IconSource(iconSource);
                profileMenuItem.Icon(iconElement);
                Automation::AutomationProperties::SetAccessibilityView(iconElement, Automation::Peers::AccessibilityView::Raw);
            }

            if (profile.Guid() == defaultProfileGuid)
            {
                // Contrast the default profile with others in font weight.
                profileMenuItem.FontWeight(FontWeights::Bold());
            }

            profileMenuItem.Click([profileIndex, weakThis{ get_weak() }](auto&&, auto&&) {
                if (auto page{ weakThis.get() })
                {
                    NewTerminalArgs newTerminalArgs{ profileIndex };

                    // if alt is pressed, open a pane
                    const CoreWindow window = CoreWindow::GetForCurrentThread();
                    const auto rAltState = window.GetKeyState(VirtualKey::RightMenu);
                    const auto lAltState = window.GetKeyState(VirtualKey::LeftMenu);
                    const bool altPressed = WI_IsFlagSet(lAltState, CoreVirtualKeyStates::Down) ||
                                            WI_IsFlagSet(rAltState, CoreVirtualKeyStates::Down);

                    // Check for DebugTap
                    bool debugTap = page->_settings.GlobalSettings().DebugFeaturesEnabled() &&
                                    WI_IsFlagSet(lAltState, CoreVirtualKeyStates::Down) &&
                                    WI_IsFlagSet(rAltState, CoreVirtualKeyStates::Down);

                    if (altPressed && !debugTap)
                    {
                        page->_SplitPane(SplitState::Automatic,
                                         SplitType::Manual,
                                         newTerminalArgs);
                    }
                    else
                    {
                        page->_OpenNewTab(newTerminalArgs);
                    }
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
    //   configurations. See TerminalSettings::BuildSettings for more details.
    void TerminalPage::_OpenNewTab(const NewTerminalArgs& newTerminalArgs)
    try
    {
        auto [profileGuid, settings] = TerminalSettings::BuildSettings(_settings, newTerminalArgs, *_bindings);

        _CreateNewTabFromSettings(profileGuid, settings);

        const uint32_t tabCount = _tabs.Size();
        const bool usedManualProfile = (newTerminalArgs != nullptr) &&
                                       (newTerminalArgs.ProfileIndex() != nullptr ||
                                        newTerminalArgs.Profile().empty());

        // Lookup the name of the color scheme used by this profile.
        const auto scheme = _settings.GetColorSchemeForProfile(profileGuid);
        // If they explicitly specified `null` as the scheme (indicating _no_ scheme), log
        // that as the empty string.
        const auto schemeName = scheme ? scheme.Name() : L"\0";

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
            TraceLoggingWideString(schemeName.data(), "SchemeName", "Color scheme set in the settings"),
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
    void TerminalPage::_CreateNewTabFromSettings(GUID profileGuid, TerminalApp::TerminalSettings settings)
    {
        // Initialize the new tab

        // Create a connection based on the values in our settings object.
        auto connection = _CreateConnectionFromSettings(profileGuid, settings);

        TerminalConnection::ITerminalConnection debugConnection{ nullptr };
        if (_settings.GlobalSettings().DebugFeaturesEnabled())
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

        // Give the tab its index in the _tabs vector so it can manage its own SwitchToTab command.
        newTabImpl->UpdateTabViewIndex(_tabs.Size() - 1);

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
        // GH#6570
        // The TabView does not apply compact sizing to items added after Compact is enabled.
        // By forcibly reapplying compact sizing every time we add a new tab, we'll make sure
        // that it works.
        // Workaround from https://github.com/microsoft/microsoft-ui-xaml/issues/2711
        if (_tabView.TabWidthMode() == MUX::Controls::TabViewWidthMode::Compact)
        {
            _tabView.UpdateLayout();
            _tabView.TabWidthMode(MUX::Controls::TabViewWidthMode::Compact);
        }

        // Set this tab's icon to the icon from the user's profile
        const auto profile = _settings.FindProfile(profileGuid);
        if (profile != nullptr && !profile.IconPath().empty())
        {
            newTabImpl->UpdateIcon(profile.ExpandedIconPath());
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
                                                                                        TerminalApp::TerminalSettings settings)
    {
        const auto profile = _settings.FindProfile(profileGuid);

        TerminalConnection::ITerminalConnection connection{ nullptr };

        winrt::guid connectionType{};
        winrt::guid sessionGuid{};

        const auto hasConnectionType = profile.HasConnectionType();
        if (hasConnectionType)
        {
            connectionType = profile.ConnectionType();
        }

        if (hasConnectionType &&
            connectionType == TerminalConnection::AzureConnection::ConnectionType() &&
            TerminalConnection::AzureConnection::IsAzureConnectionAvailable())
        {
            // TODO GH#4661: Replace this with directly using the AzCon when our VT is better
            std::filesystem::path azBridgePath{ wil::GetModuleFileNameW<std::wstring>(nullptr) };
            azBridgePath.replace_filename(L"TerminalAzBridge.exe");
            connection = TerminalConnection::ConptyConnection(azBridgePath.wstring(),
                                                              L".",
                                                              L"Azure",
                                                              nullptr,
                                                              settings.InitialRows(),
                                                              settings.InitialCols(),
                                                              winrt::guid());
        }

        else if (hasConnectionType &&
                 connectionType == TerminalConnection::TelnetConnection::ConnectionType())
        {
            connection = TerminalConnection::TelnetConnection(settings.Commandline());
        }

        else
        {
            std::wstring guidWString = Utils::GuidToString(profileGuid);

            StringMap envMap{};
            envMap.Insert(L"WT_PROFILE_ID", guidWString);
            envMap.Insert(L"WSLENV", L"WT_PROFILE_ID");

            auto conhostConn = TerminalConnection::ConptyConnection(
                settings.Commandline(),
                settings.StartingDirectory(),
                settings.StartingTitle(),
                envMap.GetView(),
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

        const auto target = altPressed ? SettingsTarget::DefaultsFile : SettingsTarget::SettingsFile;
        _LaunchSettings(target);
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
    // - Configure the AppKeyBindings to use our ShortcutActionDispatch and the updated KeyMapping
    // as the object to handle dispatching ShortcutAction events.
    // Arguments:
    // - bindings: A AppKeyBindings object to wire up with our event handlers
    void TerminalPage::_HookupKeyBindings(const KeyMapping& keymap) noexcept
    {
        _bindings->SetDispatch(*_actionDispatch);
        _bindings->SetKeyMapping(keymap);
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
        _actionDispatch->SendInput({ this, &TerminalPage::_HandleSendInput });
        _actionDispatch->SplitPane({ this, &TerminalPage::_HandleSplitPane });
        _actionDispatch->TogglePaneZoom({ this, &TerminalPage::_HandleTogglePaneZoom });
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
        _actionDispatch->ToggleRetroEffect({ this, &TerminalPage::_HandleToggleRetroEffect });
        _actionDispatch->ToggleFocusMode({ this, &TerminalPage::_HandleToggleFocusMode });
        _actionDispatch->ToggleFullscreen({ this, &TerminalPage::_HandleToggleFullscreen });
        _actionDispatch->ToggleAlwaysOnTop({ this, &TerminalPage::_HandleToggleAlwaysOnTop });
        _actionDispatch->ToggleCommandPalette({ this, &TerminalPage::_HandleToggleCommandPalette });
        _actionDispatch->SetColorScheme({ this, &TerminalPage::_HandleSetColorScheme });
        _actionDispatch->SetTabColor({ this, &TerminalPage::_HandleSetTabColor });
        _actionDispatch->OpenTabColorPicker({ this, &TerminalPage::_HandleOpenTabColorPicker });
        _actionDispatch->RenameTab({ this, &TerminalPage::_HandleRenameTab });
        _actionDispatch->ExecuteCommandline({ this, &TerminalPage::_HandleExecuteCommandline });
        _actionDispatch->CloseOtherTabs({ this, &TerminalPage::_HandleCloseOtherTabs });
        _actionDispatch->CloseTabsAfter({ this, &TerminalPage::_HandleCloseTabsAfter });
        _actionDispatch->TabSearch({ this, &TerminalPage::_HandleOpenTabSearch });
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

        if (_settings.GlobalSettings().ShowTitleInTitlebar() &&
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
            const auto matchingProfile = _settings.FindProfile(lastFocusedProfile);
            if (matchingProfile)
            {
                tab.UpdateIcon(matchingProfile.ExpandedIconPath());
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
        _tabView.TabWidthMode(_settings.GlobalSettings().TabWidthMode());
    }

    // Method Description:
    // - Handle changes in tab layout.
    void TerminalPage::_UpdateTabView()
    {
        // Never show the tab row when we're fullscreen. Otherwise:
        // Show tabs when there's more than 1, or the user has chosen to always
        // show the tab bar.
        const bool isVisible = (!_isFullscreen && !_isInFocusMode) &&
                               (_settings.GlobalSettings().ShowTabsInTitlebar() ||
                                (_tabs.Size() > 1) ||
                                _settings.GlobalSettings().AlwaysShowTabs());

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
                    const auto settings{ winrt::make<TerminalSettings>(_settings, profileGuid.value(), *_bindings) };
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
        if (_tabView.TabItems().IndexOf(tabViewItem, tabIndexFromControl))
        {
            // If IndexOf returns true, we've actually got an index
            _RemoveTabViewItemByIndex(tabIndexFromControl);
        }
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
        _UpdateTabIndices();

        // To close the window here, we need to close the hosting window.
        if (_tabs.Size() == 0)
        {
            _lastTabClosedHandlers(*this, nullptr);
        }
        else if (_isFullscreen || _rearranging)
        {
            // GH#5799 - If we're fullscreen, the TabView isn't visible. If it's
            // not Visible, it's _not_ going to raise a SelectionChanged event,
            // which is what we usually use to focus another tab. Instead, we'll
            // have to do it manually here.
            //
            // GH#5559 Similarly, we suppress _OnTabItemsChanged events during a
            // rearrange, so if a tab is closed while we're rearranging tabs, do
            // this manually.
            //
            // We can't use
            //   auto selectedIndex = _tabView.SelectedIndex();
            // Because this will always return -1 in this scenario unfortunately.
            //
            // So, what we're going to try to do is move the focus to the tab
            // to the left, within the bounds of how many tabs we have.
            //
            // EX: we have 4 tabs: [A, B, C, D]. If we close:
            // * A (tabIndex=0): We'll want to focus tab B (now in index 0)
            // * B (tabIndex=1): We'll want to focus tab A (now in index 0)
            // * C (tabIndex=2): We'll want to focus tab B (now in index 1)
            // * D (tabIndex=3): We'll want to focus tab C (now in index 2)
            const auto newSelectedIndex = std::clamp<int32_t>(tabIndex - 1, 0, _tabs.Size());
            // _UpdatedSelectedTab will do the work of setting up the new tab as
            // the focused one, and unfocusing all the others.
            _UpdatedSelectedTab(newSelectedIndex);

            // Also, we need to _manually_ set the SelectedItem of the tabView
            // here. If we don't, then the TabView will technically not have a
            // selected item at all, which can make things like ClosePane not
            // work correctly.
            auto newSelectedTab{ _GetStrongTabImpl(newSelectedIndex) };
            _tabView.SelectedItem(newSelectedTab->GetTabViewItem());
        }

        // GH#5559 - If we were in the middle of a drag/drop, end it by clearing
        // out our state.
        if (_rearranging)
        {
            _rearranging = false;
            _rearrangeFrom = std::nullopt;
            _rearrangeTo = std::nullopt;
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

        term.OpenHyperlink({ this, &TerminalPage::_OpenHyperlinkHandler });

        // Bind Tab events to the TermControl and the Tab's Pane
        hostingTab.Initialize(term);

        auto weakTab{ hostingTab.get_weak() };
        auto weakThis{ get_weak() };
        // PropertyChanged is the generic mechanism by which the Tab
        // communicates changes to any of its observable properties, including
        // the Title
        hostingTab.PropertyChanged([weakTab, weakThis](auto&&, const WUX::Data::PropertyChangedEventArgs& args) {
            auto page{ weakThis.get() };
            auto tab{ weakTab.get() };
            if (page && tab)
            {
                if (args.PropertyName() == L"Title")
                {
                    page->_UpdateTitle(*tab);
                }
            }
        });

        // react on color changed events
        hostingTab.ColorSelected([weakTab, weakThis](auto&& color) {
            auto page{ weakThis.get() };
            auto tab{ weakTab.get() };

            if (page && tab && tab->IsFocused())
            {
                page->_SetNonClientAreaColors(color);
            }
        });

        hostingTab.ColorCleared([weakTab, weakThis]() {
            auto page{ weakThis.get() };
            auto tab{ weakTab.get() };

            if (page && tab && tab->IsFocused())
            {
                page->_ClearNonClientAreaColors();
            }
        });

        // TODO GH#3327: Once we support colorizing the NewTab button based on
        // the color of the tab, we'll want to make sure to call
        // _ClearNewTabButtonColor here, to reset it to the default (for the
        // newly created tab).
        // remove any colors left by other colored tabs
        // _ClearNewTabButtonColor();
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

            if (_settings.GlobalSettings().UseTabSwitcher())
            {
                if (CommandPalette().Visibility() == Visibility::Visible)
                {
                    CommandPalette().SelectNextItem(bMoveRight);
                }

                CommandPalette().EnableTabSwitcherMode(false, newTabIndex);
                CommandPalette().Visibility(Visibility::Visible);
            }
            else
            {
                _SelectTab(newTabIndex);
            }
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
    // - Helper to manually exit "zoom" when certain actions take place.
    //   Anything that modifies the state of the pane tree should probably
    //   un-zoom the focused pane first, so that the user can see the full pane
    //   tree again. These actions include:
    //   * Splitting a new pane
    //   * Closing a pane
    //   * Moving focus between panes
    //   * Resizing a pane
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_UnZoomIfNeeded()
    {
        auto activeTab = _GetFocusedTab();
        if (activeTab && activeTab->IsZoomed())
        {
            // Remove the content from the tab first, so Pane::UnZoom can
            // re-attach the content to the tree w/in the pane
            _tabContent.Children().Clear();
            activeTab->ExitZoom();
            // Re-attach the tab's content to the UI tree.
            _tabContent.Children().Append(activeTab->GetRootElement());
        }
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
            _UnZoomIfNeeded();
            focusedTab->NavigateFocus(direction);
        }
    }

    TermControl TerminalPage::_GetActiveControl()
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
    //      no tab is currently selected, returns nullopt.
    // Return Value:
    // - the index of the currently focused tab if there is one, else nullopt
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
    // - returns a com_ptr to the currently focused tab. This might return null,
    //   so make sure to check the result!
    winrt::com_ptr<Tab> TerminalPage::_GetFocusedTab()
    {
        if (auto index{ _GetFocusedTabIndex() })
        {
            return _GetStrongTabImpl(*index);
        }
        return nullptr;
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
            _UnZoomIfNeeded();
            focusedTab->ClosePane();
        }
    }

    // Method Description:
    // - Close the terminal app. If there is more
    //   than one tab opened, show a warning dialog.
    void TerminalPage::CloseWindow()
    {
        if (_tabs.Size() > 1 && _settings.GlobalSettings().ConfirmCloseAllTabs())
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
    void TerminalPage::_SplitPane(const SplitState splitType,
                                  const SplitType splitMode,
                                  const NewTerminalArgs& newTerminalArgs)
    {
        // Do nothing if we're requesting no split.
        if (splitType == SplitState::None)
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
            TerminalApp::TerminalSettings controlSettings;
            GUID realGuid;
            bool profileFound = false;

            if (splitMode == SplitType::Duplicate)
            {
                std::optional<GUID> current_guid = focusedTab->GetFocusedProfile();
                if (current_guid)
                {
                    profileFound = true;
                    controlSettings = { winrt::make<TerminalSettings>(_settings, current_guid.value(), *_bindings) };
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
                std::tie(realGuid, controlSettings) = TerminalSettings::BuildSettings(_settings, newTerminalArgs, *_bindings);
            }

            const auto controlConnection = _CreateConnectionFromSettings(realGuid, controlSettings);

            const float contentWidth = ::base::saturated_cast<float>(_tabContent.ActualWidth());
            const float contentHeight = ::base::saturated_cast<float>(_tabContent.ActualHeight());
            const winrt::Windows::Foundation::Size availableSpace{ contentWidth, contentHeight };

            auto realSplitType = splitType;
            if (realSplitType == SplitState::Automatic)
            {
                realSplitType = focusedTab->PreCalculateAutoSplit(availableSpace);
            }

            const auto canSplit = focusedTab->PreCalculateCanSplit(realSplitType, availableSpace);
            if (!canSplit)
            {
                return;
            }

            TermControl newControl{ controlSettings, controlConnection };

            // Hookup our event handlers to the new terminal
            _RegisterTerminalEvents(newControl, *focusedTab);

            _UnZoomIfNeeded();

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
            _UnZoomIfNeeded();
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
        if (_settings.GlobalSettings().ShowTitleInTitlebar())
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
    static std::wstring _FormatOverrideShortcutText(KeyModifiers modifiers)
    {
        std::wstring buffer{ L"" };

        if (WI_IsFlagSet(modifiers, KeyModifiers::Ctrl))
        {
            buffer += L"Ctrl+";
        }

        if (WI_IsFlagSet(modifiers, KeyModifiers::Shift))
        {
            buffer += L"Shift+";
        }

        if (WI_IsFlagSet(modifiers, KeyModifiers::Alt))
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
                                                  const KeyChord& keyChord)
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
        if (_settings.GlobalSettings().SnapToGridOnResize())
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
                                                                 const CopyToClipboardEventArgs copiedData)
    {
        co_await winrt::resume_foreground(Dispatcher(), CoreDispatcherPriority::High);

        DataPackage dataPack = DataPackage();
        dataPack.RequestedOperation(DataPackageOperation::Copy);

        // The EventArgs.Formats() is an override for the global setting "copyFormatting"
        //   iff it is set
        bool useGlobal = copiedData.Formats() == nullptr;
        auto copyFormats = useGlobal ?
                               _settings.GlobalSettings().CopyFormatting() :
                               copiedData.Formats().Value();

        // copy text to dataPack
        dataPack.SetText(copiedData.Text());

        if (WI_IsFlagSet(copyFormats, CopyFormat::HTML))
        {
            // copy html to dataPack
            const auto htmlData = copiedData.Html();
            if (!htmlData.empty())
            {
                dataPack.SetHtmlFormat(htmlData);
            }
        }

        if (WI_IsFlagSet(copyFormats, CopyFormat::RTF))
        {
            // copy rtf data to dataPack
            const auto rtfData = copiedData.Rtf();
            if (!rtfData.empty())
            {
                dataPack.SetRtf(rtfData);
            }
        }

        try
        {
            Clipboard::SetContent(dataPack);
            Clipboard::Flush();
        }
        CATCH_LOG();
    }

    // Function Description:
    // - This function is called when the `TermControl` requests that we send
    //   it the clipboard's content.
    // - Retrieves the data from the Windows Clipboard and converts it to text.
    // - Shows warnings if the clipboard is too big or contains multiple lines
    //   of text.
    // - Sends the text back to the TermControl through the event's
    //   `HandleClipboardData` member function.
    // - Does some of this in a background thread, as to not hang/crash the UI thread.
    // Arguments:
    // - eventArgs: the PasteFromClipboard event sent from the TermControl
    fire_and_forget TerminalPage::_PasteFromClipboardHandler(const IInspectable /*sender*/,
                                                             const PasteFromClipboardEventArgs eventArgs)
    {
        const DataPackageView data = Clipboard::GetContent();

        // This will switch the execution of the function to a background (not
        // UI) thread. This is IMPORTANT, because the getting the clipboard data
        // will crash on the UI thread, because the main thread is a STA.
        co_await winrt::resume_background();

        try
        {
            hstring text = L"";
            if (data.Contains(StandardDataFormats::Text()))
            {
                text = co_await data.GetTextAsync();
            }
            // Windows Explorer's "Copy address" menu item stores a StorageItem in the clipboard, and no text.
            else if (data.Contains(StandardDataFormats::StorageItems()))
            {
                Windows::Foundation::Collections::IVectorView<Windows::Storage::IStorageItem> items = co_await data.GetStorageItemsAsync();
                if (items.Size() > 0)
                {
                    Windows::Storage::IStorageItem item = items.GetAt(0);
                    text = item.Path();
                }
            }

            const bool hasNewLine = std::find(text.cbegin(), text.cend(), L'\n') != text.cend();
            const bool warnMultiLine = hasNewLine && _settings.GlobalSettings().WarnAboutMultiLinePaste();

            constexpr const std::size_t minimumSizeForWarning = 1024 * 5; // 5 KiB
            const bool warnLargeText = text.size() > minimumSizeForWarning &&
                                       _settings.GlobalSettings().WarnAboutLargePaste();

            if (warnMultiLine || warnLargeText)
            {
                co_await winrt::resume_foreground(Dispatcher());

                ContentDialogResult warningResult;
                if (warnMultiLine)
                {
                    warningResult = co_await _ShowMultiLinePasteWarningDialog();
                }
                else if (warnLargeText)
                {
                    warningResult = co_await _ShowLargePasteWarningDialog();
                }

                if (warningResult != ContentDialogResult::Primary)
                {
                    // user rejected the paste
                    co_return;
                }
            }

            eventArgs.HandleClipboardData(text);
        }
        CATCH_LOG();
    }

    void TerminalPage::_OpenHyperlinkHandler(const IInspectable /*sender*/, const Microsoft::Terminal::TerminalControl::OpenHyperlinkEventArgs eventArgs)
    {
        try
        {
            auto parsed = winrt::Windows::Foundation::Uri(eventArgs.Uri().c_str());
            if (parsed.SchemeName() == L"http" || parsed.SchemeName() == L"https")
            {
                ShellExecute(nullptr, L"open", eventArgs.Uri().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
            else
            {
                _ShowCouldNotOpenDialog(RS_(L"UnsupportedSchemeText"), eventArgs.Uri());
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            _ShowCouldNotOpenDialog(RS_(L"InvalidUriText"), eventArgs.Uri());
        }
    }

    // Method Description:
    // - Opens up a dialog box explaining why we could not open a URI
    // Arguments:
    // - The reason (unsupported scheme, invalid uri, potentially more in the future)
    // - The uri
    void TerminalPage::_ShowCouldNotOpenDialog(winrt::hstring reason, winrt::hstring uri)
    {
        if (auto presenter{ _dialogPresenter.get() })
        {
            // FindName needs to be called first to actually load the xaml object
            auto unopenedUriDialog = FindName(L"CouldNotOpenUriDialog").try_as<WUX::Controls::ContentDialog>();

            // Insert the reason and the URI
            CouldNotOpenUriReason().Text(reason);
            UnopenedUri().Text(uri);

            // Show the dialog
            presenter.ShowDialog(unopenedUriDialog);
        }
    }

    // Method Description:
    // - Copy text from the focused terminal to the Windows Clipboard
    // Arguments:
    // - singleLine: if enabled, copy contents as a single line of text
    // - formats: dictate which formats need to be copied
    // Return Value:
    // - true iff we we able to copy text (if a selection was active)
    bool TerminalPage::_CopyText(const bool singleLine, const Windows::Foundation::IReference<CopyFormat>& formats)
    {
        const auto control = _GetActiveControl();
        return control.CopySelectionToClipboard(singleLine, formats);
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
    fire_and_forget TerminalPage::_LaunchSettings(const SettingsTarget target)
    {
        // This will switch the execution of the function to a background (not
        // UI) thread. This is IMPORTANT, because the Windows.Storage API's
        // (used for retrieving the path to the file) will crash on the UI
        // thread, because the main thread is a STA.
        co_await winrt::resume_background();

        auto openFile = [](const auto& filePath) {
            HINSTANCE res = ShellExecute(nullptr, nullptr, filePath.c_str(), nullptr, nullptr, SW_SHOW);
            if (static_cast<int>(reinterpret_cast<uintptr_t>(res)) <= 32)
            {
                ShellExecute(nullptr, nullptr, L"notepad", filePath.c_str(), nullptr, SW_SHOW);
            }
        };

        switch (target)
        {
        case SettingsTarget::DefaultsFile:
            openFile(CascadiaSettings::DefaultSettingsPath());
            break;
        case SettingsTarget::SettingsFile:
            openFile(CascadiaSettings::SettingsPath());
            break;
        case SettingsTarget::AllFiles:
            openFile(CascadiaSettings::DefaultSettingsPath());
            openFile(CascadiaSettings::SettingsPath());
            break;
        }
    }

    // Method Description:
    // - Responds to changes in the TabView's item list by changing the
    //   tabview's visibility.
    // - This method is also invoked when tabs are dragged / dropped as part of
    //   tab reordering and this method hands that case as well in concert with
    //   TabDragStarting and TabDragCompleted handlers that are set up in
    //   TerminalPage::Create()
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
        else
        {
            _UpdateCommandsForPalette();
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
        else if (eventArgs.GetCurrentPoint(*this).Properties().IsRightButtonPressed())
        {
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

                // Raise an event that our title changed
                _titleChangeHandlers(*this, tab->GetActiveTitle());
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
        _HookupKeyBindings(_settings.KeyMap());

        // Refresh UI elements
        auto profiles = _settings.Profiles();
        for (const auto& profile : profiles)
        {
            const auto profileGuid = profile.Guid();

            try
            {
                // This can throw an exception if the profileGuid does
                // not belong to an actual profile in the list of profiles.
                auto settings{ winrt::make<TerminalSettings>(_settings, profileGuid, *_bindings) };

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

        // Reload the current value of alwaysOnTop from the settings file. This
        // will let the user hot-reload this setting, but any runtime changes to
        // the alwaysOnTop setting will be lost.
        _isAlwaysOnTop = _settings.GlobalSettings().AlwaysOnTop();
        _alwaysOnTopChangedHandlers(*this, nullptr);
    }

    // This is a helper to aid in sorting commands by their `Name`s, alphabetically.
    static bool _compareSchemeNames(const ColorScheme& lhs, const ColorScheme& rhs)
    {
        std::wstring leftName{ lhs.Name() };
        std::wstring rightName{ rhs.Name() };
        return leftName.compare(rightName) < 0;
    }

    // Method Description:
    // - Takes a mapping of names->commands and expands them
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    IMap<winrt::hstring, Command> TerminalPage::_ExpandCommands(IMapView<winrt::hstring, Command> commandsToExpand,
                                                                IVectorView<Profile> profiles,
                                                                IMapView<winrt::hstring, ColorScheme> schemes)
    {
        IVector<SettingsLoadWarnings> warnings;

        std::vector<ColorScheme> sortedSchemes;
        sortedSchemes.reserve(schemes.Size());

        for (const auto& nameAndScheme : schemes)
        {
            sortedSchemes.push_back(nameAndScheme.Value());
        }
        std::sort(sortedSchemes.begin(),
                  sortedSchemes.end(),
                  _compareSchemeNames);

        IMap<winrt::hstring, Command> copyOfCommands = winrt::single_threaded_map<winrt::hstring, Command>();
        for (const auto& nameAndCommand : commandsToExpand)
        {
            copyOfCommands.Insert(nameAndCommand.Key(), nameAndCommand.Value());
        }

        Command::ExpandCommands(copyOfCommands,
                                profiles,
                                { sortedSchemes },
                                warnings);

        return copyOfCommands;
    }
    // Method Description:
    // - Repopulates the list of commands in the command palette with the
    //   current commands in the settings. Also updates the keybinding labels to
    //   reflect any matching keybindings.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_UpdateCommandsForPalette()
    {
        IMap<winrt::hstring, Command> copyOfCommands = _ExpandCommands(_settings.GlobalSettings().Commands(),
                                                                       _settings.Profiles().GetView(),
                                                                       _settings.GlobalSettings().ColorSchemes());

        _recursiveUpdateCommandKeybindingLabels(_settings, copyOfCommands.GetView());
        _recursiveUpdateCommandIcons(copyOfCommands.GetView());

        // Update the command palette when settings reload
        auto commandsCollection = winrt::single_threaded_vector<Command>();
        for (const auto& nameAndCommand : copyOfCommands)
        {
            commandsCollection.Append(nameAndCommand.Value());
        }

        CommandPalette().SetCommands(commandsCollection);
    }

    // Method Description:
    // - Sets the initial actions to process on startup. We'll make a copy of
    //   this list, and process these actions when we're loaded.
    // - This function will have no effective result after Create() is called.
    // Arguments:
    // - actions: a list of Actions to process on startup.
    // Return Value:
    // - <none>
    void TerminalPage::SetStartupActions(std::vector<ActionAndArgs>& actions)
    {
        // The fastest way to copy all the actions out of the std::vector and
        // put them into a winrt::IVector is by making a copy, then moving the
        // copy into the winrt vector ctor.
        auto listCopy = actions;
        _startupActions = winrt::single_threaded_vector<ActionAndArgs>(std::move(listCopy));
    }

    winrt::TerminalApp::IDialogPresenter TerminalPage::DialogPresenter() const
    {
        return _dialogPresenter.get();
    }

    void TerminalPage::DialogPresenter(winrt::TerminalApp::IDialogPresenter dialogPresenter)
    {
        _dialogPresenter = dialogPresenter;
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
    // - Toggles borderless mode. Hides the tab row, and raises our
    //   FocusModeChanged event.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::ToggleFocusMode()
    {
        _isInFocusMode = !_isInFocusMode;
        _UpdateTabView();
        _focusModeChangedHandlers(*this, nullptr);
    }

    // Method Description:
    // - Toggles fullscreen mode. Hides the tab row, and raises our
    //   FullscreenChanged event.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::ToggleFullscreen()
    {
        _isFullscreen = !_isFullscreen;
        _UpdateTabView();
        _fullscreenChangedHandlers(*this, nullptr);
    }

    // Method Description:
    // - Toggles always on top mode. Raises our AlwaysOnTopChanged event.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::ToggleAlwaysOnTop()
    {
        _isAlwaysOnTop = !_isAlwaysOnTop;
        _alwaysOnTopChangedHandlers(*this, nullptr);
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

    // Method Description:
    // - Sets the tab split button color when a new tab color is selected
    // Arguments:
    // - color: The color of the newly selected tab, used to properly calculate
    //          the foreground color of the split button (to match the font
    //          color of the tab)
    // - accentColor: the actual color we are going to use to paint the tab row and
    //                split button, so that there is some contrast between the tab
    //                and the non-client are behind it
    // Return Value:
    // - <none>
    void TerminalPage::_SetNewTabButtonColor(const Windows::UI::Color& color, const Windows::UI::Color& accentColor)
    {
        // TODO GH#3327: Look at what to do with the tab button when we have XAML theming
        bool IsBrightColor = ColorHelper::IsBrightColor(color);
        bool isLightAccentColor = ColorHelper::IsBrightColor(accentColor);
        winrt::Windows::UI::Color pressedColor{};
        winrt::Windows::UI::Color hoverColor{};
        winrt::Windows::UI::Color foregroundColor{};
        const float hoverColorAdjustment = 5.f;
        const float pressedColorAdjustment = 7.f;

        if (IsBrightColor)
        {
            foregroundColor = winrt::Windows::UI::Colors::Black();
        }
        else
        {
            foregroundColor = winrt::Windows::UI::Colors::White();
        }

        if (isLightAccentColor)
        {
            hoverColor = ColorHelper::Darken(accentColor, hoverColorAdjustment);
            pressedColor = ColorHelper::Darken(accentColor, pressedColorAdjustment);
        }
        else
        {
            hoverColor = ColorHelper::Lighten(accentColor, hoverColorAdjustment);
            pressedColor = ColorHelper::Lighten(accentColor, pressedColorAdjustment);
        }

        Media::SolidColorBrush backgroundBrush{ accentColor };
        Media::SolidColorBrush backgroundHoverBrush{ hoverColor };
        Media::SolidColorBrush backgroundPressedBrush{ pressedColor };
        Media::SolidColorBrush foregroundBrush{ foregroundColor };

        _newTabButton.Resources().Insert(winrt::box_value(L"SplitButtonBackground"), backgroundBrush);
        _newTabButton.Resources().Insert(winrt::box_value(L"SplitButtonBackgroundPointerOver"), backgroundHoverBrush);
        _newTabButton.Resources().Insert(winrt::box_value(L"SplitButtonBackgroundPressed"), backgroundPressedBrush);

        _newTabButton.Resources().Insert(winrt::box_value(L"SplitButtonForeground"), foregroundBrush);
        _newTabButton.Resources().Insert(winrt::box_value(L"SplitButtonForegroundPointerOver"), foregroundBrush);
        _newTabButton.Resources().Insert(winrt::box_value(L"SplitButtonForegroundPressed"), foregroundBrush);

        _newTabButton.Background(backgroundBrush);
        _newTabButton.Foreground(foregroundBrush);
    }

    // Method Description:
    // - Clears the tab split button color to a system color
    //   (or white if none is found) when the tab's color is cleared
    // - Clears the tab row color to a system color
    //   (or white if none is found) when the tab's color is cleared
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_ClearNewTabButtonColor()
    {
        // TODO GH#3327: Look at what to do with the tab button when we have XAML theming
        winrt::hstring keys[] = {
            L"SplitButtonBackground",
            L"SplitButtonBackgroundPointerOver",
            L"SplitButtonBackgroundPressed",
            L"SplitButtonForeground",
            L"SplitButtonForegroundPointerOver",
            L"SplitButtonForegroundPressed"
        };

        // simply clear any of the colors in the split button's dict
        for (auto keyString : keys)
        {
            auto key = winrt::box_value(keyString);
            if (_newTabButton.Resources().HasKey(key))
            {
                _newTabButton.Resources().Remove(key);
            }
        }

        const auto res = Application::Current().Resources();

        const auto defaultBackgroundKey = winrt::box_value(L"TabViewItemHeaderBackground");
        const auto defaultForegroundKey = winrt::box_value(L"SystemControlForegroundBaseHighBrush");
        winrt::Windows::UI::Xaml::Media::SolidColorBrush backgroundBrush;
        winrt::Windows::UI::Xaml::Media::SolidColorBrush foregroundBrush;

        // TODO: Related to GH#3917 - I think if the system is set to "Dark"
        // theme, but the app is set to light theme, then this lookup still
        // returns to us the dark theme brushes. There's gotta be a way to get
        // the right brushes...
        // See also GH#5741
        if (res.HasKey(defaultBackgroundKey))
        {
            winrt::Windows::Foundation::IInspectable obj = res.Lookup(defaultBackgroundKey);
            backgroundBrush = obj.try_as<winrt::Windows::UI::Xaml::Media::SolidColorBrush>();
        }
        else
        {
            backgroundBrush = winrt::Windows::UI::Xaml::Media::SolidColorBrush{ winrt::Windows::UI::Colors::Black() };
        }

        if (res.HasKey(defaultForegroundKey))
        {
            winrt::Windows::Foundation::IInspectable obj = res.Lookup(defaultForegroundKey);
            foregroundBrush = obj.try_as<winrt::Windows::UI::Xaml::Media::SolidColorBrush>();
        }
        else
        {
            foregroundBrush = winrt::Windows::UI::Xaml::Media::SolidColorBrush{ winrt::Windows::UI::Colors::White() };
        }

        _newTabButton.Background(backgroundBrush);
        _newTabButton.Foreground(foregroundBrush);
    }

    // Method Description:
    // - Sets the tab split button color when a new tab color is selected
    // - This method could also set the color of the title bar and tab row
    // in the future
    // Arguments:
    // - selectedTabColor: The color of the newly selected tab
    // Return Value:
    // - <none>
    void TerminalPage::_SetNonClientAreaColors(const Windows::UI::Color& /*selectedTabColor*/)
    {
        // TODO GH#3327: Look at what to do with the NC area when we have XAML theming
    }

    // Method Description:
    // - Clears the tab split button color when the tab's color is cleared
    // - This method could also clear the color of the title bar and tab row
    // in the future
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_ClearNonClientAreaColors()
    {
        // TODO GH#3327: Look at what to do with the NC area when we have XAML theming
    }

    // Function Description:
    // - This is a helper method to get the commandline out of a
    //   ExecuteCommandline action, break it into subcommands, and attempt to
    //   parse it into actions. This is used by _HandleExecuteCommandline for
    //   processing commandlines in the current WT window.
    // Arguments:
    // - args: the ExecuteCommandlineArgs to synthesize a list of startup actions for.
    // Return Value:
    // - an empty list if we failed to parse, otherwise a list of actions to execute.
    std::vector<ActionAndArgs> TerminalPage::ConvertExecuteCommandlineToActions(const ExecuteCommandlineArgs& args)
    {
        if (!args || args.Commandline().empty())
        {
            return {};
        }
        // Convert the commandline into an array of args with
        // CommandLineToArgvW, similar to how the app typically does when
        // called from the commandline.
        int argc = 0;
        wil::unique_any<LPWSTR*, decltype(&::LocalFree), ::LocalFree> argv{ CommandLineToArgvW(args.Commandline().c_str(), &argc) };
        if (argv)
        {
            std::vector<winrt::hstring> args;

            // Make sure the first argument is wt.exe, because ParseArgs will
            // always skip the program name. The particular value of this first
            // string doesn't terribly matter.
            args.emplace_back(L"wt.exe");
            for (auto& elem : wil::make_range(argv.get(), argc))
            {
                args.emplace_back(elem);
            }
            winrt::array_view<const winrt::hstring> argsView{ args };

            ::TerminalApp::AppCommandlineArgs appArgs;
            if (appArgs.ParseArgs(argsView) == 0)
            {
                return appArgs.GetStartupActions();
            }
        }
        return {};
    }

    void TerminalPage::_CommandPaletteClosed(const IInspectable& /*sender*/,
                                             const RoutedEventArgs& /*eventArgs*/)
    {
        // Return focus to the active control
        if (auto index{ _GetFocusedTabIndex() })
        {
            _GetStrongTabImpl(index.value())->SetFocused(true);
        }
    }

    bool TerminalPage::FocusMode() const
    {
        return _isInFocusMode;
    }

    bool TerminalPage::Fullscreen() const
    {
        return _isFullscreen;
    }
    // Method Description:
    // - Returns true if we're currently in "Always on top" mode. When we're in
    //   always on top mode, the window should be on top of all other windows.
    //   If multiple windows are all "always on top", they'll maintain their own
    //   z-order, with all the windows on top of all other non-topmost windows.
    // Arguments:
    // - <none>
    // Return Value:
    // - true if we should be in "always on top" mode
    bool TerminalPage::AlwaysOnTop() const
    {
        return _isAlwaysOnTop;
    }

    // Method Description:
    // - Updates all tabs with their current index in _tabs.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_UpdateTabIndices()
    {
        for (uint32_t i = 0; i < _tabs.Size(); ++i)
        {
            _GetStrongTabImpl(i)->UpdateTabViewIndex(i);
        }
    }

    // -------------------------------- WinRT Events ---------------------------------
    // Winrt events need a method for adding a callback to the event and removing the callback.
    // These macros will define them both for you.
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TerminalPage, TitleChanged, _titleChangeHandlers, winrt::Windows::Foundation::IInspectable, winrt::hstring);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TerminalPage, LastTabClosed, _lastTabClosedHandlers, winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::LastTabClosedEventArgs);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TerminalPage, SetTitleBarContent, _setTitleBarContentHandlers, winrt::Windows::Foundation::IInspectable, UIElement);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TerminalPage, FocusModeChanged, _focusModeChangedHandlers, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TerminalPage, FullscreenChanged, _fullscreenChangedHandlers, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TerminalPage, AlwaysOnTopChanged, _alwaysOnTopChangedHandlers, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
}
