// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ShortcutActionDispatch.h"
#include "KeyChordSerialization.h"

#include "ShortcutActionDispatch.g.cpp"

using namespace winrt::Microsoft::Terminal;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::Data::Json;

namespace winrt::TerminalApp::implementation
{
    bool ShortcutActionDispatch::DoAction(ShortcutAction action)
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

    // -------------------------------- Events ---------------------------------
    // clang-format off
    DEFINE_EVENT(ShortcutActionDispatch, CopyText,          _CopyTextHandlers,          TerminalApp::CopyTextEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, PasteText,         _PasteTextHandlers,         TerminalApp::PasteTextEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, NewTab,            _NewTabHandlers,            TerminalApp::NewTabEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, DuplicateTab,      _DuplicateTabHandlers,      TerminalApp::DuplicateTabEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, NewTabWithProfile, _NewTabWithProfileHandlers, TerminalApp::NewTabWithProfileEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, NewWindow,         _NewWindowHandlers,         TerminalApp::NewWindowEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, CloseWindow,       _CloseWindowHandlers,       TerminalApp::CloseWindowEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, CloseTab,          _CloseTabHandlers,          TerminalApp::CloseTabEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, ClosePane,         _ClosePaneHandlers,         TerminalApp::ClosePaneEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, SwitchToTab,       _SwitchToTabHandlers,       TerminalApp::SwitchToTabEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, NextTab,           _NextTabHandlers,           TerminalApp::NextTabEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, PrevTab,           _PrevTabHandlers,           TerminalApp::PrevTabEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, SplitVertical,     _SplitVerticalHandlers,     TerminalApp::SplitVerticalEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, SplitHorizontal,   _SplitHorizontalHandlers,   TerminalApp::SplitHorizontalEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, IncreaseFontSize,  _IncreaseFontSizeHandlers,  TerminalApp::IncreaseFontSizeEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, DecreaseFontSize,  _DecreaseFontSizeHandlers,  TerminalApp::DecreaseFontSizeEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, ScrollUp,          _ScrollUpHandlers,          TerminalApp::ScrollUpEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, ScrollDown,        _ScrollDownHandlers,        TerminalApp::ScrollDownEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, ScrollUpPage,      _ScrollUpPageHandlers,      TerminalApp::ScrollUpPageEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, ScrollDownPage,    _ScrollDownPageHandlers,    TerminalApp::ScrollDownPageEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, OpenSettings,      _OpenSettingsHandlers,      TerminalApp::OpenSettingsEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, ResizePane,        _ResizePaneHandlers,        TerminalApp::ResizePaneEventArgs);
    DEFINE_EVENT(ShortcutActionDispatch, MoveFocus,         _MoveFocusHandlers,         TerminalApp::MoveFocusEventArgs);
    // clang-format on
}
