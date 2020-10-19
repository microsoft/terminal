// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ShortcutActionDispatch.h"

#include "ShortcutActionDispatch.g.cpp"

using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Settings::Model;
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
        auto eventArgs = args ? ActionEventArgs{ args } :
                                ActionEventArgs{};

        switch (action)
        {
        case ShortcutAction::CopyText:
        {
            _CopyTextHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::PasteText:
        {
            _PasteTextHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::OpenNewTabDropdown:
        {
            _OpenNewTabDropdownHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::DuplicateTab:
        {
            _DuplicateTabHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::OpenSettings:
        {
            _OpenSettingsHandlers(*this, eventArgs);
            break;
        }

        case ShortcutAction::NewTab:
        {
            _NewTabHandlers(*this, eventArgs);
            break;
        }

        case ShortcutAction::NewWindow:
        {
            _NewWindowHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::CloseWindow:
        {
            _CloseWindowHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::CloseTab:
        {
            _CloseTabHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::ClosePane:
        {
            _ClosePaneHandlers(*this, eventArgs);
            break;
        }

        case ShortcutAction::ScrollUp:
        {
            _ScrollUpHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::ScrollDown:
        {
            _ScrollDownHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::ScrollUpPage:
        {
            _ScrollUpPageHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::ScrollDownPage:
        {
            _ScrollDownPageHandlers(*this, eventArgs);
            break;
        }

        case ShortcutAction::NextTab:
        {
            _NextTabHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::PrevTab:
        {
            _PrevTabHandlers(*this, eventArgs);
            break;
        }

        case ShortcutAction::SendInput:
        {
            _SendInputHandlers(*this, eventArgs);
            break;
        }

        case ShortcutAction::SplitVertical:
        case ShortcutAction::SplitHorizontal:
        case ShortcutAction::SplitPane:
        {
            _SplitPaneHandlers(*this, eventArgs);
            break;
        }

        case ShortcutAction::TogglePaneZoom:
        {
            _TogglePaneZoomHandlers(*this, eventArgs);
            break;
        }

        case ShortcutAction::SwitchToTab:
        {
            _SwitchToTabHandlers(*this, eventArgs);
            break;
        }

        case ShortcutAction::ResizePane:
        {
            _ResizePaneHandlers(*this, eventArgs);
            break;
        }

        case ShortcutAction::MoveFocus:
        {
            _MoveFocusHandlers(*this, eventArgs);
            break;
        }

        case ShortcutAction::AdjustFontSize:
        {
            _AdjustFontSizeHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::Find:
        {
            _FindHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::ResetFontSize:
        {
            _ResetFontSizeHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::ToggleRetroEffect:
        {
            _ToggleRetroEffectHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::ToggleFocusMode:
        {
            _ToggleFocusModeHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::ToggleFullscreen:
        {
            _ToggleFullscreenHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::ToggleAlwaysOnTop:
        {
            _ToggleAlwaysOnTopHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::ToggleCommandPalette:
        {
            _ToggleCommandPaletteHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::SetColorScheme:
        {
            _SetColorSchemeHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::SetTabColor:
        {
            _SetTabColorHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::OpenTabColorPicker:
        {
            _OpenTabColorPickerHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::RenameTab:
        {
            _RenameTabHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::ExecuteCommandline:
        {
            _ExecuteCommandlineHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::CloseOtherTabs:
        {
            _CloseOtherTabsHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::CloseTabsAfter:
        {
            _CloseTabsAfterHandlers(*this, eventArgs);
            break;
        }
        case ShortcutAction::TabSearch:
        {
            _TabSearchHandlers(*this, eventArgs);
            break;
        }
        default:
            return false;
        }
        return eventArgs.Handled();
    }

}
