// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ShortcutActionDispatch.g.h"
#include "../inc/cppwinrt_utils.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
    class KeyBindingsTests;
}

#define DECLARE_ACTION(action) TYPED_EVENT(action, TerminalApp::ShortcutActionDispatch, Microsoft::Terminal::Settings::Model::ActionEventArgs);

namespace winrt::TerminalApp::implementation
{
    struct ShortcutActionDispatch : ShortcutActionDispatchT<ShortcutActionDispatch>
    {
        ShortcutActionDispatch() = default;

        bool DoAction(const Microsoft::Terminal::Settings::Model::ActionAndArgs& actionAndArgs);

        // clang-format off
        DECLARE_ACTION(CopyText);
        DECLARE_ACTION(PasteText);
        DECLARE_ACTION(OpenNewTabDropdown);
        DECLARE_ACTION(DuplicateTab);
        DECLARE_ACTION(NewTab);
        DECLARE_ACTION(CloseWindow);
        DECLARE_ACTION(CloseTab);
        DECLARE_ACTION(ClosePane);
        DECLARE_ACTION(SwitchToTab);
        DECLARE_ACTION(NextTab);
        DECLARE_ACTION(PrevTab);
        DECLARE_ACTION(SendInput);
        DECLARE_ACTION(SplitPane);
        DECLARE_ACTION(TogglePaneZoom);
        DECLARE_ACTION(AdjustFontSize);
        DECLARE_ACTION(ResetFontSize);
        DECLARE_ACTION(ScrollUp);
        DECLARE_ACTION(ScrollDown);
        DECLARE_ACTION(ScrollUpPage);
        DECLARE_ACTION(ScrollDownPage);
        DECLARE_ACTION(ScrollToTop);
        DECLARE_ACTION(ScrollToBottom);
        DECLARE_ACTION(OpenSettings);
        DECLARE_ACTION(ResizePane);
        DECLARE_ACTION(Find);
        DECLARE_ACTION(MoveFocus);
        DECLARE_ACTION(ToggleShaderEffects);
        DECLARE_ACTION(ToggleFocusMode);
        DECLARE_ACTION(ToggleFullscreen);
        DECLARE_ACTION(ToggleAlwaysOnTop);
        DECLARE_ACTION(ToggleCommandPalette);
        DECLARE_ACTION(SetColorScheme);
        DECLARE_ACTION(SetTabColor);
        DECLARE_ACTION(OpenTabColorPicker);
        DECLARE_ACTION(RenameTab);
        DECLARE_ACTION(OpenTabRenamer);
        DECLARE_ACTION(ExecuteCommandline);
        DECLARE_ACTION(CloseOtherTabs);
        DECLARE_ACTION(CloseTabsAfter);
        DECLARE_ACTION(TabSearch);
        DECLARE_ACTION(MoveTab);
        DECLARE_ACTION(BreakIntoDebugger);
        DECLARE_ACTION(FindMatch);
        DECLARE_ACTION(TogglePaneReadOnly);
        DECLARE_ACTION(NewWindow);
        DECLARE_ACTION(IdentifyWindow);
        DECLARE_ACTION(IdentifyWindows);
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
