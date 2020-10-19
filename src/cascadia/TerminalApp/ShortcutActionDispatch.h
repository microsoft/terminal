// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ShortcutActionDispatch.g.h"
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

        bool DoAction(const Microsoft::Terminal::Settings::Model::ActionAndArgs& actionAndArgs);

        // clang-format off
        TYPED_EVENT(CopyText,             TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(PasteText,            TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(OpenNewTabDropdown,   TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(DuplicateTab,         TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(NewTab,               TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(NewWindow,            TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(CloseWindow,          TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(CloseTab,             TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(ClosePane,            TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(SwitchToTab,          TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(NextTab,              TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(PrevTab,              TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(SendInput,            TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(SplitPane,            TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(TogglePaneZoom,       TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(AdjustFontSize,       TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(ResetFontSize,        TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(ScrollUp,             TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(ScrollDown,           TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(ScrollUpPage,         TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(ScrollDownPage,       TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(OpenSettings,         TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(ResizePane,           TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(Find,                 TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(MoveFocus,            TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(ToggleRetroEffect,    TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(ToggleFocusMode,      TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(ToggleFullscreen,     TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(ToggleAlwaysOnTop,    TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(ToggleCommandPalette, TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(SetColorScheme,       TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(SetTabColor,          TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(OpenTabColorPicker,   TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(RenameTab,            TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(ExecuteCommandline,   TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(CloseOtherTabs,       TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(CloseTabsAfter,       TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
        TYPED_EVENT(TabSearch,            TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);
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
