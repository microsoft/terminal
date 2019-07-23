// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppKeyBindings.h"
#include "KeyChordSerialization.h"

#include "AppKeyBindings.g.cpp"

using namespace winrt::Microsoft::Terminal;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::Data::Json;

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
            _CopyTextHandlers(true);
            return true;
        case ShortcutAction::CopyTextWithoutNewlines:
            _CopyTextHandlers(false);
            return true;
        case ShortcutAction::PasteText:
            _PasteTextHandlers();
            return true;
        case ShortcutAction::NewTab:
            _NewTabHandlers();
            return true;
        case ShortcutAction::DuplicateTab:
            _DuplicateTabHandlers();
            return true;
        case ShortcutAction::OpenSettings:
            _OpenSettingsHandlers();
            return true;

        case ShortcutAction::NewTabProfile0:
            _NewTabWithProfileHandlers(0);
            return true;
        case ShortcutAction::NewTabProfile1:
            _NewTabWithProfileHandlers(1);
            return true;
        case ShortcutAction::NewTabProfile2:
            _NewTabWithProfileHandlers(2);
            return true;
        case ShortcutAction::NewTabProfile3:
            _NewTabWithProfileHandlers(3);
            return true;
        case ShortcutAction::NewTabProfile4:
            _NewTabWithProfileHandlers(4);
            return true;
        case ShortcutAction::NewTabProfile5:
            _NewTabWithProfileHandlers(5);
            return true;
        case ShortcutAction::NewTabProfile6:
            _NewTabWithProfileHandlers(6);
            return true;
        case ShortcutAction::NewTabProfile7:
            _NewTabWithProfileHandlers(7);
            return true;
        case ShortcutAction::NewTabProfile8:
            _NewTabWithProfileHandlers(8);
            return true;

        case ShortcutAction::NewWindow:
            _NewWindowHandlers();
            return true;
        case ShortcutAction::CloseWindow:
            _CloseWindowHandlers();
            return true;
        case ShortcutAction::CloseTab:
            _CloseTabHandlers();
            return true;
        case ShortcutAction::ClosePane:
            _ClosePaneHandlers();
            return true;

        case ShortcutAction::ScrollUp:
            _ScrollUpHandlers();
            return true;
        case ShortcutAction::ScrollDown:
            _ScrollDownHandlers();
            return true;
        case ShortcutAction::ScrollUpPage:
            _ScrollUpPageHandlers();
            return true;
        case ShortcutAction::ScrollDownPage:
            _ScrollDownPageHandlers();
            return true;

        case ShortcutAction::NextTab:
            _NextTabHandlers();
            return true;
        case ShortcutAction::PrevTab:
            _PrevTabHandlers();
            return true;

        case ShortcutAction::SplitVertical:
            _SplitVerticalHandlers();
            return true;
        case ShortcutAction::SplitHorizontal:
            _SplitHorizontalHandlers();
            return true;

        case ShortcutAction::SwitchToTab0:
            _SwitchToTabHandlers(0);
            return true;
        case ShortcutAction::SwitchToTab1:
            _SwitchToTabHandlers(1);
            return true;
        case ShortcutAction::SwitchToTab2:
            _SwitchToTabHandlers(2);
            return true;
        case ShortcutAction::SwitchToTab3:
            _SwitchToTabHandlers(3);
            return true;
        case ShortcutAction::SwitchToTab4:
            _SwitchToTabHandlers(4);
            return true;
        case ShortcutAction::SwitchToTab5:
            _SwitchToTabHandlers(5);
            return true;
        case ShortcutAction::SwitchToTab6:
            _SwitchToTabHandlers(6);
            return true;
        case ShortcutAction::SwitchToTab7:
            _SwitchToTabHandlers(7);
            return true;
        case ShortcutAction::SwitchToTab8:
            _SwitchToTabHandlers(8);
            return true;
        case ShortcutAction::ResizePaneLeft:
            _ResizePaneHandlers(Direction::Left);
            return true;
        case ShortcutAction::ResizePaneRight:
            _ResizePaneHandlers(Direction::Right);
            return true;
        case ShortcutAction::ResizePaneUp:
            _ResizePaneHandlers(Direction::Up);
            return true;
        case ShortcutAction::ResizePaneDown:
            _ResizePaneHandlers(Direction::Down);
            return true;
        case ShortcutAction::MoveFocusLeft:
            _MoveFocusHandlers(Direction::Left);
            return true;
        case ShortcutAction::MoveFocusRight:
            _MoveFocusHandlers(Direction::Right);
            return true;
        case ShortcutAction::MoveFocusUp:
            _MoveFocusHandlers(Direction::Up);
            return true;
        case ShortcutAction::MoveFocusDown:
            _MoveFocusHandlers(Direction::Down);
            return true;
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

    // -------------------------------- Events ---------------------------------
    // clang-format off
    DEFINE_EVENT(AppKeyBindings, CopyText,          _CopyTextHandlers,          TerminalApp::CopyTextEventArgs);
    DEFINE_EVENT(AppKeyBindings, PasteText,         _PasteTextHandlers,         TerminalApp::PasteTextEventArgs);
    DEFINE_EVENT(AppKeyBindings, NewTab,            _NewTabHandlers,            TerminalApp::NewTabEventArgs);
    DEFINE_EVENT(AppKeyBindings, DuplicateTab,      _DuplicateTabHandlers,      TerminalApp::DuplicateTabEventArgs);
    DEFINE_EVENT(AppKeyBindings, NewTabWithProfile, _NewTabWithProfileHandlers, TerminalApp::NewTabWithProfileEventArgs);
    DEFINE_EVENT(AppKeyBindings, NewWindow,         _NewWindowHandlers,         TerminalApp::NewWindowEventArgs);
    DEFINE_EVENT(AppKeyBindings, CloseWindow,       _CloseWindowHandlers,       TerminalApp::CloseWindowEventArgs);
    DEFINE_EVENT(AppKeyBindings, CloseTab,          _CloseTabHandlers,          TerminalApp::CloseTabEventArgs);
    DEFINE_EVENT(AppKeyBindings, ClosePane,         _ClosePaneHandlers,         TerminalApp::ClosePaneEventArgs);
    DEFINE_EVENT(AppKeyBindings, SwitchToTab,       _SwitchToTabHandlers,       TerminalApp::SwitchToTabEventArgs);
    DEFINE_EVENT(AppKeyBindings, NextTab,           _NextTabHandlers,           TerminalApp::NextTabEventArgs);
    DEFINE_EVENT(AppKeyBindings, PrevTab,           _PrevTabHandlers,           TerminalApp::PrevTabEventArgs);
    DEFINE_EVENT(AppKeyBindings, SplitVertical,     _SplitVerticalHandlers,     TerminalApp::SplitVerticalEventArgs);
    DEFINE_EVENT(AppKeyBindings, SplitHorizontal,   _SplitHorizontalHandlers,   TerminalApp::SplitHorizontalEventArgs);
    DEFINE_EVENT(AppKeyBindings, IncreaseFontSize,  _IncreaseFontSizeHandlers,  TerminalApp::IncreaseFontSizeEventArgs);
    DEFINE_EVENT(AppKeyBindings, DecreaseFontSize,  _DecreaseFontSizeHandlers,  TerminalApp::DecreaseFontSizeEventArgs);
    DEFINE_EVENT(AppKeyBindings, ScrollUp,          _ScrollUpHandlers,          TerminalApp::ScrollUpEventArgs);
    DEFINE_EVENT(AppKeyBindings, ScrollDown,        _ScrollDownHandlers,        TerminalApp::ScrollDownEventArgs);
    DEFINE_EVENT(AppKeyBindings, ScrollUpPage,      _ScrollUpPageHandlers,      TerminalApp::ScrollUpPageEventArgs);
    DEFINE_EVENT(AppKeyBindings, ScrollDownPage,    _ScrollDownPageHandlers,    TerminalApp::ScrollDownPageEventArgs);
    DEFINE_EVENT(AppKeyBindings, OpenSettings,      _OpenSettingsHandlers,      TerminalApp::OpenSettingsEventArgs);
    DEFINE_EVENT(AppKeyBindings, ResizePane,        _ResizePaneHandlers,        TerminalApp::ResizePaneEventArgs);
    DEFINE_EVENT(AppKeyBindings, MoveFocus,         _MoveFocusHandlers,         TerminalApp::MoveFocusEventArgs);
    // clang-format on
}
