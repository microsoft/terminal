// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>
#include "TabBase.h"
#include "TabBase.g.cpp"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::System;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
}

namespace winrt::TerminalApp::implementation
{
    WUX::FocusState TabBase::FocusState() const noexcept
    {
        return _focusState;
    }

    // Method Description:
    // - Prepares this tab for being removed from the UI hierarchy
    void TabBase::Shutdown()
    {
        Content(nullptr);
        _ClosedHandlers(nullptr, nullptr);
    }

    // Method Description:
    // - Creates a context menu attached to the tab.
    // Currently contains elements allowing the user to close the selected tab
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TabBase::_CreateContextMenu()
    {
        auto weakThis{ get_weak() };

        // Close
        Controls::MenuFlyoutItem closeTabMenuItem;
        Controls::FontIcon closeSymbol;
        closeSymbol.FontFamily(Media::FontFamily{ L"Segoe MDL2 Assets" });
        closeSymbol.Glyph(L"\xE8BB");

        closeTabMenuItem.Click([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->_ClosedHandlers(nullptr, nullptr);
            }
        });
        closeTabMenuItem.Text(RS_(L"TabClose"));
        closeTabMenuItem.Icon(closeSymbol);

        // Build the menu
        Controls::MenuFlyout newTabFlyout;
        newTabFlyout.Items().Append(_CreateCloseSubMenu());
        newTabFlyout.Items().Append(closeTabMenuItem);
        TabViewItem().ContextFlyout(newTabFlyout);
    }

    // Method Description:
    // - Creates a sub-menu containing menu items to close multiple tabs
    // Arguments:
    // - <none>
    // Return Value:
    // - the created MenuFlyoutSubItem
    Controls::MenuFlyoutSubItem TabBase::_CreateCloseSubMenu()
    {
        auto weakThis{ get_weak() };

        // Close tabs after
        _closeTabsAfterMenuItem.Click([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->_CloseTabsAfter();
            }
        });
        _closeTabsAfterMenuItem.Text(RS_(L"TabCloseAfter"));

        // Close other tabs
        _closeOtherTabsMenuItem.Click([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->_CloseOtherTabs();
            }
        });
        _closeOtherTabsMenuItem.Text(RS_(L"TabCloseOther"));

        Controls::MenuFlyoutSubItem closeSubMenu;
        closeSubMenu.Text(RS_(L"TabCloseSubMenu"));
        closeSubMenu.Items().Append(_closeTabsAfterMenuItem);
        closeSubMenu.Items().Append(_closeOtherTabsMenuItem);

        return closeSubMenu;
    }

    // Method Description:
    // - Enable the Close menu items based on tab index and total number of tabs
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TabBase::_EnableCloseMenuItems()
    {
        // close other tabs is enabled only if there are other tabs
        _closeOtherTabsMenuItem.IsEnabled(TabViewNumTabs() > 1);
        // close tabs after is enabled only if there are other tabs on the right
        _closeTabsAfterMenuItem.IsEnabled(TabViewIndex() < TabViewNumTabs() - 1);
    }

    void TabBase::_CloseTabsAfter()
    {
        CloseTabsAfterArgs args{ _TabViewIndex };
        ActionAndArgs closeTabsAfter{ ShortcutAction::CloseTabsAfter, args };

        _dispatch.DoAction(closeTabsAfter);
    }

    void TabBase::_CloseOtherTabs()
    {
        CloseOtherTabsArgs args{ _TabViewIndex };
        ActionAndArgs closeOtherTabs{ ShortcutAction::CloseOtherTabs, args };

        _dispatch.DoAction(closeOtherTabs);
    }

    void TabBase::UpdateTabViewIndex(const uint32_t idx, const uint32_t numTabs)
    {
        TabViewIndex(idx);
        TabViewNumTabs(numTabs);
        _EnableCloseMenuItems();
    }

    void TabBase::SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch)
    {
        _dispatch = dispatch;
    }
}
