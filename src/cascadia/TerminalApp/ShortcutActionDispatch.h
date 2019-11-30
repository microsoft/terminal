// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ShortcutActionDispatch.g.h"
#include "ActionArgs.h"
#include "..\inc\cppwinrt_utils.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
    class KeyBindingsTests;
}

namespace winrt::TerminalApp::implementation
{
    struct ShortcutActionDispatch : ShortcutActionDispatchT<ShortcutActionDispatch>
    {
        ShortcutActionDispatch() = default;

        bool DoAction(const ActionAndArgs& actionAndArgs);

        // clang-format off
        TYPED_EVENT(CopyText,          TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(PasteText,         TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(OpenNewTabDropdown,TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(DuplicateTab,      TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(NewTab,            TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(NewWindow,         TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(CloseWindow,       TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(CloseTab,          TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(ClosePane,         TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(SwitchToTab,       TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(NextTab,           TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(PrevTab,           TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(SplitPane,         TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(AdjustFontSize,    TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(ResetFontSize,     TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(ScrollUp,          TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(ScrollDown,        TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(ScrollUpPage,      TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(ScrollDownPage,    TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(OpenSettings,      TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(ResizePane,        TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(MoveFocus,         TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        TYPED_EVENT(ToggleFullscreen,  TerminalApp::ShortcutActionDispatch, TerminalApp::ActionEventArgs);
        // clang-format on

    private:
        friend class TerminalAppLocalTests::SettingsTests;
        friend class TerminalAppLocalTests::KeyBindingsTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(ShortcutActionDispatch);
}
