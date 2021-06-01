// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// This file contains much of the code related to tab management for the
// TerminalPage. Things like opening new tabs, selecting different tabs,
// switching tabs, should all be handled in this file. Hypothetically, in the
// future, the contents of this file could be moved to a separate class
// entirely.
//

#include "pch.h"
#include "TerminalPage.h"
#include "Utils.h"
#include "../../types/inc/utils.hpp"

#include <LibraryResources.h>

#include "TabRowControl.h"
#include "ColorHelper.h"
#include "DebugTapConnection.h"
#include "SettingsTab.h"

using namespace winrt;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::System;
using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::UI::Text;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Control;
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
    // Method Description:
    // - Open a new tab. This will create the TerminalControl hosting the
    //   terminal, and add a new Tab to our list of tabs. The method can
    //   optionally be provided a NewTerminalArgs, which will be used to create
    //   a tab using the values in that object.
    // Arguments:
    // - newTerminalArgs: An object that may contain a blob of parameters to
    //   control which profile is created and with possible other
    //   configurations. See TerminalSettings::CreateWithNewTerminalArgs for more details.
    // - existingConnection: An optional connection that is already established to a PTY
    //   for this tab to host instead of creating one.
    //   If not defined, the tab will create the connection.
    void TerminalPage::_OpenNewTab(const NewTerminalArgs& newTerminalArgs, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection existingConnection)
    try
    {
        const auto profileGuid{ _settings.GetProfileForArgs(newTerminalArgs) };
        const auto settings{ TerminalSettings::CreateWithNewTerminalArgs(_settings, newTerminalArgs, *_bindings) };

        _CreateNewTabFromSettings(profileGuid, settings, existingConnection);

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
            TraceLoggingBool(settings.DefaultSettings().UseAcrylic(), "UseAcrylic", "The acrylic preference from the settings"),
            TraceLoggingFloat64(settings.DefaultSettings().TintOpacity(), "TintOpacity", "Opacity preference from the settings"),
            TraceLoggingWideString(settings.DefaultSettings().FontFace().c_str(), "FontFace", "Font face chosen in the settings"),
            TraceLoggingWideString(schemeName.data(), "SchemeName", "Color scheme set in the settings"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));
    }
    CATCH_LOG();

    // Method Description:
    // - Creates a new tab with the given settings. If the tab bar is not being
    //      currently displayed, it will be shown.
    // Arguments:
    // - profileGuid: ID to use to lookup profile settings for this connection
    // - settings: the TerminalSettings object to use to create the TerminalControl with.
    // - existingConnection: optionally receives a connection from the outside world instead of attempting to create one
    void TerminalPage::_CreateNewTabFromSettings(GUID profileGuid, const TerminalSettingsCreateResult& settings, TerminalConnection::ITerminalConnection existingConnection)
    {
        // Initialize the new tab
        // Create a connection based on the values in our settings object if we weren't given one.
        auto connection = existingConnection ? existingConnection : _CreateConnectionFromSettings(profileGuid, settings.DefaultSettings());

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

        // Give term control a child of the settings so that any overrides go in the child
        // This way, when we do a settings reload we just update the parent and the overrides remain
        auto term = _InitControl(settings, connection);

        auto newTabImpl = winrt::make_self<TerminalTab>(profileGuid, term);

        // Add the new tab to the list of our tabs.
        _tabs.Append(*newTabImpl);
        _mruTabs.Append(*newTabImpl);

        newTabImpl->SetDispatch(*_actionDispatch);
        newTabImpl->SetActionMap(_settings.ActionMap());

        // Give the tab its index in the _tabs vector so it can manage its own SwitchToTab command.
        _UpdateTabIndices();

        // Hookup our event handlers to the new terminal
        _RegisterTerminalEvents(term, *newTabImpl);

        // Don't capture a strong ref to the tab. If the tab is removed as this
        // is called, we don't really care anymore about handling the event.
        auto weakTab = make_weak(newTabImpl);

        // When the tab's active pane changes, we'll want to lookup a new icon
        // for it. The Title change will be propagated upwards through the tab's
        // PropertyChanged event handler.
        newTabImpl->ActivePaneChanged([weakTab, weakThis{ get_weak() }]() {
            auto page{ weakThis.get() };
            auto tab{ weakTab.get() };

            if (page && tab)
            {
                // Possibly update the icon of the tab.
                page->_UpdateTabIcon(*tab);

                // Update the taskbar progress as well. We'll raise our own
                // SetTaskbarProgress event here, to get tell the hosting
                // application to re-query this value from us.
                page->_SetTaskbarProgressHandlers(*page, nullptr);
            }
        });

        // The RaiseVisualBell event has been bubbled up to here from the pane,
        // the next part of the chain is bubbling up to app logic, which will
        // forward it to app host.
        newTabImpl->TabRaiseVisualBell([weakTab, weakThis{ get_weak() }]() {
            auto page{ weakThis.get() };
            auto tab{ weakTab.get() };

            if (page && tab)
            {
                page->_RaiseVisualBellHandlers(nullptr, nullptr);
            }
        });

        newTabImpl->DuplicateRequested([weakTab, weakThis{ get_weak() }]() {
            auto page{ weakThis.get() };
            auto tab{ weakTab.get() };

            if (page && tab)
            {
                page->_DuplicateTab(*tab);
            }
        });

        auto tabViewItem = newTabImpl->TabViewItem();
        _tabView.TabItems().Append(tabViewItem);

        // Set this tab's icon to the icon from the user's profile
        const auto profile = _settings.FindProfile(profileGuid);
        if (profile != nullptr && !profile.Icon().empty())
        {
            newTabImpl->UpdateIcon(profile.Icon());
        }

        tabViewItem.PointerReleased({ this, &TerminalPage::_OnTabClick });

        // When the tab requests close, try to close it (prompt for approval, if required)
        newTabImpl->CloseRequested([weakTab, weakThis{ get_weak() }](auto&& /*s*/, auto&& /*e*/) {
            auto page{ weakThis.get() };
            auto tab{ weakTab.get() };

            if (page && tab)
            {
                page->_HandleCloseTabRequested(*tab);
            }
        });

        // When the tab is closed, remove it from our list of tabs.
        newTabImpl->Closed([tabViewItem, weakThis{ get_weak() }](auto&& /*s*/, auto&& /*e*/) {
            if (auto page{ weakThis.get() })
            {
                page->_RemoveOnCloseRoutine(tabViewItem, page);
            }
        });

        // The tab might want us to toss focus into the control, especially when
        // transient UIs (like the context menu, or the renamer) are dismissed.
        newTabImpl->RequestFocusActiveControl([weakThis{ get_weak() }]() {
            if (const auto page{ weakThis.get() })
            {
                page->_FocusCurrentTab(false);
            }
        });

        if (debugConnection) // this will only be set if global debugging is on and tap is active
        {
            auto newControl = _InitControl(settings, debugConnection);
            _RegisterTerminalEvents(newControl, *newTabImpl);
            // Split (auto) with the debug tap.
            newTabImpl->SplitPane(SplitState::Automatic, 0.5f, profileGuid, newControl);
        }

        // This kicks off TabView::SelectionChanged, in response to which
        // we'll attach the terminal's Xaml control to the Xaml root.
        _tabView.SelectedItem(tabViewItem);
    }

    // Method Description:
    // - Get the icon of the currently focused terminal control, and set its
    //   tab's icon to that icon.
    // Arguments:
    // - tab: the Tab to update the title for.
    void TerminalPage::_UpdateTabIcon(TerminalTab& tab)
    {
        const auto lastFocusedProfileOpt = tab.GetFocusedProfile();
        if (lastFocusedProfileOpt.has_value())
        {
            const auto lastFocusedProfile = lastFocusedProfileOpt.value();
            const auto matchingProfile = _settings.FindProfile(lastFocusedProfile);
            if (matchingProfile)
            {
                tab.UpdateIcon(matchingProfile.Icon());
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

        if (_tabView)
        {
            // collapse/show the tabs themselves
            _tabView.Visibility(isVisible ? Visibility::Visible : Visibility::Collapsed);
        }
        if (_tabRow)
        {
            // collapse/show the row that the tabs are in.
            // NaN is the special value XAML uses for "Auto" sizing.
            _tabRow.Height(isVisible ? NAN : 0);
        }
    }

    // Method Description:
    // - Duplicates the current focused tab
    void TerminalPage::_DuplicateFocusedTab()
    {
        if (const auto terminalTab{ _GetFocusedTabImpl() })
        {
            _DuplicateTab(*terminalTab);
        }
    }

    // Method Description:
    // - Duplicates specified tab
    // Arguments:
    // - tab: tab to duplicate
    void TerminalPage::_DuplicateTab(const TerminalTab& tab)
    {
        try
        {
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

            const auto& profileGuid = tab.GetFocusedProfile();
            if (profileGuid.has_value())
            {
                const auto settingsCreateResult{ TerminalSettings::CreateWithProfileByID(_settings, profileGuid.value(), *_bindings) };
                const auto workingDirectory = tab.GetActiveTerminalControl().WorkingDirectory();
                const auto validWorkingDirectory = !workingDirectory.empty();
                if (validWorkingDirectory)
                {
                    settingsCreateResult.DefaultSettings().StartingDirectory(workingDirectory);
                }

                _CreateNewTabFromSettings(profileGuid.value(), settingsCreateResult);

                const auto runtimeTabText{ tab.GetTabText() };
                if (!runtimeTabText.empty())
                {
                    if (auto newTab{ _GetFocusedTabImpl() })
                    {
                        newTab->SetTabText(runtimeTabText);
                    }
                }
            }
        }
        CATCH_LOG();
    }

    // Method Description:
    // - Removes the tab (both TerminalControl and XAML) after prompting for approval
    // Arguments:
    // - tab: the tab to remove
    winrt::Windows::Foundation::IAsyncAction TerminalPage::_HandleCloseTabRequested(winrt::TerminalApp::TabBase tab)
    {
        if (tab.ReadOnly())
        {
            ContentDialogResult warningResult = co_await _ShowCloseReadOnlyDialog();

            // If the user didn't explicitly click on close tab - leave
            if (warningResult != ContentDialogResult::Primary)
            {
                co_return;
            }
        }
        _RemoveTab(tab);
    }

    // Method Description:
    // - Removes the tab (both TerminalControl and XAML)
    // Arguments:
    // - tab: the tab to remove
    void TerminalPage::_RemoveTab(const winrt::TerminalApp::TabBase& tab)
    {
        uint32_t tabIndex{};
        if (!_tabs.IndexOf(tab, tabIndex))
        {
            // The tab is already removed
            return;
        }

        // We use _removing flag to suppress _OnTabSelectionChanged events
        // that might get triggered while removing
        _removing = true;
        auto unsetRemoving = wil::scope_exit([&]() noexcept { _removing = false; });

        const auto focusedTabIndex{ _GetFocusedTabIndex() };

        // Removing the tab from the collection should destroy its control and disconnect its connection,
        // but it doesn't always do so. The UI tree may still be holding the control and preventing its destruction.
        tab.Shutdown();

        uint32_t mruIndex{};
        if (_mruTabs.IndexOf(tab, mruIndex))
        {
            _mruTabs.RemoveAt(mruIndex);
        }

        _tabs.RemoveAt(tabIndex);
        _tabView.TabItems().RemoveAt(tabIndex);
        _UpdateTabIndices();

        // To close the window here, we need to close the hosting window.
        if (_tabs.Size() == 0)
        {
            _LastTabClosedHandlers(*this, nullptr);
        }
        else if (focusedTabIndex.has_value() && focusedTabIndex.value() == gsl::narrow_cast<uint32_t>(tabIndex))
        {
            // Manually select the new tab to get focus, rather than relying on TabView since:
            // 1. We want to customize this behavior (e.g., use MRU logic)
            // 2. In fullscreen (GH#5799) and focus (GH#7916) modes the _OnTabItemsChanged is not fired
            // 3. When rearranging tabs (GH#7916) _OnTabItemsChanged is suppressed
            const auto tabSwitchMode = _settings.GlobalSettings().TabSwitcherMode();

            if (tabSwitchMode == TabSwitcherMode::MostRecentlyUsed)
            {
                const auto newSelectedTab = _mruTabs.GetAt(0);
                _UpdatedSelectedTab(newSelectedTab);
                _tabView.SelectedItem(newSelectedTab.TabViewItem());
            }
            else
            {
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
                auto newSelectedTab{ _tabs.GetAt(newSelectedIndex) };
                _UpdatedSelectedTab(newSelectedTab);

                // Also, we need to _manually_ set the SelectedItem of the tabView
                // here. If we don't, then the TabView will technically not have a
                // selected item at all, which can make things like ClosePane not
                // work correctly.
                _tabView.SelectedItem(newSelectedTab.TabViewItem());
            }
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
    // - Sets focus to the tab to the right or left the currently selected tab.
    void TerminalPage::_SelectNextTab(const bool bMoveRight, const Windows::Foundation::IReference<Microsoft::Terminal::Settings::Model::TabSwitcherMode>& customTabSwitcherMode)
    {
        const auto index{ _GetFocusedTabIndex().value_or(0) };
        const auto tabSwitchMode = customTabSwitcherMode ? customTabSwitcherMode.Value() : _settings.GlobalSettings().TabSwitcherMode();
        if (tabSwitchMode == TabSwitcherMode::Disabled)
        {
            uint32_t tabCount = _tabs.Size();
            // Wraparound math. By adding tabCount and then calculating
            // modulo tabCount, we clamp the values to the range [0,
            // tabCount) while still supporting moving leftward from 0 to
            // tabCount - 1.
            const auto newTabIndex = ((tabCount + index + (bMoveRight ? 1 : -1)) % tabCount);
            _SelectTab(newTabIndex);
        }
        else
        {
            CommandPalette().SetTabs(_tabs, _mruTabs);

            // Otherwise, set up the tab switcher in the selected mode, with
            // the given ordering, and make it visible.
            CommandPalette().EnableTabSwitcherMode(index, tabSwitchMode);
            CommandPalette().Visibility(Visibility::Visible);
            CommandPalette().SelectNextItem(bMoveRight);
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
            auto tab{ _tabs.GetAt(tabIndex) };
            if (_startupState == StartupState::InStartup)
            {
                _tabView.SelectedItem(tab.TabViewItem());
                _UpdatedSelectedTab(tab);
            }
            else
            {
                _SetFocusedTab(tab);
            }

            return true;
        }
        return false;
    }

    // Method Description:
    // - This method is called once a tab was selected in tab switcher
    //   We'll use this event to select the relevant tab
    // Arguments:
    // - tab - tab to select
    // Return Value:
    // - <none>
    void TerminalPage::_OnSwitchToTabRequested(const IInspectable& /*sender*/, const winrt::TerminalApp::TabBase& tab)
    {
        uint32_t index{};
        if (_tabs.IndexOf(tab, index))
        {
            _SelectTab(index);
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
    // - returns the currently focused tab. This might return null,
    //   so make sure to check the result!
    winrt::TerminalApp::TabBase TerminalPage::_GetFocusedTab() const noexcept
    {
        if (auto index{ _GetFocusedTabIndex() })
        {
            return _tabs.GetAt(*index);
        }
        return nullptr;
    }

    // Method Description:
    // - returns a com_ptr to the currently focused tab implementation. This might return null,
    //   so make sure to check the result!
    winrt::com_ptr<TerminalTab> TerminalPage::_GetFocusedTabImpl() const noexcept
    {
        if (auto tab{ _GetFocusedTab() })
        {
            return _GetTerminalTabImpl(tab);
        }
        return nullptr;
    }

    // Method Description:
    // - returns a tab corresponding to a view item. This might return null,
    //   so make sure to check the result!
    winrt::TerminalApp::TabBase TerminalPage::_GetTabByTabViewItem(const Microsoft::UI::Xaml::Controls::TabViewItem& tabViewItem) const noexcept
    {
        uint32_t tabIndexFromControl{};
        if (_tabView.TabItems().IndexOf(tabViewItem, tabIndexFromControl))
        {
            // If IndexOf returns true, we've actually got an index
            return _tabs.GetAt(tabIndexFromControl);
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
    // - tab: tab to focus.
    // Return Value:
    // - <none>
    winrt::fire_and_forget TerminalPage::_SetFocusedTab(const winrt::TerminalApp::TabBase tab)
    {
        // GH#1117: This is a workaround because _tabView.SelectedIndex(tabIndex)
        //          sometimes set focus to an incorrect tab after removing some tabs
        auto weakThis{ get_weak() };

        co_await winrt::resume_foreground(_tabView.Dispatcher());

        if (auto page{ weakThis.get() })
        {
            // Make sure the tab was not removed
            uint32_t tabIndex{};
            if (_tabs.IndexOf(tab, tabIndex))
            {
                _tabView.SelectedItem(tab.TabViewItem());
            }
        }
    }

    // Method Description:
    // - Close the currently focused tab. Focus will move to the left, if possible.
    void TerminalPage::_CloseFocusedTab()
    {
        if (auto index{ _GetFocusedTabIndex() })
        {
            auto tab{ _tabs.GetAt(*index) };
            _HandleCloseTabRequested(tab);
        }
    }

    // Method Description:
    // - Close the currently focused pane. If the pane is the last pane in the
    //   tab, the tab will also be closed. This will happen when we handle the
    //   tab's Closed event.
    winrt::fire_and_forget TerminalPage::_CloseFocusedPane()
    {
        if (const auto terminalTab{ _GetFocusedTabImpl() })
        {
            _UnZoomIfNeeded();

            auto pane = terminalTab->GetActivePane();

            if (const auto pane{ terminalTab->GetActivePane() })
            {
                if (const auto control{ pane->GetTerminalControl() })
                {
                    if (control.ReadOnly())
                    {
                        ContentDialogResult warningResult = co_await _ShowCloseReadOnlyDialog();

                        // If the user didn't explicitly click on close tab - leave
                        if (warningResult != ContentDialogResult::Primary)
                        {
                            co_return;
                        }

                        // Clean read-only mode to prevent additional prompt if closing the pane triggers closing of a hosting tab
                        if (control.ReadOnly())
                        {
                            control.ToggleReadOnly();
                        }
                    }

                    pane->Close();
                }
            }
        }
        else if (auto index{ _GetFocusedTabIndex() })
        {
            const auto tab{ _tabs.GetAt(*index) };
            if (tab.try_as<TerminalApp::SettingsTab>())
            {
                _HandleCloseTabRequested(tab);
            }
        }
    }

    // Method Description:
    // - Closes provided tabs one by one
    // Arguments:
    // - tabs - tabs to remove
    winrt::fire_and_forget TerminalPage::_RemoveTabs(const std::vector<winrt::TerminalApp::TabBase> tabs)
    {
        for (auto& tab : tabs)
        {
            co_await _HandleCloseTabRequested(tab);
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

        CommandPalette().Visibility(Visibility::Collapsed);
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
            const auto tabViewItem = sender.try_as<MUX::Controls::TabViewItem>();
            if (auto tab{ _GetTabByTabViewItem(tabViewItem) })
            {
                _HandleCloseTabRequested(tab);
            }
            eventArgs.Handled(true);
        }
        else if (eventArgs.GetCurrentPoint(*this).Properties().IsRightButtonPressed())
        {
            eventArgs.Handled(true);
        }
    }

    void TerminalPage::_UpdatedSelectedTab(const winrt::TerminalApp::TabBase& tab)
    {
        // Unfocus all the tabs.
        for (auto tab : _tabs)
        {
            tab.Focus(FocusState::Unfocused);
        }

        try
        {
            _tabContent.Children().Clear();
            _tabContent.Children().Append(tab.Content());

            // GH#7409: If the tab switcher is open, then we _don't_ want to
            // automatically focus the new tab here. The tab switcher wants
            // to be able to "preview" the selected tab as the user tabs
            // through the menu, but if we toss the focus to the control
            // here, then the user won't be able to navigate the ATS any
            // longer.
            //
            // When the tab switcher is eventually dismissed, the focus will
            // get tossed back to the focused terminal control, so we don't
            // need to worry about focus getting lost.
            if (CommandPalette().Visibility() != Visibility::Visible)
            {
                tab.Focus(FocusState::Programmatic);
                _UpdateMRUTab(tab);
            }

            tab.TabViewItem().StartBringIntoView();

            // Raise an event that our title changed
            if (_settings.GlobalSettings().ShowTitleInTitlebar())
            {
                _TitleChangedHandlers(*this, tab.Title());
            }
        }
        CATCH_LOG();
    }

    // Method Description:
    // - Responds to the TabView control's Selection Changed event (to move a
    //      new terminal control into focus) when not in in the middle of a tab rearrangement.
    // Arguments:
    // - sender: the control that originated this event
    // - eventArgs: the event's constituent arguments
    void TerminalPage::_OnTabSelectionChanged(const IInspectable& sender, const WUX::Controls::SelectionChangedEventArgs& /*eventArgs*/)
    {
        if (!_rearranging && !_removing)
        {
            auto tabView = sender.as<MUX::Controls::TabView>();
            auto selectedIndex = tabView.SelectedIndex();
            if (selectedIndex >= 0 && selectedIndex < gsl::narrow_cast<int32_t>(_tabs.Size()))
            {
                const auto tab{ _tabs.GetAt(selectedIndex) };
                _UpdatedSelectedTab(tab);
            }
        }
    }

    // Method Description:
    // - Updates all tabs with their current index in _tabs.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_UpdateTabIndices()
    {
        const uint32_t size = _tabs.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            auto tab{ _tabs.GetAt(i) };
            auto tabImpl{ winrt::get_self<TabBase>(tab) };
            tabImpl->UpdateTabViewIndex(i, size);
        }
    }

    // Method Description:
    // - Bumps the tab in its in-order index up to the top of the mru list.
    // Arguments:
    // - tab: tab to bump.
    // Return Value:
    // - <none>
    void TerminalPage::_UpdateMRUTab(const winrt::TerminalApp::TabBase& tab)
    {
        uint32_t mruIndex;
        if (_mruTabs.IndexOf(tab, mruIndex))
        {
            if (mruIndex > 0)
            {
                _mruTabs.RemoveAt(mruIndex);
                _mruTabs.InsertAt(0, tab);
            }
        }
    }

    // Method Description:
    // - Moves the tab to another index in the tabs row (if required).
    // Arguments:
    // - currentTabIndex: the current index of the tab to move
    // - suggestedNewTabIndex: the new index of the tab, might get clamped to fit int the tabs row boundaries
    // Return Value:
    // - <none>
    void TerminalPage::_TryMoveTab(const uint32_t currentTabIndex, const int32_t suggestedNewTabIndex)
    {
        auto newTabIndex = gsl::narrow_cast<uint32_t>(std::clamp<int32_t>(suggestedNewTabIndex, 0, _tabs.Size() - 1));
        if (currentTabIndex != newTabIndex)
        {
            auto tab = _tabs.GetAt(currentTabIndex);
            auto tabViewItem = tab.TabViewItem();
            _tabs.RemoveAt(currentTabIndex);
            _tabs.InsertAt(newTabIndex, tab);
            _UpdateTabIndices();

            _tabView.TabItems().RemoveAt(currentTabIndex);
            _tabView.TabItems().InsertAt(newTabIndex, tabViewItem);
            _tabView.SelectedItem(tabViewItem);
        }
    }

    void TerminalPage::_TabDragStarted(const IInspectable& /*sender*/,
                                       const IInspectable& /*eventArgs*/)
    {
        _rearranging = true;
        _rearrangeFrom = std::nullopt;
        _rearrangeTo = std::nullopt;
    }

    void TerminalPage::_TabDragCompleted(const IInspectable& /*sender*/,
                                         const IInspectable& /*eventArgs*/)
    {
        auto& from{ _rearrangeFrom };
        auto& to{ _rearrangeTo };

        if (from.has_value() && to.has_value() && to != from)
        {
            auto& tabs{ _tabs };
            auto tab = tabs.GetAt(from.value());
            tabs.RemoveAt(from.value());
            tabs.InsertAt(to.value(), tab);
            _UpdateTabIndices();
        }

        _rearranging = false;
        from = std::nullopt;
        to = std::nullopt;
    }

    void TerminalPage::_DismissTabContextMenus()
    {
        for (const auto& tab : _tabs)
        {
            if (tab.TabViewItem().ContextFlyout())
            {
                tab.TabViewItem().ContextFlyout().Hide();
            }
        }
    }

    void TerminalPage::_FocusCurrentTab(const bool focusAlways)
    {
        // We don't want to set focus on the tab if fly-out is open as it will
        // be closed TODO GH#5400: consider checking we are not in the opening
        // state, by hooking both Opening and Open events
        if (focusAlways || !_newTabButton.Flyout().IsOpen())
        {
            // Return focus to the active control
            if (auto tab{ _GetFocusedTab() })
            {
                tab.Focus(FocusState::Programmatic);
                _UpdateMRUTab(tab);
            }
        }
    }

    bool TerminalPage::_HasMultipleTabs() const
    {
        return _tabs.Size() > 1;
    }

    void TerminalPage::_RemoveAllTabs()
    {
        // Since _RemoveTabs is asynchronous, create a snapshot of the  tabs we want to remove
        std::vector<winrt::TerminalApp::TabBase> tabsToRemove;
        std::copy(begin(_tabs), end(_tabs), std::back_inserter(tabsToRemove));
        _RemoveTabs(tabsToRemove);
    }

    void TerminalPage::_ResizeTabContent(const winrt::Windows::Foundation::Size& newSize)
    {
        for (auto tab : _tabs)
        {
            if (auto terminalTab = _GetTerminalTabImpl(tab))
            {
                terminalTab->ResizeContent(newSize);
            }
        }
    }
}
