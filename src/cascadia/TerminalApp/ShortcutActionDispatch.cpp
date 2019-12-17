// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ShortcutActionDispatch.h"

#include "ShortcutActionDispatch.g.cpp"

using namespace winrt::Microsoft::Terminal;
using namespace winrt::TerminalApp;

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
        case ShortcutAction::SplitHorizontal:
        case ShortcutAction::SplitPane:
        {
            _SplitPaneHandlers(*this, *eventArgs);
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
        case ShortcutAction::ResetFontSize:
        {
            _ResetFontSizeHandlers(*this, *eventArgs);
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

}
