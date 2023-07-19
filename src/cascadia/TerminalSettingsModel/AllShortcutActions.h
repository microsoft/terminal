// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// For a clearer explanation of how this file should be used, see:
// https://en.wikipedia.org/wiki/X_Macro
//
// Include this file to be able to quickly define some code in the exact same
// way for _every single shortcut action_. To use:
//
// 1. Include this file
// 2. Define the ON_ALL_ACTIONS macro with what you want each action to show up
//    as. Ex:
//
//    #define ON_ALL_ACTIONS(action) void action##Handler();
//
// 3. Then, use the ALL_SHORTCUT_ACTIONS macro to get the ON_ALL_ACTIONS macro
//    repeated once for every ShortcutAction
//
// This is used in KeyMapping.idl, ShortcutAction.*, TerminalPage.*, etc. to
// reduce the number of places where we must copy-paste boiler-plate code for
// each action. This is _NOT_ something that should be used when any individual
// case should be customized.

#define ALL_SHORTCUT_ACTIONS                \
    ON_ALL_ACTIONS(CopyText)                \
    ON_ALL_ACTIONS(PasteText)               \
    ON_ALL_ACTIONS(OpenNewTabDropdown)      \
    ON_ALL_ACTIONS(DuplicateTab)            \
    ON_ALL_ACTIONS(NewTab)                  \
    ON_ALL_ACTIONS(CloseWindow)             \
    ON_ALL_ACTIONS(CloseTab)                \
    ON_ALL_ACTIONS(ClosePane)               \
    ON_ALL_ACTIONS(NextTab)                 \
    ON_ALL_ACTIONS(PrevTab)                 \
    ON_ALL_ACTIONS(SendInput)               \
    ON_ALL_ACTIONS(SplitPane)               \
    ON_ALL_ACTIONS(ToggleSplitOrientation)  \
    ON_ALL_ACTIONS(TogglePaneZoom)          \
    ON_ALL_ACTIONS(SwitchToTab)             \
    ON_ALL_ACTIONS(AdjustFontSize)          \
    ON_ALL_ACTIONS(ResetFontSize)           \
    ON_ALL_ACTIONS(ScrollUp)                \
    ON_ALL_ACTIONS(ScrollDown)              \
    ON_ALL_ACTIONS(ScrollUpPage)            \
    ON_ALL_ACTIONS(ScrollDownPage)          \
    ON_ALL_ACTIONS(ScrollToTop)             \
    ON_ALL_ACTIONS(ScrollToBottom)          \
    ON_ALL_ACTIONS(ScrollToMark)            \
    ON_ALL_ACTIONS(AddMark)                 \
    ON_ALL_ACTIONS(ClearMark)               \
    ON_ALL_ACTIONS(ClearAllMarks)           \
    ON_ALL_ACTIONS(ResizePane)              \
    ON_ALL_ACTIONS(MoveFocus)               \
    ON_ALL_ACTIONS(MovePane)                \
    ON_ALL_ACTIONS(SwapPane)                \
    ON_ALL_ACTIONS(Find)                    \
    ON_ALL_ACTIONS(ToggleShaderEffects)     \
    ON_ALL_ACTIONS(ToggleFocusMode)         \
    ON_ALL_ACTIONS(ToggleFullscreen)        \
    ON_ALL_ACTIONS(ToggleAlwaysOnTop)       \
    ON_ALL_ACTIONS(OpenSettings)            \
    ON_ALL_ACTIONS(SetFocusMode)            \
    ON_ALL_ACTIONS(SetFullScreen)           \
    ON_ALL_ACTIONS(SetMaximized)            \
    ON_ALL_ACTIONS(SetColorScheme)          \
    ON_ALL_ACTIONS(SetTabColor)             \
    ON_ALL_ACTIONS(OpenTabColorPicker)      \
    ON_ALL_ACTIONS(RenameTab)               \
    ON_ALL_ACTIONS(OpenTabRenamer)          \
    ON_ALL_ACTIONS(ExecuteCommandline)      \
    ON_ALL_ACTIONS(ToggleCommandPalette)    \
    ON_ALL_ACTIONS(CloseOtherTabs)          \
    ON_ALL_ACTIONS(CloseTabsAfter)          \
    ON_ALL_ACTIONS(TabSearch)               \
    ON_ALL_ACTIONS(MoveTab)                 \
    ON_ALL_ACTIONS(BreakIntoDebugger)       \
    ON_ALL_ACTIONS(TogglePaneReadOnly)      \
    ON_ALL_ACTIONS(EnablePaneReadOnly)      \
    ON_ALL_ACTIONS(DisablePaneReadOnly)     \
    ON_ALL_ACTIONS(FindMatch)               \
    ON_ALL_ACTIONS(NewWindow)               \
    ON_ALL_ACTIONS(IdentifyWindow)          \
    ON_ALL_ACTIONS(IdentifyWindows)         \
    ON_ALL_ACTIONS(RenameWindow)            \
    ON_ALL_ACTIONS(OpenWindowRenamer)       \
    ON_ALL_ACTIONS(DisplayWorkingDirectory) \
    ON_ALL_ACTIONS(SearchForText)           \
    ON_ALL_ACTIONS(GlobalSummon)            \
    ON_ALL_ACTIONS(QuakeMode)               \
    ON_ALL_ACTIONS(FocusPane)               \
    ON_ALL_ACTIONS(OpenSystemMenu)          \
    ON_ALL_ACTIONS(ExportBuffer)            \
    ON_ALL_ACTIONS(ClearBuffer)             \
    ON_ALL_ACTIONS(MultipleActions)         \
    ON_ALL_ACTIONS(Quit)                    \
    ON_ALL_ACTIONS(AdjustOpacity)           \
    ON_ALL_ACTIONS(RestoreLastClosed)       \
    ON_ALL_ACTIONS(SelectAll)               \
    ON_ALL_ACTIONS(SelectCommand)           \
    ON_ALL_ACTIONS(SelectOutput)            \
    ON_ALL_ACTIONS(MarkMode)                \
    ON_ALL_ACTIONS(ToggleBlockSelection)    \
    ON_ALL_ACTIONS(SwitchSelectionEndpoint) \
    ON_ALL_ACTIONS(ColorSelection)          \
    ON_ALL_ACTIONS(ShowContextMenu)         \
    ON_ALL_ACTIONS(ExpandSelectionToWord)   \
    ON_ALL_ACTIONS(CloseOtherPanes)         \
    ON_ALL_ACTIONS(RestartConnection)       \
    ON_ALL_ACTIONS(ToggleBroadcastInput)

#define ALL_SHORTCUT_ACTIONS_WITH_ARGS             \
    ON_ALL_ACTIONS_WITH_ARGS(AdjustFontSize)       \
    ON_ALL_ACTIONS_WITH_ARGS(CloseOtherTabs)       \
    ON_ALL_ACTIONS_WITH_ARGS(CloseTabsAfter)       \
    ON_ALL_ACTIONS_WITH_ARGS(CloseTab)             \
    ON_ALL_ACTIONS_WITH_ARGS(CopyText)             \
    ON_ALL_ACTIONS_WITH_ARGS(ExecuteCommandline)   \
    ON_ALL_ACTIONS_WITH_ARGS(FindMatch)            \
    ON_ALL_ACTIONS_WITH_ARGS(SearchForText)        \
    ON_ALL_ACTIONS_WITH_ARGS(GlobalSummon)         \
    ON_ALL_ACTIONS_WITH_ARGS(MoveFocus)            \
    ON_ALL_ACTIONS_WITH_ARGS(MovePane)             \
    ON_ALL_ACTIONS_WITH_ARGS(SwapPane)             \
    ON_ALL_ACTIONS_WITH_ARGS(MoveTab)              \
    ON_ALL_ACTIONS_WITH_ARGS(NewTab)               \
    ON_ALL_ACTIONS_WITH_ARGS(NewWindow)            \
    ON_ALL_ACTIONS_WITH_ARGS(NextTab)              \
    ON_ALL_ACTIONS_WITH_ARGS(OpenSettings)         \
    ON_ALL_ACTIONS_WITH_ARGS(SetFocusMode)         \
    ON_ALL_ACTIONS_WITH_ARGS(SetFullScreen)        \
    ON_ALL_ACTIONS_WITH_ARGS(SetMaximized)         \
    ON_ALL_ACTIONS_WITH_ARGS(PrevTab)              \
    ON_ALL_ACTIONS_WITH_ARGS(RenameTab)            \
    ON_ALL_ACTIONS_WITH_ARGS(RenameWindow)         \
    ON_ALL_ACTIONS_WITH_ARGS(ResizePane)           \
    ON_ALL_ACTIONS_WITH_ARGS(ScrollDown)           \
    ON_ALL_ACTIONS_WITH_ARGS(ScrollUp)             \
    ON_ALL_ACTIONS_WITH_ARGS(ScrollToMark)         \
    ON_ALL_ACTIONS_WITH_ARGS(AddMark)              \
    ON_ALL_ACTIONS_WITH_ARGS(SendInput)            \
    ON_ALL_ACTIONS_WITH_ARGS(SetColorScheme)       \
    ON_ALL_ACTIONS_WITH_ARGS(SetTabColor)          \
    ON_ALL_ACTIONS_WITH_ARGS(SplitPane)            \
    ON_ALL_ACTIONS_WITH_ARGS(SwitchToTab)          \
    ON_ALL_ACTIONS_WITH_ARGS(ToggleCommandPalette) \
    ON_ALL_ACTIONS_WITH_ARGS(FocusPane)            \
    ON_ALL_ACTIONS_WITH_ARGS(ExportBuffer)         \
    ON_ALL_ACTIONS_WITH_ARGS(ClearBuffer)          \
    ON_ALL_ACTIONS_WITH_ARGS(MultipleActions)      \
    ON_ALL_ACTIONS_WITH_ARGS(AdjustOpacity)        \
    ON_ALL_ACTIONS_WITH_ARGS(SelectCommand)        \
    ON_ALL_ACTIONS_WITH_ARGS(SelectOutput)         \
    ON_ALL_ACTIONS_WITH_ARGS(ColorSelection)
