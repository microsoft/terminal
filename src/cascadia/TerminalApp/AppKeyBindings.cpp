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
        {
            auto args = CopyTextEventArgs();
            args.TrimWhitespace(true);
            _CopyTextHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::CopyTextWithoutNewlines:
        {
            auto args = CopyTextEventArgs();
            args.TrimWhitespace(false);
            _CopyTextHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::PasteText:
        {
            auto args = PasteTextEventArgs();
            _PasteTextHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::NewTab:
        {
            auto args = NewTabEventArgs();
            _NewTabHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::DuplicateTab:
        {
            auto args = DuplicateTabEventArgs();
            _DuplicateTabHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::OpenSettings:
        {
            auto args = OpenSettingsEventArgs();
            _OpenSettingsHandlers(*this, args);
            return args.Handled();
        }

        case ShortcutAction::NewTabProfile0:
        {
            auto args = NewTabWithProfileEventArgs();
            args.ProfileIndex(0);
            _NewTabWithProfileHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::NewTabProfile1:
        {
            auto args = NewTabWithProfileEventArgs();
            args.ProfileIndex(1);
            _NewTabWithProfileHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::NewTabProfile2:
        {
            auto args = NewTabWithProfileEventArgs();
            args.ProfileIndex(2);
            _NewTabWithProfileHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::NewTabProfile3:
        {
            auto args = NewTabWithProfileEventArgs();
            args.ProfileIndex(3);
            _NewTabWithProfileHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::NewTabProfile4:
        {
            auto args = NewTabWithProfileEventArgs();
            args.ProfileIndex(4);
            _NewTabWithProfileHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::NewTabProfile5:
        {
            auto args = NewTabWithProfileEventArgs();
            args.ProfileIndex(5);
            _NewTabWithProfileHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::NewTabProfile6:
        {
            auto args = NewTabWithProfileEventArgs();
            args.ProfileIndex(6);
            _NewTabWithProfileHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::NewTabProfile7:
        {
            auto args = NewTabWithProfileEventArgs();
            args.ProfileIndex(7);
            _NewTabWithProfileHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::NewTabProfile8:
        {
            auto args = NewTabWithProfileEventArgs();
            args.ProfileIndex(8);
            _NewTabWithProfileHandlers(*this, args);
            return args.Handled();
        }

        case ShortcutAction::NewWindow:
        {
            auto args = NewWindowEventArgs();
            _NewWindowHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::CloseWindow:
        {
            auto args = CloseWindowEventArgs();
            _CloseWindowHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::CloseTab:
        {
            auto args = CloseTabEventArgs();
            _CloseTabHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::ClosePane:
        {
            auto args = ClosePaneEventArgs();
            _ClosePaneHandlers(*this, args);
            return args.Handled();
        }

        case ShortcutAction::ScrollUp:
        {
            auto args = ScrollUpEventArgs();
            _ScrollUpHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::ScrollDown:
        {
            auto args = ScrollDownEventArgs();
            _ScrollDownHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::ScrollUpPage:
        {
            auto args = ScrollUpPageEventArgs();
            _ScrollUpPageHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::ScrollDownPage:
        {
            auto args = ScrollDownPageEventArgs();
            _ScrollDownPageHandlers(*this, args);
            return args.Handled();
        }

        case ShortcutAction::NextTab:
        {
            auto args = NextTabEventArgs();
            _NextTabHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::PrevTab:
        {
            auto args = PrevTabEventArgs();
            _PrevTabHandlers(*this, args);
            return args.Handled();
        }

        case ShortcutAction::SplitVertical:
        {
            auto args = SplitVerticalEventArgs();
            _SplitVerticalHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::SplitHorizontal:
        {
            auto args = SplitHorizontalEventArgs();
            _SplitHorizontalHandlers(*this, args);
            return args.Handled();
        }

        case ShortcutAction::SwitchToTab0:
        {
            auto args = SwitchToTabEventArgs();
            args.TabIndex(0);
            _SwitchToTabHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::SwitchToTab1:
        {
            auto args = SwitchToTabEventArgs();
            args.TabIndex(1);
            _SwitchToTabHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::SwitchToTab2:
        {
            auto args = SwitchToTabEventArgs();
            args.TabIndex(2);
            _SwitchToTabHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::SwitchToTab3:
        {
            auto args = SwitchToTabEventArgs();
            args.TabIndex(3);
            _SwitchToTabHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::SwitchToTab4:
        {
            auto args = SwitchToTabEventArgs();
            args.TabIndex(4);
            _SwitchToTabHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::SwitchToTab5:
        {
            auto args = SwitchToTabEventArgs();
            args.TabIndex(5);
            _SwitchToTabHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::SwitchToTab6:
        {
            auto args = SwitchToTabEventArgs();
            args.TabIndex(6);
            _SwitchToTabHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::SwitchToTab7:
        {
            auto args = SwitchToTabEventArgs();
            args.TabIndex(7);
            _SwitchToTabHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::SwitchToTab8:
        {
            auto args = SwitchToTabEventArgs();
            args.TabIndex(8);
            _SwitchToTabHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::ResizePaneLeft:
        {
            auto args = ResizePaneEventArgs();
            args.Direction(Direction::Left);
            _ResizePaneHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::ResizePaneRight:
        {
            auto args = ResizePaneEventArgs();
            args.Direction(Direction::Right);
            _ResizePaneHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::ResizePaneUp:
        {
            auto args = ResizePaneEventArgs();
            args.Direction(Direction::Up);
            _ResizePaneHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::ResizePaneDown:
        {
            auto args = ResizePaneEventArgs();
            args.Direction(Direction::Down);
            _ResizePaneHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::MoveFocusLeft:
        {
            auto args = MoveFocusEventArgs();
            args.Direction(Direction::Left);
            _MoveFocusHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::MoveFocusRight:
        {
            auto args = MoveFocusEventArgs();
            args.Direction(Direction::Right);
            _MoveFocusHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::MoveFocusUp:
        {
            auto args = MoveFocusEventArgs();
            args.Direction(Direction::Up);
            _MoveFocusHandlers(*this, args);
            return args.Handled();
        }
        case ShortcutAction::MoveFocusDown:
        {
            auto args = MoveFocusEventArgs();
            args.Direction(Direction::Down);
            _MoveFocusHandlers(*this, args);
            return args.Handled();
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

    // -------------------------------- Events ---------------------------------
    // clang-format off
    // DEFINE_EVENT(AppKeyBindings, CopyText,          _CopyTextHandlers,          TerminalApp::CopyTextEventArgs);
    // DEFINE_EVENT(AppKeyBindings, PasteText,         _PasteTextHandlers,         TerminalApp::PasteTextEventArgs);
    // DEFINE_EVENT(AppKeyBindings, NewTab,            _NewTabHandlers,            TerminalApp::NewTabEventArgs);
    // DEFINE_EVENT(AppKeyBindings, DuplicateTab,      _DuplicateTabHandlers,      TerminalApp::DuplicateTabEventArgs);
    // DEFINE_EVENT(AppKeyBindings, NewTabWithProfile, _NewTabWithProfileHandlers, TerminalApp::NewTabWithProfileEventArgs);
    // DEFINE_EVENT(AppKeyBindings, NewWindow,         _NewWindowHandlers,         TerminalApp::NewWindowEventArgs);
    // DEFINE_EVENT(AppKeyBindings, CloseWindow,       _CloseWindowHandlers,       TerminalApp::CloseWindowEventArgs);
    // DEFINE_EVENT(AppKeyBindings, CloseTab,          _CloseTabHandlers,          TerminalApp::CloseTabEventArgs);
    // DEFINE_EVENT(AppKeyBindings, ClosePane,         _ClosePaneHandlers,         TerminalApp::ClosePaneEventArgs);
    // DEFINE_EVENT(AppKeyBindings, SwitchToTab,       _SwitchToTabHandlers,       TerminalApp::SwitchToTabEventArgs);
    // DEFINE_EVENT(AppKeyBindings, NextTab,           _NextTabHandlers,           TerminalApp::NextTabEventArgs);
    // DEFINE_EVENT(AppKeyBindings, PrevTab,           _PrevTabHandlers,           TerminalApp::PrevTabEventArgs);
    // DEFINE_EVENT(AppKeyBindings, SplitVertical,     _SplitVerticalHandlers,     TerminalApp::SplitVerticalEventArgs);
    // DEFINE_EVENT(AppKeyBindings, SplitHorizontal,   _SplitHorizontalHandlers,   TerminalApp::SplitHorizontalEventArgs);
    // DEFINE_EVENT(AppKeyBindings, IncreaseFontSize,  _IncreaseFontSizeHandlers,  TerminalApp::IncreaseFontSizeEventArgs);
    // DEFINE_EVENT(AppKeyBindings, DecreaseFontSize,  _DecreaseFontSizeHandlers,  TerminalApp::DecreaseFontSizeEventArgs);
    // DEFINE_EVENT(AppKeyBindings, ScrollUp,          _ScrollUpHandlers,          TerminalApp::ScrollUpEventArgs);
    // DEFINE_EVENT(AppKeyBindings, ScrollDown,        _ScrollDownHandlers,        TerminalApp::ScrollDownEventArgs);
    // DEFINE_EVENT(AppKeyBindings, ScrollUpPage,      _ScrollUpPageHandlers,      TerminalApp::ScrollUpPageEventArgs);
    // DEFINE_EVENT(AppKeyBindings, ScrollDownPage,    _ScrollDownPageHandlers,    TerminalApp::ScrollDownPageEventArgs);
    // DEFINE_EVENT(AppKeyBindings, OpenSettings,      _OpenSettingsHandlers,      TerminalApp::OpenSettingsEventArgs);
    // DEFINE_EVENT(AppKeyBindings, ResizePane,        _ResizePaneHandlers,        TerminalApp::ResizePaneEventArgs);
    // DEFINE_EVENT(AppKeyBindings, MoveFocus,         _MoveFocusHandlers,         TerminalApp::MoveFocusEventArgs);

    // clang-format on
}
