// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "_output.h"
#include "stream.h"
#include "scrolling.hpp"

#include "../interactivity/inc/ServiceLocator.hpp"

using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::Types;

Selection::Selection() :
    _fSelectionVisible(false),
    _ulSavedCursorSize(0),
    _fSavedCursorVisible(false),
    _savedCursorColor(INVALID_COLOR),
    _savedCursorType(CursorType::Legacy),
    _dwSelectionFlags(0),
    _fLineSelection(true),
    _fUseAlternateSelection(false),
    _allowMouseDragSelection{ true }
{
    ZeroMemory((void*)&_srSelectionRect, sizeof(_srSelectionRect));
    ZeroMemory((void*)&_coordSelectionAnchor, sizeof(_coordSelectionAnchor));
    ZeroMemory((void*)&_coordSavedCursorPosition, sizeof(_coordSavedCursorPosition));
}

Selection& Selection::Instance()
{
    static std::unique_ptr<Selection> _instance{ new Selection() };
    return *_instance;
}

// Routine Description:
// - Detemines the line-by-line selection rectangles based on global selection state.
// Arguments:
// - selectionRect - The selection rectangle outlining the region to be selected
// - selectionAnchor - The corner of the selection rectangle that selection started from
// - lineSelection - True to process in line mode. False to process in block mode.
// Return Value:
// - Returns a vector where each SMALL_RECT is one Row worth of the area to be selected.
// - Returns empty vector if no rows are selected.
// - Throws exceptions for out of memory issues
std::vector<SMALL_RECT> Selection::s_GetSelectionRects(const SMALL_RECT& selectionRect,
                                                       const COORD selectionAnchor,
                                                       const bool lineSelection)
{
    std::vector<SMALL_RECT> selectionAreas;

    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& screenInfo = gci.GetActiveOutputBuffer();

    // if the anchor (start of select) was in the top right or bottom left of the box,
    // we need to remove rectangular overlap in the middle.
    // e.g.
    // For selections with the anchor in the top left (A) or bottom right (B),
    // it is valid to maintain the inner rectangle (+) as part of the selection
    //               A+++++++================
    // ==============++++++++B
    // + and = are valid highlights in this scenario.
    // For selections with the anchor in in the top right (A) or bottom left (B),
    // we must remove a portion of the first/last line that lies within the rectangle (+)
    //               +++++++A=================
    // ==============B+++++++
    // Only = is valid for highlight in this scenario.
    // This is only needed for line selection. Box selection doesn't need to account for this.

    bool removeRectPortion = false;

    if (lineSelection)
    {
        const auto selectionStart = selectionAnchor;

        // only if top and bottom aren't the same line... we need the whole rectangle if we're on the same line.
        // e.g.         A++++++++++++++B
        // All the + are valid select points.
        if (selectionRect.Top != selectionRect.Bottom)
        {
            if ((selectionStart.X == selectionRect.Right && selectionStart.Y == selectionRect.Top) ||
                (selectionStart.X == selectionRect.Left && selectionStart.Y == selectionRect.Bottom))
            {
                removeRectPortion = true;
            }
        }
    }

    // for each row within the selection rectangle
    for (short i = selectionRect.Top; i <= selectionRect.Bottom; i++)
    {
        // create a rectangle representing the highlight on one row
        SMALL_RECT highlightRow;
        highlightRow.Top = i;
        highlightRow.Bottom = i;
        highlightRow.Left = selectionRect.Left;
        highlightRow.Right = selectionRect.Right;

        // compensate for line selection by extending one or both ends of the rectangle to the edge
        if (lineSelection)
        {
            // if not the first row, pad the left selection to the buffer edge
            if (i != selectionRect.Top)
            {
                highlightRow.Left = 0;
            }

            // if not the last row, pad the right selection to the buffer edge
            if (i != selectionRect.Bottom)
            {
                highlightRow.Right = screenInfo.GetBufferSize().RightInclusive();
            }

            // if we've determined we're in a scenario where we must remove the inner rectangle from the lines...
            if (removeRectPortion)
            {
                if (i == selectionRect.Top)
                {
                    // from the top row, move the left edge of the highlight line to the right edge of the rectangle
                    highlightRow.Left = selectionRect.Right;
                }
                else if (i == selectionRect.Bottom)
                {
                    // from the bottom row, move the right edge of the highlight line to the left edge of the rectangle
                    highlightRow.Right = selectionRect.Left;
                }
            }
        }

        // compensate for double width characters by calling double-width measuring/limiting function
        const COORD targetPoint{ highlightRow.Left, highlightRow.Top };
        const SHORT stringLength = highlightRow.Right - highlightRow.Left + 1;
        highlightRow = s_BisectSelection(stringLength, targetPoint, screenInfo, highlightRow);

        selectionAreas.emplace_back(highlightRow);
    }

    return selectionAreas;
}

// Routine Description:
// - Detemines the line-by-line selection rectangles based on global selection state.
// Arguments:
// - <none> - Uses internal state to know what area is selected already.
// Return Value:
// - Returns a vector where each SMALL_RECT is one Row worth of the area to be selected.
// - Returns empty vector if no rows are selected.
// - Throws exceptions for out of memory issues
std::vector<SMALL_RECT> Selection::GetSelectionRects() const
{
    if (!_fSelectionVisible)
    {
        return std::vector<SMALL_RECT>();
    }

    return s_GetSelectionRects(_srSelectionRect, _coordSelectionAnchor, IsLineSelection());
}

// Routine Description:
// - This routine checks to ensure that clipboard selection isn't trying to cut a double byte character in half.
//   It will adjust the SmallRect rectangle size to ensure this.
// Arguments:
// - sStringLength - The length of the string we're attempting to clip.
// - coordTargetPoint - The row/column position within the text buffer that we're about to try to clip.
// - screenInfo - Screen information structure containing relevant text and dimension information.
// - rect - The region of the text that we want to clip, and then adjusted to the region that should be
// clipped without splicing double-width characters.
// Return Value:
// - the clipped region
SMALL_RECT Selection::s_BisectSelection(const short sStringLength,
                                        const COORD coordTargetPoint,
                                        const SCREEN_INFORMATION& screenInfo,
                                        const SMALL_RECT rect)
{
    SMALL_RECT outRect = rect;
    try
    {
        auto iter = screenInfo.GetCellDataAt(coordTargetPoint);
        if (iter->DbcsAttr().IsTrailing())
        {
            if (coordTargetPoint.X == 0)
            {
                outRect.Left++;
            }
            else
            {
                outRect.Left--;
            }
        }

        // Check end position of strings
        if (coordTargetPoint.X + sStringLength < screenInfo.GetBufferSize().Width())
        {
            iter += sStringLength;
            if (iter->DbcsAttr().IsTrailing())
            {
                outRect.Right++;
            }
        }
        else
        {
            if (coordTargetPoint.Y + 1 < screenInfo.GetBufferSize().Height())
            {
                const auto nextLineIter = screenInfo.GetCellDataAt({ 0, coordTargetPoint.Y + 1 });
                if (nextLineIter->DbcsAttr().IsTrailing())
                {
                    outRect.Right--;
                }
            }
        }
    }
    CATCH_LOG();

    return outRect;
}

// Routine Description:
// - Shows the selection area in the window if one is available and not already showing.
// Arguments:
//  <none>
// Return Value:
//  <none>
void Selection::ShowSelection()
{
    _SetSelectionVisibility(true);
}

// Routine Description:
// - Hides the selection area in the window if one is available and already showing.
// Arguments:
//  <none>
// Return Value:
//  <none>
void Selection::HideSelection()
{
    _SetSelectionVisibility(false);
}

// Routine Description:
// - Changes the visibility of the selection area on the screen.
// - Used to turn the selection area on or off.
// Arguments:
// - fMakeVisible - If TRUE, we're turning the selection ON.
//                - If FALSE, we're turning the selection OFF.
// Return Value:
//   <none>
void Selection::_SetSelectionVisibility(const bool fMakeVisible)
{
    if (IsInSelectingState() && IsAreaSelected())
    {
        if (fMakeVisible == _fSelectionVisible)
        {
            return;
        }

        _fSelectionVisible = fMakeVisible;

        _PaintSelection();
    }
    LOG_IF_FAILED(ServiceLocator::LocateConsoleWindow()->SignalUia(UIA_Text_TextSelectionChangedEventId));
}

// Routine Description:
//  - Inverts the selected region on the current screen buffer.
//  - Reads the selected area, selection mode, and active screen buffer
//    from the global properties and dispatches a GDI invert on the selected text area.
// Arguments:
//  - <none>
// Return Value:
//  - <none>
void Selection::_PaintSelection() const
{
    if (ServiceLocator::LocateGlobals().pRender != nullptr)
    {
        ServiceLocator::LocateGlobals().pRender->TriggerSelection();
    }
}

// Routine Description:
// - Starts the selection with the given initial position
// Arguments:
// - coordBufferPos - Position in which user started a selection
// Return Value:
// - <none>
void Selection::InitializeMouseSelection(const COORD coordBufferPos)
{
    Scrolling::s_ClearScroll();

    // set flags
    _SetSelectingState(true);
    _dwSelectionFlags = CONSOLE_MOUSE_SELECTION | CONSOLE_SELECTION_NOT_EMPTY;

    // store anchor and rectangle of selection
    _coordSelectionAnchor = coordBufferPos;

    // since we've started with just a point, the rectangle is 1x1 on the point given
    _srSelectionRect.Left = coordBufferPos.X;
    _srSelectionRect.Right = coordBufferPos.X;
    _srSelectionRect.Top = coordBufferPos.Y;
    _srSelectionRect.Bottom = coordBufferPos.Y;

    // Check for ALT-Mouse Down "use alternate selection"
    // If in box mode, use line mode. If in line mode, use box mode.
    CheckAndSetAlternateSelection();

    // set window title to mouse selection mode
    IConsoleWindow* const pWindow = ServiceLocator::LocateConsoleWindow();
    if (pWindow != nullptr)
    {
        pWindow->UpdateWindowText();
        LOG_IF_FAILED(pWindow->SignalUia(UIA_Text_TextSelectionChangedEventId));
    }

    // Fire off an event to let accessibility apps know the selection has changed.
    ServiceLocator::LocateAccessibilityNotifier()->NotifyConsoleCaretEvent(IAccessibilityNotifier::ConsoleCaretEventFlags::CaretSelection, PACKCOORD(coordBufferPos));
}

// Routine Description:
// - Modifies both ends of the current selection.
// - Intended for use with functions that help auto-complete a selection area (e.g. double clicking)
// Arguments:
// - coordSelectionStart - Replaces the selection anchor, a.k.a. where the selection started from originally.
// - coordSelectionEnd - The linear final position or opposite corner of the anchor to represent the complete selection area.
// Return Value:
// - <none>
void Selection::AdjustSelection(const COORD coordSelectionStart, const COORD coordSelectionEnd)
{
    // modify the anchor and then just use extend to adjust the other portion of the selection rectangle
    _coordSelectionAnchor = coordSelectionStart;
    ExtendSelection(coordSelectionEnd);
    _allowMouseDragSelection = false;
}

// Routine Description:
// - Extends the selection out to the given position from the initial anchor point.
//   This means that a coordinate farther away will make the rectangle larger and a closer one will shrink it.
// Arguments:
// - coordBufferPos - Position to extend/contract the current selection up to.
// Return Value:
// - <none>
void Selection::ExtendSelection(_In_ COORD coordBufferPos)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& screenInfo = gci.GetActiveOutputBuffer();

    _allowMouseDragSelection = true;

    // ensure position is within buffer bounds. Not less than 0 and not greater than the screen buffer size.
    try
    {
        screenInfo.GetTerminalBufferSize().Clamp(coordBufferPos);
    }
    CATCH_LOG_RETURN();

    if (!IsAreaSelected())
    {
        // we should only be extending a selection that has no area yet if we're coming from mark mode.
        // if not, just return.
        if (IsMouseInitiatedSelection())
        {
            return;
        }

        // scroll if necessary to make cursor visible.
        screenInfo.MakeCursorVisible(coordBufferPos, false);

        _dwSelectionFlags |= CONSOLE_SELECTION_NOT_EMPTY;
        _srSelectionRect.Left = _srSelectionRect.Right = _coordSelectionAnchor.X;
        _srSelectionRect.Top = _srSelectionRect.Bottom = _coordSelectionAnchor.Y;

        ShowSelection();
    }
    else
    {
        // scroll if necessary to make cursor visible.
        screenInfo.MakeCursorVisible(coordBufferPos, false);
    }

    // remember previous selection rect
    SMALL_RECT srNewSelection = _srSelectionRect;

    // update selection rect
    // this adjusts the rectangle dimensions based on which way the move was requested
    // in respect to the original selection position (the anchor)
    if (coordBufferPos.X <= _coordSelectionAnchor.X)
    {
        srNewSelection.Left = coordBufferPos.X;
        srNewSelection.Right = _coordSelectionAnchor.X;
    }
    else if (coordBufferPos.X > _coordSelectionAnchor.X)
    {
        srNewSelection.Right = coordBufferPos.X;
        srNewSelection.Left = _coordSelectionAnchor.X;
    }
    if (coordBufferPos.Y <= _coordSelectionAnchor.Y)
    {
        srNewSelection.Top = coordBufferPos.Y;
        srNewSelection.Bottom = _coordSelectionAnchor.Y;
    }
    else if (coordBufferPos.Y > _coordSelectionAnchor.Y)
    {
        srNewSelection.Bottom = coordBufferPos.Y;
        srNewSelection.Top = _coordSelectionAnchor.Y;
    }

    // call special update method to modify the displayed selection in-place
    // NOTE: Using HideSelection, editing the rectangle, then ShowSelection will cause flicker.
    //_PaintUpdateSelection(&srNewSelection);
    _srSelectionRect = srNewSelection;
    _PaintSelection();

    // Fire off an event to let accessibility apps know the selection has changed.
    ServiceLocator::LocateAccessibilityNotifier()->NotifyConsoleCaretEvent(IAccessibilityNotifier::ConsoleCaretEventFlags::CaretSelection, PACKCOORD(coordBufferPos));
    LOG_IF_FAILED(ServiceLocator::LocateConsoleWindow()->SignalUia(UIA_Text_TextSelectionChangedEventId));
}

// Routine Description:
// - Cancels any mouse selection state to return to normal mode.
// Arguments:
// - <none> (Uses global state)
// Return Value:
// - <none>
void Selection::_CancelMouseSelection()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& ScreenInfo = gci.GetActiveOutputBuffer();

    // invert old select rect.  if we're selecting by mouse, we
    // always have a selection rect.
    HideSelection();

    // turn off selection flag
    _SetSelectingState(false);

    IConsoleWindow* const pWindow = ServiceLocator::LocateConsoleWindow();
    if (pWindow != nullptr)
    {
        pWindow->UpdateWindowText();
    }

    // Mark the cursor position as changed so we'll fire off a win event.
    ScreenInfo.GetTextBuffer().GetCursor().SetHasMoved(true);
}

// Routine Description:
// - Cancels any mark mode selection state to return to normal mode.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Selection::_CancelMarkSelection()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& ScreenInfo = gci.GetActiveOutputBuffer();

    // Hide existing selection, if we have one.
    if (IsAreaSelected())
    {
        HideSelection();
    }

    // Turn off selection flag.
    _SetSelectingState(false);

    IConsoleWindow* const pWindow = ServiceLocator::LocateConsoleWindow();
    if (pWindow != nullptr)
    {
        pWindow->UpdateWindowText();
    }

    // restore text cursor
    _RestoreDataToCursor(ScreenInfo.GetTextBuffer().GetCursor());
}

// Routine Description:
// - If a selection exists, clears it and restores the state.
//   Will also unblock a blocked write if one exists.
// Arguments:
// - <none> (Uses global state)
// Return Value:
// - <none>
void Selection::ClearSelection()
{
    ClearSelection(false);
}

// Routine Description:
// - If a selection exists, clears it and restores the state.
// - Will only unblock a write if not starting a new selection.
// Arguments:
// - fStartingNewSelection - If we're going to start another selection right away, we'll keep the write blocked.
// Return Value:
// - <none>
void Selection::ClearSelection(const bool fStartingNewSelection)
{
    if (IsInSelectingState())
    {
        if (IsMouseInitiatedSelection())
        {
            _CancelMouseSelection();
        }
        else
        {
            _CancelMarkSelection();
        }
        LOG_IF_FAILED(ServiceLocator::LocateConsoleWindow()->SignalUia(UIA_Text_TextSelectionChangedEventId));

        _dwSelectionFlags = 0;

        // If we were using alternate selection, cancel it here before starting a new area.
        _fUseAlternateSelection = false;

        // Only unblock if we're not immediately starting a new selection. Otherwise stay blocked.
        if (!fStartingNewSelection)
        {
            UnblockWriteConsole(CONSOLE_SELECTING);
        }
    }
}

// Routine Description:
// - Colors all text in the given rectangle with the color attribute provided.
// - This does not validate whether there is a valid selection right now or not.
//   It is assumed to already be in a proper selecting state and the given rectangle should be highlighted with the given color unconditionally.
// Arguments:
// - psrRect - Rectangular area to fill with color
// - attr - The color attributes to apply
void Selection::ColorSelection(const SMALL_RECT& srRect, const TextAttribute attr)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    // Read selection rectangle, assumed already clipped to buffer.
    SCREEN_INFORMATION& screenInfo = gci.GetActiveOutputBuffer();

    COORD coordTargetSize;
    coordTargetSize.X = CalcWindowSizeX(srRect);
    coordTargetSize.Y = CalcWindowSizeY(srRect);

    COORD coordTarget;
    coordTarget.X = srRect.Left;
    coordTarget.Y = srRect.Top;

    // Now color the selection a line at a time.
    for (; (coordTarget.Y < srRect.Top + coordTargetSize.Y); ++coordTarget.Y)
    {
        const size_t cchWrite = gsl::narrow<size_t>(coordTargetSize.X);

        try
        {
            screenInfo.Write(OutputCellIterator(attr, cchWrite), coordTarget);
        }
        CATCH_LOG();
    }
}

// Routine Description:
// - Given two points in the buffer space, color the selection between the two with the given attribute.
// - This will create an internal selection rectangle covering the two points, assume a line selection,
//   and use the first point as the anchor for the selection (as if the mouse click started at that point)
// Arguments:
// - coordSelectionStart - Anchor point (start of selection) for the region to be colored
// - coordSelectionEnd - Other point referencing the rectangle inscribing the selection area
// - attr - Color to apply to region.
void Selection::ColorSelection(const COORD coordSelectionStart, const COORD coordSelectionEnd, const TextAttribute attr)
{
    // Make a rectangle for the region as if it were selected by a mouse.
    // We will use the first one as the "anchor" to represent where the mouse went down.
    SMALL_RECT srSelection;
    srSelection.Top = std::min(coordSelectionStart.Y, coordSelectionEnd.Y);
    srSelection.Bottom = std::max(coordSelectionStart.Y, coordSelectionEnd.Y);
    srSelection.Left = std::min(coordSelectionStart.X, coordSelectionEnd.X);
    srSelection.Right = std::max(coordSelectionStart.X, coordSelectionEnd.X);

    // Extract row-by-row selection rectangles for the selection area.
    try
    {
        const auto rectangles = s_GetSelectionRects(srSelection, coordSelectionStart, true);
        for (const auto& rect : rectangles)
        {
            ColorSelection(rect, attr);
        }
    }
    CATCH_LOG();
}

// Routine Description:
// - Enters mark mode selection. Prepares the cursor to move around to select a region and sets up state variables.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Selection::InitializeMarkSelection()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    // clear any existing selection.
    ClearSelection(true);

    Scrolling::s_ClearScroll();

    // set flags
    _SetSelectingState(true);
    _dwSelectionFlags = 0;

    // save old cursor position and make console cursor into selection cursor.
    SCREEN_INFORMATION& screenInfo = gci.GetActiveOutputBuffer();
    const auto& cursor = screenInfo.GetTextBuffer().GetCursor();
    _SaveCursorData(cursor);
    screenInfo.SetCursorInformation(100, TRUE);

    const COORD coordPosition = cursor.GetPosition();
    LOG_IF_FAILED(screenInfo.SetCursorPosition(coordPosition, true));

    // set the cursor position as the anchor position
    // it will get updated as the cursor moves for mark mode,
    // but it serves to prepare us for the inevitable start of the selection with Shift+Arrow Key
    _coordSelectionAnchor = coordPosition;

    // set frame title text
    IConsoleWindow* const pWindow = ServiceLocator::LocateConsoleWindow();
    if (pWindow != nullptr)
    {
        pWindow->UpdateWindowText();
        LOG_IF_FAILED(pWindow->SignalUia(UIA_Text_TextSelectionChangedEventId));
    }
}

// Routine Description:
// - Resets the current selection and selects a new region from the start to end coordinates
// Arguments:
// - coordStart - Position to start selection area from
// - coordEnd - Position to select up to
// Return Value:
// - <none>
void Selection::SelectNewRegion(const COORD coordStart, const COORD coordEnd)
{
    // clear existing selection if applicable
    ClearSelection();

    // initialize selection
    InitializeMouseSelection(coordStart);

    ShowSelection();

    // extend selection
    ExtendSelection(coordEnd);
}

// Routine Description:
// - Creates a new selection region of "all" available text.
//   The meaning of "all" can vary. If we have input text, then "all" is just the input text.
//   If we have no input text, "all" is the entire valid screen buffer (output text and the prompt)
// Arguments:
// - <none> (Uses global state)
// Return Value:
// - <none>
void Selection::SelectAll()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    // save the old window position
    SCREEN_INFORMATION& screenInfo = gci.GetActiveOutputBuffer();

    COORD coordWindowOrigin = screenInfo.GetViewport().Origin();

    // Get existing selection rectangle parameters
    const bool fOldSelectionExisted = IsAreaSelected();
    const SMALL_RECT srOldSelection = _srSelectionRect;
    const COORD coordOldAnchor = _coordSelectionAnchor;

    // Attempt to get the boundaries of the current input line.
    COORD coordInputStart;
    COORD coordInputEnd;
    const bool fHasInputArea = s_GetInputLineBoundaries(&coordInputStart, &coordInputEnd);

    // These variables will be used to specify the new selection area when we're done
    COORD coordNewSelStart;
    COORD coordNewSelEnd;

    // Now evaluate conditions and attempt to assign a new selection area.
    if (!fHasInputArea)
    {
        // If there's no input area, just select the entire valid text region.
        GetValidAreaBoundaries(&coordNewSelStart, &coordNewSelEnd);
    }
    else
    {
        if (!fOldSelectionExisted)
        {
            // Temporary workaround until MSFT: 614579 is completed.
            const auto bufferSize = screenInfo.GetBufferSize();
            COORD coordOneAfterEnd = coordInputEnd;
            bufferSize.IncrementInBounds(coordOneAfterEnd);

            if (s_IsWithinBoundaries(screenInfo.GetTextBuffer().GetCursor().GetPosition(), coordInputStart, coordInputEnd))
            {
                // If there was no previous selection and the cursor is within the input line, select the input line only
                coordNewSelStart = coordInputStart;
                coordNewSelEnd = coordInputEnd;
            }
            else if (s_IsWithinBoundaries(screenInfo.GetTextBuffer().GetCursor().GetPosition(), coordOneAfterEnd, coordOneAfterEnd))
            {
                // Temporary workaround until MSFT: 614579 is completed.
                // Select only the input line if the cursor is one after the final position of the input line.
                coordNewSelStart = coordInputStart;
                coordNewSelEnd = coordInputEnd;
            }
            else
            {
                // otherwise if the cursor is elsewhere, select everything
                GetValidAreaBoundaries(&coordNewSelStart, &coordNewSelEnd);
            }
        }
        else
        {
            // This is the complex case. We had an existing selection and we have an input area.

            // To figure this out, we need the anchor (the point where the selection starts) and its opposite corner
            COORD coordOldAnchorOpposite = Utils::s_GetOppositeCorner(srOldSelection, coordOldAnchor);

            // Check if both anchor and opposite corner fall within the input line
            const bool fIsOldSelWithinInput =
                s_IsWithinBoundaries(coordOldAnchor, coordInputStart, coordInputEnd) &&
                s_IsWithinBoundaries(coordOldAnchorOpposite, coordInputStart, coordInputEnd);

            // Check if both anchor and opposite corner are exactly the bounds of the input line
            const bool fAllInputSelected =
                ((Utils::s_CompareCoords(coordInputStart, coordOldAnchor) == 0 && Utils::s_CompareCoords(coordInputEnd, coordOldAnchorOpposite) == 0) ||
                 (Utils::s_CompareCoords(coordInputStart, coordOldAnchorOpposite) == 0 && Utils::s_CompareCoords(coordInputEnd, coordOldAnchor) == 0));

            if (fIsOldSelWithinInput && !fAllInputSelected)
            {
                // If it's within the input area and the whole input is not selected, then select just the input
                coordNewSelStart = coordInputStart;
                coordNewSelEnd = coordInputEnd;
            }
            else
            {
                // Otherwise just select the whole valid area
                GetValidAreaBoundaries(&coordNewSelStart, &coordNewSelEnd);
            }
        }
    }

    // If we're in box selection, adjust end coordinate to end of line and start coordinate to start of line
    // or it won't be selecting all the text.
    if (!IsLineSelection())
    {
        coordNewSelStart.X = 0;
        coordNewSelEnd.X = screenInfo.GetBufferSize().RightInclusive();
    }

    SelectNewRegion(coordNewSelStart, coordNewSelEnd);

    // restore the old window position
    LOG_IF_FAILED(screenInfo.SetViewportOrigin(true, coordWindowOrigin, true));
}
