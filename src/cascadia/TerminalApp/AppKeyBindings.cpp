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
            auto args = CopyTextArgs();
            args.TrimWhitespace(true);
            auto eventArgs = ActionEventArgs(args);
            _CopyTextHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::CopyTextWithoutNewlines:
        {
            auto args = CopyTextArgs();
            args.TrimWhitespace(false);
            auto eventArgs = ActionEventArgs(args);
            _CopyTextHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::PasteText:
        {
            auto eventArgs = ActionEventArgs();
            _PasteTextHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::NewTab:
        {
            auto eventArgs = ActionEventArgs();
            _NewTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::DuplicateTab:
        {
            auto eventArgs = ActionEventArgs();
            _DuplicateTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::OpenSettings:
        {
            auto eventArgs = ActionEventArgs();
            _OpenSettingsHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }

        case ShortcutAction::NewTabProfile0:
        {
            auto args = NewTabWithProfileArgs();
            args.ProfileIndex(0);
            auto eventArgs = ActionEventArgs(args);
            _NewTabWithProfileHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::NewTabProfile1:
        {
            auto args = NewTabWithProfileArgs();
            args.ProfileIndex(1);
            auto eventArgs = ActionEventArgs(args);
            _NewTabWithProfileHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::NewTabProfile2:
        {
            auto args = NewTabWithProfileArgs();
            args.ProfileIndex(2);
            auto eventArgs = ActionEventArgs(args);
            _NewTabWithProfileHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::NewTabProfile3:
        {
            auto args = NewTabWithProfileArgs();
            args.ProfileIndex(3);
            auto eventArgs = ActionEventArgs(args);
            _NewTabWithProfileHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::NewTabProfile4:
        {
            auto args = NewTabWithProfileArgs();
            args.ProfileIndex(4);
            auto eventArgs = ActionEventArgs(args);
            _NewTabWithProfileHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::NewTabProfile5:
        {
            auto args = NewTabWithProfileArgs();
            args.ProfileIndex(5);
            auto eventArgs = ActionEventArgs(args);
            _NewTabWithProfileHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::NewTabProfile6:
        {
            auto args = NewTabWithProfileArgs();
            args.ProfileIndex(6);
            auto eventArgs = ActionEventArgs(args);
            _NewTabWithProfileHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::NewTabProfile7:
        {
            auto args = NewTabWithProfileArgs();
            args.ProfileIndex(7);
            auto eventArgs = ActionEventArgs(args);
            _NewTabWithProfileHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::NewTabProfile8:
        {
            auto args = NewTabWithProfileArgs();
            args.ProfileIndex(8);
            auto eventArgs = ActionEventArgs(args);
            _NewTabWithProfileHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }

        case ShortcutAction::NewWindow:
        {
            auto eventArgs = ActionEventArgs();
            _NewWindowHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::CloseWindow:
        {
            auto eventArgs = ActionEventArgs();
            _CloseWindowHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::CloseTab:
        {
            auto eventArgs = ActionEventArgs();
            _CloseTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::ClosePane:
        {
            auto eventArgs = ActionEventArgs();
            _ClosePaneHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }

        case ShortcutAction::ScrollUp:
        {
            auto eventArgs = ActionEventArgs();
            _ScrollUpHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::ScrollDown:
        {
            auto eventArgs = ActionEventArgs();
            _ScrollDownHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::ScrollUpPage:
        {
            auto eventArgs = ActionEventArgs();
            _ScrollUpPageHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::ScrollDownPage:
        {
            auto eventArgs = ActionEventArgs();
            _ScrollDownPageHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }

        case ShortcutAction::NextTab:
        {
            auto eventArgs = ActionEventArgs();
            _NextTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::PrevTab:
        {
            auto eventArgs = ActionEventArgs();
            _PrevTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }

        case ShortcutAction::SplitVertical:
        {
            auto eventArgs = ActionEventArgs();
            _SplitVerticalHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::SplitHorizontal:
        {
            auto eventArgs = ActionEventArgs();
            _SplitHorizontalHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }

        case ShortcutAction::SwitchToTab0:
        {
            auto args = SwitchToTabArgs();
            args.TabIndex(0);
            auto eventArgs = ActionEventArgs(args);
            _SwitchToTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::SwitchToTab1:
        {
            auto args = SwitchToTabArgs();
            args.TabIndex(1);
            auto eventArgs = ActionEventArgs(args);
            _SwitchToTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::SwitchToTab2:
        {
            auto args = SwitchToTabArgs();
            args.TabIndex(2);
            auto eventArgs = ActionEventArgs(args);
            _SwitchToTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::SwitchToTab3:
        {
            auto args = SwitchToTabArgs();
            args.TabIndex(3);
            auto eventArgs = ActionEventArgs(args);
            _SwitchToTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::SwitchToTab4:
        {
            auto args = SwitchToTabArgs();
            args.TabIndex(4);
            auto eventArgs = ActionEventArgs(args);
            _SwitchToTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::SwitchToTab5:
        {
            auto args = SwitchToTabArgs();
            args.TabIndex(5);
            auto eventArgs = ActionEventArgs(args);
            _SwitchToTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::SwitchToTab6:
        {
            auto args = SwitchToTabArgs();
            args.TabIndex(6);
            auto eventArgs = ActionEventArgs(args);
            _SwitchToTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::SwitchToTab7:
        {
            auto args = SwitchToTabArgs();
            args.TabIndex(7);
            auto eventArgs = ActionEventArgs(args);
            _SwitchToTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::SwitchToTab8:
        {
            auto args = SwitchToTabArgs();
            args.TabIndex(8);
            auto eventArgs = ActionEventArgs(args);
            _SwitchToTabHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::ResizePaneLeft:
        {
            auto args = ResizePaneArgs();
            args.Direction(Direction::Left);
            auto eventArgs = ActionEventArgs(args);
            _ResizePaneHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::ResizePaneRight:
        {
            auto args = ResizePaneArgs();
            args.Direction(Direction::Right);
            auto eventArgs = ActionEventArgs(args);
            _ResizePaneHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::ResizePaneUp:
        {
            auto args = ResizePaneArgs();
            args.Direction(Direction::Up);
            auto eventArgs = ActionEventArgs(args);
            _ResizePaneHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::ResizePaneDown:
        {
            auto args = ResizePaneArgs();
            args.Direction(Direction::Down);
            auto eventArgs = ActionEventArgs(args);
            _ResizePaneHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::MoveFocusLeft:
        {
            auto args = MoveFocusArgs();
            args.Direction(Direction::Left);
            auto eventArgs = ActionEventArgs(args);
            _MoveFocusHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::MoveFocusRight:
        {
            auto args = MoveFocusArgs();
            args.Direction(Direction::Right);
            auto eventArgs = ActionEventArgs(args);
            _MoveFocusHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::MoveFocusUp:
        {
            auto args = MoveFocusArgs();
            args.Direction(Direction::Up);
            auto eventArgs = ActionEventArgs(args);
            _MoveFocusHandlers(*this, eventArgs);
            return eventArgs.Handled();
        }
        case ShortcutAction::MoveFocusDown:
        {
            auto args = MoveFocusArgs();
            args.Direction(Direction::Down);
            auto eventArgs = ActionEventArgs(args);
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
