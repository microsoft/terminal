// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppKeyBindings.h"
#include "KeyChordSerialization.h"

#include "AppKeyBindings.g.cpp"

using namespace winrt::Microsoft::Terminal;
using namespace winrt::TerminalApp;
using namespace winrt::Microsoft::Terminal::Settings;

namespace winrt::TerminalApp::implementation
{
    void AppKeyBindings::SetKeyBinding(const TerminalApp::ActionAndArgs& actionAndArgs,
                                       const Settings::KeyChord& chord)
    {
        _keyShortcuts[chord] = actionAndArgs;
    }

    // Method Description:
    // - Remove the action that's bound to a particular KeyChord.
    // Arguments:
    // - chord: the keystroke to remove the action for.
    // Return Value:
    // - <none>
    void AppKeyBindings::ClearKeyBinding(const Settings::KeyChord& chord)
    {
        _keyShortcuts.erase(chord);
    }

    KeyChord AppKeyBindings::GetKeyBindingForAction(TerminalApp::ShortcutAction const& action)
    {
        for (auto& kv : _keyShortcuts)
        {
            if (kv.second.Action() == action)
            {
                return kv.first;
            }
        }
        return { nullptr };
    }

    // Method Description:
    // - Lookup the keychord bound to a particular combination of ShortcutAction
    //   and IActionArgs. This enables searching no only for the binding of a
    //   particular ShortcutAction, but also a particular set of values for
    //   arguments to that action.
    // Arguments:
    // - actionAndArgs: The ActionAndArgs to lookup the keybinding for.
    // Return Value:
    // - The bound keychord, if this ActionAndArgs is bound to a key, otherwise nullptr.
    KeyChord AppKeyBindings::GetKeyBindingForActionWithArgs(TerminalApp::ActionAndArgs const& actionAndArgs)
    {
        for (auto& kv : _keyShortcuts)
        {
            if (kv.second.Action() == actionAndArgs.Action() &&
                kv.second.Args().Equals(actionAndArgs.Args()))
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
            const auto actionAndArgs = keyIter->second;
            return _DoAction(actionAndArgs);
        }
        return false;
    }

    bool AppKeyBindings::_DoAction(ActionAndArgs actionAndArgs)
    {
        const auto& action = actionAndArgs.Action();
        const auto& args = actionAndArgs.Args();
        auto eventArgs = args ? winrt::make_self<ActionEventArgs>(args) :
                                winrt::make_self<ActionEventArgs>();

        switch (action)
        {
        case ShortcutAction::CopyText:
        {
            _CopyTextHandlers(*this, *eventArgs);
            break;
        }
        case ShortcutAction::CopyTextWithoutNewlines:
        {
            _CopyTextHandlers(*this, *eventArgs);
            break;
        }
        case ShortcutAction::PasteText:
        {
            _PasteTextHandlers(*this, *eventArgs);
            break;
        }
        case ShortcutAction::OpenNewTabDropdown:
        {
            _OpenNewTabDropdownHandlers(*this, *eventArgs);
            break;
        }
        case ShortcutAction::DuplicateTab:
        {
            _DuplicateTabHandlers(*this, *eventArgs);
            break;
        }
        case ShortcutAction::OpenSettings:
        {
            _OpenSettingsHandlers(*this, *eventArgs);
            break;
        }

        case ShortcutAction::NewTab:
        case ShortcutAction::NewTabProfile0:
        case ShortcutAction::NewTabProfile1:
        case ShortcutAction::NewTabProfile2:
        case ShortcutAction::NewTabProfile3:
        case ShortcutAction::NewTabProfile4:
        case ShortcutAction::NewTabProfile5:
        case ShortcutAction::NewTabProfile6:
        case ShortcutAction::NewTabProfile7:
        case ShortcutAction::NewTabProfile8:
        {
            _NewTabHandlers(*this, *eventArgs);
            break;
        }

        case ShortcutAction::NewWindow:
        {
            _NewWindowHandlers(*this, *eventArgs);
            break;
        }
        case ShortcutAction::CloseWindow:
        {
            _CloseWindowHandlers(*this, *eventArgs);
            break;
        }
        case ShortcutAction::CloseTab:
        {
            _CloseTabHandlers(*this, *eventArgs);
            break;
        }
        case ShortcutAction::ClosePane:
        {
            _ClosePaneHandlers(*this, *eventArgs);
            break;
        }

        case ShortcutAction::ScrollUp:
        {
            _ScrollUpHandlers(*this, *eventArgs);
            break;
        }
        case ShortcutAction::ScrollDown:
        {
            _ScrollDownHandlers(*this, *eventArgs);
            break;
        }
        case ShortcutAction::ScrollUpPage:
        {
            _ScrollUpPageHandlers(*this, *eventArgs);
            break;
        }
        case ShortcutAction::ScrollDownPage:
        {
            _ScrollDownPageHandlers(*this, *eventArgs);
            break;
        }

        case ShortcutAction::NextTab:
        {
            _NextTabHandlers(*this, *eventArgs);
            break;
        }
        case ShortcutAction::PrevTab:
        {
            _PrevTabHandlers(*this, *eventArgs);
            break;
        }

        case ShortcutAction::SplitVertical:
        {
            _SplitVerticalHandlers(*this, *eventArgs);
            break;
        }
        case ShortcutAction::SplitHorizontal:
        {
            _SplitHorizontalHandlers(*this, *eventArgs);
            break;
        }

        case ShortcutAction::SwitchToTab:
        case ShortcutAction::SwitchToTab0:
        case ShortcutAction::SwitchToTab1:
        case ShortcutAction::SwitchToTab2:
        case ShortcutAction::SwitchToTab3:
        case ShortcutAction::SwitchToTab4:
        case ShortcutAction::SwitchToTab5:
        case ShortcutAction::SwitchToTab6:
        case ShortcutAction::SwitchToTab7:
        case ShortcutAction::SwitchToTab8:
        {
            _SwitchToTabHandlers(*this, *eventArgs);
            break;
        }

        case ShortcutAction::ResizePane:
        case ShortcutAction::ResizePaneLeft:
        case ShortcutAction::ResizePaneRight:
        case ShortcutAction::ResizePaneUp:
        case ShortcutAction::ResizePaneDown:
        {
            _ResizePaneHandlers(*this, *eventArgs);
            break;
        }

        case ShortcutAction::MoveFocus:
        case ShortcutAction::MoveFocusLeft:
        case ShortcutAction::MoveFocusRight:
        case ShortcutAction::MoveFocusUp:
        case ShortcutAction::MoveFocusDown:
        {
            _MoveFocusHandlers(*this, *eventArgs);
            break;
        }

        case ShortcutAction::IncreaseFontSize:
        {
            _AdjustFontSizeHandlers(*this, *eventArgs);
            break;
        }
        case ShortcutAction::DecreaseFontSize:
        {
            _AdjustFontSizeHandlers(*this, *eventArgs);
            break;
        }
        case ShortcutAction::ToggleFullscreen:
        {
            _ToggleFullscreenHandlers(*this, *eventArgs);
            break;
        }
        default:
            return false;
        }
        return eventArgs->Handled();
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
