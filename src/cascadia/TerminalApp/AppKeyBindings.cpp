// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppKeyBindings.h"
#include "KeyChordSerialization.h"

#include "AppKeyBindings.g.cpp"

using namespace winrt::Microsoft::Terminal;
using namespace winrt::TerminalApp;

namespace winrt::TerminalApp::implementation
{
    void AppKeyBindings::SetKeyBinding(const TerminalApp::ShortcutAction& action,
                                       const Settings::KeyChord& chord)
    {
        _keyShortcuts[chord] = action;
    }

    Microsoft::Terminal::Settings::KeyChord AppKeyBindings::GetKeyBinding(TerminalApp::ShortcutAction const& action)
    {
        for (auto& kv : _keyShortcuts)
        {
            if (kv.second == action)
            {
                return kv.first;
            }
        }
        return { nullptr };
    }

    bool AppKeyBindings::TryKeyChord(const Settings::KeyChord& kc)
    {
        const auto keyIter = _keyShortcuts.find(kc);
        if (keyIter != _keyShortcuts.end())
        {
            const auto action = keyIter->second;
            return _DoAction(action);
        }
        return false;
    }

    bool AppKeyBindings::_DoAction(ShortcutAction action)
    {
        switch (action)
        {
        case ShortcutAction::CopyText:
        {
            CopyTextArgs args{};
            args.TrimWhitespace(true);
            ActionEventArgs eventArgs{ args };
            _CopyTextHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::CopyTextWithoutNewlines:
        {
            CopyTextArgs args{};
            args.TrimWhitespace(false);
            ActionEventArgs eventArgs{ args };
            _CopyTextHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::PasteText:
        {
            ActionEventArgs eventArgs{};
            _PasteTextHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::NewTab:
        {
            ActionEventArgs eventArgs{};
            _NewTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::DuplicateTab:
        {
            ActionEventArgs eventArgs{};
            _DuplicateTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::OpenSettings:
        {
            ActionEventArgs eventArgs{};
            _OpenSettingsHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }

        case ShortcutAction::NewTabProfile0:
        {
            NewTabWithProfileArgs args{};
            args.ProfileIndex(0);
            ActionEventArgs eventArgs{ args };
            _NewTabWithProfileHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::NewTabProfile1:
        {
            NewTabWithProfileArgs args{};
            args.ProfileIndex(1);
            ActionEventArgs eventArgs{ args };
            _NewTabWithProfileHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::NewTabProfile2:
        {
            NewTabWithProfileArgs args{};
            args.ProfileIndex(2);
            ActionEventArgs eventArgs{ args };
            _NewTabWithProfileHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::NewTabProfile3:
        {
            NewTabWithProfileArgs args{};
            args.ProfileIndex(3);
            ActionEventArgs eventArgs{ args };
            _NewTabWithProfileHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::NewTabProfile4:
        {
            NewTabWithProfileArgs args{};
            args.ProfileIndex(4);
            ActionEventArgs eventArgs{ args };
            _NewTabWithProfileHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::NewTabProfile5:
        {
            NewTabWithProfileArgs args{};
            args.ProfileIndex(5);
            ActionEventArgs eventArgs{ args };
            _NewTabWithProfileHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::NewTabProfile6:
        {
            NewTabWithProfileArgs args{};
            args.ProfileIndex(6);
            ActionEventArgs eventArgs{ args };
            _NewTabWithProfileHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::NewTabProfile7:
        {
            NewTabWithProfileArgs args{};
            args.ProfileIndex(7);
            ActionEventArgs eventArgs{ args };
            _NewTabWithProfileHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::NewTabProfile8:
        {
            NewTabWithProfileArgs args{};
            args.ProfileIndex(8);
            ActionEventArgs eventArgs{ args };
            _NewTabWithProfileHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }

        case ShortcutAction::NewWindow:
        {
            ActionEventArgs eventArgs{};
            _NewWindowHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::CloseWindow:
        {
            ActionEventArgs eventArgs{};
            _CloseWindowHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::CloseTab:
        {
            ActionEventArgs eventArgs{};
            _CloseTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::ClosePane:
        {
            ActionEventArgs eventArgs{};
            _ClosePaneHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }

        case ShortcutAction::ScrollUp:
        {
            ActionEventArgs eventArgs{};
            _ScrollUpHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::ScrollDown:
        {
            ActionEventArgs eventArgs{};
            _ScrollDownHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::ScrollUpPage:
        {
            ActionEventArgs eventArgs{};
            _ScrollUpPageHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::ScrollDownPage:
        {
            ActionEventArgs eventArgs{};
            _ScrollDownPageHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }

        case ShortcutAction::NextTab:
        {
            ActionEventArgs eventArgs{};
            _NextTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::PrevTab:
        {
            ActionEventArgs eventArgs{};
            _PrevTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }

        case ShortcutAction::SplitVertical:
        {
            ActionEventArgs eventArgs{};
            _SplitVerticalHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::SplitHorizontal:
        {
            ActionEventArgs eventArgs{};
            _SplitHorizontalHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }

        case ShortcutAction::SwitchToTab0:
        {
            SwitchToTabArgs args{};
            args.TabIndex(0);
            ActionEventArgs eventArgs{ args };
            _SwitchToTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::SwitchToTab1:
        {
            SwitchToTabArgs args{};
            args.TabIndex(1);
            ActionEventArgs eventArgs{ args };
            _SwitchToTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::SwitchToTab2:
        {
            SwitchToTabArgs args{};
            args.TabIndex(2);
            ActionEventArgs eventArgs{ args };
            _SwitchToTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::SwitchToTab3:
        {
            SwitchToTabArgs args{};
            args.TabIndex(3);
            ActionEventArgs eventArgs{ args };
            _SwitchToTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::SwitchToTab4:
        {
            SwitchToTabArgs args{};
            args.TabIndex(4);
            ActionEventArgs eventArgs{ args };
            _SwitchToTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::SwitchToTab5:
        {
            SwitchToTabArgs args{};
            args.TabIndex(5);
            ActionEventArgs eventArgs{ args };
            _SwitchToTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::SwitchToTab6:
        {
            SwitchToTabArgs args{};
            args.TabIndex(6);
            ActionEventArgs eventArgs{ args };
            _SwitchToTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::SwitchToTab7:
        {
            SwitchToTabArgs args{};
            args.TabIndex(7);
            ActionEventArgs eventArgs{ args };
            _SwitchToTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::SwitchToTab8:
        {
            SwitchToTabArgs args{};
            args.TabIndex(8);
            ActionEventArgs eventArgs{ args };
            _SwitchToTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::ResizePaneLeft:
        {
            ResizePaneArgs args{};
            args.Direction(Direction::Left);
            ActionEventArgs eventArgs{ args };
            _ResizePaneHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::ResizePaneRight:
        {
            ResizePaneArgs args{};
            args.Direction(Direction::Right);
            ActionEventArgs eventArgs{ args };
            _ResizePaneHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::ResizePaneUp:
        {
            ResizePaneArgs args{};
            args.Direction(Direction::Up);
            ActionEventArgs eventArgs{ args };
            _ResizePaneHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::ResizePaneDown:
        {
            ResizePaneArgs args{};
            args.Direction(Direction::Down);
            ActionEventArgs eventArgs{ args };
            _ResizePaneHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::MoveFocusLeft:
        {
            MoveFocusArgs args{};
            args.Direction(Direction::Left);
            ActionEventArgs eventArgs{ args };
            _MoveFocusHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::MoveFocusRight:
        {
            MoveFocusArgs args{};
            args.Direction(Direction::Right);
            ActionEventArgs eventArgs{ args };
            _MoveFocusHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::MoveFocusUp:
        {
            MoveFocusArgs args{};
            args.Direction(Direction::Up);
            ActionEventArgs eventArgs{ args };
            _MoveFocusHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::MoveFocusDown:
        {
            MoveFocusArgs args{};
            args.Direction(Direction::Down);
            ActionEventArgs eventArgs{ args };
            _MoveFocusHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        default:
            return false;
        }
        return false;
    }

    // Method Description:
    // - Takes the KeyModifier flags from Terminal and maps them to the WinRT types which are used by XAML
    // Return Value:
    // - a Windows::System::VirtualKeyModifiers object with the flags of which modifiers used.
    Windows::System::VirtualKeyModifiers AppKeyBindings::ConvertVKModifiers(Settings::KeyModifiers modifiers)
    {
        Windows::System::VirtualKeyModifiers keyModifiers = Windows::System::VirtualKeyModifiers::None;

        if (WI_IsFlagSet(modifiers, Settings::KeyModifiers::Ctrl))
        {
            keyModifiers |= Windows::System::VirtualKeyModifiers::Control;
        }
        if (WI_IsFlagSet(modifiers, Settings::KeyModifiers::Shift))
        {
            keyModifiers |= Windows::System::VirtualKeyModifiers::Shift;
        }
        if (WI_IsFlagSet(modifiers, Settings::KeyModifiers::Alt))
        {
            // note: Menu is the Alt VK_MENU
            keyModifiers |= Windows::System::VirtualKeyModifiers::Menu;
        }

        return keyModifiers;
    }

}
