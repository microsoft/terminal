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
#include "../../inc/til/string.h"

#include <LibraryResources.h>

#include "TabRowControl.h"
#include "ColorHelper.h"
#include "DebugTapConnection.h"
#include "SettingsTab.h"
#include "..\TerminalSettingsModel\FileUtils.h"

#include <shlobj.h>

using namespace winrt;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::System;
using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Pickers;
using namespace winrt::Windows::Storage::Provider;
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
    HRESULT TerminalPage::_OpenNewTab(const NewTerminalArgs& newTerminalArgs, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection existingConnection)
    try
    {
        const auto profile{ _settings.GetProfileForArgs(newTerminalArgs) };
        // GH#11114: GetProfileForArgs can return null if the index is higher
        // than the number of available profiles.
        if (!profile)
        {
            return S_FALSE;
        }
        const auto settings{ TerminalSettings::CreateWithNewTerminalArgs(_settings, newTerminalArgs, *_bindings) };

        // Try to handle auto-elevation
        if (_maybeElevate(newTerminalArgs, settings, profile))
        {
            return S_OK;
        }
        // We can't go in the other direction (elevated->unelevated)
        // unfortunately. This seems to be due to Centennial quirks. It works
        // unpackaged, but not packaged.
        //
        // This call to _MakePane won't return nullptr, we already checked that
        // case above with the _maybeElevate call.
        _CreateNewTabFromPane(_MakePane(newTerminalArgs, nullptr, existingConnection));
        return S_OK;
    }
    CATCH_RETURN();

    // Method Description:
    // - Sets up state, event handlers, etc on a tab object that was just made.
    // Arguments:
    // - newTabImpl: the uninitialized tab.
    // - insertPosition: Optional parameter to indicate the position of tab.
    void TerminalPage::_InitializeTab(winrt::com_ptr<TerminalTab> newTabImpl, uint32_t insertPosition)
    {
        newTabImpl->Initialize();

        // If insert position is not passed, calculate it
        if (insertPosition == -1)
        {
            insertPosition = _tabs.Size();
            if (_settings.GlobalSettings().NewTabPosition() == NewTabPosition::AfterCurrentTab)
            {
                auto currentTabIndex = _GetFocusedTabIndex();
                if (currentTabIndex.has_value())
                {
                    insertPosition = currentTabIndex.value() + 1;
                }
            }
        }

        // Add the new tab to the list of our tabs.
        _tabs.InsertAt(insertPosition, *newTabImpl);
        _mruTabs.Append(*newTabImpl);

        newTabImpl->SetDispatch(*_actionDispatch);
        newTabImpl->SetActionMap(_settings.ActionMap());

        // Give the tab its index in the _tabs vector so it can manage its own SwitchToTab command.
        _UpdateTabIndices();

        // Hookup our event handlers to the new terminal
        _RegisterTabEvents(*newTabImpl);

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

                page->_updateThemeColors();

                // Update the taskbar progress as well. We'll raise our own
                // SetTaskbarProgress event here, to get tell the hosting
                // application to re-query this value from us.
                page->_SetTaskbarProgressHandlers(*page, nullptr);

                auto profile = tab->GetFocusedProfile();
                page->_UpdateBackground(profile);
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

        newTabImpl->SplitTabRequested([weakTab, weakThis{ get_weak() }]() {
            auto page{ weakThis.get() };
            auto tab{ weakTab.get() };

            if (page && tab)
            {
                page->_SplitTab(*tab);
            }
        });

        newTabImpl->MoveTabToNewWindowRequested([weakTab, weakThis{ get_weak() }]() {
            auto page{ weakThis.get() };
            auto tab{ weakTab.get() };

            if (page && tab)
            {
                MoveTabArgs args{ hstring{ L"new" }, MoveTabDirection::Forward };
                page->_SetFocusedTab(*tab);
                page->_MoveTab(args);
            }
        });

        newTabImpl->ExportTabRequested([weakTab, weakThis{ get_weak() }]() {
            auto page{ weakThis.get() };
            auto tab{ weakTab.get() };

            if (page && tab)
            {
                // Passing empty string as the path to export tab will make it
                // prompt for the path
                page->_ExportTab(*tab, L"");
            }
        });

        newTabImpl->FindRequested([weakTab, weakThis{ get_weak() }]() {
            auto page{ weakThis.get() };
            auto tab{ weakTab.get() };

            if (page && tab)
            {
                page->_SetFocusedTab(*tab);
                page->_Find(*tab);
            }
        });

        auto tabViewItem = newTabImpl->TabViewItem();
        _tabView.TabItems().InsertAt(insertPosition, tabViewItem);

        // Set this tab's icon to the icon from the user's profile
        if (const auto profile{ newTabImpl->GetFocusedProfile() })
        {
            if (!profile.Icon().empty())
            {
                newTabImpl->UpdateIcon(profile.Icon());
            }
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

        // This kicks off TabView::SelectionChanged, in response to which
        // we'll attach the terminal's Xaml control to the Xaml root.
        _tabView.SelectedItem(tabViewItem);
    }

    // Method Description:
    // - Create a new tab using a specified pane as the root.
    // Arguments:
    // - pane: The pane to use as the root.
    // - insertPosition: Optional parameter to indicate the position of tab.
    void TerminalPage::_CreateNewTabFromPane(std::shared_ptr<Pane> pane, uint32_t insertPosition)
    {
        if (pane)
        {
            auto newTabImpl = winrt::make_self<TerminalTab>(pane);
            _InitializeTab(newTabImpl, insertPosition);
        }
    }

    // Method Description:
    // - Get the icon of the currently focused terminal control, and set its
    //   tab's icon to that icon.
    // Arguments:
    // - tab: the Tab to update the title for.
    void TerminalPage::_UpdateTabIcon(TerminalTab& tab)
    {
        if (const auto profile = tab.GetFocusedProfile())
        {
            tab.UpdateIcon(profile.Icon());
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
        const auto isVisible = (!_isFullscreen && !_isInFocusMode) &&
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
            // TODO: GH#5047 - We're duplicating the whole profile, which might
            // be a dangling reference to old settings.
            //
            // In the future, it may be preferable to just duplicate the
            // current control's live settings (which will include changes
            // made through VT).
            _CreateNewTabFromPane(_MakePane(nullptr, tab, nullptr), tab.TabViewIndex() + 1);

            const auto runtimeTabText{ tab.GetTabText() };
            if (!runtimeTabText.empty())
            {
                if (auto newTab{ _GetFocusedTabImpl() })
                {
                    newTab->SetTabText(runtimeTabText);
                }
            }
        }
        CATCH_LOG();
    }

    // Method Description:
    // - Sets the specified tab as the focused tab and splits its active pane
    // Arguments:
    // - tab: tab to split
    void TerminalPage::_SplitTab(TerminalTab& tab)
    {
        try
        {
            _SetFocusedTab(tab);
            _SplitPane(tab, SplitDirection::Automatic, 0.5f, _MakePane(nullptr, tab));
        }
        CATCH_LOG();
    }

    // Method Description:
    // - Exports the content of the Terminal Buffer inside the tab
    // Arguments:
    // - tab: tab to export
    winrt::fire_and_forget TerminalPage::_ExportTab(const TerminalTab& tab, winrt::hstring filepath)
    {
        // This will be used to set up the file picker "filter", to select .txt
        // files by default.
        static constexpr COMDLG_FILTERSPEC supportedFileTypes[] = {
            { L"Text Files (*.txt)", L"*.txt" },
            { L"All Files (*.*)", L"*.*" }
        };
        // An arbitrary GUID to associate with all instances of this
        // dialog, so they all re-open in the same path as they were
        // open before:
        static constexpr winrt::guid clientGuidExportFile{ 0xF6AF20BB, 0x0800, 0x48E6, { 0xB0, 0x17, 0xA1, 0x4C, 0xD8, 0x73, 0xDD, 0x58 } };

        try
        {
            if (const auto control{ tab.GetActiveTerminalControl() })
            {
                auto path = filepath;

                if (path.empty())
                {
                    // GH#11356 - we can't use the UWP apis for writing the file,
                    // because they don't work elevated (shocker) So just use the
                    // shell32 file picker manually.
                    std::wstring filename{ tab.Title() };
                    filename = til::clean_filename(filename);
                    path = co_await SaveFilePicker(*_hostingHwnd, [filename = std::move(filename)](auto&& dialog) {
                        THROW_IF_FAILED(dialog->SetClientGuid(clientGuidExportFile));
                        try
                        {
                            // Default to the Downloads folder
                            auto folderShellItem{ winrt::capture<IShellItem>(&SHGetKnownFolderItem, FOLDERID_Downloads, KF_FLAG_DEFAULT, nullptr) };
                            dialog->SetDefaultFolder(folderShellItem.get());
                        }
                        CATCH_LOG(); // non-fatal
                        THROW_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(supportedFileTypes), supportedFileTypes));
                        THROW_IF_FAILED(dialog->SetFileTypeIndex(1)); // the array is 1-indexed
                        THROW_IF_FAILED(dialog->SetDefaultExtension(L"txt"));

                        // Default to using the tab title as the file name
                        THROW_IF_FAILED(dialog->SetFileName((filename + L".txt").c_str()));
                    });
                }
                else
                {
                    // The file picker isn't going to give us paths with
                    // environment variables, but the user might have set one in
                    // the settings. Expand those here.

                    path = winrt::hstring{ wil::ExpandEnvironmentStringsW<std::wstring>(path.c_str()) };
                }

                if (!path.empty())
                {
                    const auto buffer = control.ReadEntireBuffer();
                    CascadiaSettings::ExportFile(path, buffer);
                }
            }
        }
        CATCH_LOG();
    }

    // Method Description:
    // - Record the configuration information of the last closed thing .
    // - Will occasionally prune the list so it doesn't grow infinitely.
    // Arguments:
    // - args: the list of actions to take to remake the pane/tab
    void TerminalPage::_AddPreviouslyClosedPaneOrTab(std::vector<ActionAndArgs>&& args)
    {
        // Just make sure we don't get infinitely large, but still
        // maintain a large replay buffer.
        if (const auto size = _previouslyClosedPanesAndTabs.size(); size > 150)
        {
            const auto it = _previouslyClosedPanesAndTabs.begin();
            // delete 50 at a time so that we don't have to do an erase
            // of the buffer every time when at capacity.
            _previouslyClosedPanesAndTabs.erase(it, it + (size - 100));
        }

        _previouslyClosedPanesAndTabs.emplace_back(args);
    }

    // Method Description:
    // - Removes the tab (both TerminalControl and XAML) after prompting for approval
    // Arguments:
    // - tab: the tab to remove
    winrt::Windows::Foundation::IAsyncAction TerminalPage::_HandleCloseTabRequested(winrt::TerminalApp::TabBase tab)
    {
        if (tab.ReadOnly())
        {
            auto warningResult = co_await _ShowCloseReadOnlyDialog();

            // If the user didn't explicitly click on close tab - leave
            if (warningResult != ContentDialogResult::Primary)
            {
                co_return;
            }
        }

        auto t = winrt::get_self<implementation::TabBase>(tab);
        auto actions = t->BuildStartupActions();
        _AddPreviouslyClosedPaneOrTab(std::move(actions));

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

        if (_stashed.draggedTab && *_stashed.draggedTab == tab)
        {
            _stashed.draggedTab = nullptr;
        }

        _tabs.RemoveAt(tabIndex);
        _tabView.TabItems().RemoveAt(tabIndex);
        _UpdateTabIndices();

        // To close the window here, we need to close the hosting window.
        if (_tabs.Size() == 0)
        {
            // If we are supposed to save state, make sure we clear it out
            // if the user manually closed all tabs.
            // Do this only if we are the last window; the monarch will notice
            // we are missing and remove us that way otherwise.
            _LastTabClosedHandlers(*this, winrt::make<LastTabClosedEventArgs>(!_maintainStateOnTabClose));
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
            auto tabCount = _tabs.Size();
            // Wraparound math. By adding tabCount and then calculating
            // modulo tabCount, we clamp the values to the range [0,
            // tabCount) while still supporting moving leftward from 0 to
            // tabCount - 1.
            const auto newTabIndex = ((tabCount + index + (bMoveRight ? 1 : -1)) % tabCount);
            _SelectTab(newTabIndex);
        }
        else
        {
            const auto p = LoadCommandPalette();
            p.SetTabs(_tabs, _mruTabs);

            // Otherwise, set up the tab switcher in the selected mode, with
            // the given ordering, and make it visible.
            p.EnableTabSwitcherMode(index, tabSwitchMode);
            p.Visibility(Visibility::Visible);
            p.SelectNextItem(bMoveRight);
        }
    }

    // Method Description:
    // - Sets focus to the desired tab. Returns false if the provided tabIndex
    //   is greater than the number of tabs we have.
    // - During startup, we'll immediately set the selected tab as focused.
    // - After startup, we'll dispatch an async method to set the selected
    //   item of the TabView, which will then also trigger a
    //   TabView::SelectionChanged, handled in
    //   TerminalPage::_OnTabSelectionChanged
    // Return Value:
    // true iff we were able to select that tab index, false otherwise
    bool TerminalPage::_SelectTab(uint32_t tabIndex)
    {
        // GH#9369 - if the argument is out of range, then clamp to the number
        // of available tabs. Previously, we'd just silently do nothing if the
        // value was greater than the number of tabs.
        tabIndex = std::clamp(tabIndex, 0u, _tabs.Size() - 1);

        auto tab{ _tabs.GetAt(tabIndex) };
        // GH#11107 - Always just set the item directly first so that if
        // tab movement is done as part of multiple actions following calls
        // to _GetFocusedTab will return the correct tab.
        _tabView.SelectedItem(tab.TabViewItem());

        if (_startupState == StartupState::InStartup)
        {
            _UpdatedSelectedTab(tab);
        }
        else
        {
            _SetFocusedTab(tab);
        }

        return true;
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
        const auto items{ _tabView.TabItems() };
        if (items.IndexOf(tabViewItem, tabIndexFromControl))
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

        co_await wil::resume_foreground(_tabView.Dispatcher());

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
    // - Disables read-only mode on pane if the user wishes to close it and read-only mode is enabled.
    // Arguments:
    // - pane: the pane that is about to be closed.
    // Return Value:
    // - bool indicating whether the (read-only) pane can be closed.
    winrt::Windows::Foundation::IAsyncOperation<bool> TerminalPage::_PaneConfirmCloseReadOnly(std::shared_ptr<Pane> pane)
    {
        if (pane->ContainsReadOnly())
        {
            auto warningResult = co_await _ShowCloseReadOnlyDialog();

            // If the user didn't explicitly click on close tab - leave
            if (warningResult != ContentDialogResult::Primary)
            {
                co_return false;
            }

            // Clean read-only mode to prevent additional prompt if closing the pane triggers closing of a hosting tab
            pane->WalkTree([](auto p) {
                if (const auto control{ p->GetTerminalControl() })
                {
                    if (control.ReadOnly())
                    {
                        control.ToggleReadOnly();
                    }
                }
            });
        }
        co_return true;
    }

    // Method Description:
    // - Removes the pane from the tab it belongs to.
    // Arguments:
    // - pane: the pane to close.
    void TerminalPage::_HandleClosePaneRequested(std::shared_ptr<Pane> pane)
    {
        // Build the list of actions to recreate the closed pane,
        // BuildStartupActions returns the "first" pane and the rest of
        // its actions are assuming that first pane has been created first.
        // This doesn't handle refocusing anything in particular, the
        // result will be that the last pane created is focused. In the
        // case of a single pane that is the desired behavior anyways.
        auto state = pane->BuildStartupActions(0, 1);
        {
            ActionAndArgs splitPaneAction{};
            splitPaneAction.Action(ShortcutAction::SplitPane);
            SplitPaneArgs splitPaneArgs{ SplitDirection::Automatic, state.firstPane->GetTerminalArgsForPane() };
            splitPaneAction.Args(splitPaneArgs);

            state.args.emplace(state.args.begin(), std::move(splitPaneAction));
        }
        _AddPreviouslyClosedPaneOrTab(std::move(state.args));

        // If specified, detach before closing to directly update the pane structure
        pane->Close();
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

            if (const auto pane{ terminalTab->GetActivePane() })
            {
                if (co_await _PaneConfirmCloseReadOnly(pane))
                {
                    _HandleClosePaneRequested(pane);
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
    // - Close all panes with the given IDs sequentially.
    // Arguments:
    // - weakTab: weak reference to the tab that the pane belongs to.
    // - paneIds: collection of the IDs of the panes that are marked for removal.
    void TerminalPage::_ClosePanes(weak_ref<TerminalTab> weakTab, std::vector<uint32_t> paneIds)
    {
        if (auto strongTab{ weakTab.get() })
        {
            // Close all unfocused panes one by one
            while (!paneIds.empty())
            {
                const auto id = paneIds.back();
                paneIds.pop_back();

                if (const auto pane{ strongTab->GetRootPane()->FindPane(id) })
                {
                    pane->ClosedByParent([ids{ std::move(paneIds) }, weakThis{ get_weak() }, weakTab]() {
                        if (auto strongThis{ weakThis.get() })
                        {
                            strongThis->_ClosePanes(weakTab, std::move(ids));
                        }
                    });

                    // Close the pane which will eventually trigger the closed by parent event
                    _HandleClosePaneRequested(pane);
                    break;
                }
            }
        }
    }

    // Method Description:
    // - Close the tab at the given index.
    void TerminalPage::_CloseTabAtIndex(uint32_t index)
    {
        if (index >= _tabs.Size())
        {
            return;
        }
        if (auto tab{ _tabs.GetAt(index) })
        {
            _HandleCloseTabRequested(tab);
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
        for (const auto& tab : _tabs)
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
            const auto p = CommandPaletteElement();
            if (!p || p.Visibility() != Visibility::Visible)
            {
                tab.Focus(FocusState::Programmatic);
                _UpdateMRUTab(tab);
                _updateAllTabCloseButtons(tab);
            }

            tab.TabViewItem().StartBringIntoView();

            // Raise an event that our title changed
            if (_settings.GlobalSettings().ShowTitleInTitlebar())
            {
                _TitleChangedHandlers(*this, tab.Title());
            }

            _updateThemeColors();

            auto tab_impl = _GetTerminalTabImpl(tab);
            if (tab_impl)
            {
                auto profile = tab_impl->GetFocusedProfile();
                _UpdateBackground(profile);
            }
        }
        CATCH_LOG();
    }

    void TerminalPage::_UpdateBackground(const winrt::Microsoft::Terminal::Settings::Model::Profile& profile)
    {
        if (profile && _settings.GlobalSettings().UseBackgroundImageForWindow())
        {
            _SetBackgroundImage(profile.DefaultAppearance());
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
        const auto size = _tabs.Size();
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

        if (to.has_value())
        {
            // Selecting the dropped tab
            TabRow().TabView().SelectedIndex(to.value());
        }

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
                _updateAllTabCloseButtons(tab);
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
}
