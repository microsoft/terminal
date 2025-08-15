// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "_output.h"
#include "stream.h"
#include "scrolling.hpp"

#include "../interactivity/inc/ServiceLocator.hpp"

using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::Types;

Selection::Selection() {}

Selection& Selection::Instance()
{
    static std::unique_ptr<Selection> _instance{ new Selection() };
    return *_instance;
}

void Selection::_RegenerateSelectionSpans() const
{
    if (_lastSelectionGeneration == _d.generation())
    {
        return;
    }

    _lastSelectionSpans.clear();

    if (!_d->fSelectionVisible)
    {
        return;
    }

    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& screenInfo = gci.GetActiveOutputBuffer();

    // _coordSelectionAnchor is at one of the corners of _srSelectionRects
    // endSelectionAnchor is at the exact opposite corner
    til::point endSelectionAnchor;
    endSelectionAnchor.x = (_d->coordSelectionAnchor.x == _d->srSelectionRect.left) ? _d->srSelectionRect.right : _d->srSelectionRect.left;
    endSelectionAnchor.y = (_d->coordSelectionAnchor.y == _d->srSelectionRect.top) ? _d->srSelectionRect.bottom : _d->srSelectionRect.top;

    // GH #18106: Conhost and Terminal share most of the selection code.
    //    Both now store the selection data as a half-open range [start, end),
    //     where "end" is the bottom-right-most point.
    //    Note that Conhost defines start/end as "start was set in time before end",
    //     whereas above (and in Terminal) we're treating start/end as "start is physically before end".
    //    We want Conhost to still operate as an inclusive range.
    //    To make it "feel" inclusive, we need to adjust the "end" endpoint
    //     by incrementing it by one, so that the "end" endpoint is rendered
    //     and handled as selected.
    const auto blockSelection = !IsLineSelection();
    const auto& buffer = screenInfo.GetTextBuffer();
    auto startSelectionAnchor = _d->coordSelectionAnchor;
    if (blockSelection)
    {
        // Compare x-values when we're in block selection!
        buffer.GetSize().IncrementInExclusiveBounds(startSelectionAnchor.x <= endSelectionAnchor.x ? endSelectionAnchor : startSelectionAnchor);
    }
    else
    {
        // General comparison for line selection.
        buffer.GetSize().IncrementInExclusiveBounds(startSelectionAnchor <= endSelectionAnchor ? endSelectionAnchor : startSelectionAnchor);
    }
    _lastSelectionSpans = buffer.GetTextSpans(startSelectionAnchor,
                                              endSelectionAnchor,
                                              blockSelection,
                                              false);
    _lastSelectionGeneration = _d.generation();
}

std::span<const til::point_span> Selection::GetSelectionSpans() const
{
    _RegenerateSelectionSpans();
    return { _lastSelectionSpans.cbegin(), _lastSelectionSpans.cend() };
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
        if (fMakeVisible == _d->fSelectionVisible)
        {
            return;
        }

        _d.write()->fSelectionVisible = fMakeVisible;

        _PaintSelection();
    }
    if (const auto window = ServiceLocator::LocateConsoleWindow())
    {
        LOG_IF_FAILED(window->SignalUia(UIA_Text_TextSelectionChangedEventId));
    }
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
void Selection::InitializeMouseSelection(const til::point coordBufferPos)
{
    Scrolling::s_ClearScroll();

    // set flags
    _SetSelectingState(true);
    auto d{ _d.write() };

    wil::hide_name _d;

    d->dwSelectionFlags = CONSOLE_MOUSE_SELECTION | CONSOLE_SELECTION_NOT_EMPTY;

    // store anchor and rectangle of selection
    d->coordSelectionAnchor = coordBufferPos;

    // since we've started with just a point, the rectangle is 1x1 on the point given
    d->srSelectionRect.left = coordBufferPos.x;
    d->srSelectionRect.right = coordBufferPos.x;
    d->srSelectionRect.top = coordBufferPos.y;
    d->srSelectionRect.bottom = coordBufferPos.y;

    // Check for ALT-Mouse Down "use alternate selection"
    // If in box mode, use line mode. If in line mode, use box mode.
    CheckAndSetAlternateSelection();

    // set window title to mouse selection mode
    const auto pWindow = ServiceLocator::LocateConsoleWindow();
    if (pWindow != nullptr)
    {
        pWindow->UpdateWindowText();
        LOG_IF_FAILED(pWindow->SignalUia(UIA_Text_TextSelectionChangedEventId));
    }

    // Fire off an event to let accessibility apps know the selection has changed.
    auto pNotifier = ServiceLocator::LocateAccessibilityNotifier();
    if (pNotifier)
    {
        pNotifier->NotifyConsoleCaretEvent(IAccessibilityNotifier::ConsoleCaretEventFlags::CaretSelection, PACKCOORD(coordBufferPos));
    }
}

// Routine Description:
// - Modifies both ends of the current selection.
// - Intended for use with functions that help auto-complete a selection area (e.g. double clicking)
// Arguments:
// - coordSelectionStart - Replaces the selection anchor, a.k.a. where the selection started from originally.
// - coordSelectionEnd - The linear final position or opposite corner of the anchor to represent the complete selection area.
// Return Value:
// - <none>
void Selection::AdjustSelection(const til::point coordSelectionStart, const til::point coordSelectionEnd)
{
    // modify the anchor and then just use extend to adjust the other portion of the selection rectangle
    auto d{ _d.write() };
    wil::hide_name _d;

    d->coordSelectionAnchor = coordSelectionStart;
    _ExtendSelection(d, coordSelectionEnd);
    d->allowMouseDragSelection = false;
}

void Selection::ExtendSelection(_In_ til::point coordBufferPos)
{
    _ExtendSelection(_d.write(), coordBufferPos);
}

// Routine Description:
// - Extends the selection out to the given position from the initial anchor point.
//   This means that a coordinate farther away will make the rectangle larger and a closer one will shrink it.
// Arguments:
// - coordBufferPos - Position to extend/contract the current selection up to.
// Return Value:
// - <none>
void Selection::_ExtendSelection(Selection::SelectionData* d, _In_ til::point coordBufferPos)
{
    wil::hide_name _d;

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& screenInfo = gci.GetActiveOutputBuffer();

    d->allowMouseDragSelection = true;

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
        screenInfo.MakeCursorVisible(coordBufferPos);

        d->dwSelectionFlags |= CONSOLE_SELECTION_NOT_EMPTY;
        d->srSelectionRect.left = d->srSelectionRect.right = d->coordSelectionAnchor.x;
        d->srSelectionRect.top = d->srSelectionRect.bottom = d->coordSelectionAnchor.y;

        ShowSelection();
    }
    else
    {
        // scroll if necessary to make cursor visible.
        screenInfo.MakeCursorVisible(coordBufferPos);
    }

    // remember previous selection rect
    auto srNewSelection = d->srSelectionRect;

    // update selection rect
    // this adjusts the rectangle dimensions based on which way the move was requested
    // in respect to the original selection position (the anchor)
    if (coordBufferPos.x <= d->coordSelectionAnchor.x)
    {
        srNewSelection.left = coordBufferPos.x;
        srNewSelection.right = d->coordSelectionAnchor.x;
    }
    else if (coordBufferPos.x > d->coordSelectionAnchor.x)
    {
        srNewSelection.right = coordBufferPos.x;
        srNewSelection.left = d->coordSelectionAnchor.x;
    }
    if (coordBufferPos.y <= d->coordSelectionAnchor.y)
    {
        srNewSelection.top = coordBufferPos.y;
        srNewSelection.bottom = d->coordSelectionAnchor.y;
    }
    else if (coordBufferPos.y > d->coordSelectionAnchor.y)
    {
        srNewSelection.bottom = coordBufferPos.y;
        srNewSelection.top = d->coordSelectionAnchor.y;
    }

    // This function is called on WM_MOUSEMOVE.
    // Prevent triggering an invalidation just because the mouse moved
    // in the same cell without changing the actual (visible) selection.
    if (d->srSelectionRect == srNewSelection)
    {
        return;
    }

    // call special update method to modify the displayed selection in-place
    // NOTE: Using HideSelection, editing the rectangle, then ShowSelection will cause flicker.
    //_PaintUpdateSelection(&srNewSelection);
    d->srSelectionRect = srNewSelection;
    _PaintSelection();

    // Fire off an event to let accessibility apps know the selection has changed.
    if (const auto pNotifier = ServiceLocator::LocateAccessibilityNotifier())
    {
        pNotifier->NotifyConsoleCaretEvent(IAccessibilityNotifier::ConsoleCaretEventFlags::CaretSelection, PACKCOORD(coordBufferPos));
    }
    if (const auto window = ServiceLocator::LocateConsoleWindow())
    {
        LOG_IF_FAILED(window->SignalUia(UIA_Text_TextSelectionChangedEventId));
    }
}

// Routine Description:
// - Cancels any mouse selection state to return to normal mode.
// Arguments:
// - <none> (Uses global state)
// Return Value:
// - <none>
void Selection::_CancelMouseSelection()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& ScreenInfo = gci.GetActiveOutputBuffer();

    // invert old select rect.  if we're selecting by mouse, we
    // always have a selection rect.
    HideSelection();

    // turn off selection flag
    _SetSelectingState(false);

    const auto pWindow = ServiceLocator::LocateConsoleWindow();
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
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& ScreenInfo = gci.GetActiveOutputBuffer();

    // Hide existing selection, if we have one.
    if (IsAreaSelected())
    {
        HideSelection();
    }

    // Turn off selection flag.
    _SetSelectingState(false);

    const auto pWindow = ServiceLocator::LocateConsoleWindow();
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
        if (const auto window = ServiceLocator::LocateConsoleWindow())
        {
            LOG_IF_FAILED(window->SignalUia(UIA_Text_TextSelectionChangedEventId));
        }

        auto d{ _d.write() };
        wil::hide_name _d;

        d->dwSelectionFlags = 0;

        // If we were using alternate selection, cancel it here before starting a new area.
        d->fUseAlternateSelection = false;

        // Only unblock if we're not immediately starting a new selection. Otherwise, stay blocked.
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
void Selection::ColorSelection(const til::inclusive_rect& srRect, const TextAttribute attr)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    // Read selection rectangle, assumed already clipped to buffer.
    auto& screenInfo = gci.GetActiveOutputBuffer();

    til::point coordTargetSize;
    coordTargetSize.x = CalcWindowSizeX(srRect);
    coordTargetSize.y = CalcWindowSizeY(srRect);

    til::point coordTarget;
    coordTarget.x = srRect.left;
    coordTarget.y = srRect.top;

    // Now color the selection a line at a time.
    for (; (coordTarget.y < srRect.top + coordTargetSize.y); ++coordTarget.y)
    {
        const auto cchWrite = gsl::narrow<size_t>(coordTargetSize.x);

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
void Selection::ColorSelection(const til::point coordSelectionStart, const til::point coordSelectionEnd, const TextAttribute attr)
{
    // Extract row-by-row selection rectangles for the selection area.
    try
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        const auto& screenInfo = gci.GetActiveOutputBuffer();

        const auto rectangles = screenInfo.GetTextBuffer().GetTextRects(coordSelectionStart, coordSelectionEnd, false, true);
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
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    // clear any existing selection.
    ClearSelection(true);

    Scrolling::s_ClearScroll();

    auto d{ _d.write() };
    wil::hide_name _d;

    // set flags
    _SetSelectingState(true);
    d->dwSelectionFlags = 0;

    // save old cursor position and make console cursor into selection cursor.
    auto& screenInfo = gci.GetActiveOutputBuffer();
    const auto& cursor = screenInfo.GetTextBuffer().GetCursor();
    _SaveCursorData(cursor);
    screenInfo.SetCursorInformation(100, TRUE);

    const auto coordPosition = cursor.GetPosition();
    LOG_IF_FAILED(screenInfo.SetCursorPosition(coordPosition, true));

    // set the cursor position as the anchor position
    // it will get updated as the cursor moves for mark mode,
    // but it serves to prepare us for the inevitable start of the selection with Shift+Arrow Key
    d->coordSelectionAnchor = coordPosition;

    // set frame title text
    const auto pWindow = ServiceLocator::LocateConsoleWindow();
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
void Selection::SelectNewRegion(const til::point coordStart, const til::point coordEnd)
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
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    // save the old window position
    auto& screenInfo = gci.GetActiveOutputBuffer();

    auto coordWindowOrigin = screenInfo.GetViewport().Origin();

    // Get existing selection rectangle parameters
    const auto fOldSelectionExisted = IsAreaSelected();
    const auto srOldSelection = _d->srSelectionRect;
    const auto coordOldAnchor = _d->coordSelectionAnchor;

    // Attempt to get the boundaries of the current input line.
    til::point coordInputStart;
    til::point coordInputEnd;
    const auto fHasInputArea = s_GetInputLineBoundaries(&coordInputStart, &coordInputEnd);

    // These variables will be used to specify the new selection area when we're done
    til::point coordNewSelStart;
    til::point coordNewSelEnd;

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
            auto coordOneAfterEnd = coordInputEnd;
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
            auto coordOldAnchorOpposite = Utils::s_GetOppositeCorner(srOldSelection, coordOldAnchor);

            // Check if both anchor and opposite corner fall within the input line
            const auto fIsOldSelWithinInput =
                s_IsWithinBoundaries(coordOldAnchor, coordInputStart, coordInputEnd) &&
                s_IsWithinBoundaries(coordOldAnchorOpposite, coordInputStart, coordInputEnd);

            // Check if both anchor and opposite corner are exactly the bounds of the input line
            const auto fAllInputSelected =
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
        coordNewSelStart.x = 0;
        coordNewSelEnd.x = screenInfo.GetBufferSize().RightInclusive();
    }

    SelectNewRegion(coordNewSelStart, coordNewSelEnd);

    // restore the old window position
    LOG_IF_FAILED(screenInfo.SetViewportOrigin(true, coordWindowOrigin, false));
}
