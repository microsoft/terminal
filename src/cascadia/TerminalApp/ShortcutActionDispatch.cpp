// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ShortcutActionDispatch.h"

#include "ShortcutActionDispatch.g.cpp"

using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::TerminalApp;

#define ACTION_CASE(action)                    \
    case ShortcutAction::action:               \
    {                                          \
        _##action##Handlers(*this, eventArgs); \
        break;                                 \
    }

namespace winrt::TerminalApp::implementation
{
    // Method Description:
    // - Dispatch the appropriate event for the given ActionAndArgs. Constructs
    //   an ActionEventArgs to hold the IActionArgs payload for the event, and
    //   calls the matching handlers for that event.
    // Arguments:
    // - actionAndArgs: the ShortcutAction and associated args to raise an event for.
    // Return Value:
    // - true if we handled the event was handled, else false.
    bool ShortcutActionDispatch::DoAction(const ActionAndArgs& actionAndArgs)
    {
        if (!actionAndArgs)
        {
            return false;
        }

        const auto& action = actionAndArgs.Action();
        const auto& args = actionAndArgs.Args();
        auto eventArgs = args ? ActionEventArgs{ args } :
                                ActionEventArgs{};

        switch (action)
        {
            ACTION_CASE(CopyText);
            ACTION_CASE(PasteText);
            ACTION_CASE(OpenNewTabDropdown);
            ACTION_CASE(DuplicateTab);
            ACTION_CASE(OpenSettings);
            ACTION_CASE(NewTab);
            ACTION_CASE(CloseWindow);
            ACTION_CASE(CloseTab);
            ACTION_CASE(ClosePane);
            ACTION_CASE(ScrollUp);
            ACTION_CASE(ScrollDown);
            ACTION_CASE(ScrollUpPage);
            ACTION_CASE(ScrollDownPage);
            ACTION_CASE(ScrollToTop);
            ACTION_CASE(ScrollToBottom);
            ACTION_CASE(NextTab);
            ACTION_CASE(PrevTab);
            ACTION_CASE(SendInput);

        case ShortcutAction::SplitVertical:
        case ShortcutAction::SplitHorizontal:
        case ShortcutAction::SplitPane:
        {
            _SplitPaneHandlers(*this, eventArgs);
            break;
        }

            ACTION_CASE(TogglePaneZoom);
            ACTION_CASE(SwitchToTab);
            ACTION_CASE(ResizePane);
            ACTION_CASE(MoveFocus);
            ACTION_CASE(AdjustFontSize);
            ACTION_CASE(Find);
            ACTION_CASE(ResetFontSize);
            ACTION_CASE(ToggleShaderEffects);
            ACTION_CASE(ToggleFocusMode);
            ACTION_CASE(ToggleFullscreen);
            ACTION_CASE(ToggleAlwaysOnTop);
            ACTION_CASE(ToggleCommandPalette);
            ACTION_CASE(SetColorScheme);
            ACTION_CASE(SetTabColor);
            ACTION_CASE(OpenTabColorPicker);
            ACTION_CASE(RenameTab);
            ACTION_CASE(OpenTabRenamer);
            ACTION_CASE(ExecuteCommandline);
            ACTION_CASE(CloseOtherTabs);
            ACTION_CASE(CloseTabsAfter);
            ACTION_CASE(MoveTab);
            ACTION_CASE(TabSearch);
            ACTION_CASE(BreakIntoDebugger);
            ACTION_CASE(FindMatch);
            ACTION_CASE(TogglePaneReadOnly);
            ACTION_CASE(NewWindow);
            ACTION_CASE(IdentifyWindow);
            ACTION_CASE(IdentifyWindows);
        default:
            return false;
        }
        return eventArgs.Handled();
    }

}
