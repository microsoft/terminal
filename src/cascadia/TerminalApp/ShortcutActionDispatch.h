// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ShortcutActionDispatch.g.h"
#include "..\inc\cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct ShortcutActionDispatch : ShortcutActionDispatchT<ShortcutActionDispatch>
    {
        ShortcutActionDispatch() = default;

        bool DoAction(ShortcutAction action);

        // clang-format off
        DECLARE_EVENT(CopyText,          _CopyTextHandlers,          TerminalApp::CopyTextEventArgs);
        DECLARE_EVENT(PasteText,         _PasteTextHandlers,         TerminalApp::PasteTextEventArgs);
        DECLARE_EVENT(NewTab,            _NewTabHandlers,            TerminalApp::NewTabEventArgs);
        DECLARE_EVENT(DuplicateTab,      _DuplicateTabHandlers,      TerminalApp::DuplicateTabEventArgs);
        DECLARE_EVENT(NewTabWithProfile, _NewTabWithProfileHandlers, TerminalApp::NewTabWithProfileEventArgs);
        DECLARE_EVENT(NewWindow,         _NewWindowHandlers,         TerminalApp::NewWindowEventArgs);
        DECLARE_EVENT(CloseWindow,       _CloseWindowHandlers,       TerminalApp::CloseWindowEventArgs);
        DECLARE_EVENT(CloseTab,          _CloseTabHandlers,          TerminalApp::CloseTabEventArgs);
        DECLARE_EVENT(ClosePane,         _ClosePaneHandlers,         TerminalApp::ClosePaneEventArgs);
        DECLARE_EVENT(SwitchToTab,       _SwitchToTabHandlers,       TerminalApp::SwitchToTabEventArgs);
        DECLARE_EVENT(NextTab,           _NextTabHandlers,           TerminalApp::NextTabEventArgs);
        DECLARE_EVENT(PrevTab,           _PrevTabHandlers,           TerminalApp::PrevTabEventArgs);
        DECLARE_EVENT(SplitVertical,     _SplitVerticalHandlers,     TerminalApp::SplitVerticalEventArgs);
        DECLARE_EVENT(SplitHorizontal,   _SplitHorizontalHandlers,   TerminalApp::SplitHorizontalEventArgs);
        DECLARE_EVENT(IncreaseFontSize,  _IncreaseFontSizeHandlers,  TerminalApp::IncreaseFontSizeEventArgs);
        DECLARE_EVENT(DecreaseFontSize,  _DecreaseFontSizeHandlers,  TerminalApp::DecreaseFontSizeEventArgs);
        DECLARE_EVENT(ScrollUp,          _ScrollUpHandlers,          TerminalApp::ScrollUpEventArgs);
        DECLARE_EVENT(ScrollDown,        _ScrollDownHandlers,        TerminalApp::ScrollDownEventArgs);
        DECLARE_EVENT(ScrollUpPage,      _ScrollUpPageHandlers,      TerminalApp::ScrollUpPageEventArgs);
        DECLARE_EVENT(ScrollDownPage,    _ScrollDownPageHandlers,    TerminalApp::ScrollDownPageEventArgs);
        DECLARE_EVENT(OpenSettings,      _OpenSettingsHandlers,      TerminalApp::OpenSettingsEventArgs);
        DECLARE_EVENT(ResizePane,        _ResizePaneHandlers,        TerminalApp::ResizePaneEventArgs);
        DECLARE_EVENT(MoveFocus,         _MoveFocusHandlers,         TerminalApp::MoveFocusEventArgs);
        // clang-format on
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct ShortcutActionDispatch : ShortcutActionDispatchT<ShortcutActionDispatch, implementation::ShortcutActionDispatch>
    {
    };
}
