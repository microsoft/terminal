// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../buffer/out/search.h"

#include "../interactivity/inc/ServiceLocator.hpp"
#include "../types/inc/convert.hpp"

using namespace Microsoft::Console::Types;
using Microsoft::Console::Interactivity::ServiceLocator;
// Routine Description:
// - Handles a keyboard event for extending the current selection
// - Must be called when the console is in selecting state.
// Arguments:
// - pInputKeyInfo : The key press state information from the keyboard
// Return Value:
// - True if the event is handled. False otherwise.
Selection::KeySelectionEventResult Selection::HandleKeySelectionEvent(const INPUT_KEY_INFO* const pInputKeyInfo)
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    FAIL_FAST_IF(!IsInSelectingState());

    const auto wVirtualKeyCode = pInputKeyInfo->GetVirtualKey();
    const auto ctrlPressed = WI_IsFlagSet(OneCoreSafeGetKeyState(VK_CONTROL), KEY_PRESSED);

    // if escape or ctrl-c, cancel selection
    if (!IsMouseButtonDown())
    {
        if (wVirtualKeyCode == VK_ESCAPE)
        {
            ClearSelection();
            return Selection::KeySelectionEventResult::EventHandled;
        }
        else if (wVirtualKeyCode == VK_RETURN ||
                 // C-c, C-Ins. C-S-c Is also handled by this case.
                 ((ctrlPressed) && (wVirtualKeyCode == 'C' || wVirtualKeyCode == VK_INSERT)))
        {
            // copy selection
            return Selection::KeySelectionEventResult::CopyToClipboard;
        }
        else if (gci.GetEnableColorSelection() &&
                 ('0' <= wVirtualKeyCode) &&
                 ('9' >= wVirtualKeyCode))
        {
            if (_HandleColorSelection(pInputKeyInfo))
            {
                return Selection::KeySelectionEventResult::EventHandled;
            }
        }
    }

    if (!IsMouseInitiatedSelection())
    {
        if (_HandleMarkModeSelectionNav(pInputKeyInfo))
        {
            return Selection::KeySelectionEventResult::EventHandled;
        }
    }
    else if (!IsMouseButtonDown())
    {
        // if the existing selection is a line selection
        if (IsLineSelection())
        {
            // try to handle it first if we've used a valid keyboard command to extend the selection
            if (HandleKeyboardLineSelectionEvent(pInputKeyInfo))
            {
                return Selection::KeySelectionEventResult::EventHandled;
            }
        }

        // if in mouse selection mode and user hits a key, cancel selection
        if (!IsSystemKey(wVirtualKeyCode))
        {
            ClearSelection();
        }
    }

    return Selection::KeySelectionEventResult::EventNotHandled;
}

// Routine Description:
// - Checks if a keyboard event can be handled by HandleKeyboardLineSelectionEvent
// Arguments:
// - pInputKeyInfo : The key press state information from the keyboard
// Return Value:
// - True if the event can be handled. False otherwise.
// NOTE:
// - Keyboard handling cases in this function should be synchronized with HandleKeyboardLineSelectionEvent
bool Selection::s_IsValidKeyboardLineSelection(const INPUT_KEY_INFO* const pInputKeyInfo)
{
    auto fIsValidCombination = false;

    const auto wVirtualKeyCode = pInputKeyInfo->GetVirtualKey();

    if (pInputKeyInfo->IsShiftOnly())
    {
        switch (wVirtualKeyCode)
        {
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_NEXT:
        case VK_PRIOR:
        case VK_HOME:
        case VK_END:
            fIsValidCombination = true;
        }
    }
    else if (pInputKeyInfo->IsShiftAndCtrlOnly())
    {
        switch (wVirtualKeyCode)
        {
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_HOME:
        case VK_END:
            fIsValidCombination = true;
        }
    }

    return fIsValidCombination;
}

// Routine Description:
// - Modifies the given selection point to the edge of the next (or previous) word.
// - By default operates in a left-to-right fashion.
// Arguments:
// - fReverse: Specifies that this function should operate in reverse. E.g. Right-to-left.
// - bufferSize: The dimensions of the screen buffer.
// - coordAnchor: The point within the buffer (inside the edges) where this selection started.
// - coordSelPoint: Defines selection region from coordAnchor to this point. Modified to define the new selection region.
// Return Value:
// - <none>
til::point Selection::WordByWordSelection(const bool fReverse,
                                          const Viewport& bufferSize,
                                          const til::point coordAnchor,
                                          const til::point coordSelPoint) const
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& screenInfo = gci.GetActiveOutputBuffer();
    auto outCoord = coordSelPoint;

    // first move one character in the requested direction
    if (!fReverse)
    {
        bufferSize.IncrementInBounds(outCoord);
    }
    else
    {
        bufferSize.DecrementInBounds(outCoord);
    }

    // get the character at the new position
    auto charData = *screenInfo.GetTextDataAt(outCoord);

    // we want to go until the state change from delim to non-delim
    auto fCurrIsDelim = IsWordDelim(charData);
    bool fPrevIsDelim;

    // find the edit-line boundaries that we can highlight
    til::point coordMaxLeft;
    til::point coordMaxRight;
    const auto fSuccess = s_GetInputLineBoundaries(&coordMaxLeft, &coordMaxRight);

    // if line boundaries fail, then set them to the buffer corners so they don't restrict anything.
    if (!fSuccess)
    {
        coordMaxLeft.x = bufferSize.Left();
        coordMaxLeft.y = bufferSize.Top();

        coordMaxRight.x = bufferSize.RightInclusive();
        coordMaxRight.y = bufferSize.BottomInclusive();
    }

    // track whether we failed to move during an operation
    // if we failed to move, we hit the end of the buffer and should just highlight to there and be done.
    auto fMoveSucceeded = false;

    // determine if we're highlighting more text or unhighlighting already selected text.
    bool fUnhighlighting;
    if (!fReverse)
    {
        // if the selection point is left of the anchor, then we're unhighlighting when moving right
        fUnhighlighting = Utils::s_CompareCoords(outCoord, coordAnchor) < 0;
    }
    else
    {
        // if the selection point is right of the anchor, then we're unhighlighting when moving left
        fUnhighlighting = Utils::s_CompareCoords(outCoord, coordAnchor) > 0;
    }

    do
    {
        // store previous state
        fPrevIsDelim = fCurrIsDelim;

        // to make us "sticky" within the edit line, stop moving once we've reached a given max position left/right
        // users can repeat the command to move past the line and continue word selecting
        // if we're at the max position left, stop moving
        if (Utils::s_CompareCoords(outCoord, coordMaxLeft) == 0)
        {
            // set move succeeded to false as we can't move any further
            fMoveSucceeded = false;
            break;
        }

        // if we're at the max position right, stop moving.
        // we don't want them to "word select" past the end of the edit line as there's likely nothing there.
        // (thus >= and not == like left)
        if (Utils::s_CompareCoords(outCoord, coordMaxRight) >= 0)
        {
            // set move succeeded to false as we can't move any further.
            fMoveSucceeded = false;
            break;
        }

        if (!fReverse)
        {
            fMoveSucceeded = bufferSize.IncrementInBounds(outCoord);
        }
        else
        {
            fMoveSucceeded = bufferSize.DecrementInBounds(outCoord);
        }

        if (!fMoveSucceeded)
        {
            break;
        }

        // get the character associated with the new position
        charData = *screenInfo.GetTextDataAt(outCoord);
        fCurrIsDelim = IsWordDelim(charData);

        // This is a bit confusing.
        // If we're going Left to Right (!fReverse)...
        // - Then we want to keep going UNTIL (!) we move from a delimiter (fPrevIsDelim) to a normal character (!fCurrIsDelim)
        //   This will then eat up all delimiters after a word and stop once we reach the first letter of the next word.
        // If we're going Right to Left (fReverse)...
        // - Then we want to keep going UNTIL (!) we move from a normal character (!fPrevIsDelim) to a delimiter (fCurrIsDelim)
        //   This will eat up all letters of the word and stop once we see the delimiter before the word.
    } while (!fReverse ? !(fPrevIsDelim && !fCurrIsDelim) : !(!fPrevIsDelim && fCurrIsDelim));

    // To stop the loop, we had to move the cursor one too far to figure out that the delta occurred from delimiter to not (or vice versa)
    // Therefore move back by one character after proceeding through the loop.
    // EXCEPT:
    // 1. If we broke out of the loop by reaching the beginning of the buffer, leave it alone.
    // 2. If we're un-highlighting a region, also leave it alone.
    //    This is an oddity that occurs because our cursor is on a character, not between two characters like most text editors.
    //    We want the current position to be ON the first letter of the word (or the last delimiter after the word) so it stays highlighted.
    if (fMoveSucceeded && !fUnhighlighting)
    {
        if (!fReverse)
        {
            bufferSize.DecrementInBounds(outCoord);
        }
        else
        {
            bufferSize.IncrementInBounds(outCoord);
        }

        FAIL_FAST_IF(!fMoveSucceeded); // we should never fail to move forward after having moved backward
    }
    return outCoord;
}

// Routine Description:
// - Handles a keyboard event for manipulating line-mode selection with the keyboard
// - If called when console isn't in selecting state, will start a new selection.
// Arguments:
// - inputKeyInfo : The key press state information from the keyboard
// Return Value:
// - True if the event is handled. False otherwise.
// NOTE:
// - Keyboard handling cases in this function should be synchronized with IsValidKeyboardLineSelection
bool Selection::HandleKeyboardLineSelectionEvent(const INPUT_KEY_INFO* const pInputKeyInfo)
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto wVirtualKeyCode = pInputKeyInfo->GetVirtualKey();

    // if this isn't a valid key combination for this function, exit quickly.
    if (!s_IsValidKeyboardLineSelection(pInputKeyInfo))
    {
        return false;
    }

    // if we're not currently selecting anything, start a new mouse selection
    if (!IsInSelectingState())
    {
        InitializeMouseSelection(gci.GetActiveOutputBuffer().GetTextBuffer().GetCursor().GetPosition());

        // force that this is a line selection
        _AlignAlternateSelection(true);

        ShowSelection();

        // if we did shift+left/right, then just exit
        if (pInputKeyInfo->IsShiftOnly())
        {
            switch (wVirtualKeyCode)
            {
            case VK_LEFT:
            case VK_RIGHT:
                return true;
            }
        }
    }

    // anchor is the first clicked position
    const auto coordAnchor = _d->coordSelectionAnchor;

    // rect covers the entire selection
    const auto rectSelection = _d->srSelectionRect;

    // the selection point is the other corner of the rectangle from the anchor that we're about to manipulate
    til::point coordSelPoint;
    coordSelPoint.x = coordAnchor.x == rectSelection.left ? rectSelection.right : rectSelection.left;
    coordSelPoint.y = coordAnchor.y == rectSelection.top ? rectSelection.bottom : rectSelection.top;

    // this is the maximum size of the buffer
    const auto bufferSize = gci.GetActiveOutputBuffer().GetBufferSize();

    const auto sWindowHeight = gci.GetActiveOutputBuffer().GetViewport().Height();

    FAIL_FAST_IF(!bufferSize.IsInBounds(coordSelPoint));

    // retrieve input line information. If we are selecting from within the input line, we need
    // to bound ourselves within the input data first and not move into the back buffer.

    til::point coordInputLineStart;
    til::point coordInputLineEnd;
    auto fHaveInputLine = s_GetInputLineBoundaries(&coordInputLineStart, &coordInputLineEnd);

    if (pInputKeyInfo->IsShiftOnly())
    {
        switch (wVirtualKeyCode)
        {
            // shift + left/right extends the selection by one character, wrapping at screen edge
        case VK_LEFT:
        {
            bufferSize.DecrementInBounds(coordSelPoint);
            break;
        }
        case VK_RIGHT:
        {
            bufferSize.IncrementInBounds(coordSelPoint);

            // if we're about to split a character in half, keep moving right
            try
            {
                const auto attr = gci.GetActiveOutputBuffer().GetCellDataAt(coordSelPoint)->DbcsAttr();
                if (attr == DbcsAttribute::Trailing)
                {
                    bufferSize.IncrementInBounds(coordSelPoint);
                }
            }
            CATCH_LOG();

            break;
        }
            // shift + up/down extends the selection by one row, stopping at top or bottom of screen
        case VK_UP:
        {
            if (coordSelPoint.y > bufferSize.Top())
            {
                coordSelPoint.y--;
            }
            break;
        }
        case VK_DOWN:
        {
            if (coordSelPoint.y < bufferSize.BottomInclusive())
            {
                coordSelPoint.y++;
            }
            break;
        }
            // shift + pgup/pgdn extends selection up or down one full screen
        case VK_NEXT:
        {
            coordSelPoint.y = base::CheckAdd(coordSelPoint.y, sWindowHeight).ValueOrDefault(bufferSize.BottomInclusive());
            if (coordSelPoint.y > bufferSize.BottomInclusive())
            {
                coordSelPoint.y = bufferSize.BottomInclusive();
            }
            break;
        }
        case VK_PRIOR:
        {
            coordSelPoint.y = base::CheckSub(coordSelPoint.y, sWindowHeight).ValueOrDefault(bufferSize.Top());
            if (coordSelPoint.y < bufferSize.Top())
            {
                coordSelPoint.y = bufferSize.Top();
            }
            break;
        }
            // shift + home/end extends selection to beginning or end of line
        case VK_HOME:
        {
            /*
            Prompt sample:
                qwertyuiopasdfg
                C:\>dir /p /w C
                :\windows\syste
                m32

                The input area runs from the d in "dir" to the space after the 2 in "32"

                We want to stop the HOME command from running to the beginning of the line only
                if we're on the first input line because then it would capture the prompt.

                So if the selection point we're manipulating is currently anywhere in the
                "dir /p /w C" area, then pressing home should only move it on top of the "d" in "dir".

                But if it's already at the "d" in dir, pressing HOME again should move us to the
                beginning of the line anyway to collect up the prompt as well.
            */

            // if we're in the input line
            if (fHaveInputLine)
            {
                // and the selection point is inside the input line area
                if (Utils::s_CompareCoords(coordSelPoint, coordInputLineStart) > 0)
                {
                    // and we're on the same line as the beginning of the input
                    if (coordInputLineStart.y == coordSelPoint.y)
                    {
                        // then only back up to the start of the input
                        coordSelPoint.x = coordInputLineStart.x;
                        break;
                    }
                }
            }

            // otherwise, fall through and select to the head of the line.
            coordSelPoint.x = 0;
            break;
        }
        case VK_END:
        {
            /*
            Prompt sample:
                qwertyuiopasdfg
                C:\>dir /p /w C
                :\windows\syste
                m32

                The input area runs from the d in "dir" to the space after the 2 in "32"

                We want to stop the END command from running to the space after the "32" because
                that's just where the cursor lies to let more text get entered and not actually
                a valid selection area.

                So if the selection point is anywhere on the "m32", pressing end should move it
                to on top of the "2".

                Additionally, if we're starting within the output buffer (qwerty, etc. and C:\>), then
                pressing END should stop us before we enter the input line the first time.

                So if we're anywhere on "C:\", we should select up to the ">" character and no further
                until a subsequent press of END.

                At the subsequent press of END when we're on the ">", we should move to the end of the input
                line or the end of the screen, whichever comes first.
            */

            // if we're in the input line
            if (fHaveInputLine)
            {
                // and the selection point is inside the input area
                if (Utils::s_CompareCoords(coordSelPoint, coordInputLineStart) >= 0)
                {
                    // and we're on the same line as the end of the input
                    if (coordInputLineEnd.y == coordSelPoint.y)
                    {
                        // and we're not already on the end of the input...
                        if (coordSelPoint.x < coordInputLineEnd.x)
                        {
                            // then only use end to the end of the input
                            coordSelPoint.x = coordInputLineEnd.x;
                            break;
                        }
                    }
                }
                else
                {
                    // otherwise if we're outside and on the same line as the start of the input
                    if (coordInputLineStart.y == coordSelPoint.y)
                    {
                        // calculate the end of the outside/output buffer position
                        const auto sEndOfOutputPos = coordInputLineStart.x - 1;

                        // if we're not already on the very last character...
                        if (coordSelPoint.x < sEndOfOutputPos)
                        {
                            // then only move to just before the beginning of the input
                            coordSelPoint.x = sEndOfOutputPos;
                            break;
                        }
                        else if (coordSelPoint.x == sEndOfOutputPos)
                        {
                            // if we were on the last character,
                            // then if the end of the input line is also on this current line,
                            // move to that.
                            if (coordSelPoint.y == coordInputLineEnd.y)
                            {
                                coordSelPoint.x = coordInputLineEnd.x;
                                break;
                            }
                        }
                    }
                }
            }

            // otherwise, fall through and go to selecting the whole line to the end.
            coordSelPoint.x = bufferSize.RightInclusive();
            break;
        }
        }
    }
    else if (pInputKeyInfo->IsShiftAndCtrlOnly())
    {
        switch (wVirtualKeyCode)
        {
            // shift + ctrl + left/right extends selection to next/prev word boundary
        case VK_LEFT:
        {
            coordSelPoint = WordByWordSelection(true, bufferSize, coordAnchor, coordSelPoint);
            break;
        }
        case VK_RIGHT:
        {
            coordSelPoint = WordByWordSelection(false, bufferSize, coordAnchor, coordSelPoint);
            break;
        }
            // shift + ctrl + up/down does the same thing that shift + up/down does
        case VK_UP:
        {
            if (coordSelPoint.y > bufferSize.Top())
            {
                coordSelPoint.y--;
            }
            break;
        }
        case VK_DOWN:
        {
            if (coordSelPoint.y < bufferSize.BottomInclusive())
            {
                coordSelPoint.y++;
            }
            break;
        }
            // shift + ctrl + home/end extends selection to top or bottom of buffer from selection
        case VK_HOME:
        {
            til::point coordValidStart;
            GetValidAreaBoundaries(&coordValidStart, nullptr);
            coordSelPoint = coordValidStart;
            break;
        }
        case VK_END:
        {
            til::point coordValidEnd;
            GetValidAreaBoundaries(nullptr, &coordValidEnd);
            coordSelPoint = coordValidEnd;
            break;
        }
        }
    }

    // ensure we're not planting the cursor in the middle of a double-wide character.
    try
    {
        const auto attr = gci.GetActiveOutputBuffer().GetCellDataAt(coordSelPoint)->DbcsAttr();
        if (attr == DbcsAttribute::Trailing)
        {
            // try to move off by highlighting the lead half too.
            auto fSuccess = bufferSize.DecrementInBounds(coordSelPoint);

            // if that fails, move off to the next character
            if (!fSuccess)
            {
                bufferSize.IncrementInBounds(coordSelPoint);
            }
        }
    }
    CATCH_LOG();

    ExtendSelection(coordSelPoint);

    return true;
}

// Routine Description:
// - Checks whether the ALT key was pressed when this method was called.
// - ALT is the modifier for the alternate selection mode, so this will set state accordingly.
// Arguments:
// - <none> (Uses global key state)
// Return Value:
// - <none>
void Selection::CheckAndSetAlternateSelection()
{
    _d.write()->fUseAlternateSelection = !!(OneCoreSafeGetKeyState(VK_MENU) & KEY_PRESSED);
}

// Routine Description:
// - Handles a keyboard event for manipulating color selection
// - If called when console isn't in selecting state, will start a new selection.
// Arguments:
// - pInputKeyInfo : The key press state information from the keyboard
// Return Value:
// - True if the event is handled. False otherwise.
bool Selection::_HandleColorSelection(const INPUT_KEY_INFO* const pInputKeyInfo)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto wVirtualKeyCode = pInputKeyInfo->GetVirtualKey();

    //  It's a numeric key,  a text mode buffer and the color selection regkey is set,
    //  then check to see if the user wants to color the selection or search and
    //  highlight the selection.
    auto fAltPressed = pInputKeyInfo->IsAltPressed();
    auto fShiftPressed = pInputKeyInfo->IsShiftPressed();
    auto fCtrlPressed = false;

    // Shift implies a find-and-color operation.
    // We only support finding a string,  not a block.
    // If it is line selection, we can assemble that across multiple lines to make a search term.
    // But if it is block selection and the selected area is > 1 line in height, ignore the shift because we can't search.
    // Also ignore if there is no current selection.
    if ((fShiftPressed) && (!IsAreaSelected() || (!IsLineSelection() && (_d->srSelectionRect.top != _d->srSelectionRect.bottom))))
    {
        fShiftPressed = false;
    }

    //  If CTRL + ALT together,  then we interpret as ALT (eg on French
    //  keyboards AltGr == RALT+LCTRL,  but we want it to behave as ALT).
    if (!fAltPressed)
    {
        fCtrlPressed = pInputKeyInfo->IsCtrlPressed();
    }

    auto& screenInfo = gci.GetActiveOutputBuffer();

    //  Clip the selection to within the console buffer
    screenInfo.ClipToScreenBuffer(&_d.write()->srSelectionRect);

    //  If ALT or CTRL are pressed,  then color the selected area.
    //  ALT+n => fg,  CTRL+n => bg
    if (fAltPressed || fCtrlPressed)
    {
        TextAttribute selectionAttr;
        // The key number corresponds to the Windows color table order, so the value
        // need to be transposed to obtain the index in an ANSI-compatible order.
        const auto colorIndex = TextColor::TransposeLegacyIndex(wVirtualKeyCode - '0' + 6);

        if (fCtrlPressed)
        {
            //  Setting background color.  Set fg color to black.
            selectionAttr.SetIndexedBackground256(colorIndex);
            selectionAttr.SetIndexedForeground256(TextColor::DARK_BLACK);
        }
        else
        {
            // Set foreground color. Maintain the current console bg color.
            selectionAttr = gci.GetActiveOutputBuffer().GetAttributes();
            selectionAttr.SetIndexedForeground256(colorIndex);
        }

        const auto& textBuffer = gci.renderData.GetTextBuffer();
        if (fShiftPressed)
        {
            // Search the selection and color *that*
            const auto req = TextBuffer::CopyRequest::FromConfig(textBuffer,
                                                                 til::point{ _d->srSelectionRect.left, _d->srSelectionRect.top },
                                                                 til::point{ _d->srSelectionRect.right, _d->srSelectionRect.bottom },
                                                                 true /* multi-line search doesn't make sense; concatenate all lines */,
                                                                 false /* we filtered out block search above */,
                                                                 true /* trim block selection */,
                                                                 true);
            const auto str = textBuffer.GetPlainText(req);
            // Clear the selection and call the search / mark function.
            ClearSelection();

            const auto hits = textBuffer.SearchText(str, SearchFlag::CaseInsensitive).value_or(std::vector<til::point_span>{});
            for (const auto& s : hits)
            {
                ColorSelection(s.start, s.end, selectionAttr);
            }
        }
        else
        {
            const auto selection = GetSelectionSpans();
            for (auto&& sp : selection)
            {
                sp.iterate_rows(textBuffer.GetSize().Width(), [&](til::CoordType row, til::CoordType beg, til::CoordType end) {
                    ColorSelection({ beg, row, end, row }, selectionAttr);
                });
            }
            ClearSelection();
        }

        return true;
    }

    return false;
}

// Routine Description:
// - Handles a keyboard event for selection in mark mode
// Arguments:
// - pInputKeyInfo : The key press state information from the keyboard
// Return Value:
// - True if the event is handled. False otherwise.
bool Selection::_HandleMarkModeSelectionNav(const INPUT_KEY_INFO* const pInputKeyInfo)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto wVirtualKeyCode = pInputKeyInfo->GetVirtualKey();

    // we're selecting via keyboard -- handle keystrokes
    if (wVirtualKeyCode == VK_RIGHT ||
        wVirtualKeyCode == VK_LEFT ||
        wVirtualKeyCode == VK_UP ||
        wVirtualKeyCode == VK_DOWN ||
        wVirtualKeyCode == VK_NEXT ||
        wVirtualKeyCode == VK_PRIOR ||
        wVirtualKeyCode == VK_END ||
        wVirtualKeyCode == VK_HOME)
    {
        auto& ScreenInfo = gci.GetActiveOutputBuffer();
        auto& textBuffer = ScreenInfo.GetTextBuffer();
        til::CoordType iNextRightX = 0;
        til::CoordType iNextLeftX = 0;

        const auto cursorPos = textBuffer.GetCursor().GetPosition();

        try
        {
            auto it = ScreenInfo.GetCellLineDataAt(cursorPos);

            // calculate next right
            if (it->DbcsAttr() == DbcsAttribute::Leading)
            {
                iNextRightX = 2;
            }
            else
            {
                iNextRightX = 1;
            }

            // calculate next left
            if (cursorPos.x > 0)
            {
                it--;
                if (it->DbcsAttr() == DbcsAttribute::Trailing)
                {
                    iNextLeftX = 2;
                }
                else if (it->DbcsAttr() == DbcsAttribute::Leading)
                {
                    if (cursorPos.x - 1 > 0)
                    {
                        it--;
                        if (it->DbcsAttr() == DbcsAttribute::Trailing)
                        {
                            iNextLeftX = 3;
                        }
                        else
                        {
                            iNextLeftX = 2;
                        }
                    }
                    else
                    {
                        iNextLeftX = 1;
                    }
                }
                else
                {
                    iNextLeftX = 1;
                }
            }
        }
        CATCH_LOG();

        auto& cursor = textBuffer.GetCursor();
        switch (wVirtualKeyCode)
        {
        case VK_RIGHT:
        {
            if (cursorPos.x + iNextRightX < ScreenInfo.GetBufferSize().Width())
            {
                cursor.IncrementXPosition(iNextRightX);
            }
            break;
        }

        case VK_LEFT:
        {
            if (cursorPos.x > 0)
            {
                cursor.DecrementXPosition(iNextLeftX);
            }
            break;
        }

        case VK_UP:
        {
            if (cursorPos.y > 0)
            {
                cursor.DecrementYPosition(1);
            }
            break;
        }

        case VK_DOWN:
        {
            if (cursorPos.y + 1 < ScreenInfo.GetTerminalBufferSize().Height())
            {
                cursor.IncrementYPosition(1);
            }
            break;
        }

        case VK_NEXT:
        {
            cursor.IncrementYPosition(ScreenInfo.GetViewport().Height() - 1);
            const auto coordBufferSize = ScreenInfo.GetTerminalBufferSize().Dimensions();
            if (cursor.GetPosition().y >= coordBufferSize.height)
            {
                cursor.SetYPosition(coordBufferSize.height - 1);
            }
            break;
        }

        case VK_PRIOR:
        {
            cursor.DecrementYPosition(ScreenInfo.GetViewport().Height() - 1);
            if (cursor.GetPosition().y < 0)
            {
                cursor.SetYPosition(0);
            }
            break;
        }

        case VK_END:
        {
            // End by itself should go to end of current line. Ctrl-End should go to end of buffer.
            cursor.SetXPosition(ScreenInfo.GetBufferSize().RightInclusive());

            if (pInputKeyInfo->IsCtrlPressed())
            {
                til::point coordValidEnd;
                GetValidAreaBoundaries(nullptr, &coordValidEnd);

                // Adjust Y position of cursor to the final line with valid text
                cursor.SetYPosition(coordValidEnd.y);
            }
            break;
        }

        case VK_HOME:
        {
            // Home by itself should go to the beginning of the current line. Ctrl-Home should go to the beginning of
            // the buffer
            cursor.SetXPosition(0);

            if (pInputKeyInfo->IsCtrlPressed())
            {
                cursor.SetYPosition(0);
            }
            break;
        }

        default:
            FAIL_FAST_HR(E_NOTIMPL);
        }

        // see if shift is down. if so, we're extending the selection. otherwise, we're resetting the anchor
        if (OneCoreSafeGetKeyState(VK_SHIFT) & KEY_PRESSED)
        {
            // if we're just starting to "extend" our selection from moving around as a cursor
            // then attempt to set the alternate selection state based on the ALT key right now
            if (!IsAreaSelected())
            {
                CheckAndSetAlternateSelection();
            }

            ExtendSelection(cursor.GetPosition());
        }
        else
        {
            auto d{ _d.write() };
            // if the selection was not empty, reset the anchor
            if (IsAreaSelected())
            {
                HideSelection();
                d->dwSelectionFlags &= ~CONSOLE_SELECTION_NOT_EMPTY;
                d->fUseAlternateSelection = false;
            }

            cursor.SetHasMoved(true);
            d->coordSelectionAnchor = textBuffer.GetCursor().GetPosition();
            ScreenInfo.MakeCursorVisible(d->coordSelectionAnchor);
            d->srSelectionRect.left = d->srSelectionRect.right = d->coordSelectionAnchor.x;
            d->srSelectionRect.top = d->srSelectionRect.bottom = d->coordSelectionAnchor.y;
        }
        return true;
    }

    return false;
}

#pragma region Calculation / Support for keyboard selection

// Routine Description:
// - Retrieves the boundaries of the input line (first and last char positions)
// Arguments:
// - pcoordInputStart - Position of the first character in the input line
// - pcoordInputEnd - Position of the last character in the input line
// Return Value:
// - If true, the boundaries returned are valid. If false, they should be discarded.
[[nodiscard]] bool Selection::s_GetInputLineBoundaries(_Out_opt_ til::point* const pcoordInputStart, _Out_opt_ til::point* const pcoordInputEnd)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    if (gci.HasPendingCookedRead())
    {
        auto boundaries = gci.CookedReadData().GetBoundaries();
        if (boundaries.start < boundaries.end)
        {
            if (pcoordInputStart != nullptr)
            {
                *pcoordInputStart = boundaries.start;
            }
            if (pcoordInputEnd != nullptr)
            {
                // - 1 so the coordinate is on top of the last position of the text, not one past it.
                gci.GetActiveOutputBuffer().GetBufferSize().WalkInBounds(boundaries.end, -1);
                *pcoordInputEnd = boundaries.end;
            }
            return true;
        }
    }

    return false;
}

// Routine Description:
// - Gets the boundaries of all valid text on the screen.
//   Includes the output/back buffer as well as the input line text.
// Arguments:
// - pcoordInputStart - Position of the first character in the buffer
// - pcoordInputEnd - Position of the last character in the buffer
// Return Value:
// - If true, the boundaries returned are valid. If false, they should be discarded.
void Selection::GetValidAreaBoundaries(_Out_opt_ til::point* const pcoordValidStart, _Out_opt_ til::point* const pcoordValidEnd) const
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    til::point coordEnd;
    coordEnd.x = 0;
    coordEnd.y = 0;

    const auto fHaveInput = s_GetInputLineBoundaries(nullptr, &coordEnd);

    if (!fHaveInput)
    {
        if (IsInSelectingState() && IsKeyboardMarkSelection())
        {
            coordEnd = _d->coordSavedCursorPosition;
        }
        else
        {
            coordEnd = gci.GetActiveOutputBuffer().GetTextBuffer().GetCursor().GetPosition();
        }
    }

    if (pcoordValidStart != nullptr)
    {
        // valid area always starts at 0,0
        pcoordValidStart->x = 0;
        pcoordValidStart->y = 0;
    }

    if (pcoordValidEnd != nullptr)
    {
        pcoordValidEnd->x = coordEnd.x;
        pcoordValidEnd->y = coordEnd.y;
    }
}

// Routine Description:
// - Determines if a coordinate lies between the start and end positions
// - NOTE: Is inclusive of the edges of the boundary.
// Arguments:
// - coordPosition - The position to test
// - coordFirst - The start or left most edge of the regional boundary.
// - coordSecond - The end or right most edge of the regional boundary.
// Return Value:
// - True if it's within the bounds (inclusive). False otherwise.
bool Selection::s_IsWithinBoundaries(const til::point coordPosition, const til::point coordStart, const til::point coordEnd)
{
    auto fInBoundaries = false;

    if (Utils::s_CompareCoords(coordStart, coordPosition) <= 0)
    {
        if (Utils::s_CompareCoords(coordPosition, coordEnd) <= 0)
        {
            fInBoundaries = true;
        }
    }

    return fInBoundaries;
}

#pragma endregion
