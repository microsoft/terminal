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
    SettingsTab::SettingsTab()
    {
        Content(winrt::Microsoft::Terminal::Settings::Editor::MainPage());

        _MakeTabViewItem();
        _MakeSwitchToTabCommand();
        _CreateContextMenu();
        _CreateIcon();
    }

    // Method Description:
    // - Initializes a TabViewItem for this Tab instance.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void SettingsTab::_MakeTabViewItem()
    {
        TabViewItem(::winrt::MUX::Controls::TabViewItem{});
        TabViewItem().Header(winrt::box_value(Title()));
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
        // TODO: Focus the settings UI
        _focused = true;
    }

    // Method Description:
    // - Set the icon on the TabViewItem for this tab.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    winrt::fire_and_forget SettingsTab::_CreateIcon()
    {
        auto weakThis{ get_weak() };

        co_await winrt::resume_foreground(TabViewItem().Dispatcher());

        if (auto tab{ weakThis.get() })
        {
            auto fontFamily = winrt::WUX::Media::FontFamily(L"Segoe MDL2 Assets");
            auto glyph = L"\xE713"; // This is the Setting icon (looks like a gear)

            // The TabViewItem Icon needs MUX while the IconSourceElement in the CommandPalette needs WUX...
            IconSource(GetFontIcon<winrt::WUX::Controls::IconSource>(fontFamily, 12, glyph));
            TabViewItem().IconSource(GetFontIcon<winrt::MUX::Controls::IconSource>(fontFamily, 12, glyph));

            // Update SwitchToTab command's icon
            SwitchToTabCommand().IconSource(GetFontIcon<winrt::WUX::Controls::IconSource>(fontFamily, 12, glyph));
        }
    }

    // Method Description:
    // - Gets the title string of the settings UI
    // Arguments:
    // - <none>
    // Return Value:
    // - the title string of the last focused terminal control in our tree.
    winrt::hstring SettingsTab::GetActiveTitle() const
    {
        // TODO: This _could_ change depending on what page of the settings UI is open/focused?
        return Title();
    }

    // Method Description:
    // - Prepares this tab for being removed from the UI hierarchy
    void SettingsTab::Shutdown()
    {
        // TODO: Does/Will the settings UI need some shutdown procedures?
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
        TabViewItem().ContextFlyout(newTabFlyout);
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
