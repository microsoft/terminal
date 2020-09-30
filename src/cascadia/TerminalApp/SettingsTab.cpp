// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>
#include "SettingsTab.h"
#include "SettingsTab.g.cpp"
#include "Utils.h"
#include "ActionAndArgs.h"
#include "ActionArgs.h"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::Windows::System;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
}

namespace winrt::TerminalApp::implementation
{
    SettingsTab::SettingsTab(winrt::Windows::UI::Xaml::UIElement settingsUI):
        _settingsUI{ settingsUI }
    {
        _MakeTabViewItem();
        _MakeSwitchToTabCommand();
        _CreateContextMenu();
    }

    // Method Description:
    // - Initializes a TabViewItem for this Tab instance.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void SettingsTab::_MakeTabViewItem()
    {
        _tabViewItem = ::winrt::MUX::Controls::TabViewItem{};
        _tabViewItem.Header(winrt::box_value(Title()));
    }

    // Method Description:
    // - Get the root UIElement of this Tab's root pane.
    // Arguments:
    // - <none>
    // Return Value:
    // - The UIElement acting as root of the Tab's root pane.
    UIElement SettingsTab::GetRootElement()
    {
        return _settingsUI;
    }

    // Method Description:
    // - Gets the TabViewItem that represents this Tab
    // Arguments:
    // - <none>
    // Return Value:
    // - The TabViewItem that represents this Tab
    winrt::MUX::Controls::TabViewItem SettingsTab::GetTabViewItem()
    {
        return _tabViewItem;
    }

    // Method Description:
    // - Returns true if this is the currently focused tab. For any set of tabs,
    //   there should only be one tab that is marked as focused, though each tab has
    //   no control over the other tabs in the set.
    // Arguments:
    // - <none>
    // Return Value:
    // - true iff this tab is focused.
    bool SettingsTab::IsFocused() const noexcept
    {
        return _focused;
    }

    // Method Description:
    // - Updates our focus state.
    // Arguments:
    // - focused: our new focus state. If true, we should be focused. If false, we
    //   should be unfocused.
    // Return Value:
    // - <none>
    void SettingsTab::SetFocused(const bool focused)
    {
        _focused = focused;

        if (_focused)
        {
            _Focus();
        }
    }

    // Method Description:
    // - Focus the settings UI
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void SettingsTab::_Focus()
    {
        _focused = true;
        // TODO: Focus the settings ui, however you can. grid can't be focused, gotta focus something in it.
    }

    // Method Description:
    // - Set the icon on the TabViewItem for this tab.
    // Arguments:
    // - iconPath: The new path string to use as the IconPath for our TabViewItem
    // Return Value:
    // - <none>
    winrt::fire_and_forget SettingsTab::UpdateIcon()
    {
        auto weakThis{ get_weak() };

        co_await winrt::resume_foreground(_tabViewItem.Dispatcher());

        if (auto tab{ weakThis.get() })
        {
            auto fontFamily = winrt::WUX::Media::FontFamily(L"Segoe MDL2 Assets");
            auto glyph = L"\xE713";

            // The TabViewItem Icon needs MUX while the IconSourceElement in the CommandPalette needs WUX...
            IconSource(GetFontIcon<winrt::WUX::Controls::IconSource>(fontFamily, 12, glyph));
            _tabViewItem.IconSource(GetFontIcon<winrt::MUX::Controls::IconSource>(fontFamily, 12, glyph));

            // Update SwitchToTab command's icon
            SwitchToTabCommand().IconSource(GetFontIcon<winrt::WUX::Controls::IconSource>(fontFamily, 12, glyph));
        }
    }

    // Method Description:
    // - Gets the title string of the last focused terminal control in our tree.
    //   Returns the empty string if there is no such control.
    // Arguments:
    // - <none>
    // Return Value:
    // - the title string of the last focused terminal control in our tree.
    winrt::hstring SettingsTab::GetActiveTitle() const
    {
        return Title();
    }

    // Method Description:
    // - Prepares this tab for being removed from the UI hierarchy by shutting down all active connections.
    void SettingsTab::Shutdown()
    {
        // TODO: LEON: Does the settings UI need any shutdown?
    }

    // Method Description:
    // - Creates a context menu attached to the tab.
    // Currently contains elements allowing to select or
    // to close the current tab
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void SettingsTab::_CreateContextMenu()
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
        Controls::MenuFlyoutSeparator menuSeparator;
        newTabFlyout.Items().Append(closeTabMenuItem);
        _tabViewItem.ContextFlyout(newTabFlyout);
    }

    // Method Description:
    // - Initializes a SwitchToTab command object for this Tab instance.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void SettingsTab::_MakeSwitchToTabCommand()
    {
        auto focusTabAction = winrt::make_self<implementation::ActionAndArgs>();
        auto args = winrt::make_self<implementation::SwitchToTabArgs>();
        args->TabIndex(_TabViewIndex);

        focusTabAction->Action(ShortcutAction::SwitchToTab);
        focusTabAction->Args(*args);

        winrt::TerminalApp::Command command;
        command.Action(*focusTabAction);
        command.Name(Title());
        command.IconSource(IconSource());

        SwitchToTabCommand(command);
    }

    void SettingsTab::UpdateTabViewIndex(const uint32_t idx)
    {
        TabViewIndex(idx);
        SwitchToTabCommand().Action().Args().as<implementation::SwitchToTabArgs>()->TabIndex(idx);
    }
}
