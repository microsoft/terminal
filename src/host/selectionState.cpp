// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../interactivity/inc/ServiceLocator.hpp"
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
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
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
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
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
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
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

    return (_d->fLineSelection != _d->fUseAlternateSelection);
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
        _d.write()->fUseAlternateSelection = !_d->fLineSelection;
    }
    else
    {
        // states are the same when in box selection.
        // e.g. Line = true, Alternate = true.
        // and  Line = false, Alternate = false.
        _d.write()->fUseAlternateSelection = _d->fLineSelection;
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
    return WI_IsFlagSet(_d->dwSelectionFlags, CONSOLE_SELECTION_NOT_EMPTY);
}

// Routine Description:
// - Determines whether mark mode specifically started this selection.
// Arguments:
// - <none>
// Return Value:
// - True if the selection was started as mark mode. False otherwise.
bool Selection::IsKeyboardMarkSelection() const
{
    return WI_IsFlagClear(_d->dwSelectionFlags, CONSOLE_MOUSE_SELECTION);
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
    return WI_IsFlagSet(_d->dwSelectionFlags, CONSOLE_MOUSE_SELECTION);
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
    return WI_IsFlagSet(_d->dwSelectionFlags, CONSOLE_MOUSE_DOWN);
}

void Selection::MouseDown()
{
    WI_SetFlag(_d.write()->dwSelectionFlags, CONSOLE_MOUSE_DOWN);

    // We must capture the mouse on button down to ensure we receive messages if
    //      it comes back up outside the window.
    const auto pWindow = ServiceLocator::LocateConsoleWindow();
    if (pWindow != nullptr)
    {
        pWindow->CaptureMouse();
    }
}

void Selection::MouseUp()
{
    WI_ClearFlag(_d.write()->dwSelectionFlags, CONSOLE_MOUSE_DOWN);

    const auto pWindow = ServiceLocator::LocateConsoleWindow();
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
    auto d{ _d.write() };
    d->coordSavedCursorPosition = cursor.GetPosition();
    d->ulSavedCursorSize = cursor.GetSize();
    d->fSavedCursorVisible = cursor.IsVisible();
    d->savedCursorType = cursor.GetType();
}

// Routine Description:
// - Restores the cursor position data that was captured when selection was started.
// Arguments:
// - <none> (Restores global state)
// Return Value:
// - <none>
void Selection::_RestoreDataToCursor(Cursor& cursor) noexcept
{
    cursor.SetSize(_d->ulSavedCursorSize);
    cursor.SetIsVisible(_d->fSavedCursorVisible);
    cursor.SetType(_d->savedCursorType);
    cursor.SetPosition(_d->coordSavedCursorPosition);
}

// Routine Description:
// - Gets the current selection anchor position
// Arguments:
// - none
// Return Value:
// - current selection anchor
til::point Selection::GetSelectionAnchor() const noexcept
{
    return _d->coordSelectionAnchor;
}

// Routine Description:
// - Gets the current selection begin and end (inclusive) anchor positions. The
//   first anchor is at the top left, and the second is at the bottom right
//   corner of the selection area.
// Return Value:
// - The current selection anchors
std::pair<til::point, til::point> Selection::GetSelectionAnchors() const noexcept
{
    if (!_d->fSelectionVisible)
    {
        // return anchors that represent an empty selection
        return { { 0, 0 }, { -1, -1 } };
    }

    auto startSelectionAnchor = _d->coordSelectionAnchor;

    // _coordSelectionAnchor is at one of the corners of _srSelectionRects
    // endSelectionAnchor is at the exact opposite corner
    til::point endSelectionAnchor;
    endSelectionAnchor.x = (_d->coordSelectionAnchor.x == _d->srSelectionRect.left) ? _d->srSelectionRect.right : _d->srSelectionRect.left;
    endSelectionAnchor.y = (_d->coordSelectionAnchor.y == _d->srSelectionRect.top) ? _d->srSelectionRect.bottom : _d->srSelectionRect.top;

    // GH #18106: Conhost and Terminal share most of the selection code.
    //    Both now store the selection data as a half-open range [start, end),
    //     where "end" is the bottom-right-most point.
    // Conhost operates as an inclusive range, so we need to adjust the "end" endpoint by incrementing it by one.
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& bufferSize = gci.GetActiveOutputBuffer().GetTextBuffer().GetSize();
    if (IsLineSelection())
    {
        // General comparison for line selection.
        bufferSize.IncrementInExclusiveBounds(startSelectionAnchor <= endSelectionAnchor ? endSelectionAnchor : startSelectionAnchor);
    }
    else
    {
        // Compare x-values when we're in block selection!
        bufferSize.IncrementInExclusiveBounds(startSelectionAnchor.x <= endSelectionAnchor.x ? endSelectionAnchor : startSelectionAnchor);
    }

    if (startSelectionAnchor > endSelectionAnchor)
    {
        return { endSelectionAnchor, startSelectionAnchor };
    }
    else
    {
        return { startSelectionAnchor, endSelectionAnchor };
    }
}

// Routine Description:
// - Gets the current selection rectangle
// Arguments:
// - none
// Return Value:
// - The rectangle to fill with selection data.
til::inclusive_rect Selection::GetSelectionRectangle() const noexcept
{
    return _d->srSelectionRect;
}

// Routine Description:
// - Gets the publicly facing set of selection flags.
//   Strips out any internal flags in use.
// Arguments:
// - none
// Return Value:
// - The public selection flags
DWORD Selection::GetPublicSelectionFlags() const noexcept
{
    // CONSOLE_SELECTION_VALID is the union (binary OR) of all externally valid flags in wincon.h
    return (_d->dwSelectionFlags & CONSOLE_SELECTION_VALID);
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
    if (_d->fLineSelection != fLineSelectionOn)
    {
        // Ensure any existing selections are cleared so the draw state is updated appropriately.
        ClearSelection();

        _d.write()->fLineSelection = fLineSelectionOn;
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
bool Selection::ShouldAllowMouseDragSelection(const til::point mousePosition) const noexcept
{
    const auto viewport = Viewport::FromInclusive(_d->srSelectionRect);
    const auto selectionContainsMouse = viewport.IsInBounds(mousePosition);
    return _d->allowMouseDragSelection || !selectionContainsMouse;
}
