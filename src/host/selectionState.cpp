// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "..\interactivity\inc\ServiceLocator.hpp"

#include "../types/inc/viewport.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity;

// Routine Description:
// - Determines whether the console is in a selecting state
// Arguments:
// - <none> (gets global state)
// Return Value:
// - True if the console is in a selecting state. False otherwise.
bool Selection::IsInSelectingState() const
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return WI_IsFlagSet(gci.Flags, CONSOLE_SELECTING);
}

// Routine Description:
// - Helps set the global selecting state.
// Arguments:
// - fSelectingOn - Set true to set the global flag on. False to turn the global flag off.
// Return Value:
// - <none>
void Selection::_SetSelectingState(const bool fSelectingOn)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    WI_UpdateFlag(gci.Flags, CONSOLE_SELECTING, fSelectingOn);
}

// Routine Description:
// - Determines whether the console should do selections with the mouse
//   a.k.a. "Quick Edit" mode
// Arguments:
// - <none> (gets global state)
// Return Value:
// - True if quick edit mode is enabled. False otherwise.
bool Selection::IsInQuickEditMode() const
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return WI_IsFlagSet(gci.Flags, CONSOLE_QUICK_EDIT_MODE);
}

// Routine Description:
// - Determines whether we are performing a line selection right now
// Arguments:
// - <none>
// Return Value:
// - True if the selection is to be treated line by line. False if it is to be a block.
bool Selection::IsLineSelection() const
{
    // if line selection is on and alternate is off -OR-
    // if line selection is off and alternate is on...

    return (_fLineSelection != _fUseAlternateSelection);
}

// Routine Description:
// - Assures that the alternate selection flag is flipped in line with the requested format.
//   If true, we'll align to ensure line selection is used. If false, we'll make sure box selection is used.
// Arguments:
// - fAlignToLineSelect - whether or not to use line selection
// Return Value:
// - <none>
void Selection::_AlignAlternateSelection(const bool fAlignToLineSelect)
{
    if (fAlignToLineSelect)
    {
        // states are opposite when in line selection.
        // e.g. Line = true, Alternate = false.
        // and  Line = false, Alternate = true.
        _fUseAlternateSelection = !_fLineSelection;
    }
    else
    {
        // states are the same when in box selection.
        // e.g. Line = true, Alternate = true.
        // and  Line = false, Alternate = false.
        _fUseAlternateSelection = _fLineSelection;
    }
}

// Routine Description:
// - Determines whether the selection area is empty.
// Arguments:
// - <none>
// Return Value:
// - True if the selection variables contain valid selection data. False otherwise.
bool Selection::IsAreaSelected() const
{
    return WI_IsFlagSet(_dwSelectionFlags, CONSOLE_SELECTION_NOT_EMPTY);
}

// Routine Description:
// - Determines whether mark mode specifically started this selction.
// Arguments:
// - <none>
// Return Value:
// - True if the selection was started as mark mode. False otherwise.
bool Selection::IsKeyboardMarkSelection() const
{
    return WI_IsFlagClear(_dwSelectionFlags, CONSOLE_MOUSE_SELECTION);
}

// Routine Description:
// - Determines whether a mouse event was responsible for initiating this selection.
// - This primarily refers to mouse drag in QuickEdit mode.
// - However, it refers to any non-mark-mode selection, whether the mouse actually started it or not.
// Arguments:
// - <none>
// Return Value:
// - True if the selection is mouse-initiated. False otherwise.
bool Selection::IsMouseInitiatedSelection() const
{
    return WI_IsFlagSet(_dwSelectionFlags, CONSOLE_MOUSE_SELECTION);
}

// Routine Description:
// - Determines whether the mouse button is currently being held down
//   to extend or otherwise manipulate the selection area.
// Arguments:
// - <none>
// Return Value:
// - True if the mouse button is currently down. False otherwise.
bool Selection::IsMouseButtonDown() const
{
    return WI_IsFlagSet(_dwSelectionFlags, CONSOLE_MOUSE_DOWN);
}

void Selection::MouseDown()
{
    WI_SetFlag(_dwSelectionFlags, CONSOLE_MOUSE_DOWN);

    // We must capture the mouse on button down to ensure we receive messages if
    //      it comes back up outside the window.
    IConsoleWindow* const pWindow = ServiceLocator::LocateConsoleWindow();
    if (pWindow != nullptr)
    {
        pWindow->CaptureMouse();
    }
}

void Selection::MouseUp()
{
    WI_ClearFlag(_dwSelectionFlags, CONSOLE_MOUSE_DOWN);

    IConsoleWindow* const pWindow = ServiceLocator::LocateConsoleWindow();
    if (pWindow != nullptr)
    {
        pWindow->ReleaseMouse();
    }
}

// Routine Description:
// - Saves the current cursor position data so it can be manipulated during selection.
// Arguments:
// - textBuffer - text buffer to set cursor data
// Return Value:
// - <none>
void Selection::_SaveCursorData(const Cursor& cursor) noexcept
{
    _coordSavedCursorPosition = cursor.GetPosition();
    _ulSavedCursorSize = cursor.GetSize();
    _fSavedCursorVisible = cursor.IsVisible();
    _savedCursorColor = cursor.GetColor();
    _savedCursorType = cursor.GetType();
}

// Routine Description:
// - Restores the cursor position data that was captured when selection was started.
// Arguments:
// - <none> (Restores global state)
// Return Value:
// - <none>
void Selection::_RestoreDataToCursor(Cursor& cursor) noexcept
{
    cursor.SetSize(_ulSavedCursorSize);
    cursor.SetIsVisible(_fSavedCursorVisible);
    cursor.SetColor(_savedCursorColor);
    cursor.SetType(_savedCursorType);
    cursor.SetIsOn(true);
    cursor.SetPosition(_coordSavedCursorPosition);
}

// Routine Description:
// - Gets the current selection anchor position
// Arguments:
// - none
// Return Value:
// - current selection anchor
COORD Selection::GetSelectionAnchor() const noexcept
{
    return _coordSelectionAnchor;
}

// Routine Description:
// - Gets the current selection rectangle
// Arguments:
// - none
// Return Value:
// - The rectangle to fill with selection data.
SMALL_RECT Selection::GetSelectionRectangle() const noexcept
{
    return _srSelectionRect;
}

// Routine Description:
// - Gets the publically facing set of selection flags.
//   Strips out any internal flags in use.
// Arguments:
// - none
// Return Value:
// - The public selection flags
DWORD Selection::GetPublicSelectionFlags() const noexcept
{
    // CONSOLE_SELECTION_VALID is the union (binary OR) of all externally valid flags in wincon.h
    return (_dwSelectionFlags & CONSOLE_SELECTION_VALID);
}

// Routine Description:
// - Sets the line selection status.
//   If true, we'll use line selection. If false, we'll use traditional box selection.
// Arguments:
// - fLineSelectionOn - whether or not to use line selection
// Return Value:
// - <none>
void Selection::SetLineSelection(const bool fLineSelectionOn)
{
    if (_fLineSelection != fLineSelectionOn)
    {
        // Ensure any existing selections are cleared so the draw state is updated appropriately.
        ClearSelection();

        _fLineSelection = fLineSelectionOn;
    }
}

// Routine Description:
// - checks if the selection can be changed by a mouse drag.
// - this is to allow double-click selection and click-mouse-drag selection to play nice together instead of
// the click-mouse-drag selection overwriting the double-click selection in case the user moves the mouse
// while double-clicking.
// Arguments:
// - mousePosition - current mouse position
// Return Value:
// - true if the selection can be changed by a mouse drag
bool Selection::ShouldAllowMouseDragSelection(const COORD mousePosition) const noexcept
{
    const Viewport viewport = Viewport::FromInclusive(_srSelectionRect);
    const bool selectionContainsMouse = viewport.IsInBounds(mousePosition);
    return _allowMouseDragSelection || !selectionContainsMouse;
}
