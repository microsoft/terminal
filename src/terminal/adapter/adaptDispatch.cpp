// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "adaptDispatch.hpp"
#include "../../renderer/base/renderer.hpp"
#include "../../types/inc/GlyphWidth.hpp"
#include "../../types/inc/Viewport.hpp"
#include "../../types/inc/utils.hpp"
#include "../../inc/unicode.hpp"
#include "../parser/ascii.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::VirtualTerminal;

static constexpr std::wstring_view whitespace{ L" " };

AdaptDispatch::AdaptDispatch(ITerminalApi& api, Renderer& renderer, RenderSettings& renderSettings, TerminalInput& terminalInput) :
    _api{ api },
    _renderer{ renderer },
    _renderSettings{ renderSettings },
    _terminalInput{ terminalInput },
    _usingAltBuffer(false),
    _termOutput()
{
}

// Routine Description:
// - Translates and displays a single character
// Arguments:
// - wchPrintable - Printable character
// Return Value:
// - <none>
void AdaptDispatch::Print(const wchar_t wchPrintable)
{
    const auto wchTranslated = _termOutput.TranslateKey(wchPrintable);
    // By default the DEL character is meant to be ignored in the same way as a
    // NUL character. However, it's possible that it could be translated to a
    // printable character in a 96-character set. This condition makes sure that
    // a character is only output if the DEL is translated to something else.
    if (wchTranslated != AsciiChars::DEL)
    {
        _WriteToBuffer({ &wchTranslated, 1 });
    }
}

// Routine Description
// - Forward an entire string through. May translate, if necessary, to key input sequences
//   based on the locale
// Arguments:
// - string - Text to display
// Return Value:
// - <none>
void AdaptDispatch::PrintString(const std::wstring_view string)
{
    if (_termOutput.NeedToTranslate())
    {
        std::wstring buffer;
        buffer.reserve(string.size());
        for (auto& wch : string)
        {
            buffer.push_back(_termOutput.TranslateKey(wch));
        }
        _WriteToBuffer(buffer);
    }
    else
    {
        _WriteToBuffer(string);
    }
}

void AdaptDispatch::_WriteToBuffer(const std::wstring_view string)
{
    auto& textBuffer = _api.GetTextBuffer();
    auto& cursor = textBuffer.GetCursor();
    auto cursorPosition = cursor.GetPosition();
    const auto wrapAtEOL = _api.GetSystemMode(ITerminalApi::Mode::AutoWrap);
    const auto& attributes = textBuffer.GetCurrentAttributes();

    const auto viewport = _api.GetViewport();
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(viewport, true);
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(textBuffer.GetSize().Width());

    auto lineWidth = textBuffer.GetLineWidth(cursorPosition.y);
    if (cursorPosition.x <= rightMargin && cursorPosition.y >= topMargin && cursorPosition.y <= bottomMargin)
    {
        lineWidth = std::min(lineWidth, rightMargin + 1);
    }

    // Turn off the cursor until we're done, so it isn't refreshed unnecessarily.
    cursor.SetIsOn(false);

    RowWriteState state{
        .text = string,
        .columnLimit = lineWidth,
    };

    while (!state.text.empty())
    {
        if (cursor.IsDelayedEOLWrap() && wrapAtEOL)
        {
            const auto delayedCursorPosition = cursor.GetDelayedAtPosition();
            cursor.ResetDelayEOLWrap();
            // Only act on a delayed EOL if we didn't move the cursor to a
            // different position from where the EOL was marked.
            if (delayedCursorPosition == cursorPosition)
            {
                _DoLineFeed(textBuffer, true, true);
                cursorPosition = cursor.GetPosition();
                // We need to recalculate the width when moving to a new line.
                lineWidth = textBuffer.GetLineWidth(cursorPosition.y);
                if (cursorPosition.y >= topMargin && cursorPosition.y <= bottomMargin)
                {
                    lineWidth = std::min(lineWidth, rightMargin + 1);
                }
                state.columnLimit = lineWidth;
            }
        }

        if (_modes.test(Mode::InsertReplace))
        {
            // If insert-replace mode is enabled, we first measure how many cells
            // the string will occupy, and scroll the target area right by that
            // amount to make space for the incoming text.
            const OutputCellIterator it(state.text, attributes);
            auto measureIt = it;
            while (measureIt && measureIt.GetCellDistance(it) < state.columnLimit)
            {
                ++measureIt;
            }
            const auto row = cursorPosition.y;
            const auto cellCount = measureIt.GetCellDistance(it);
            _ScrollRectHorizontally(textBuffer, { cursorPosition.x, row, state.columnLimit, row + 1 }, cellCount);
        }

        state.columnBegin = cursorPosition.x;

        const auto textPositionBefore = state.text.data();
        textBuffer.Write(cursorPosition.y, attributes, state);
        const auto textPositionAfter = state.text.data();

        // TODO: A row should not be marked as wrapped just because we wrote the last column.
        // It should be marked whenever we write _past_ it (above, _DoLineFeed call). See GH#15602.
        if (wrapAtEOL && state.columnEnd >= state.columnLimit)
        {
            textBuffer.SetWrapForced(cursorPosition.y, true);
        }

        if (state.columnBeginDirty != state.columnEndDirty)
        {
            const til::rect changedRect{ state.columnBeginDirty, cursorPosition.y, state.columnEndDirty, cursorPosition.y + 1 };
            _api.NotifyAccessibilityChange(changedRect);
        }

        // If we're past the end of the line, we need to clamp the cursor
        // back into range, and if wrapping is enabled, set the delayed wrap
        // flag. The wrapping only occurs once another character is output.
        const auto isWrapping = state.columnEnd >= state.columnLimit;
        cursorPosition.x = isWrapping ? state.columnLimit - 1 : state.columnEnd;
        cursor.SetPosition(cursorPosition);

        if (isWrapping)
        {
            // We want to wrap, but we failed to write even a single character into the row.
            // ROW::Write() returns the lineWidth and leaves stringIterator untouched. To prevent a
            // deadlock, because stringIterator never advances, we need to throw that glyph away.
            //
            // This can happen under two circumstances:
            // * The glyph is wider than the buffer and can never be inserted in
            //   the first place. There's no good way to detect this, so we check
            //   whether the begin column is the left margin, which is the column
            //   at which any legit insertion should work at a minimum.
            // * The DECAWM Autowrap mode is disabled ("\x1b[?7l", !wrapAtEOL) and
            //   we tried writing a wide glyph into the last column which can't work.
            if (textPositionBefore == textPositionAfter && (state.columnBegin == 0 || !wrapAtEOL))
            {
                state.text = state.text.substr(textBuffer.GraphemeNext(state.text, 0));
            }

            if (wrapAtEOL)
            {
                cursor.DelayEOLWrap();
            }
        }
    }

    _ApplyCursorMovementFlags(cursor);

    // Notify UIA of new text.
    // It's important to do this here instead of in TextBuffer, because here you
    // have access to the entire line of text, whereas TextBuffer writes it one
    // character at a time via the OutputCellIterator.
    textBuffer.TriggerNewTextNotification(string);
}

// Routine Description:
// - CUU - Handles cursor upward movement by given distance.
// CUU and CUD are handled separately from other CUP sequences, because they are
//      constrained by the margins.
// See: https://vt100.net/docs/vt510-rm/CUU.html
//  "The cursor stops at the top margin. If the cursor is already above the top
//   margin, then the cursor stops at the top line."
// Arguments:
// - distance - Distance to move
// Return Value:
// - True.
bool AdaptDispatch::CursorUp(const VTInt distance)
{
    return _CursorMovePosition(Offset::Backward(distance), Offset::Unchanged(), true);
}

// Routine Description:
// - CUD - Handles cursor downward movement by given distance
// CUU and CUD are handled separately from other CUP sequences, because they are
//      constrained by the margins.
// See: https://vt100.net/docs/vt510-rm/CUD.html
//  "The cursor stops at the bottom margin. If the cursor is already above the
//   bottom margin, then the cursor stops at the bottom line."
// Arguments:
// - distance - Distance to move
// Return Value:
// - True.
bool AdaptDispatch::CursorDown(const VTInt distance)
{
    return _CursorMovePosition(Offset::Forward(distance), Offset::Unchanged(), true);
}

// Routine Description:
// - CUF - Handles cursor forward movement by given distance
// Arguments:
// - distance - Distance to move
// Return Value:
// - True.
bool AdaptDispatch::CursorForward(const VTInt distance)
{
    return _CursorMovePosition(Offset::Unchanged(), Offset::Forward(distance), true);
}

// Routine Description:
// - CUB - Handles cursor backward movement by given distance
// Arguments:
// - distance - Distance to move
// Return Value:
// - True.
bool AdaptDispatch::CursorBackward(const VTInt distance)
{
    return _CursorMovePosition(Offset::Unchanged(), Offset::Backward(distance), true);
}

// Routine Description:
// - CNL - Handles cursor movement to the following line (or N lines down)
// - Moves to the beginning X/Column position of the line.
// Arguments:
// - distance - Distance to move
// Return Value:
// - True.
bool AdaptDispatch::CursorNextLine(const VTInt distance)
{
    return _CursorMovePosition(Offset::Forward(distance), Offset::Absolute(1), true);
}

// Routine Description:
// - CPL - Handles cursor movement to the previous line (or N lines up)
// - Moves to the beginning X/Column position of the line.
// Arguments:
// - distance - Distance to move
// Return Value:
// - True.
bool AdaptDispatch::CursorPrevLine(const VTInt distance)
{
    return _CursorMovePosition(Offset::Backward(distance), Offset::Absolute(1), true);
}

// Routine Description:
// - Returns the coordinates of the vertical scroll margins.
// Arguments:
// - viewport - The viewport rect (exclusive).
// - absolute - Should coordinates be absolute or relative to the viewport.
// Return Value:
// - A std::pair containing the top and bottom coordinates (inclusive).
std::pair<int, int> AdaptDispatch::_GetVerticalMargins(const til::rect& viewport, const bool absolute) noexcept
{
    // If the top is out of range, reset the margins completely.
    const auto bottommostRow = viewport.bottom - viewport.top - 1;
    if (_scrollMargins.top >= bottommostRow)
    {
        _scrollMargins.top = _scrollMargins.bottom = 0;
    }
    // If margins aren't set, use the full extent of the viewport.
    const auto marginsSet = _scrollMargins.top < _scrollMargins.bottom;
    auto topMargin = marginsSet ? _scrollMargins.top : 0;
    auto bottomMargin = marginsSet ? _scrollMargins.bottom : bottommostRow;
    // If the bottom is out of range, clamp it to the bottommost row.
    bottomMargin = std::min(bottomMargin, bottommostRow);
    if (absolute)
    {
        topMargin += viewport.top;
        bottomMargin += viewport.top;
    }
    return { topMargin, bottomMargin };
}

// Routine Description:
// - Returns the coordinates of the horizontal scroll margins.
// Arguments:
// - bufferWidth - The width of the buffer
// Return Value:
// - A std::pair containing the left and right coordinates (inclusive).
std::pair<int, int> AdaptDispatch::_GetHorizontalMargins(const til::CoordType bufferWidth) noexcept
{
    // If the left is out of range, reset the margins completely.
    const auto rightmostColumn = bufferWidth - 1;
    if (_scrollMargins.left >= rightmostColumn)
    {
        _scrollMargins.left = _scrollMargins.right = 0;
    }
    // If margins aren't set, use the full extent of the buffer.
    const auto marginsSet = _scrollMargins.left < _scrollMargins.right;
    auto leftMargin = marginsSet ? _scrollMargins.left : 0;
    auto rightMargin = marginsSet ? _scrollMargins.right : rightmostColumn;
    // If the right is out of range, clamp it to the rightmost column.
    rightMargin = std::min(rightMargin, rightmostColumn);
    return { leftMargin, rightMargin };
}

// Routine Description:
// - Generalizes cursor movement to a specific position, which can be absolute or relative.
// Arguments:
// - rowOffset - The row to move to
// - colOffset - The column to move to
// - clampInMargins - Should the position be clamped within the scrolling margins
// Return Value:
// - True.
bool AdaptDispatch::_CursorMovePosition(const Offset rowOffset, const Offset colOffset, const bool clampInMargins)
{
    // First retrieve some information about the buffer
    const auto viewport = _api.GetViewport();
    auto& textBuffer = _api.GetTextBuffer();
    auto& cursor = textBuffer.GetCursor();
    const auto bufferWidth = textBuffer.GetSize().Width();
    const auto cursorPosition = cursor.GetPosition();
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(viewport, true);
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(bufferWidth);

    // For relative movement, the given offsets will be relative to
    // the current cursor position.
    auto row = cursorPosition.y;
    auto col = cursorPosition.x;

    // But if the row is absolute, it will be relative to the top of the
    // viewport, or the top margin, depending on the origin mode.
    if (rowOffset.IsAbsolute)
    {
        row = _modes.test(Mode::Origin) ? topMargin : viewport.top;
    }

    // And if the column is absolute, it'll be relative to column 0,
    // or the left margin, depending on the origin mode.
    // Horizontal positions are not affected by the viewport.
    if (colOffset.IsAbsolute)
    {
        col = _modes.test(Mode::Origin) ? leftMargin : 0;
    }

    // Adjust the base position by the given offsets and clamp the results.
    // The row is constrained within the viewport's vertical boundaries,
    // while the column is constrained by the buffer width.
    row = std::clamp(row + rowOffset.Value, viewport.top, viewport.bottom - 1);
    col = std::clamp(col + colOffset.Value, 0, bufferWidth - 1);

    // If the operation needs to be clamped inside the margins, or the origin
    // mode is relative (which always requires margin clamping), then the row
    // and column may need to be adjusted further.
    if (clampInMargins || _modes.test(Mode::Origin))
    {
        // Vertical margins only apply if the original position is inside the
        // horizontal margins. Also, the cursor will only be clamped inside the
        // top margin if it was already below the top margin to start with, and
        // it will only be clamped inside the bottom margin if it was already
        // above the bottom margin to start with.
        if (cursorPosition.x >= leftMargin && cursorPosition.x <= rightMargin)
        {
            if (cursorPosition.y >= topMargin)
            {
                row = std::max(row, topMargin);
            }
            if (cursorPosition.y <= bottomMargin)
            {
                row = std::min(row, bottomMargin);
            }
        }
        // Similarly, horizontal margins only apply if the new row is inside the
        // vertical margins. And the cursor is only clamped inside the horizontal
        // margins if it was already inside to start with.
        if (row >= topMargin && row <= bottomMargin)
        {
            if (cursorPosition.x >= leftMargin)
            {
                col = std::max(col, leftMargin);
            }
            if (cursorPosition.x <= rightMargin)
            {
                col = std::min(col, rightMargin);
            }
        }
    }

    // Finally, attempt to set the adjusted cursor position back into the console.
    cursor.SetPosition(textBuffer.ClampPositionWithinLine({ col, row }));
    _ApplyCursorMovementFlags(cursor);

    return true;
}

// Routine Description:
// - Helper method which applies a bunch of flags that are typically set whenever
//   the cursor is moved. The IsOn flag is set to true, and the Delay flag to false,
//   to force a blinking cursor to be visible, so the user can immediately see the
//   new position. The HasMoved flag is set to let the accessibility notifier know
//   that there was movement that needs to be reported.
// Arguments:
// - cursor - The cursor instance to be updated
// Return Value:
// - <none>
void AdaptDispatch::_ApplyCursorMovementFlags(Cursor& cursor) noexcept
{
    cursor.SetDelay(false);
    cursor.SetIsOn(true);
    cursor.SetHasMoved(true);
}

// Routine Description:
// - CHA - Moves the cursor to an exact X/Column position on the current line.
// Arguments:
// - column - Specific X/Column position to move to
// Return Value:
// - True.
bool AdaptDispatch::CursorHorizontalPositionAbsolute(const VTInt column)
{
    return _CursorMovePosition(Offset::Unchanged(), Offset::Absolute(column), false);
}

// Routine Description:
// - VPA - Moves the cursor to an exact Y/row position on the current column.
// Arguments:
// - line - Specific Y/Row position to move to
// Return Value:
// - True.
bool AdaptDispatch::VerticalLinePositionAbsolute(const VTInt line)
{
    return _CursorMovePosition(Offset::Absolute(line), Offset::Unchanged(), false);
}

// Routine Description:
// - HPR - Handles cursor forward movement by given distance
// - Unlike CUF, this is not constrained by margin settings.
// Arguments:
// - distance - Distance to move
// Return Value:
// - True.
bool AdaptDispatch::HorizontalPositionRelative(const VTInt distance)
{
    return _CursorMovePosition(Offset::Unchanged(), Offset::Forward(distance), false);
}

// Routine Description:
// - VPR - Handles cursor downward movement by given distance
// - Unlike CUD, this is not constrained by margin settings.
// Arguments:
// - distance - Distance to move
// Return Value:
// - True.
bool AdaptDispatch::VerticalPositionRelative(const VTInt distance)
{
    return _CursorMovePosition(Offset::Forward(distance), Offset::Unchanged(), false);
}

// Routine Description:
// - CUP - Moves the cursor to an exact X/Column and Y/Row/Line coordinate position.
// Arguments:
// - line - Specific Y/Row/Line position to move to
// - column - Specific X/Column position to move to
// Return Value:
// - True.
bool AdaptDispatch::CursorPosition(const VTInt line, const VTInt column)
{
    return _CursorMovePosition(Offset::Absolute(line), Offset::Absolute(column), false);
}

// Routine Description:
// - DECSC - Saves the current "cursor state" into a memory buffer. This
//   includes the cursor position, origin mode, graphic rendition, and
//   active character set.
// Arguments:
// - <none>
// Return Value:
// - True.
bool AdaptDispatch::CursorSaveState()
{
    // First retrieve some information about the buffer
    const auto viewport = _api.GetViewport();
    const auto& textBuffer = _api.GetTextBuffer();
    const auto& attributes = textBuffer.GetCurrentAttributes();

    // The cursor is given to us by the API as relative to the whole buffer.
    // But in VT speak, the cursor row should be relative to the current viewport top.
    auto cursorPosition = textBuffer.GetCursor().GetPosition();
    cursorPosition.y -= viewport.top;

    // Although if origin mode is set, the cursor is relative to the margin origin.
    if (_modes.test(Mode::Origin))
    {
        cursorPosition.x -= _GetHorizontalMargins(textBuffer.GetSize().Width()).first;
        cursorPosition.y -= _GetVerticalMargins(viewport, false).first;
    }

    // VT is also 1 based, not 0 based, so correct by 1.
    auto& savedCursorState = _savedCursorState.at(_usingAltBuffer);
    savedCursorState.Column = cursorPosition.x + 1;
    savedCursorState.Row = cursorPosition.y + 1;
    savedCursorState.IsDelayedEOLWrap = textBuffer.GetCursor().IsDelayedEOLWrap();
    savedCursorState.IsOriginModeRelative = _modes.test(Mode::Origin);
    savedCursorState.Attributes = attributes;
    savedCursorState.TermOutput = _termOutput;

    return true;
}

// Routine Description:
// - DECRC - Restores a saved "cursor state" from the DECSC command back into
//   the console state. This includes the cursor position, origin mode, graphic
//   rendition, and active character set.
// Arguments:
// - <none>
// Return Value:
// - True.
bool AdaptDispatch::CursorRestoreState()
{
    auto& savedCursorState = _savedCursorState.at(_usingAltBuffer);

    // Restore the origin mode first, since the cursor coordinates may be relative.
    _modes.set(Mode::Origin, savedCursorState.IsOriginModeRelative);

    // We can then restore the position with a standard CUP operation.
    CursorPosition(savedCursorState.Row, savedCursorState.Column);

    // If the delayed wrap flag was set when the cursor was saved, we need to restore that now.
    if (savedCursorState.IsDelayedEOLWrap)
    {
        _api.GetTextBuffer().GetCursor().DelayEOLWrap();
    }

    // Restore text attributes.
    _api.SetTextAttributes(savedCursorState.Attributes);

    // Restore designated character sets.
    _termOutput.RestoreFrom(savedCursorState.TermOutput);

    return true;
}

// Routine Description:
// - Returns the attributes that should be used when erasing the buffer. When
//   the Erase Color mode is set, we use the default attributes, but when reset,
//   we use the active color attributes with the character attributes cleared.
// Arguments:
// - textBuffer - Target buffer that is being erased.
// Return Value:
// - The erase TextAttribute value.
TextAttribute AdaptDispatch::_GetEraseAttributes(const TextBuffer& textBuffer) const noexcept
{
    if (_modes.test(Mode::EraseColor))
    {
        return {};
    }
    else
    {
        auto eraseAttributes = textBuffer.GetCurrentAttributes();
        eraseAttributes.SetStandardErase();
        return eraseAttributes;
    }
}

// Routine Description:
// - Scrolls an area of the buffer in a vertical direction.
// Arguments:
// - textBuffer - Target buffer to be scrolled.
// - fillRect - Area of the buffer that will be affected.
// - delta - Distance to move (positive is down, negative is up).
// Return Value:
// - <none>
void AdaptDispatch::_ScrollRectVertically(TextBuffer& textBuffer, const til::rect& scrollRect, const VTInt delta)
{
    const auto absoluteDelta = std::min(std::abs(delta), scrollRect.height());
    if (absoluteDelta < scrollRect.height())
    {
        const auto top = delta > 0 ? scrollRect.top : scrollRect.top + absoluteDelta;
        const auto width = scrollRect.width();
        const auto height = scrollRect.height() - absoluteDelta;
        const auto actualDelta = delta > 0 ? absoluteDelta : -absoluteDelta;
        if (width == textBuffer.GetSize().Width())
        {
            // If the scrollRect is the full width of the buffer, we can scroll
            // more efficiently by rotating the row storage.
            textBuffer.ScrollRows(top, height, actualDelta);
            textBuffer.TriggerRedraw(Viewport::FromExclusive(scrollRect));
        }
        else
        {
            // Otherwise we have to move the content up or down by copying the
            // requested buffer range one cell at a time.
            const auto srcOrigin = til::point{ scrollRect.left, top };
            const auto dstOrigin = til::point{ scrollRect.left, top + actualDelta };
            const auto srcView = Viewport::FromDimensions(srcOrigin, width, height);
            const auto dstView = Viewport::FromDimensions(dstOrigin, width, height);
            const auto walkDirection = Viewport::DetermineWalkDirection(srcView, dstView);
            auto srcPos = srcView.GetWalkOrigin(walkDirection);
            auto dstPos = dstView.GetWalkOrigin(walkDirection);
            do
            {
                const auto current = OutputCell(*textBuffer.GetCellDataAt(srcPos));
                textBuffer.WriteLine(OutputCellIterator({ &current, 1 }), dstPos);
                srcView.WalkInBounds(srcPos, walkDirection);
            } while (dstView.WalkInBounds(dstPos, walkDirection));
        }
    }

    // Rows revealed by the scroll are filled with standard erase attributes.
    auto eraseRect = scrollRect;
    eraseRect.top = delta > 0 ? scrollRect.top : (scrollRect.bottom - absoluteDelta);
    eraseRect.bottom = eraseRect.top + absoluteDelta;
    const auto eraseAttributes = _GetEraseAttributes(textBuffer);
    _FillRect(textBuffer, eraseRect, whitespace, eraseAttributes);

    // Also reset the line rendition for the erased rows.
    textBuffer.ResetLineRenditionRange(eraseRect.top, eraseRect.bottom);
}

// Routine Description:
// - Scrolls an area of the buffer in a horizontal direction.
// Arguments:
// - textBuffer - Target buffer to be scrolled.
// - fillRect - Area of the buffer that will be affected.
// - delta - Distance to move (positive is right, negative is left).
// Return Value:
// - <none>
void AdaptDispatch::_ScrollRectHorizontally(TextBuffer& textBuffer, const til::rect& scrollRect, const VTInt delta)
{
    const auto absoluteDelta = std::min(std::abs(delta), scrollRect.width());
    if (absoluteDelta < scrollRect.width())
    {
        const auto left = delta > 0 ? scrollRect.left : (scrollRect.left + absoluteDelta);
        const auto top = scrollRect.top;
        const auto width = scrollRect.width() - absoluteDelta;
        const auto height = scrollRect.height();
        const auto actualDelta = delta > 0 ? absoluteDelta : -absoluteDelta;

        const auto source = Viewport::FromDimensions({ left, top }, width, height);
        const auto target = Viewport::Offset(source, { actualDelta, 0 });
        const auto walkDirection = Viewport::DetermineWalkDirection(source, target);
        auto sourcePos = source.GetWalkOrigin(walkDirection);
        auto targetPos = target.GetWalkOrigin(walkDirection);
        // Note that we read two cells from the source before we start writing
        // to the target, so a two-cell DBCS character can't accidentally delete
        // itself when moving one cell horizontally.
        auto next = OutputCell(*textBuffer.GetCellDataAt(sourcePos));
        do
        {
            const auto current = next;
            source.WalkInBounds(sourcePos, walkDirection);
            next = OutputCell(*textBuffer.GetCellDataAt(sourcePos));
            textBuffer.WriteLine(OutputCellIterator({ &current, 1 }), targetPos);
        } while (target.WalkInBounds(targetPos, walkDirection));
    }

    // Columns revealed by the scroll are filled with standard erase attributes.
    auto eraseRect = scrollRect;
    eraseRect.left = delta > 0 ? scrollRect.left : (scrollRect.right - absoluteDelta);
    eraseRect.right = eraseRect.left + absoluteDelta;
    const auto eraseAttributes = _GetEraseAttributes(textBuffer);
    _FillRect(textBuffer, eraseRect, whitespace, eraseAttributes);
}

// Routine Description:
// - This helper will do the work of performing an insert or delete character operation
// - Both operations are similar in that they cut text and move it left or right in the buffer, padding the leftover area with spaces.
// Arguments:
// - delta - Number of characters to modify (positive if inserting, negative if deleting).
// Return Value:
// - <none>
void AdaptDispatch::_InsertDeleteCharacterHelper(const VTInt delta)
{
    const auto viewport = _api.GetViewport();
    auto& textBuffer = _api.GetTextBuffer();
    const auto row = textBuffer.GetCursor().GetPosition().y;
    const auto col = textBuffer.GetCursor().GetPosition().x;
    const auto lineWidth = textBuffer.GetLineWidth(row);
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(viewport, true);
    const auto [leftMargin, rightMargin] = (row >= topMargin && row <= bottomMargin) ?
                                               _GetHorizontalMargins(lineWidth) :
                                               std::make_pair(0, lineWidth - 1);
    if (col >= leftMargin && col <= rightMargin)
    {
        _ScrollRectHorizontally(textBuffer, { col, row, rightMargin + 1, row + 1 }, delta);
        // The ICH and DCH controls are expected to reset the delayed wrap flag.
        textBuffer.GetCursor().ResetDelayEOLWrap();
    }
}

// Routine Description:
// ICH - Insert Character - Blank/default attribute characters will be inserted at the current cursor position.
//     - Each inserted character will push all text in the row to the right.
// Arguments:
// - count - The number of characters to insert
// Return Value:
// - True.
bool AdaptDispatch::InsertCharacter(const VTInt count)
{
    _InsertDeleteCharacterHelper(count);
    return true;
}

// Routine Description:
// DCH - Delete Character - The character at the cursor position will be deleted. Blank/attribute characters will
//       be inserted from the right edge of the current line.
// Arguments:
// - count - The number of characters to delete
// Return Value:
// - True.
bool AdaptDispatch::DeleteCharacter(const VTInt count)
{
    _InsertDeleteCharacterHelper(-count);
    return true;
}

// Routine Description:
// - Fills an area of the buffer with a given character and attributes.
// Arguments:
// - textBuffer - Target buffer to be filled.
// - fillRect - Area of the buffer that will be affected.
// - fillChar - Character to be written to the buffer.
// - fillAttrs - Attributes to be written to the buffer.
// Return Value:
// - <none>
void AdaptDispatch::_FillRect(TextBuffer& textBuffer, const til::rect& fillRect, const std::wstring_view& fillChar, const TextAttribute& fillAttrs) const
{
    textBuffer.FillRect(fillRect, fillChar, fillAttrs);
    _api.NotifyAccessibilityChange(fillRect);
}

// Routine Description:
// - ECH - Erase Characters from the current cursor position, by replacing
//     them with a space. This will only erase characters in the current line,
//     and won't wrap to the next. The attributes of any erased positions
//     receive the currently selected attributes.
// Arguments:
// - numChars - The number of characters to erase.
// Return Value:
// - True.
bool AdaptDispatch::EraseCharacters(const VTInt numChars)
{
    auto& textBuffer = _api.GetTextBuffer();
    const auto row = textBuffer.GetCursor().GetPosition().y;
    const auto startCol = textBuffer.GetCursor().GetPosition().x;
    const auto endCol = std::min<VTInt>(startCol + numChars, textBuffer.GetLineWidth(row));

    // The ECH control is expected to reset the delayed wrap flag.
    textBuffer.GetCursor().ResetDelayEOLWrap();

    const auto eraseAttributes = _GetEraseAttributes(textBuffer);
    _FillRect(textBuffer, { startCol, row, endCol, row + 1 }, whitespace, eraseAttributes);

    return true;
}

// Routine Description:
// - ED - Erases a portion of the current viewable area (viewport) of the console.
// Arguments:
// - eraseType - Determines whether to erase:
//      From beginning (top-left corner) to the cursor
//      From cursor to end (bottom-right corner)
//      The entire viewport area
//      The scrollback (outside the viewport area)
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::EraseInDisplay(const DispatchTypes::EraseType eraseType)
{
    RETURN_BOOL_IF_FALSE(eraseType <= DispatchTypes::EraseType::Scrollback);

    // First things first. If this is a "Scrollback" clear, then just do that.
    // Scrollback clears erase everything in the "scrollback" of a *nix terminal
    //      Everything that's scrolled off the screen so far.
    // Or if it's an Erase All, then we also need to handle that specially
    //      by moving the current contents of the viewport into the scrollback.
    if (eraseType == DispatchTypes::EraseType::Scrollback)
    {
        return _EraseScrollback();
    }
    else if (eraseType == DispatchTypes::EraseType::All)
    {
        return _EraseAll();
    }

    const auto viewport = _api.GetViewport();
    auto& textBuffer = _api.GetTextBuffer();
    const auto bufferWidth = textBuffer.GetSize().Width();
    const auto row = textBuffer.GetCursor().GetPosition().y;
    const auto col = textBuffer.GetCursor().GetPosition().x;

    // The ED control is expected to reset the delayed wrap flag.
    // The special case variants above ("erase all" and "erase scrollback")
    // take care of that themselves when they set the cursor position.
    textBuffer.GetCursor().ResetDelayEOLWrap();

    const auto eraseAttributes = _GetEraseAttributes(textBuffer);

    // When erasing the display, every line that is erased in full should be
    // reset to single width. When erasing to the end, this could include
    // the current line, if the cursor is in the first column. When erasing
    // from the beginning, though, the current line would never be included,
    // because the cursor could never be in the rightmost column (assuming
    // the line is double width).
    if (eraseType == DispatchTypes::EraseType::FromBeginning)
    {
        textBuffer.ResetLineRenditionRange(viewport.top, row);
        _FillRect(textBuffer, { 0, viewport.top, bufferWidth, row }, whitespace, eraseAttributes);
        _FillRect(textBuffer, { 0, row, col + 1, row + 1 }, whitespace, eraseAttributes);
    }
    if (eraseType == DispatchTypes::EraseType::ToEnd)
    {
        textBuffer.ResetLineRenditionRange(col > 0 ? row + 1 : row, viewport.bottom);
        _FillRect(textBuffer, { col, row, bufferWidth, row + 1 }, whitespace, eraseAttributes);
        _FillRect(textBuffer, { 0, row + 1, bufferWidth, viewport.bottom }, whitespace, eraseAttributes);
    }

    return true;
}

// Routine Description:
// - EL - Erases the line that the cursor is currently on.
// Arguments:
// - eraseType - Determines whether to erase: From beginning (left edge) to the cursor, from cursor to end (right edge), or the entire line.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::EraseInLine(const DispatchTypes::EraseType eraseType)
{
    auto& textBuffer = _api.GetTextBuffer();
    const auto row = textBuffer.GetCursor().GetPosition().y;
    const auto col = textBuffer.GetCursor().GetPosition().x;

    // The EL control is expected to reset the delayed wrap flag.
    textBuffer.GetCursor().ResetDelayEOLWrap();

    const auto eraseAttributes = _GetEraseAttributes(textBuffer);
    switch (eraseType)
    {
    case DispatchTypes::EraseType::FromBeginning:
        _FillRect(textBuffer, { 0, row, col + 1, row + 1 }, whitespace, eraseAttributes);
        return true;
    case DispatchTypes::EraseType::ToEnd:
        _FillRect(textBuffer, { col, row, textBuffer.GetLineWidth(row), row + 1 }, whitespace, eraseAttributes);
        return true;
    case DispatchTypes::EraseType::All:
        _FillRect(textBuffer, { 0, row, textBuffer.GetLineWidth(row), row + 1 }, whitespace, eraseAttributes);
        return true;
    default:
        return false;
    }
}

// Routine Description:
// - Selectively erases unprotected cells in an area of the buffer.
// Arguments:
// - textBuffer - Target buffer to be erased.
// - eraseRect - Area of the buffer that will be affected.
// Return Value:
// - <none>
void AdaptDispatch::_SelectiveEraseRect(TextBuffer& textBuffer, const til::rect& eraseRect)
{
    if (eraseRect)
    {
        for (auto row = eraseRect.top; row < eraseRect.bottom; row++)
        {
            auto& rowBuffer = textBuffer.GetMutableRowByOffset(row);
            for (auto col = eraseRect.left; col < eraseRect.right; col++)
            {
                // Only unprotected cells are affected.
                if (!rowBuffer.GetAttrByColumn(col).IsProtected())
                {
                    // The text is cleared but the attributes are left as is.
                    rowBuffer.ClearCell(col);
                    textBuffer.TriggerRedraw(Viewport::FromCoord({ col, row }));
                }
            }
        }
        _api.NotifyAccessibilityChange(eraseRect);
    }
}

// Routine Description:
// - DECSED - Selectively erases unprotected cells in a portion of the viewport.
// Arguments:
// - eraseType - Determines whether to erase:
//      From beginning (top-left corner) to the cursor
//      From cursor to end (bottom-right corner)
//      The entire viewport area
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::SelectiveEraseInDisplay(const DispatchTypes::EraseType eraseType)
{
    const auto viewport = _api.GetViewport();
    auto& textBuffer = _api.GetTextBuffer();
    const auto bufferWidth = textBuffer.GetSize().Width();
    const auto row = textBuffer.GetCursor().GetPosition().y;
    const auto col = textBuffer.GetCursor().GetPosition().x;

    // The DECSED control is expected to reset the delayed wrap flag.
    textBuffer.GetCursor().ResetDelayEOLWrap();

    switch (eraseType)
    {
    case DispatchTypes::EraseType::FromBeginning:
        _SelectiveEraseRect(textBuffer, { 0, viewport.top, bufferWidth, row });
        _SelectiveEraseRect(textBuffer, { 0, row, col + 1, row + 1 });
        return true;
    case DispatchTypes::EraseType::ToEnd:
        _SelectiveEraseRect(textBuffer, { col, row, bufferWidth, row + 1 });
        _SelectiveEraseRect(textBuffer, { 0, row + 1, bufferWidth, viewport.bottom });
        return true;
    case DispatchTypes::EraseType::All:
        _SelectiveEraseRect(textBuffer, { 0, viewport.top, bufferWidth, viewport.bottom });
        return true;
    default:
        return false;
    }
}

// Routine Description:
// - DECSEL - Selectively erases unprotected cells on line with the cursor.
// Arguments:
// - eraseType - Determines whether to erase:
//      From beginning (left edge) to the cursor
//      From cursor to end (right edge)
//      The entire line.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::SelectiveEraseInLine(const DispatchTypes::EraseType eraseType)
{
    auto& textBuffer = _api.GetTextBuffer();
    const auto row = textBuffer.GetCursor().GetPosition().y;
    const auto col = textBuffer.GetCursor().GetPosition().x;

    // The DECSEL control is expected to reset the delayed wrap flag.
    textBuffer.GetCursor().ResetDelayEOLWrap();

    switch (eraseType)
    {
    case DispatchTypes::EraseType::FromBeginning:
        _SelectiveEraseRect(textBuffer, { 0, row, col + 1, row + 1 });
        return true;
    case DispatchTypes::EraseType::ToEnd:
        _SelectiveEraseRect(textBuffer, { col, row, textBuffer.GetLineWidth(row), row + 1 });
        return true;
    case DispatchTypes::EraseType::All:
        _SelectiveEraseRect(textBuffer, { 0, row, textBuffer.GetLineWidth(row), row + 1 });
        return true;
    default:
        return false;
    }
}

// Routine Description:
// - Changes the attributes of each cell in a rectangular area of the buffer.
// Arguments:
// - textBuffer - Target buffer to be changed.
// - changeRect - A rectangular area of the buffer that will be affected.
// - changeOps - Changes that will be applied to each of the attributes.
// Return Value:
// - <none>
void AdaptDispatch::_ChangeRectAttributes(TextBuffer& textBuffer, const til::rect& changeRect, const ChangeOps& changeOps)
{
    if (changeRect)
    {
        for (auto row = changeRect.top; row < changeRect.bottom; row++)
        {
            auto& rowBuffer = textBuffer.GetMutableRowByOffset(row);
            for (auto col = changeRect.left; col < changeRect.right; col++)
            {
                auto attr = rowBuffer.GetAttrByColumn(col);
                auto characterAttributes = attr.GetCharacterAttributes();
                characterAttributes &= changeOps.andAttrMask;
                characterAttributes ^= changeOps.xorAttrMask;
                attr.SetCharacterAttributes(characterAttributes);
                if (changeOps.foreground)
                {
                    attr.SetForeground(*changeOps.foreground);
                }
                if (changeOps.background)
                {
                    attr.SetBackground(*changeOps.background);
                }
                if (changeOps.underlineColor)
                {
                    attr.SetUnderlineColor(*changeOps.underlineColor);
                }
                rowBuffer.ReplaceAttributes(col, col + 1, attr);
            }
        }
        textBuffer.TriggerRedraw(Viewport::FromExclusive(changeRect));
        _api.NotifyAccessibilityChange(changeRect);
    }
}

// Routine Description:
// - Changes the attributes of each cell in an area of the buffer.
// Arguments:
// - changeArea - Area of the buffer that will be affected. This may be
//     interpreted as a rectangle or a stream depending on the state of the
//     RectangularChangeExtent mode.
// - changeOps - Changes that will be applied to each of the attributes.
// Return Value:
// - <none>
void AdaptDispatch::_ChangeRectOrStreamAttributes(const til::rect& changeArea, const ChangeOps& changeOps)
{
    auto& textBuffer = _api.GetTextBuffer();
    const auto bufferSize = textBuffer.GetSize().Dimensions();
    const auto changeRect = _CalculateRectArea(changeArea.top, changeArea.left, changeArea.bottom, changeArea.right, bufferSize);
    const auto lineCount = changeRect.height();

    // If the change extent is rectangular, we can apply the change with a
    // single call. The same is true for a stream extent that is only one line.
    if (_modes.test(Mode::RectangularChangeExtent) || lineCount == 1)
    {
        _ChangeRectAttributes(textBuffer, changeRect, changeOps);
    }
    // If the stream extent is more than one line we require three passes. The
    // top line is altered from the left offset up to the end of the line. The
    // bottom line is altered from the start up to the right offset. All the
    // lines in-between have their entire length altered. The right coordinate
    // must be greater than the left, otherwise the operation is ignored.
    else if (lineCount > 1 && changeRect.right > changeRect.left)
    {
        const auto bufferWidth = bufferSize.width;
        _ChangeRectAttributes(textBuffer, { changeRect.origin(), til::size{ bufferWidth - changeRect.left, 1 } }, changeOps);
        _ChangeRectAttributes(textBuffer, { { 0, changeRect.top + 1 }, til::size{ bufferWidth, lineCount - 2 } }, changeOps);
        _ChangeRectAttributes(textBuffer, { { 0, changeRect.bottom - 1 }, til::size{ changeRect.right, 1 } }, changeOps);
    }
}

// Routine Description:
// - Helper method to calculate the applicable buffer coordinates for use with
//   the various rectangular area operations.
// Arguments:
// - top - The first row of the area.
// - left - The first column of the area.
// - bottom - The last row of the area (inclusive).
// - right - The last column of the area (inclusive).
// - bufferSize - The size of the target buffer.
// Return value:
// - An exclusive rect with the absolute buffer coordinates.
til::rect AdaptDispatch::_CalculateRectArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right, const til::size bufferSize)
{
    const auto viewport = _api.GetViewport();

    // We start by calculating the margin offsets and maximum dimensions.
    // If the origin mode isn't set, we use the viewport extent.
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(viewport, false);
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(bufferSize.width);
    const auto yOffset = _modes.test(Mode::Origin) ? topMargin : 0;
    const auto yMaximum = _modes.test(Mode::Origin) ? bottomMargin + 1 : viewport.height();
    const auto xOffset = _modes.test(Mode::Origin) ? leftMargin : 0;
    const auto xMaximum = _modes.test(Mode::Origin) ? rightMargin + 1 : bufferSize.width;

    auto fillRect = til::inclusive_rect{};
    fillRect.left = left + xOffset;
    fillRect.top = top + yOffset;
    // Right and bottom default to the maximum dimensions.
    fillRect.right = (right ? right + xOffset : xMaximum);
    fillRect.bottom = (bottom ? bottom + yOffset : yMaximum);

    // We also clamp everything to the maximum dimensions, and subtract 1
    // to convert from VT coordinates which have an origin of 1;1.
    fillRect.left = std::min(fillRect.left, xMaximum) - 1;
    fillRect.right = std::min(fillRect.right, xMaximum) - 1;
    fillRect.top = std::min(fillRect.top, yMaximum) - 1;
    fillRect.bottom = std::min(fillRect.bottom, yMaximum) - 1;

    // To get absolute coordinates we offset with the viewport top.
    fillRect.top += viewport.top;
    fillRect.bottom += viewport.top;

    return til::rect{ fillRect };
}

// Routine Description:
// - DECCARA - Changes the attributes in a rectangular area. The affected range
//   is dependent on the change extent setting defined by DECSACE.
// Arguments:
// - top - The first row of the area.
// - left - The first column of the area.
// - bottom - The last row of the area (inclusive).
// - right - The last column of the area (inclusive).
// - attrs - The rendition attributes that will be applied to the area.
// Return Value:
// - True.
bool AdaptDispatch::ChangeAttributesRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right, const VTParameters attrs)
{
    auto changeOps = ChangeOps{};

    // We apply the attribute parameters to two TextAttribute instances: one
    // with no character attributes set, and one with all attributes set. This
    // provides us with an OR mask and an AND mask which can then be applied to
    // each cell to set and reset the appropriate attribute bits.
    auto allAttrsOff = TextAttribute{};
    auto allAttrsOn = TextAttribute{ 0, 0, 0 };
    allAttrsOn.SetCharacterAttributes(CharacterAttributes::All);
    _ApplyGraphicsOptions(attrs, allAttrsOff);
    _ApplyGraphicsOptions(attrs, allAttrsOn);
    const auto orAttrMask = allAttrsOff.GetCharacterAttributes();
    const auto andAttrMask = allAttrsOn.GetCharacterAttributes();
    // But to minimize the required ops, which we share with the DECRARA control
    // below, we want to use an XOR rather than OR. For that to work, we have to
    // combine the AND mask with the inverse of the OR mask in advance.
    changeOps.andAttrMask = andAttrMask & ~orAttrMask;
    changeOps.xorAttrMask = orAttrMask;

    // We also make use of the two TextAttributes calculated above to determine
    // whether colors need to be applied. Since allAttrsOff started off with
    // default colors, and allAttrsOn started with black, we know something has
    // been set if the former is no longer default, or the latter is now default.
    const auto foreground = allAttrsOff.GetForeground();
    const auto background = allAttrsOff.GetBackground();
    const auto foregroundChanged = !foreground.IsDefault() || allAttrsOn.GetForeground().IsDefault();
    const auto backgroundChanged = !background.IsDefault() || allAttrsOn.GetBackground().IsDefault();
    changeOps.foreground = foregroundChanged ? std::optional{ foreground } : std::nullopt;
    changeOps.background = backgroundChanged ? std::optional{ background } : std::nullopt;

    const auto underlineColor = allAttrsOff.GetUnderlineColor();
    const auto underlineColorChanged = !underlineColor.IsDefault() || allAttrsOn.GetUnderlineColor().IsDefault();
    changeOps.underlineColor = underlineColorChanged ? std::optional{ underlineColor } : std::nullopt;

    _ChangeRectOrStreamAttributes({ left, top, right, bottom }, changeOps);

    return true;
}

// Routine Description:
// - DECRARA - Reverses the attributes in a rectangular area. The affected range
//   is dependent on the change extent setting defined by DECSACE.
//   Note: Reversing the underline style has some unexpected consequences.
//         See https://github.com/microsoft/terminal/pull/15795#issuecomment-1702559350.
// Arguments:
// - top - The first row of the area.
// - left - The first column of the area.
// - bottom - The last row of the area (inclusive).
// - right - The last column of the area (inclusive).
// - attrs - The rendition attributes that will be applied to the area.
// Return Value:
// - True.
bool AdaptDispatch::ReverseAttributesRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right, const VTParameters attrs)
{
    // In order to create a mask of the attributes that we want to reverse, we
    // need to go through the options one by one, applying each of them to an
    // empty TextAttribute object from which can extract the effected bits. We
    // then combine them with XOR, because if we're reversing the same attribute
    // twice, we'd expect the two instances to cancel each other out.
    auto reverseMask = CharacterAttributes::Normal;

    if (!attrs.empty())
    {
        for (size_t i = 0; i < attrs.size();)
        {
            // A zero or default option is a special case that reverses all the
            // rendition bits. But note that this shouldn't be triggered by an
            // empty attribute list, so we explicitly exclude that case in
            // the empty check above.
            if (attrs.at(i).value_or(0) == 0)
            {
                // With param 0, we only reverse the SinglyUnderlined bit.
                const auto singlyUnderlinedAttr = static_cast<CharacterAttributes>(WI_EnumValue(UnderlineStyle::SinglyUnderlined) << UNDERLINE_STYLE_SHIFT);
                reverseMask ^= (CharacterAttributes::Rendition & ~CharacterAttributes::UnderlineStyle) | singlyUnderlinedAttr;
                i++;
            }
            else
            {
                auto allAttrsOff = TextAttribute{};
                i += _ApplyGraphicsOption(attrs, i, allAttrsOff);
                reverseMask ^= allAttrsOff.GetCharacterAttributes();
            }
        }
    }

    // If the accumulated mask ends up blank, there's nothing for us to do.
    if (reverseMask != CharacterAttributes::Normal)
    {
        _ChangeRectOrStreamAttributes({ left, top, right, bottom }, { .xorAttrMask = reverseMask });
    }

    return true;
}

// Routine Description:
// - DECCRA - Copies a rectangular area from one part of the buffer to another.
// Arguments:
// - top - The first row of the source area.
// - left - The first column of the source area.
// - bottom - The last row of the source area (inclusive).
// - right - The last column of the source area (inclusive).
// - page - The source page number (unused for now).
// - dstTop - The first row of the destination.
// - dstLeft - The first column of the destination.
// - dstPage - The destination page number (unused for now).
// Return Value:
// - True.
bool AdaptDispatch::CopyRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right, const VTInt /*page*/, const VTInt dstTop, const VTInt dstLeft, const VTInt /*dstPage*/)
{
    // GH#13892 We don't yet support the paging extension, so for now we ignore
    // the page parameters. This is the same as if the maximum page count was 1.

    auto& textBuffer = _api.GetTextBuffer();
    const auto bufferSize = textBuffer.GetSize().Dimensions();
    const auto srcRect = _CalculateRectArea(top, left, bottom, right, bufferSize);
    const auto dstBottom = dstTop + srcRect.height() - 1;
    const auto dstRight = dstLeft + srcRect.width() - 1;
    const auto dstRect = _CalculateRectArea(dstTop, dstLeft, dstBottom, dstRight, bufferSize);

    if (dstRect && dstRect.origin() != srcRect.origin())
    {
        // If the source is bigger than the available space at the destination
        // it needs to be clipped, so we only care about the destination size.
        const auto srcView = Viewport::FromDimensions(srcRect.origin(), dstRect.size());
        const auto dstView = Viewport::FromDimensions(dstRect.origin(), dstRect.size());
        const auto walkDirection = Viewport::DetermineWalkDirection(srcView, dstView);
        auto srcPos = srcView.GetWalkOrigin(walkDirection);
        auto dstPos = dstView.GetWalkOrigin(walkDirection);
        // Note that we read two cells from the source before we start writing
        // to the target, so a two-cell DBCS character can't accidentally delete
        // itself when moving one cell horizontally.
        auto next = OutputCell(*textBuffer.GetCellDataAt(srcPos));
        do
        {
            const auto current = next;
            const auto currentSrcPos = srcPos;
            srcView.WalkInBounds(srcPos, walkDirection);
            next = OutputCell(*textBuffer.GetCellDataAt(srcPos));
            // If the source position is offscreen (which can occur on double
            // width lines), then we shouldn't copy anything to the destination.
            if (currentSrcPos.x < textBuffer.GetLineWidth(currentSrcPos.y))
            {
                textBuffer.WriteLine(OutputCellIterator({ &current, 1 }), dstPos);
            }
        } while (dstView.WalkInBounds(dstPos, walkDirection));
        _api.NotifyAccessibilityChange(dstRect);
    }

    return true;
}

// Routine Description:
// - DECFRA - Fills a rectangular area with the given character and using the
//     currently active rendition attributes.
// Arguments:
// - ch - The ordinal value of the character used to fill the area.
// - top - The first row of the area.
// - left - The first column of the area.
// - bottom - The last row of the area (inclusive).
// - right - The last column of the area (inclusive).
// Return Value:
// - True.
bool AdaptDispatch::FillRectangularArea(const VTParameter ch, const VTInt top, const VTInt left, const VTInt bottom, const VTInt right)
{
    auto& textBuffer = _api.GetTextBuffer();
    const auto fillRect = _CalculateRectArea(top, left, bottom, right, textBuffer.GetSize().Dimensions());

    // The standard only allows for characters in the range of the GL and GR
    // character set tables, but we also support additional Unicode characters
    // from the BMP if the code page is UTF-8. Default and 0 are treated as 32.
    const auto charValue = ch.value_or(0) == 0 ? 32 : ch.value();
    const auto glChar = (charValue >= 32 && charValue <= 126);
    const auto grChar = (charValue >= 160 && charValue <= 255);
    const auto unicodeChar = (charValue >= 256 && charValue <= 65535 && _api.GetConsoleOutputCP() == CP_UTF8);
    if (glChar || grChar || unicodeChar)
    {
        const auto fillChar = _termOutput.TranslateKey(gsl::narrow_cast<wchar_t>(charValue));
        const auto& fillAttributes = textBuffer.GetCurrentAttributes();
        _FillRect(textBuffer, fillRect, { &fillChar, 1 }, fillAttributes);
    }

    return true;
}

// Routine Description:
// - DECERA - Erases a rectangular area, replacing all cells with a space
//     character and the default rendition attributes.
// Arguments:
// - top - The first row of the area.
// - left - The first column of the area.
// - bottom - The last row of the area (inclusive).
// - right - The last column of the area (inclusive).
// Return Value:
// - True.
bool AdaptDispatch::EraseRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right)
{
    auto& textBuffer = _api.GetTextBuffer();
    const auto eraseRect = _CalculateRectArea(top, left, bottom, right, textBuffer.GetSize().Dimensions());
    const auto eraseAttributes = _GetEraseAttributes(textBuffer);
    _FillRect(textBuffer, eraseRect, whitespace, eraseAttributes);
    return true;
}

// Routine Description:
// - DECSERA - Selectively erases a rectangular area, replacing unprotected
//     cells with a space character, but retaining the rendition attributes.
// Arguments:
// - top - The first row of the area.
// - left - The first column of the area.
// - bottom - The last row of the area (inclusive).
// - right - The last column of the area (inclusive).
// Return Value:
// - True.
bool AdaptDispatch::SelectiveEraseRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right)
{
    auto& textBuffer = _api.GetTextBuffer();
    const auto eraseRect = _CalculateRectArea(top, left, bottom, right, textBuffer.GetSize().Dimensions());
    _SelectiveEraseRect(textBuffer, eraseRect);
    return true;
}

// Routine Description:
// - DECSACE - Selects the format of the character range that will be affected
//   by the DECCARA and DECRARA attribute operations.
// Arguments:
// - changeExtent - Whether the character range is a stream or a rectangle.
// Return value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::SelectAttributeChangeExtent(const DispatchTypes::ChangeExtent changeExtent) noexcept
{
    switch (changeExtent)
    {
    case DispatchTypes::ChangeExtent::Default:
    case DispatchTypes::ChangeExtent::Stream:
        _modes.reset(Mode::RectangularChangeExtent);
        return true;
    case DispatchTypes::ChangeExtent::Rectangle:
        _modes.set(Mode::RectangularChangeExtent);
        return true;
    default:
        return false;
    }
}

// Routine Description:
// - DECRQCRA - Computes and reports a checksum of the specified area of
//   the buffer memory.
// Arguments:
// - id - a numeric label used to identify the request.
// - page - The page number (unused for now).
// - top - The first row of the area.
// - left - The first column of the area.
// - bottom - The last row of the area (inclusive).
// - right - The last column of the area (inclusive).
// Return value:
// - True.
bool AdaptDispatch::RequestChecksumRectangularArea(const VTInt id, const VTInt page, const VTInt top, const VTInt left, const VTInt bottom, const VTInt right)
{
    uint16_t checksum = 0;
    // If this feature is not enabled, we'll just report a zero checksum.
    if constexpr (Feature_VtChecksumReport::IsEnabled())
    {
        if (page == 1)
        {
            // As part of the checksum, we need to include the color indices of each
            // cell, and in the case of default colors, those indices come from the
            // color alias table. But if they're not in the bottom 16 range, we just
            // fallback to using white on black (7 and 0).
            auto defaultFgIndex = _renderSettings.GetColorAliasIndex(ColorAlias::DefaultForeground);
            auto defaultBgIndex = _renderSettings.GetColorAliasIndex(ColorAlias::DefaultBackground);
            defaultFgIndex = defaultFgIndex < 16 ? defaultFgIndex : 7;
            defaultBgIndex = defaultBgIndex < 16 ? defaultBgIndex : 0;

            const auto& textBuffer = _api.GetTextBuffer();
            const auto eraseRect = _CalculateRectArea(top, left, bottom, right, textBuffer.GetSize().Dimensions());
            for (auto row = eraseRect.top; row < eraseRect.bottom; row++)
            {
                for (auto col = eraseRect.left; col < eraseRect.right; col++)
                {
                    // The algorithm we're using here should match the DEC terminals
                    // for the ASCII and Latin-1 range. Their other character sets
                    // predate Unicode, though, so we'd need a custom mapping table
                    // to lookup the correct checksums. Considering this is only for
                    // testing at the moment, that doesn't seem worth the effort.
                    const auto cell = textBuffer.GetCellDataAt({ col, row });
                    for (auto ch : cell->Chars())
                    {
                        // That said, I've made a special allowance for U+2426,
                        // since that is widely used in a lot of character sets.
                        checksum -= (ch == L'\u2426' ? 0x1B : ch);
                    }

                    // Since we're attempting to match the DEC checksum algorithm,
                    // the only attributes affecting the checksum are the ones that
                    // were supported by DEC terminals.
                    const auto attr = cell->TextAttr();
                    checksum -= attr.IsProtected() ? 0x04 : 0;
                    checksum -= attr.IsInvisible() ? 0x08 : 0;
                    checksum -= attr.IsUnderlined() ? 0x10 : 0;
                    checksum -= attr.IsReverseVideo() ? 0x20 : 0;
                    checksum -= attr.IsBlinking() ? 0x40 : 0;
                    checksum -= attr.IsIntense() ? 0x80 : 0;

                    // For the same reason, we only care about the eight basic ANSI
                    // colors, although technically we also report the 8-16 index
                    // range. Everything else gets mapped to the default colors.
                    const auto colorIndex = [](const auto color, const auto defaultIndex) {
                        return color.IsLegacy() ? color.GetIndex() : defaultIndex;
                    };
                    const auto fgIndex = colorIndex(attr.GetForeground(), defaultFgIndex);
                    const auto bgIndex = colorIndex(attr.GetBackground(), defaultBgIndex);
                    checksum -= gsl::narrow_cast<uint16_t>(fgIndex << 4);
                    checksum -= gsl::narrow_cast<uint16_t>(bgIndex);
                }
            }
        }
    }
    const auto response = wil::str_printf<std::wstring>(L"\033P%d!~%04X\033\\", id, checksum);
    _api.ReturnResponse(response);
    return true;
}

// Routine Description:
// - DECSWL/DECDWL/DECDHL - Sets the line rendition attribute for the current line.
// Arguments:
// - rendition - Determines whether the line will be rendered as single width, double
//   width, or as one half of a double height line.
// Return Value:
// - True.
bool AdaptDispatch::SetLineRendition(const LineRendition rendition)
{
    // The line rendition can't be changed if left/right margins are allowed.
    if (!_modes.test(Mode::AllowDECSLRM))
    {
        auto& textBuffer = _api.GetTextBuffer();
        const auto eraseAttributes = _GetEraseAttributes(textBuffer);
        textBuffer.SetCurrentLineRendition(rendition, eraseAttributes);
        // There is some variation in how this was handled by the different DEC
        // terminals, but the STD 070 reference (on page D-13) makes it clear that
        // the delayed wrap (aka the Last Column Flag) was expected to be reset when
        // line rendition controls were executed.
        textBuffer.GetCursor().ResetDelayEOLWrap();
    }
    return true;
}

// Routine Description:
// - DSR - Reports status of a console property back to the STDIN based on the type of status requested.
// Arguments:
// - statusType - status type indicating what property we should report back
// - id - a numeric label used to identify the request in DECCKSR reports
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::DeviceStatusReport(const DispatchTypes::StatusType statusType, const VTParameter id)
{
    constexpr auto GoodCondition = L"0";
    constexpr auto PrinterNotConnected = L"?13";
    constexpr auto UserDefinedKeysNotSupported = L"?23";
    constexpr auto UnknownPcKeyboard = L"?27;0;0;5";
    constexpr auto LocatorNotConnected = L"?53";
    constexpr auto UnknownLocatorDevice = L"?57;0";
    constexpr auto TerminalReady = L"?70";
    constexpr auto MultipleSessionsNotSupported = L"?83";

    switch (statusType)
    {
    case DispatchTypes::StatusType::OperatingStatus:
        _DeviceStatusReport(GoodCondition);
        return true;
    case DispatchTypes::StatusType::CursorPositionReport:
        _CursorPositionReport(false);
        return true;
    case DispatchTypes::StatusType::ExtendedCursorPositionReport:
        _CursorPositionReport(true);
        return true;
    case DispatchTypes::StatusType::PrinterStatus:
        _DeviceStatusReport(PrinterNotConnected);
        return true;
    case DispatchTypes::StatusType::UserDefinedKeys:
        _DeviceStatusReport(UserDefinedKeysNotSupported);
        return true;
    case DispatchTypes::StatusType::KeyboardStatus:
        _DeviceStatusReport(UnknownPcKeyboard);
        return true;
    case DispatchTypes::StatusType::LocatorStatus:
        _DeviceStatusReport(LocatorNotConnected);
        return true;
    case DispatchTypes::StatusType::LocatorIdentity:
        _DeviceStatusReport(UnknownLocatorDevice);
        return true;
    case DispatchTypes::StatusType::MacroSpaceReport:
        _MacroSpaceReport();
        return true;
    case DispatchTypes::StatusType::MemoryChecksum:
        _MacroChecksumReport(id);
        return true;
    case DispatchTypes::StatusType::DataIntegrity:
        _DeviceStatusReport(TerminalReady);
        return true;
    case DispatchTypes::StatusType::MultipleSessionStatus:
        _DeviceStatusReport(MultipleSessionsNotSupported);
        return true;
    default:
        return false;
    }
}

// Routine Description:
// - DA - Reports the service class or conformance level that the terminal
//   supports, and the set of implemented extensions.
// Arguments:
// - <none>
// Return Value:
// - True.
bool AdaptDispatch::DeviceAttributes()
{
    // This first parameter of the response is 61, representing a conformance
    // level of 1. The subsequent parameters identify the supported feature
    // extensions.
    //
    // 1 = 132 column mode (ConHost only)
    // 6 = Selective erase
    // 7 = Soft fonts
    // 14 = 8-bit interface architecture
    // 21 = Horizontal scrolling
    // 22 = Color text
    // 23 = Greek character sets
    // 24 = Turkish character sets
    // 28 = Rectangular area operations
    // 32 = Text macros
    // 42 = ISO Latin-2 character set

    if (_api.IsConsolePty())
    {
        _api.ReturnResponse(L"\x1b[?61;6;7;14;21;22;23;24;28;32;42c");
    }
    else
    {
        _api.ReturnResponse(L"\x1b[?61;1;6;7;14;21;22;23;24;28;32;42c");
    }
    return true;
}

// Routine Description:
// - DA2 - Reports the terminal type, firmware version, and hardware options.
//   For now we're following the XTerm practice of using 0 to represent a VT100
//   terminal, the version is hard-coded as 10 (1.0), and the hardware option
//   is set to 1 (indicating a PC Keyboard).
// Arguments:
// - <none>
// Return Value:
// - True.
bool AdaptDispatch::SecondaryDeviceAttributes()
{
    _api.ReturnResponse(L"\x1b[>0;10;1c");
    return true;
}

// Routine Description:
// - DA3 - Reports the terminal unit identification code. Terminal emulators
//   typically return a hard-coded value, the most common being all zeros.
// Arguments:
// - <none>
// Return Value:
// - True.
bool AdaptDispatch::TertiaryDeviceAttributes()
{
    _api.ReturnResponse(L"\x1bP!|00000000\x1b\\");
    return true;
}

// Routine Description:
// - VT52 Identify - Reports the identity of the terminal in VT52 emulation mode.
//   An actual VT52 terminal would typically identify itself with ESC / K.
//   But for a terminal that is emulating a VT52, the sequence should be ESC / Z.
// Arguments:
// - <none>
// Return Value:
// - True.
bool AdaptDispatch::Vt52DeviceAttributes()
{
    _api.ReturnResponse(L"\x1b/Z");
    return true;
}

// Routine Description:
// - DECREQTPARM - This sequence was originally used on the VT100 terminal to
//   report the serial communication parameters (baud rate, data bits, parity,
//   etc.). On modern terminal emulators, the response is simply hard-coded.
// Arguments:
// - permission - This would originally have determined whether the terminal
//   was allowed to send unsolicited reports or not.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::RequestTerminalParameters(const DispatchTypes::ReportingPermission permission)
{
    // We don't care whether unsolicited reports are allowed or not, but the
    // requested permission does determine the value of the first response
    // parameter. The remaining parameters are just hard-coded to indicate a
    // 38400 baud connection, which matches the XTerm response. The full
    // parameter sequence is as follows:
    // - response type:    2 or 3 (unsolicited or solicited)
    // - parity:           1 (no parity)
    // - data bits:        1 (8 bits per character)
    // - transmit speed:   128 (38400 baud)
    // - receive speed:    128 (38400 baud)
    // - clock multiplier: 1
    // - flags:            0
    switch (permission)
    {
    case DispatchTypes::ReportingPermission::Unsolicited:
        _api.ReturnResponse(L"\x1b[2;1;1;128;128;1;0x");
        return true;
    case DispatchTypes::ReportingPermission::Solicited:
        _api.ReturnResponse(L"\x1b[3;1;1;128;128;1;0x");
        return true;
    default:
        return false;
    }
}

// Routine Description:
// - DSR - Transmits a device status report with a given parameter string.
// Arguments:
// - parameters - One or more parameter values representing the status
// Return Value:
// - <none>
void AdaptDispatch::_DeviceStatusReport(const wchar_t* parameters) const
{
    _api.ReturnResponse(fmt::format(FMT_COMPILE(L"\033[{}n"), parameters));
}

// Routine Description:
// - CPR and DECXCPR- Reports the current cursor position within the viewport,
//   as well as the current page number if this is an extended report.
// Arguments:
// - extendedReport - Set to true if the report should include the page number
// Return Value:
// - <none>
void AdaptDispatch::_CursorPositionReport(const bool extendedReport)
{
    const auto viewport = _api.GetViewport();
    const auto& textBuffer = _api.GetTextBuffer();

    // First pull the cursor position relative to the entire buffer out of the console.
    til::point cursorPosition{ textBuffer.GetCursor().GetPosition() };

    // Now adjust it for its position in respect to the current viewport top.
    cursorPosition.y -= viewport.top;

    // NOTE: 1,1 is the top-left corner of the viewport in VT-speak, so add 1.
    cursorPosition.x++;
    cursorPosition.y++;

    // If the origin mode is set, the cursor is relative to the margin origin.
    if (_modes.test(Mode::Origin))
    {
        cursorPosition.x -= _GetHorizontalMargins(textBuffer.GetSize().Width()).first;
        cursorPosition.y -= _GetVerticalMargins(viewport, false).first;
    }

    // Now send it back into the input channel of the console.
    if (extendedReport)
    {
        // An extended report should also include the page number, but for now
        // we hard-code it to 1, since we don't yet support paging (GH#13892).
        const auto pageNumber = 1;
        const auto response = wil::str_printf<std::wstring>(L"\x1b[?%d;%d;%dR", cursorPosition.y, cursorPosition.x, pageNumber);
        _api.ReturnResponse(response);
    }
    else
    {
        // The standard report only returns the cursor position.
        const auto response = wil::str_printf<std::wstring>(L"\x1b[%d;%dR", cursorPosition.y, cursorPosition.x);
        _api.ReturnResponse(response);
    }
}

// Routine Description:
// - DECMSR - Reports the amount of space available for macro definitions.
// Arguments:
// - <none>
// Return Value:
// - <none>
void AdaptDispatch::_MacroSpaceReport() const
{
    const auto spaceInBytes = _macroBuffer ? _macroBuffer->GetSpaceAvailable() : MacroBuffer::MAX_SPACE;
    // The available space is measured in blocks of 16 bytes, so we need to divide by 16.
    const auto response = wil::str_printf<std::wstring>(L"\x1b[%zu*{", spaceInBytes / 16);
    _api.ReturnResponse(response);
}

// Routine Description:
// - DECCKSR - Reports a checksum of the current macro definitions.
// Arguments:
// - id - a numeric label used to identify the DSR request
// Return Value:
// - <none>
void AdaptDispatch::_MacroChecksumReport(const VTParameter id) const
{
    const auto requestId = id.value_or(0);
    const auto checksum = _macroBuffer ? _macroBuffer->CalculateChecksum() : 0;
    const auto response = wil::str_printf<std::wstring>(L"\033P%d!~%04X\033\\", requestId, checksum);
    _api.ReturnResponse(response);
}

// Routine Description:
// - Generalizes scrolling movement for up/down
// Arguments:
// - delta - Distance to move (positive is down, negative is up)
// Return Value:
// - <none>
void AdaptDispatch::_ScrollMovement(const VTInt delta)
{
    const auto viewport = _api.GetViewport();
    auto& textBuffer = _api.GetTextBuffer();
    const auto bufferWidth = textBuffer.GetSize().Width();
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(viewport, true);
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(bufferWidth);
    _ScrollRectVertically(textBuffer, { leftMargin, topMargin, rightMargin + 1, bottomMargin + 1 }, delta);
}

// Routine Description:
// - SU - Pans the window DOWN by given distance (distance new lines appear at the bottom of the screen)
// Arguments:
// - distance - Distance to move
// Return Value:
// - True.
bool AdaptDispatch::ScrollUp(const VTInt uiDistance)
{
    _ScrollMovement(-uiDistance);
    return true;
}

// Routine Description:
// - SD - Pans the window UP by given distance (distance new lines appear at the top of the screen)
// Arguments:
// - distance - Distance to move
// Return Value:
// - True.
bool AdaptDispatch::ScrollDown(const VTInt uiDistance)
{
    _ScrollMovement(uiDistance);
    return true;
}

// Routine Description:
// - DECCOLM not only sets the number of columns, but also clears the screen buffer,
//    resets the page margins and origin mode, and places the cursor at 1,1
// Arguments:
// - enable - the number of columns is set to 132 if true, 80 if false.
// Return Value:
// - <none>
void AdaptDispatch::_SetColumnMode(const bool enable)
{
    // Only proceed if DECCOLM is allowed. Return true, as this is technically a successful handling.
    if (_modes.test(Mode::AllowDECCOLM) && !_api.IsConsolePty())
    {
        const auto viewport = _api.GetViewport();
        const auto viewportHeight = viewport.bottom - viewport.top;
        const auto viewportWidth = (enable ? DispatchTypes::s_sDECCOLMSetColumns : DispatchTypes::s_sDECCOLMResetColumns);
        _api.ResizeWindow(viewportWidth, viewportHeight);
        _modes.set(Mode::Column, enable);
        _modes.reset(Mode::Origin, Mode::AllowDECSLRM);
        CursorPosition(1, 1);
        EraseInDisplay(DispatchTypes::EraseType::All);
        _DoSetTopBottomScrollingMargins(0, 0);
        _DoSetLeftRightScrollingMargins(0, 0);
    }
}

// Routine Description:
// - Set the alternate screen buffer mode. In virtual terminals, there exists
//     both a "main" screen buffer and an alternate. This mode is used to switch
//     between the two.
// Arguments:
// - enable - true selects the alternate buffer, false returns to the main buffer.
// Return Value:
// - <none>
void AdaptDispatch::_SetAlternateScreenBufferMode(const bool enable)
{
    if (enable)
    {
        CursorSaveState();
        const auto& textBuffer = _api.GetTextBuffer();
        _api.UseAlternateScreenBuffer(_GetEraseAttributes(textBuffer));
        _usingAltBuffer = true;
    }
    else
    {
        _api.UseMainScreenBuffer();
        _usingAltBuffer = false;
        CursorRestoreState();
    }
}

// Routine Description:
// - Determines whether we need to pass through input mode requests.
//   If we're a conpty, AND WE'RE IN VT INPUT MODE, always pass input mode requests
//   The VT Input mode check is to work around ssh.exe v7.7, which uses VT
//   output, but not Input.
//   The original comment said, "Once the conpty supports these types of input,
//   this check can be removed. See GH#4911". Unfortunately, time has shown
//   us that SSH 7.7 _also_ requests mouse input and that can have a user interface
//   impact on the actual connected terminal. We can't remove this check,
//   because SSH <=7.7 is out in the wild on all versions of Windows <=2004.
// Return Value:
// - True if we should pass through. False otherwise.
bool AdaptDispatch::_PassThroughInputModes()
{
    return _api.IsConsolePty() && _api.IsVtInputEnabled();
}

// Routine Description:
// - Support routine for routing mode parameters to be set/reset as flags
// Arguments:
// - param - mode parameter to set/reset
// - enable - True for set, false for unset.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::_ModeParamsHelper(const DispatchTypes::ModeParams param, const bool enable)
{
    switch (param)
    {
    case DispatchTypes::ModeParams::IRM_InsertReplaceMode:
        _modes.set(Mode::InsertReplace, enable);
        return true;
    case DispatchTypes::ModeParams::LNM_LineFeedNewLineMode:
        // VT apps expect that the system and input modes are the same, so if
        // they become out of sync, we just act as if LNM mode isn't supported.
        if (_api.GetSystemMode(ITerminalApi::Mode::LineFeed) == _terminalInput.GetInputMode(TerminalInput::Mode::LineFeed))
        {
            _api.SetSystemMode(ITerminalApi::Mode::LineFeed, enable);
            _terminalInput.SetInputMode(TerminalInput::Mode::LineFeed, enable);
        }
        return true;
    case DispatchTypes::ModeParams::DECCKM_CursorKeysMode:
        _terminalInput.SetInputMode(TerminalInput::Mode::CursorKey, enable);
        return !_PassThroughInputModes();
    case DispatchTypes::ModeParams::DECANM_AnsiMode:
        return SetAnsiMode(enable);
    case DispatchTypes::ModeParams::DECCOLM_SetNumberOfColumns:
        _SetColumnMode(enable);
        return true;
    case DispatchTypes::ModeParams::DECSCNM_ScreenMode:
        _renderSettings.SetRenderMode(RenderSettings::Mode::ScreenReversed, enable);
        // No need to force a redraw in pty mode.
        if (_api.IsConsolePty())
        {
            return false;
        }
        _renderer.TriggerRedrawAll();
        return true;
    case DispatchTypes::ModeParams::DECOM_OriginMode:
        _modes.set(Mode::Origin, enable);
        // The cursor is also moved to the new home position when the origin mode is set or reset.
        CursorPosition(1, 1);
        return true;
    case DispatchTypes::ModeParams::DECAWM_AutoWrapMode:
        _api.SetSystemMode(ITerminalApi::Mode::AutoWrap, enable);
        // Resetting DECAWM should also reset the delayed wrap flag.
        if (!enable)
        {
            _api.GetTextBuffer().GetCursor().ResetDelayEOLWrap();
        }
        return true;
    case DispatchTypes::ModeParams::DECARM_AutoRepeatMode:
        _terminalInput.SetInputMode(TerminalInput::Mode::AutoRepeat, enable);
        return !_PassThroughInputModes();
    case DispatchTypes::ModeParams::ATT610_StartCursorBlink:
        _api.GetTextBuffer().GetCursor().SetBlinkingAllowed(enable);
        return !_api.IsConsolePty();
    case DispatchTypes::ModeParams::DECTCEM_TextCursorEnableMode:
        _api.GetTextBuffer().GetCursor().SetIsVisible(enable);
        return true;
    case DispatchTypes::ModeParams::XTERM_EnableDECCOLMSupport:
        _modes.set(Mode::AllowDECCOLM, enable);
        return true;
    case DispatchTypes::ModeParams::DECNKM_NumericKeypadMode:
        _terminalInput.SetInputMode(TerminalInput::Mode::Keypad, enable);
        return !_PassThroughInputModes();
    case DispatchTypes::ModeParams::DECBKM_BackarrowKeyMode:
        _terminalInput.SetInputMode(TerminalInput::Mode::BackarrowKey, enable);
        return !_PassThroughInputModes();
    case DispatchTypes::ModeParams::DECLRMM_LeftRightMarginMode:
        _modes.set(Mode::AllowDECSLRM, enable);
        _DoSetLeftRightScrollingMargins(0, 0);
        if (enable)
        {
            // If we've allowed left/right margins, we can't have line renditions.
            const auto viewport = _api.GetViewport();
            auto& textBuffer = _api.GetTextBuffer();
            textBuffer.ResetLineRenditionRange(viewport.top, viewport.bottom);
        }
        return true;
    case DispatchTypes::ModeParams::DECECM_EraseColorMode:
        _modes.set(Mode::EraseColor, enable);
        return true;
    case DispatchTypes::ModeParams::VT200_MOUSE_MODE:
        _terminalInput.SetInputMode(TerminalInput::Mode::DefaultMouseTracking, enable);
        return !_PassThroughInputModes();
    case DispatchTypes::ModeParams::BUTTON_EVENT_MOUSE_MODE:
        _terminalInput.SetInputMode(TerminalInput::Mode::ButtonEventMouseTracking, enable);
        return !_PassThroughInputModes();
    case DispatchTypes::ModeParams::ANY_EVENT_MOUSE_MODE:
        _terminalInput.SetInputMode(TerminalInput::Mode::AnyEventMouseTracking, enable);
        return !_PassThroughInputModes();
    case DispatchTypes::ModeParams::UTF8_EXTENDED_MODE:
        _terminalInput.SetInputMode(TerminalInput::Mode::Utf8MouseEncoding, enable);
        return !_PassThroughInputModes();
    case DispatchTypes::ModeParams::SGR_EXTENDED_MODE:
        _terminalInput.SetInputMode(TerminalInput::Mode::SgrMouseEncoding, enable);
        return !_PassThroughInputModes();
    case DispatchTypes::ModeParams::FOCUS_EVENT_MODE:
        _terminalInput.SetInputMode(TerminalInput::Mode::FocusEvent, enable);
        // GH#12799 - If the app requested that we disable focus events, DON'T pass
        // that through. ConPTY would _always_ like to know about focus events.
        return !_PassThroughInputModes() || !enable;
    case DispatchTypes::ModeParams::ALTERNATE_SCROLL:
        _terminalInput.SetInputMode(TerminalInput::Mode::AlternateScroll, enable);
        return !_PassThroughInputModes();
    case DispatchTypes::ModeParams::ASB_AlternateScreenBuffer:
        _SetAlternateScreenBufferMode(enable);
        return true;
    case DispatchTypes::ModeParams::XTERM_BracketedPasteMode:
        _api.SetSystemMode(ITerminalApi::Mode::BracketedPaste, enable);
        return !_api.IsConsolePty();
    case DispatchTypes::ModeParams::W32IM_Win32InputMode:
        _terminalInput.SetInputMode(TerminalInput::Mode::Win32, enable);
        // ConPTY requests the Win32InputMode on startup and disables it on shutdown. When nesting ConPTY inside
        // ConPTY then this should not bubble up. Otherwise, when the inner ConPTY exits and the outer ConPTY
        // passes the disable sequence up to the hosting terminal, we'd stop getting Win32InputMode entirely!
        // It also makes more sense to not bubble it up, because this mode is specifically for INPUT_RECORD interop
        // and thus entirely between a PTY's input records and its INPUT_RECORD-aware VT-aware console clients.
        // Returning true here will mark this as being handled and avoid this.
        return true;
    default:
        // If no functions to call, overall dispatch was a failure.
        return false;
    }
}

// Routine Description:
// - SM/DECSET - Enables the given mode parameter (both ANSI and private).
// Arguments:
// - param - mode parameter to set
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::SetMode(const DispatchTypes::ModeParams param)
{
    return _ModeParamsHelper(param, true);
}

// Routine Description:
// - RM/DECRST - Disables the given mode parameter (both ANSI and private).
// Arguments:
// - param - mode parameter to reset
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::ResetMode(const DispatchTypes::ModeParams param)
{
    return _ModeParamsHelper(param, false);
}

// Routine Description:
// - DECRQM - Requests the current state of a given mode number. The result
//   is reported back with a DECRPM escape sequence.
// Arguments:
// - param - the mode number being queried
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::RequestMode(const DispatchTypes::ModeParams param)
{
    auto enabled = std::optional<bool>{};

    switch (param)
    {
    case DispatchTypes::ModeParams::IRM_InsertReplaceMode:
        enabled = _modes.test(Mode::InsertReplace);
        break;
    case DispatchTypes::ModeParams::LNM_LineFeedNewLineMode:
        // VT apps expect that the system and input modes are the same, so if
        // they become out of sync, we just act as if LNM mode isn't supported.
        if (_api.GetSystemMode(ITerminalApi::Mode::LineFeed) == _terminalInput.GetInputMode(TerminalInput::Mode::LineFeed))
        {
            enabled = _terminalInput.GetInputMode(TerminalInput::Mode::LineFeed);
        }
        break;
    case DispatchTypes::ModeParams::DECCKM_CursorKeysMode:
        enabled = _terminalInput.GetInputMode(TerminalInput::Mode::CursorKey);
        break;
    case DispatchTypes::ModeParams::DECANM_AnsiMode:
        enabled = _api.GetStateMachine().GetParserMode(StateMachine::Mode::Ansi);
        break;
    case DispatchTypes::ModeParams::DECCOLM_SetNumberOfColumns:
        // DECCOLM is not supported in conpty mode
        if (!_api.IsConsolePty())
        {
            enabled = _modes.test(Mode::Column);
        }
        break;
    case DispatchTypes::ModeParams::DECSCNM_ScreenMode:
        enabled = _renderSettings.GetRenderMode(RenderSettings::Mode::ScreenReversed);
        break;
    case DispatchTypes::ModeParams::DECOM_OriginMode:
        enabled = _modes.test(Mode::Origin);
        break;
    case DispatchTypes::ModeParams::DECAWM_AutoWrapMode:
        enabled = _api.GetSystemMode(ITerminalApi::Mode::AutoWrap);
        break;
    case DispatchTypes::ModeParams::DECARM_AutoRepeatMode:
        enabled = _terminalInput.GetInputMode(TerminalInput::Mode::AutoRepeat);
        break;
    case DispatchTypes::ModeParams::ATT610_StartCursorBlink:
        enabled = _api.GetTextBuffer().GetCursor().IsBlinkingAllowed();
        break;
    case DispatchTypes::ModeParams::DECTCEM_TextCursorEnableMode:
        enabled = _api.GetTextBuffer().GetCursor().IsVisible();
        break;
    case DispatchTypes::ModeParams::XTERM_EnableDECCOLMSupport:
        // DECCOLM is not supported in conpty mode
        if (!_api.IsConsolePty())
        {
            enabled = _modes.test(Mode::AllowDECCOLM);
        }
        break;
    case DispatchTypes::ModeParams::DECNKM_NumericKeypadMode:
        enabled = _terminalInput.GetInputMode(TerminalInput::Mode::Keypad);
        break;
    case DispatchTypes::ModeParams::DECBKM_BackarrowKeyMode:
        enabled = _terminalInput.GetInputMode(TerminalInput::Mode::BackarrowKey);
        break;
    case DispatchTypes::ModeParams::DECLRMM_LeftRightMarginMode:
        enabled = _modes.test(Mode::AllowDECSLRM);
        break;
    case DispatchTypes::ModeParams::DECECM_EraseColorMode:
        enabled = _modes.test(Mode::EraseColor);
        break;
    case DispatchTypes::ModeParams::VT200_MOUSE_MODE:
        enabled = _terminalInput.GetInputMode(TerminalInput::Mode::DefaultMouseTracking);
        break;
    case DispatchTypes::ModeParams::BUTTON_EVENT_MOUSE_MODE:
        enabled = _terminalInput.GetInputMode(TerminalInput::Mode::ButtonEventMouseTracking);
        break;
    case DispatchTypes::ModeParams::ANY_EVENT_MOUSE_MODE:
        enabled = _terminalInput.GetInputMode(TerminalInput::Mode::AnyEventMouseTracking);
        break;
    case DispatchTypes::ModeParams::UTF8_EXTENDED_MODE:
        enabled = _terminalInput.GetInputMode(TerminalInput::Mode::Utf8MouseEncoding);
        break;
    case DispatchTypes::ModeParams::SGR_EXTENDED_MODE:
        enabled = _terminalInput.GetInputMode(TerminalInput::Mode::SgrMouseEncoding);
        break;
    case DispatchTypes::ModeParams::FOCUS_EVENT_MODE:
        enabled = _terminalInput.GetInputMode(TerminalInput::Mode::FocusEvent);
        break;
    case DispatchTypes::ModeParams::ALTERNATE_SCROLL:
        enabled = _terminalInput.GetInputMode(TerminalInput::Mode::AlternateScroll);
        break;
    case DispatchTypes::ModeParams::ASB_AlternateScreenBuffer:
        enabled = _usingAltBuffer;
        break;
    case DispatchTypes::ModeParams::XTERM_BracketedPasteMode:
        enabled = _api.GetSystemMode(ITerminalApi::Mode::BracketedPaste);
        break;
    case DispatchTypes::ModeParams::W32IM_Win32InputMode:
        enabled = _terminalInput.GetInputMode(TerminalInput::Mode::Win32);
        break;
    default:
        enabled = std::nullopt;
        break;
    }

    // 1 indicates the mode is enabled, 2 it's disabled, and 0 it's unsupported
    const auto state = enabled.has_value() ? (enabled.value() ? 1 : 2) : 0;
    const auto isPrivate = param >= DispatchTypes::DECPrivateMode(0);
    const auto prefix = isPrivate ? L"?" : L"";
    const auto mode = isPrivate ? param - DispatchTypes::DECPrivateMode(0) : param;
    const auto response = wil::str_printf<std::wstring>(L"\x1b[%s%d;%d$y", prefix, mode, state);
    _api.ReturnResponse(response);
    return true;
}

// - DECKPAM, DECKPNM - Sets the keypad input mode to either Application mode or Numeric mode (true, false respectively)
// Arguments:
// - applicationMode - set to true to enable Application Mode Input, false for Numeric Mode Input.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::SetKeypadMode(const bool fApplicationMode)
{
    _terminalInput.SetInputMode(TerminalInput::Mode::Keypad, fApplicationMode);
    return !_PassThroughInputModes();
}

// Routine Description:
// - Internal logic for adding or removing lines in the active screen buffer.
//   This also moves the cursor to the left margin, which is expected behavior for IL and DL.
// Parameters:
// - delta - Number of lines to modify (positive if inserting, negative if deleting).
// Return Value:
// - <none>
void AdaptDispatch::_InsertDeleteLineHelper(const VTInt delta)
{
    const auto viewport = _api.GetViewport();
    auto& textBuffer = _api.GetTextBuffer();
    const auto bufferWidth = textBuffer.GetSize().Width();

    auto& cursor = textBuffer.GetCursor();
    const auto col = cursor.GetPosition().x;
    const auto row = cursor.GetPosition().y;

    const auto [topMargin, bottomMargin] = _GetVerticalMargins(viewport, true);
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(bufferWidth);
    if (row >= topMargin && row <= bottomMargin && col >= leftMargin && col <= rightMargin)
    {
        // We emulate inserting and deleting by scrolling the area between the cursor and the bottom margin.
        _ScrollRectVertically(textBuffer, { leftMargin, row, rightMargin + 1, bottomMargin + 1 }, delta);

        // The IL and DL controls are also expected to move the cursor to the left margin.
        cursor.SetXPosition(leftMargin);
        _ApplyCursorMovementFlags(cursor);
    }
}

// Routine Description:
// - IL - This control function inserts one or more blank lines, starting at the cursor.
//    As lines are inserted, lines below the cursor and in the scrolling region move down.
//    Lines scrolled off the page are lost. IL has no effect outside the page margins.
// Arguments:
// - distance - number of lines to insert
// Return Value:
// - True.
bool AdaptDispatch::InsertLine(const VTInt distance)
{
    _InsertDeleteLineHelper(distance);
    return true;
}

// Routine Description:
// - DL - This control function deletes one or more lines in the scrolling
//    region, starting with the line that has the cursor.
//    As lines are deleted, lines below the cursor and in the scrolling region
//    move up. The terminal adds blank lines with no visual character
//    attributes at the bottom of the scrolling region. If distance is greater than
//    the number of lines remaining on the page, DL deletes only the remaining
//    lines. DL has no effect outside the scrolling margins.
// Arguments:
// - distance - number of lines to delete
// Return Value:
// - True.
bool AdaptDispatch::DeleteLine(const VTInt distance)
{
    _InsertDeleteLineHelper(-distance);
    return true;
}

// Routine Description:
// - Internal logic for adding or removing columns in the active screen buffer.
// Parameters:
// - delta - Number of columns to modify (positive if inserting, negative if deleting).
// Return Value:
// - <none>
void AdaptDispatch::_InsertDeleteColumnHelper(const VTInt delta)
{
    const auto viewport = _api.GetViewport();
    auto& textBuffer = _api.GetTextBuffer();
    const auto bufferWidth = textBuffer.GetSize().Width();

    const auto& cursor = textBuffer.GetCursor();
    const auto col = cursor.GetPosition().x;
    const auto row = cursor.GetPosition().y;

    const auto [topMargin, bottomMargin] = _GetVerticalMargins(viewport, true);
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(bufferWidth);
    if (row >= topMargin && row <= bottomMargin && col >= leftMargin && col <= rightMargin)
    {
        // We emulate inserting and deleting by scrolling the area between the cursor and the right margin.
        _ScrollRectHorizontally(textBuffer, { col, topMargin, rightMargin + 1, bottomMargin + 1 }, delta);
    }
}

// Routine Description:
// - DECIC - This control function inserts one or more blank columns in the
//    scrolling region, starting at the column that has the cursor.
// Arguments:
// - distance - number of columns to insert
// Return Value:
// - True.
bool AdaptDispatch::InsertColumn(const VTInt distance)
{
    _InsertDeleteColumnHelper(distance);
    return true;
}

// Routine Description:
// - DECDC - This control function deletes one or more columns in the scrolling
//    region, starting with the column that has the cursor.
// Arguments:
// - distance - number of columns to delete
// Return Value:
// - True.
bool AdaptDispatch::DeleteColumn(const VTInt distance)
{
    _InsertDeleteColumnHelper(-distance);
    return true;
}

// - DECANM - Sets the terminal emulation mode to either ANSI-compatible or VT52.
// Arguments:
// - ansiMode - set to true to enable the ANSI mode, false for VT52 mode.
// Return Value:
// - True.
bool AdaptDispatch::SetAnsiMode(const bool ansiMode)
{
    // When an attempt is made to update the mode, the designated character sets
    // need to be reset to defaults, even if the mode doesn't actually change.
    _termOutput.SoftReset();

    _api.GetStateMachine().SetParserMode(StateMachine::Mode::Ansi, ansiMode);
    _terminalInput.SetInputMode(TerminalInput::Mode::Ansi, ansiMode);

    // While input mode changes are often forwarded over conpty, we never want
    // to do that for the DECANM mode.
    return true;
}

// Routine Description:
// - DECSTBM - Set Scrolling Region
// This control function sets the top and bottom margins for the current page.
//  You cannot perform scrolling outside the margins.
//  Default: Margins are at the page limits.
// Arguments:
// - topMargin - the line number for the top margin.
// - bottomMargin - the line number for the bottom margin.
// - homeCursor - move the cursor to the home position.
// Return Value:
// - <none>
void AdaptDispatch::_DoSetTopBottomScrollingMargins(const VTInt topMargin,
                                                    const VTInt bottomMargin,
                                                    const bool homeCursor)
{
    // so notes time: (input -> state machine out -> adapter out -> conhost internal)
    // having only a top param is legal         ([3;r   -> 3,0   -> 3,h  -> 3,h,true)
    // having only a bottom param is legal      ([;3r   -> 0,3   -> 1,3  -> 1,3,true)
    // having neither uses the defaults         ([;r [r -> 0,0   -> 0,0  -> 0,0,false)
    // an illegal combo (eg, 3;2r) is ignored
    til::CoordType actualTop = topMargin;
    til::CoordType actualBottom = bottomMargin;

    const auto viewport = _api.GetViewport();
    const auto screenHeight = viewport.bottom - viewport.top;
    // The default top margin is line 1
    if (actualTop == 0)
    {
        actualTop = 1;
    }
    // The default bottom margin is the screen height
    if (actualBottom == 0)
    {
        actualBottom = screenHeight;
    }
    // The top margin must be less than the bottom margin, and the
    // bottom margin must be less than or equal to the screen height
    if (actualTop < actualBottom && actualBottom <= screenHeight)
    {
        if (actualTop == 1 && actualBottom == screenHeight)
        {
            // Client requests setting margins to the entire screen
            //    - clear them instead of setting them.
            // This is for apps like `apt` (NOT `apt-get` which set scroll
            //      margins, but don't use the alt buffer.)
            actualTop = 0;
            actualBottom = 0;
        }
        else
        {
            // In VT, the origin is 1,1. For our array, it's 0,0. So subtract 1.
            actualTop -= 1;
            actualBottom -= 1;
        }
        _scrollMargins.top = actualTop;
        _scrollMargins.bottom = actualBottom;
        // If requested, we may also need to move the cursor to the home
        // position, but only if the requested margins were valid.
        if (homeCursor)
        {
            CursorPosition(1, 1);
        }
    }
}

// Routine Description:
// - DECSTBM - Set Scrolling Region
// This control function sets the top and bottom margins for the current page.
//  You cannot perform scrolling outside the margins.
//  Default: Margins are at the page limits.
// Arguments:
// - topMargin - the line number for the top margin.
// - bottomMargin - the line number for the bottom margin.
// Return Value:
// - True.
bool AdaptDispatch::SetTopBottomScrollingMargins(const VTInt topMargin,
                                                 const VTInt bottomMargin)
{
    _DoSetTopBottomScrollingMargins(topMargin, bottomMargin, true);
    return true;
}

// Routine Description:
// - DECSLRM - Set Scrolling Region
// This control function sets the left and right margins for the current page.
//  You cannot perform scrolling outside the margins.
//  Default: Margins are at the page limits.
// Arguments:
// - leftMargin - the column number for the left margin.
// - rightMargin - the column number for the right margin.
// - homeCursor - move the cursor to the home position.
// Return Value:
// - <none>
void AdaptDispatch::_DoSetLeftRightScrollingMargins(const VTInt leftMargin,
                                                    const VTInt rightMargin,
                                                    const bool homeCursor)
{
    til::CoordType actualLeft = leftMargin;
    til::CoordType actualRight = rightMargin;

    const auto& textBuffer = _api.GetTextBuffer();
    const auto bufferWidth = textBuffer.GetSize().Width();
    // The default left margin is column 1
    if (actualLeft == 0)
    {
        actualLeft = 1;
    }
    // The default right margin is the buffer width
    if (actualRight == 0)
    {
        actualRight = bufferWidth;
    }
    // The left margin must be less than the right margin, and the
    // right margin must be less than or equal to the buffer width
    if (actualLeft < actualRight && actualRight <= bufferWidth)
    {
        if (actualLeft == 1 && actualRight == bufferWidth)
        {
            // Client requests setting margins to the entire screen
            //    - clear them instead of setting them.
            actualLeft = 0;
            actualRight = 0;
        }
        else
        {
            // In VT, the origin is 1,1. For our array, it's 0,0. So subtract 1.
            actualLeft -= 1;
            actualRight -= 1;
        }
        _scrollMargins.left = actualLeft;
        _scrollMargins.right = actualRight;
        // If requested, we may also need to move the cursor to the home
        // position, but only if the requested margins were valid.
        if (homeCursor)
        {
            CursorPosition(1, 1);
        }
    }
}

// Routine Description:
// - DECSLRM - Set Scrolling Region
// This control function sets the left and right margins for the current page.
//  You cannot perform scrolling outside the margins.
//  Default: Margins are at the page limits.
// Arguments:
// - leftMargin - the column number for the left margin.
// - rightMargin - the column number for the right margin.
// Return Value:
// - True.
bool AdaptDispatch::SetLeftRightScrollingMargins(const VTInt leftMargin,
                                                 const VTInt rightMargin)
{
    if (_modes.test(Mode::AllowDECSLRM))
    {
        _DoSetLeftRightScrollingMargins(leftMargin, rightMargin, true);
    }
    else
    {
        // When DECSLRM isn't allowed, `CSI s` is interpreted as ANSISYSSC.
        CursorSaveState();
    }
    return true;
}

// Routine Description:
// - BEL - Rings the warning bell.
//    Causes the terminal to emit an audible tone of brief duration.
// Arguments:
// - None
// Return Value:
// - True.
bool AdaptDispatch::WarningBell()
{
    _api.WarningBell();
    return true;
}

// Routine Description:
// - CR - Performs a carriage return.
//    Moves the cursor to the leftmost column.
// Arguments:
// - None
// Return Value:
// - True.
bool AdaptDispatch::CarriageReturn()
{
    return _CursorMovePosition(Offset::Unchanged(), Offset::Absolute(1), true);
}

// Routine Description:
// - Helper method for executing a line feed, possibly preceded by carriage return.
// Arguments:
// - textBuffer - Target buffer on which the line feed is executed.
// - withReturn - Set to true if a carriage return should be performed as well.
// - wrapForced - Set to true is the line feed was the result of the line wrapping.
// Return Value:
// - <none>
void AdaptDispatch::_DoLineFeed(TextBuffer& textBuffer, const bool withReturn, const bool wrapForced)
{
    const auto viewport = _api.GetViewport();
    const auto bufferWidth = textBuffer.GetSize().Width();
    const auto bufferHeight = textBuffer.GetSize().Height();
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(viewport, true);
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(bufferWidth);

    auto& cursor = textBuffer.GetCursor();
    const auto currentPosition = cursor.GetPosition();
    auto newPosition = currentPosition;

    // If the line was forced to wrap, set the wrap status.
    // When explicitly moving down a row, clear the wrap status.
    textBuffer.GetMutableRowByOffset(currentPosition.y).SetWrapForced(wrapForced);

    // If a carriage return was requested, we move to the leftmost column or
    // the left margin, depending on whether we started within the margins.
    if (withReturn)
    {
        const auto clampToMargin = currentPosition.y >= topMargin &&
                                   currentPosition.y <= bottomMargin &&
                                   currentPosition.x >= leftMargin;
        newPosition.x = clampToMargin ? leftMargin : 0;
    }

    if (currentPosition.y != bottomMargin || newPosition.x < leftMargin || newPosition.x > rightMargin)
    {
        // If we're not at the bottom margin, or outside the horizontal margins,
        // then there's no scrolling, so we make sure we don't move past the
        // bottom of the viewport.
        newPosition.y = std::min(currentPosition.y + 1, viewport.bottom - 1);
        newPosition = textBuffer.ClampPositionWithinLine(newPosition);
    }
    else if (topMargin > viewport.top || leftMargin > 0 || rightMargin < bufferWidth - 1)
    {
        // If the top margin isn't at the top of the viewport, or the
        // horizontal margins are set, then we're just scrolling the margin
        // area and the cursor stays where it is.
        _ScrollRectVertically(textBuffer, { leftMargin, topMargin, rightMargin + 1, bottomMargin + 1 }, -1);
    }
    else if (viewport.bottom < bufferHeight)
    {
        // If the top margin is at the top of the viewport, then we'll scroll
        // the content up by panning the viewport down, and also move the cursor
        // down a row. But we only do this if the viewport hasn't yet reached
        // the end of the buffer.
        _api.SetViewportPosition({ viewport.left, viewport.top + 1 });
        newPosition.y++;

        // And if the bottom margin didn't cover the full viewport, we copy the
        // lower part of the viewport down so it remains static. But for a full
        // pan we reset the newly revealed row with the erase attributes.
        if (bottomMargin < viewport.bottom - 1)
        {
            _ScrollRectVertically(textBuffer, { 0, bottomMargin + 1, bufferWidth, viewport.bottom + 1 }, 1);
        }
        else
        {
            const auto eraseAttributes = _GetEraseAttributes(textBuffer);
            textBuffer.GetMutableRowByOffset(newPosition.y).Reset(eraseAttributes);
        }
    }
    else
    {
        // If the viewport has reached the end of the buffer, we can't pan down,
        // so we cycle the row coordinates, which effectively scrolls the buffer
        // content up. In this case we don't need to move the cursor down.
        const auto eraseAttributes = _GetEraseAttributes(textBuffer);
        textBuffer.IncrementCircularBuffer(eraseAttributes);
        _api.NotifyBufferRotation(1);

        // We trigger a scroll rather than a redraw, since that's more efficient,
        // but we need to turn the cursor off before doing so, otherwise a ghost
        // cursor can be left behind in the previous position.
        cursor.SetIsOn(false);
        textBuffer.TriggerScroll({ 0, -1 });

        // And again, if the bottom margin didn't cover the full viewport, we
        // copy the lower part of the viewport down so it remains static.
        if (bottomMargin < viewport.bottom - 1)
        {
            _ScrollRectVertically(textBuffer, { 0, bottomMargin, bufferWidth, bufferHeight }, 1);
        }
    }

    cursor.SetPosition(newPosition);
    _ApplyCursorMovementFlags(cursor);
}

// Routine Description:
// - IND/NEL - Performs a line feed, possibly preceded by carriage return.
//    Moves the cursor down one line, and possibly also to the leftmost column.
// Arguments:
// - lineFeedType - Specify whether a carriage return should be performed as well.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::LineFeed(const DispatchTypes::LineFeedType lineFeedType)
{
    auto& textBuffer = _api.GetTextBuffer();
    switch (lineFeedType)
    {
    case DispatchTypes::LineFeedType::DependsOnMode:
        _DoLineFeed(textBuffer, _api.GetSystemMode(ITerminalApi::Mode::LineFeed), false);
        return true;
    case DispatchTypes::LineFeedType::WithoutReturn:
        _DoLineFeed(textBuffer, false, false);
        return true;
    case DispatchTypes::LineFeedType::WithReturn:
        _DoLineFeed(textBuffer, true, false);
        return true;
    default:
        return false;
    }
}

// Routine Description:
// - RI - Performs a "Reverse line feed", essentially, the opposite of '\n'.
//    Moves the cursor up one line, and tries to keep its position in the line
// Arguments:
// - None
// Return Value:
// - True.
bool AdaptDispatch::ReverseLineFeed()
{
    const auto viewport = _api.GetViewport();
    auto& textBuffer = _api.GetTextBuffer();
    auto& cursor = textBuffer.GetCursor();
    const auto cursorPosition = cursor.GetPosition();
    const auto bufferWidth = textBuffer.GetSize().Width();
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(bufferWidth);
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(viewport, true);

    // If the cursor is at the top of the margin area, we shift the buffer
    // contents down, to emulate inserting a line at that point.
    if (cursorPosition.y == topMargin && cursorPosition.x >= leftMargin && cursorPosition.x <= rightMargin)
    {
        _ScrollRectVertically(textBuffer, { leftMargin, topMargin, rightMargin + 1, bottomMargin + 1 }, 1);
    }
    else if (cursorPosition.y > viewport.top)
    {
        // Otherwise we move the cursor up, but not past the top of the viewport.
        cursor.SetPosition(textBuffer.ClampPositionWithinLine({ cursorPosition.x, cursorPosition.y - 1 }));
        _ApplyCursorMovementFlags(cursor);
    }
    return true;
}

// Routine Description:
// - DECBI - Moves the cursor back one column, scrolling the screen
//    horizontally if it reaches the left margin.
// Arguments:
// - None
// Return Value:
// - True.
bool AdaptDispatch::BackIndex()
{
    const auto viewport = _api.GetViewport();
    auto& textBuffer = _api.GetTextBuffer();
    auto& cursor = textBuffer.GetCursor();
    const auto cursorPosition = cursor.GetPosition();
    const auto bufferWidth = textBuffer.GetSize().Width();
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(bufferWidth);
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(viewport, true);

    // If the cursor is at the left of the margin area, we shift the buffer right.
    if (cursorPosition.x == leftMargin && cursorPosition.y >= topMargin && cursorPosition.y <= bottomMargin)
    {
        _ScrollRectHorizontally(textBuffer, { leftMargin, topMargin, rightMargin + 1, bottomMargin + 1 }, 1);
    }
    // Otherwise we move the cursor left, but not past the start of the line.
    else if (cursorPosition.x > 0)
    {
        cursor.SetXPosition(cursorPosition.x - 1);
        _ApplyCursorMovementFlags(cursor);
    }
    return true;
}

// Routine Description:
// - DECFI - Moves the cursor forward one column, scrolling the screen
//    horizontally if it reaches the right margin.
// Arguments:
// - None
// Return Value:
// - True.
bool AdaptDispatch::ForwardIndex()
{
    const auto viewport = _api.GetViewport();
    auto& textBuffer = _api.GetTextBuffer();
    auto& cursor = textBuffer.GetCursor();
    const auto cursorPosition = cursor.GetPosition();
    const auto bufferWidth = textBuffer.GetSize().Width();
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(bufferWidth);
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(viewport, true);

    // If the cursor is at the right of the margin area, we shift the buffer left.
    if (cursorPosition.x == rightMargin && cursorPosition.y >= topMargin && cursorPosition.y <= bottomMargin)
    {
        _ScrollRectHorizontally(textBuffer, { leftMargin, topMargin, rightMargin + 1, bottomMargin + 1 }, -1);
    }
    // Otherwise we move the cursor right, but not past the end of the line.
    else if (cursorPosition.x < textBuffer.GetLineWidth(cursorPosition.y) - 1)
    {
        cursor.SetXPosition(cursorPosition.x + 1);
        _ApplyCursorMovementFlags(cursor);
    }
    return true;
}

// Routine Description:
// - OSC Set Window Title - Sets the title of the window
// Arguments:
// - title - The string to set the title to.
// Return Value:
// - True.
bool AdaptDispatch::SetWindowTitle(std::wstring_view title)
{
    _api.SetWindowTitle(title);
    return true;
}

//Routine Description:
// HTS - sets a VT tab stop in the cursor's current column.
//Arguments:
// - None
// Return value:
// - True.
bool AdaptDispatch::HorizontalTabSet()
{
    const auto& textBuffer = _api.GetTextBuffer();
    const auto width = textBuffer.GetSize().Dimensions().width;
    const auto column = textBuffer.GetCursor().GetPosition().x;

    _InitTabStopsForWidth(width);
    _tabStopColumns.at(column) = true;

    return true;
}

//Routine Description:
// CHT - performing a forwards tab. This will take the
//     cursor to the tab stop following its current location. If there are no
//     more tabs in this row, it will take it to the right side of the window.
//     If it's already in the last column of the row, it will move it to the next line.
//Arguments:
// - numTabs - the number of tabs to perform
// Return value:
// - True.
bool AdaptDispatch::ForwardTab(const VTInt numTabs)
{
    auto& textBuffer = _api.GetTextBuffer();
    auto& cursor = textBuffer.GetCursor();
    auto column = cursor.GetPosition().x;
    const auto row = cursor.GetPosition().y;
    const auto width = textBuffer.GetLineWidth(row);
    auto tabsPerformed = 0;

    const auto viewport = _api.GetViewport();
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(viewport, true);
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(width);
    const auto clampToMargin = row >= topMargin && row <= bottomMargin && column <= rightMargin;
    const auto maxColumn = clampToMargin ? rightMargin : width - 1;

    _InitTabStopsForWidth(width);
    while (column < maxColumn && tabsPerformed < numTabs)
    {
        column++;
        if (til::at(_tabStopColumns, column))
        {
            tabsPerformed++;
        }
    }

    // While the STD 070 reference suggests that horizontal tabs should reset
    // the delayed wrap, almost none of the DEC terminals actually worked that
    // way, and most modern terminal emulators appear to have taken the same
    // approach (i.e. they don't reset). For us this is a bit messy, since all
    // cursor movement resets the flag automatically, so we need to save the
    // original state here, and potentially reapply it after the move.
    const auto delayedWrapOriginallySet = cursor.IsDelayedEOLWrap();
    cursor.SetXPosition(column);
    _ApplyCursorMovementFlags(cursor);
    if (delayedWrapOriginallySet)
    {
        cursor.DelayEOLWrap();
    }

    return true;
}

//Routine Description:
// CBT - performing a backwards tab. This will take the cursor to the tab stop
//     previous to its current location. It will not reverse line feed.
//Arguments:
// - numTabs - the number of tabs to perform
// Return value:
// - True.
bool AdaptDispatch::BackwardsTab(const VTInt numTabs)
{
    auto& textBuffer = _api.GetTextBuffer();
    auto& cursor = textBuffer.GetCursor();
    auto column = cursor.GetPosition().x;
    const auto row = cursor.GetPosition().y;
    const auto width = textBuffer.GetLineWidth(row);
    auto tabsPerformed = 0;

    const auto viewport = _api.GetViewport();
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(viewport, true);
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(width);
    const auto clampToMargin = row >= topMargin && row <= bottomMargin && column >= leftMargin;
    const auto minColumn = clampToMargin ? leftMargin : 0;

    _InitTabStopsForWidth(width);
    while (column > minColumn && tabsPerformed < numTabs)
    {
        column--;
        if (til::at(_tabStopColumns, column))
        {
            tabsPerformed++;
        }
    }

    cursor.SetXPosition(column);
    _ApplyCursorMovementFlags(cursor);
    return true;
}

//Routine Description:
// TBC - Used to clear set tab stops. ClearType ClearCurrentColumn (0) results
//     in clearing only the tab stop in the cursor's current column, if there
//     is one. ClearAllColumns (3) results in resetting all set tab stops.
//Arguments:
// - clearType - Whether to clear the current column, or all columns, defined in DispatchTypes::TabClearType
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::TabClear(const DispatchTypes::TabClearType clearType)
{
    switch (clearType)
    {
    case DispatchTypes::TabClearType::ClearCurrentColumn:
        _ClearSingleTabStop();
        return true;
    case DispatchTypes::TabClearType::ClearAllColumns:
        _ClearAllTabStops();
        return true;
    default:
        return false;
    }
}

// Routine Description:
// - Clears the tab stop in the cursor's current column, if there is one.
// Arguments:
// - <none>
// Return value:
// - <none>
void AdaptDispatch::_ClearSingleTabStop()
{
    const auto& textBuffer = _api.GetTextBuffer();
    const auto width = textBuffer.GetSize().Dimensions().width;
    const auto column = textBuffer.GetCursor().GetPosition().x;

    _InitTabStopsForWidth(width);
    _tabStopColumns.at(column) = false;
}

// Routine Description:
// - Clears all tab stops and resets the _initDefaultTabStops flag to indicate
//    that they shouldn't be reinitialized at the default positions.
// Arguments:
// - <none>
// Return value:
// - <none>
void AdaptDispatch::_ClearAllTabStops() noexcept
{
    _tabStopColumns.clear();
    _initDefaultTabStops = false;
}

// Routine Description:
// - DECST8C - If the parameter is SetEvery8Columns or is omitted, then this
//    clears all tab stops and sets the _initDefaultTabStops flag to indicate
//    that the default positions should be reinitialized when needed.
// Arguments:
// - setType - only SetEvery8Columns is supported
// Return value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::TabSet(const VTParameter setType) noexcept
{
    constexpr auto SetEvery8Columns = DispatchTypes::TabSetType::SetEvery8Columns;
    if (setType.value_or(SetEvery8Columns) == SetEvery8Columns)
    {
        _tabStopColumns.clear();
        _initDefaultTabStops = true;
        return true;
    }
    return false;
}

// Routine Description:
// - Resizes the _tabStopColumns table so it's large enough to support the
//    current screen width, initializing tab stops every 8 columns in the
//    newly allocated space, iff the _initDefaultTabStops flag is set.
// Arguments:
// - width - the width of the screen buffer that we need to accommodate
// Return value:
// - <none>
void AdaptDispatch::_InitTabStopsForWidth(const VTInt width)
{
    const auto screenWidth = gsl::narrow<size_t>(width);
    const auto initialWidth = _tabStopColumns.size();
    if (screenWidth > initialWidth)
    {
        _tabStopColumns.resize(screenWidth);
        if (_initDefaultTabStops)
        {
            for (auto column = 8u; column < _tabStopColumns.size(); column += 8)
            {
                if (column >= initialWidth)
                {
                    til::at(_tabStopColumns, column) = true;
                }
            }
        }
    }
}

//Routine Description:
// DOCS - Selects the coding system through which character sets are activated.
//     When ISO2022 is selected, the code page is set to ISO-8859-1, C1 control
//     codes are accepted, and both GL and GR areas of the code table can be
//     remapped. When UTF8 is selected, the code page is set to UTF-8, the C1
//     control codes are disabled, and only the GL area can be remapped.
//Arguments:
// - codingSystem - The coding system that will be selected.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::DesignateCodingSystem(const VTID codingSystem)
{
    // If we haven't previously saved the initial code page, do so now.
    // This will be used to restore the code page in response to a reset.
    if (!_initialCodePage.has_value())
    {
        _initialCodePage = _api.GetConsoleOutputCP();
    }

    switch (codingSystem)
    {
    case DispatchTypes::CodingSystem::ISO2022:
        _api.SetConsoleOutputCP(28591);
        AcceptC1Controls(true);
        _termOutput.EnableGrTranslation(true);
        return true;
    case DispatchTypes::CodingSystem::UTF8:
        _api.SetConsoleOutputCP(CP_UTF8);
        AcceptC1Controls(false);
        _termOutput.EnableGrTranslation(false);
        return true;
    default:
        return false;
    }
}

//Routine Description:
// Designate Charset - Selects a specific 94-character set into one of the four G-sets.
//     See http://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Controls-beginning-with-ESC
//       for a list of all charsets and their codes.
//     If the specified charset is unsupported, we do nothing (remain on the current one)
//Arguments:
// - gsetNumber - The G-set into which the charset will be selected.
// - charset - The identifier indicating the charset that will be used.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::Designate94Charset(const VTInt gsetNumber, const VTID charset)
{
    return _termOutput.Designate94Charset(gsetNumber, charset);
}

//Routine Description:
// Designate Charset - Selects a specific 96-character set into one of the four G-sets.
//     See http://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Controls-beginning-with-ESC
//       for a list of all charsets and their codes.
//     If the specified charset is unsupported, we do nothing (remain on the current one)
//Arguments:
// - gsetNumber - The G-set into which the charset will be selected.
// - charset - The identifier indicating the charset that will be used.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::Designate96Charset(const VTInt gsetNumber, const VTID charset)
{
    return _termOutput.Designate96Charset(gsetNumber, charset);
}

//Routine Description:
// Locking Shift - Invoke one of the G-sets into the left half of the code table.
//Arguments:
// - gsetNumber - The G-set that will be invoked.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::LockingShift(const VTInt gsetNumber)
{
    return _termOutput.LockingShift(gsetNumber);
}

//Routine Description:
// Locking Shift Right - Invoke one of the G-sets into the right half of the code table.
//Arguments:
// - gsetNumber - The G-set that will be invoked.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::LockingShiftRight(const VTInt gsetNumber)
{
    return _termOutput.LockingShiftRight(gsetNumber);
}

//Routine Description:
// Single Shift - Temporarily invoke one of the G-sets into the code table.
//Arguments:
// - gsetNumber - The G-set that will be invoked.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::SingleShift(const VTInt gsetNumber) noexcept
{
    return _termOutput.SingleShift(gsetNumber);
}

//Routine Description:
// DECAC1 - Enable or disable the reception of C1 control codes in the parser.
//Arguments:
// - enabled - true to allow C1 controls to be used, false to disallow.
// Return value:
// - True.
bool AdaptDispatch::AcceptC1Controls(const bool enabled)
{
    _api.GetStateMachine().SetParserMode(StateMachine::Mode::AcceptC1, enabled);
    return true;
}

//Routine Description:
// ACS - Announces the ANSI conformance level for subsequent data exchange.
//  This requires certain character sets to be mapped into the terminal's
//  G-sets and in-use tables.
//Arguments:
// - ansiLevel - the expected conformance level
// Return value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::AnnounceCodeStructure(const VTInt ansiLevel)
{
    // Levels 1 and 2 require ASCII in G0/GL and Latin-1 in G1/GR.
    // Level 3 only requires ASCII in G0/GL.
    switch (ansiLevel)
    {
    case 1:
    case 2:
        Designate96Charset(1, VTID("A")); // Latin-1 designated as G1
        LockingShiftRight(1); // G1 mapped into GR
        [[fallthrough]];
    case 3:
        Designate94Charset(0, VTID("B")); // ASCII designated as G0
        LockingShift(0); // G0 mapped into GL
        return true;
    default:
        return false;
    }
}

//Routine Description:
// Soft Reset - Perform a soft reset. See http://www.vt100.net/docs/vt510-rm/DECSTR.html
// The following table lists everything that should be done, 'X's indicate the ones that
//   we actually perform. As the appropriate functionality is added to our ANSI support,
//   we should update this.
//  X Text cursor enable          DECTCEM     Cursor enabled.
//  X Insert/replace              IRM         Replace mode.
//  X Origin                      DECOM       Absolute (cursor origin at upper-left of screen.)
//  X Autowrap                    DECAWM      Autowrap enabled (matches XTerm behavior).
//    National replacement        DECNRCM     Multinational set.
//        character set
//    Keyboard action             KAM         Unlocked.
//  X Numeric keypad              DECNKM      Numeric characters.
//  X Cursor keys                 DECCKM      Normal (arrow keys).
//  X Set top and bottom margins  DECSTBM     Top margin = 1; bottom margin = page length.
//  X All character sets          G0, G1, G2, Default settings.
//                                G3, GL, GR
//  X Select graphic rendition    SGR         Normal rendition.
//  X Select character attribute  DECSCA      Normal (erasable by DECSEL and DECSED).
//  X Save cursor state           DECSC       Home position.
//  X Assign user preference      DECAUPSS    Always Latin-1 (not configurable).
//        supplemental set
//    Select active               DECSASD     Main display.
//        status display
//    Keyboard position mode      DECKPM      Character codes.
//    Cursor direction            DECRLM      Reset (Left-to-right), regardless of NVR setting.
//    PC Term mode                DECPCTERM   Always reset.
//Arguments:
// <none>
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::SoftReset()
{
    _api.GetTextBuffer().GetCursor().SetIsVisible(true); // Cursor enabled.

    // Replace mode; Absolute cursor addressing; Disallow left/right margins.
    _modes.reset(Mode::InsertReplace, Mode::Origin, Mode::AllowDECSLRM);

    _api.SetSystemMode(ITerminalApi::Mode::AutoWrap, true); // Wrap at end of line.
    _terminalInput.SetInputMode(TerminalInput::Mode::CursorKey, false); // Normal characters.
    _terminalInput.SetInputMode(TerminalInput::Mode::Keypad, false); // Numeric characters.

    // Top margin = 1; bottom margin = page length.
    _DoSetTopBottomScrollingMargins(0, 0);
    // Left margin = 1; right margin = page width.
    _DoSetLeftRightScrollingMargins(0, 0);

    _termOutput.SoftReset(); // Reset all character set designations.

    SetGraphicsRendition({}); // Normal rendition.
    SetCharacterProtectionAttribute({}); // Default (unprotected)

    // Reset the saved cursor state.
    // Note that XTerm only resets the main buffer state, but that
    // seems likely to be a bug. Most other terminals reset both.
    _savedCursorState.at(0) = {}; // Main buffer
    _savedCursorState.at(1) = {}; // Alt buffer

    // The TerminalOutput state in these buffers must be reset to
    // the same state as the _termOutput instance, which is not
    // necessarily equivalent to a full reset.
    _savedCursorState.at(0).TermOutput = _termOutput;
    _savedCursorState.at(1).TermOutput = _termOutput;

    return !_api.IsConsolePty();
}

//Routine Description:
// Full Reset - Perform a hard reset of the terminal. http://vt100.net/docs/vt220-rm/chapter4.html
//  RIS performs the following actions: (Items with sub-bullets are supported)
//   - Switches to the main screen buffer if in the alt buffer.
//      * This matches the XTerm behaviour, which is the de facto standard for the alt buffer.
//   - Performs a communications line disconnect.
//   - Clears UDKs.
//   - Clears a down-line-loaded character set.
//      * The soft font is reset in the renderer and the font buffer is deleted.
//   - Clears the screen.
//      * This is like Erase in Display (3), also clearing scrollback, as well as ED(2)
//   - Returns the cursor to the upper-left corner of the screen.
//      * CUP(1;1)
//   - Sets the SGR state to normal.
//      * SGR(Off)
//   - Sets the selective erase attribute write state to "not erasable".
//   - Sets all character sets to the default.
//      * G0(USASCII)
//Arguments:
// <none>
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::HardReset()
{
    // If in the alt buffer, switch back to main before doing anything else.
    if (_usingAltBuffer)
    {
        _api.UseMainScreenBuffer();
        _usingAltBuffer = false;
    }

    // Completely reset the TerminalOutput state.
    _termOutput = {};
    if (_initialCodePage.has_value())
    {
        // Restore initial code page if previously changed by a DOCS sequence.
        _api.SetConsoleOutputCP(_initialCodePage.value());
    }
    // Disable parsing of C1 control codes.
    AcceptC1Controls(false);

    // Sets the SGR state to normal - this must be done before EraseInDisplay
    //      to ensure that it clears with the default background color.
    SoftReset();

    // Clears the screen - Needs to be done in two operations.
    EraseInDisplay(DispatchTypes::EraseType::All);
    EraseInDisplay(DispatchTypes::EraseType::Scrollback);

    // Set the DECSCNM screen mode back to normal.
    _renderSettings.SetRenderMode(RenderSettings::Mode::ScreenReversed, false);

    // Cursor to 1,1 - the Soft Reset guarantees this is absolute
    CursorPosition(1, 1);

    // We only reset the system line feed mode if the input mode is set. If it
    // isn't set, that either means they're both reset, and there's nothing for
    // us to do, or they're out of sync, which implies the system mode was set
    // via the console API, so it's not our responsibility.
    if (_terminalInput.GetInputMode(TerminalInput::Mode::LineFeed))
    {
        _api.SetSystemMode(ITerminalApi::Mode::LineFeed, false);
    }

    // Reset input modes to their initial state
    _terminalInput.ResetInputModes();

    // Reset bracketed paste mode
    _api.SetSystemMode(ITerminalApi::Mode::BracketedPaste, false);

    // Restore cursor blinking mode.
    _api.GetTextBuffer().GetCursor().SetBlinkingAllowed(true);

    // Delete all current tab stops and reapply
    TabSet(DispatchTypes::TabSetType::SetEvery8Columns);

    // Clear the soft font in the renderer and delete the font buffer.
    _renderer.UpdateSoftFont({}, {}, false);
    _fontBuffer = nullptr;

    // Reset internal modes to their initial state
    _modes = {};

    // Clear and release the macro buffer.
    if (_macroBuffer)
    {
        _macroBuffer->ClearMacrosIfInUse();
        _macroBuffer = nullptr;
    }

    // If we're in a conpty, we need flush this RIS sequence to the connected
    // terminal application, but we also need to follow that up with a DECSET
    // sequence to re-enable the modes that we require (namely win32 input mode
    // and focus event mode). It's important that this is kept in sync with the
    // VtEngine::RequestWin32Input method which requests the modes on startup.
    if (_api.IsConsolePty())
    {
        auto& stateMachine = _api.GetStateMachine();
        if (stateMachine.FlushToTerminal())
        {
            auto& engine = stateMachine.Engine();
            engine.ActionPassThroughString(L"\033[?9001h\033[?1004h");
        }
    }
    return true;
}

// Routine Description:
// - DECALN - Fills the entire screen with a test pattern of uppercase Es,
//    resets the margins and rendition attributes, and moves the cursor to
//    the home position.
// Arguments:
// - None
// Return Value:
// - True.
bool AdaptDispatch::ScreenAlignmentPattern()
{
    const auto viewport = _api.GetViewport();
    auto& textBuffer = _api.GetTextBuffer();
    const auto bufferWidth = textBuffer.GetSize().Dimensions().width;

    // Fill the screen with the letter E using the default attributes.
    _FillRect(textBuffer, { 0, viewport.top, bufferWidth, viewport.bottom }, L"E", {});
    // Reset the line rendition for all of these rows.
    textBuffer.ResetLineRenditionRange(viewport.top, viewport.bottom);
    // Reset the meta/extended attributes (but leave the colors unchanged).
    auto attr = textBuffer.GetCurrentAttributes();
    attr.SetStandardErase();
    _api.SetTextAttributes(attr);
    // Reset the origin mode to absolute, and disallow left/right margins.
    _modes.reset(Mode::Origin, Mode::AllowDECSLRM);
    // Clear the scrolling margins.
    _DoSetTopBottomScrollingMargins(0, 0);
    _DoSetLeftRightScrollingMargins(0, 0);
    // Set the cursor position to home.
    CursorPosition(1, 1);

    return true;
}

//Routine Description:
//  - Erase Scrollback (^[[3J - ED extension by xterm)
//    Because conhost doesn't exactly have a scrollback, We have to be tricky here.
//    We need to move the entire viewport to 0,0, and clear everything outside
//      (0, 0, viewportWidth, viewportHeight) To give the appearance that
//      everything above the viewport was cleared.
//    We don't want to save the text BELOW the viewport, because in *nix, there isn't anything there
//      (There isn't a scroll-forward, only a scrollback)
// Arguments:
// - <none>
// Return value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::_EraseScrollback()
{
    const auto viewport = _api.GetViewport();
    const auto top = viewport.top;
    const auto height = viewport.bottom - viewport.top;
    auto& textBuffer = _api.GetTextBuffer();
    const auto bufferSize = textBuffer.GetSize().Dimensions();
    auto& cursor = textBuffer.GetCursor();
    const auto row = cursor.GetPosition().y;

    textBuffer.ClearScrollback(top, height);
    // Move the viewport
    _api.SetViewportPosition({ viewport.left, 0 });
    // Move the cursor to the same relative location.
    cursor.SetYPosition(row - top);
    cursor.SetHasMoved(true);

    // GH#2715 - If this succeeded, but we're in a conpty, return `false` to
    // make the state machine propagate this ED sequence to the connected
    // terminal application. While we're in conpty mode, we don't really
    // have a scrollback, but the attached terminal might.
    return !_api.IsConsolePty();
}

//Routine Description:
// - Erase All (^[[2J - ED)
//   Performs a VT Erase All operation. In most terminals, this is done by
//   moving the viewport into the scrollback, clearing out the current screen.
//   For them, there can never be any characters beneath the viewport, as the
//   viewport is always at the bottom. So, we can accomplish the same behavior
//   by using the LastNonspaceCharacter as the "bottom", and placing the new
//   viewport underneath that character.
// Arguments:
// - <none>
// Return value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::_EraseAll()
{
    const auto viewport = _api.GetViewport();
    const auto viewportHeight = viewport.bottom - viewport.top;
    auto& textBuffer = _api.GetTextBuffer();
    const auto bufferSize = textBuffer.GetSize();
    const auto inPtyMode = _api.IsConsolePty();

    // Stash away the current position of the cursor within the viewport.
    // We'll need to restore the cursor to that same relative position, after
    //      we move the viewport.
    auto& cursor = textBuffer.GetCursor();
    const auto row = cursor.GetPosition().y - viewport.top;

    // Calculate new viewport position. Typically we want to move one line below
    // the last non-space row, but if the last non-space character is the very
    // start of the buffer, then we shouldn't move down at all.
    const auto lastChar = textBuffer.GetLastNonSpaceCharacter();
    auto newViewportTop = lastChar == til::point{} ? 0 : lastChar.y + 1;
    auto newViewportBottom = newViewportTop + viewportHeight;
    const auto delta = newViewportBottom - (bufferSize.Height());
    if (delta > 0)
    {
        for (auto i = 0; i < delta; i++)
        {
            textBuffer.IncrementCircularBuffer();
        }
        _api.NotifyBufferRotation(delta);
        newViewportTop -= delta;
        newViewportBottom -= delta;
        // We don't want to trigger a scroll in pty mode, because we're going to
        // pass through the ED sequence anyway, and this will just result in the
        // buffer being scrolled up by two pages instead of one.
        if (!inPtyMode)
        {
            textBuffer.TriggerScroll({ 0, -delta });
        }
    }
    // Move the viewport
    _api.SetViewportPosition({ viewport.left, newViewportTop });
    // Restore the relative cursor position
    cursor.SetYPosition(row + newViewportTop);
    cursor.SetHasMoved(true);

    // Erase all the rows in the current viewport.
    const auto eraseAttributes = _GetEraseAttributes(textBuffer);
    _FillRect(textBuffer, { 0, newViewportTop, bufferSize.Width(), newViewportBottom }, whitespace, eraseAttributes);

    // Also reset the line rendition for the erased rows.
    textBuffer.ResetLineRenditionRange(newViewportTop, newViewportBottom);

    // Clear any marks that remain below the start of the
    textBuffer.ClearMarksInRange(til::point{ 0, newViewportTop },
                                 til::point{ bufferSize.Width(), bufferSize.Height() });

    // GH#5683 - If this succeeded, but we're in a conpty, return `false` to
    // make the state machine propagate this ED sequence to the connected
    // terminal application. While we're in conpty mode, when the client
    // requests a Erase All operation, we need to manually tell the
    // connected terminal to do the same thing, so that the terminal will
    // move it's own buffer contents into the scrollback.
    return !inPtyMode;
}

//Routine Description:
// Set Cursor Style - Changes the cursor's style to match the given Dispatch
//      cursor style. Unix styles are a combination of the shape and the blinking state.
//Arguments:
// - cursorStyle - The unix-like cursor style to apply to the cursor
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::SetCursorStyle(const DispatchTypes::CursorStyle cursorStyle)
{
    auto actualType = CursorType::Legacy;
    auto fEnableBlinking = false;

    switch (cursorStyle)
    {
    case DispatchTypes::CursorStyle::UserDefault:
        fEnableBlinking = true;
        actualType = _api.GetUserDefaultCursorStyle();
        break;
    case DispatchTypes::CursorStyle::BlinkingBlock:
        fEnableBlinking = true;
        actualType = CursorType::FullBox;
        break;
    case DispatchTypes::CursorStyle::SteadyBlock:
        fEnableBlinking = false;
        actualType = CursorType::FullBox;
        break;

    case DispatchTypes::CursorStyle::BlinkingUnderline:
        fEnableBlinking = true;
        actualType = CursorType::Underscore;
        break;
    case DispatchTypes::CursorStyle::SteadyUnderline:
        fEnableBlinking = false;
        actualType = CursorType::Underscore;
        break;

    case DispatchTypes::CursorStyle::BlinkingBar:
        fEnableBlinking = true;
        actualType = CursorType::VerticalBar;
        break;
    case DispatchTypes::CursorStyle::SteadyBar:
        fEnableBlinking = false;
        actualType = CursorType::VerticalBar;
        break;

    default:
        // Invalid argument should be handled by the connected terminal.
        return false;
    }

    auto& cursor = _api.GetTextBuffer().GetCursor();
    cursor.SetType(actualType);
    cursor.SetBlinkingAllowed(fEnableBlinking);

    // If we're a conpty, always return false, so that this cursor state will be
    // sent to the connected terminal
    return !_api.IsConsolePty();
}

// Method Description:
// - Sets a single entry of the colortable to a new value
// Arguments:
// - tableIndex: The VT color table index
// - dwColor: The new RGB color value to use.
// Return Value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::SetCursorColor(const COLORREF cursorColor)
{
    return SetColorTableEntry(TextColor::CURSOR_COLOR, cursorColor);
}

// Routine Description:
// - OSC Copy to Clipboard
// Arguments:
// - content - The content to copy to clipboard. Must be null terminated.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::SetClipboard(const std::wstring_view content)
{
    // Return false to forward the operation to the hosting terminal,
    // since ConPTY can't handle this itself.
    if (_api.IsConsolePty())
    {
        return false;
    }
    _api.CopyToClipboard(content);
    return true;
}

// Method Description:
// - Sets a single entry of the colortable to a new value
// Arguments:
// - tableIndex: The VT color table index
// - dwColor: The new RGB color value to use.
// Return Value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::SetColorTableEntry(const size_t tableIndex, const DWORD dwColor)
{
    _renderSettings.SetColorTableEntry(tableIndex, dwColor);

    // If we're a conpty, always return false, so that we send the updated color
    //      value to the terminal. Still handle the sequence so apps that use
    //      the API or VT to query the values of the color table still read the
    //      correct color.
    if (_api.IsConsolePty())
    {
        return false;
    }

    // If we're updating the background color, we need to let the renderer
    // know, since it may want to repaint the window background to match.
    const auto backgroundIndex = _renderSettings.GetColorAliasIndex(ColorAlias::DefaultBackground);
    const auto backgroundChanged = (tableIndex == backgroundIndex);

    // Similarly for the frame color, the tab may need to be repainted.
    const auto frameIndex = _renderSettings.GetColorAliasIndex(ColorAlias::FrameBackground);
    const auto frameChanged = (tableIndex == frameIndex);

    // Update the screen colors if we're not a pty
    // No need to force a redraw in pty mode.
    _renderer.TriggerRedrawAll(backgroundChanged, frameChanged);
    return true;
}

// Method Description:
// - Sets the default foreground color to a new value
// Arguments:
// - dwColor: The new RGB color value to use, as a COLORREF, format 0x00BBGGRR.
// Return Value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::SetDefaultForeground(const DWORD dwColor)
{
    _renderSettings.SetColorAliasIndex(ColorAlias::DefaultForeground, TextColor::DEFAULT_FOREGROUND);
    return SetColorTableEntry(TextColor::DEFAULT_FOREGROUND, dwColor);
}

// Method Description:
// - Sets the default background color to a new value
// Arguments:
// - dwColor: The new RGB color value to use, as a COLORREF, format 0x00BBGGRR.
// Return Value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::SetDefaultBackground(const DWORD dwColor)
{
    _renderSettings.SetColorAliasIndex(ColorAlias::DefaultBackground, TextColor::DEFAULT_BACKGROUND);
    return SetColorTableEntry(TextColor::DEFAULT_BACKGROUND, dwColor);
}

// Method Description:
// DECAC - Assigns the foreground and background color indexes that should be
//   used for a given aspect of the user interface.
// Arguments:
// - item: The aspect of the interface that will have its colors altered.
// - fgIndex: The color table index to be used for the foreground.
// - bgIndex: The color table index to be used for the background.
// Return Value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::AssignColor(const DispatchTypes::ColorItem item, const VTInt fgIndex, const VTInt bgIndex)
{
    switch (item)
    {
    case DispatchTypes::ColorItem::NormalText:
        _renderSettings.SetColorAliasIndex(ColorAlias::DefaultForeground, fgIndex);
        _renderSettings.SetColorAliasIndex(ColorAlias::DefaultBackground, bgIndex);
        break;
    case DispatchTypes::ColorItem::WindowFrame:
        _renderSettings.SetColorAliasIndex(ColorAlias::FrameForeground, fgIndex);
        _renderSettings.SetColorAliasIndex(ColorAlias::FrameBackground, bgIndex);
        break;
    default:
        return false;
    }

    // No need to force a redraw in pty mode.
    const auto inPtyMode = _api.IsConsolePty();
    if (!inPtyMode)
    {
        const auto backgroundChanged = item == DispatchTypes::ColorItem::NormalText;
        const auto frameChanged = item == DispatchTypes::ColorItem::WindowFrame;
        _renderer.TriggerRedrawAll(backgroundChanged, frameChanged);
    }
    return !inPtyMode;
}

//Routine Description:
// Window Manipulation - Performs a variety of actions relating to the window,
//      such as moving the window position, resizing the window, querying
//      window state, forcing the window to repaint, etc.
//  This is kept separate from the input version, as there may be
//      codes that are supported in one direction but not the other.
//Arguments:
// - function - An identifier of the WindowManipulation function to perform
// - parameter1 - The first optional parameter for the function
// - parameter2 - The second optional parameter for the function
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::WindowManipulation(const DispatchTypes::WindowManipulationType function,
                                       const VTParameter parameter1,
                                       const VTParameter parameter2)
{
    // Other Window Manipulation functions:
    //  MSFT:13271098 - QueryViewport
    //  MSFT:13271146 - QueryScreenSize
    switch (function)
    {
    case DispatchTypes::WindowManipulationType::DeIconifyWindow:
        _api.ShowWindow(true);
        return true;
    case DispatchTypes::WindowManipulationType::IconifyWindow:
        _api.ShowWindow(false);
        return true;
    case DispatchTypes::WindowManipulationType::RefreshWindow:
        _api.GetTextBuffer().TriggerRedrawAll();
        return true;
    case DispatchTypes::WindowManipulationType::ResizeWindowInCharacters:
        _api.ResizeWindow(parameter2.value_or(0), parameter1.value_or(0));
        return true;
    case DispatchTypes::WindowManipulationType::ReportTextSizeInCharacters:
        _api.ReturnResponse(fmt::format(FMT_COMPILE(L"\033[8;{};{}t"), _api.GetViewport().height(), _api.GetTextBuffer().GetSize().Width()));
        return true;
    default:
        return false;
    }
}

// Method Description:
// - Starts a hyperlink
// Arguments:
// - The hyperlink URI, optional additional parameters
// Return Value:
// - true
bool AdaptDispatch::AddHyperlink(const std::wstring_view uri, const std::wstring_view params)
{
    auto& textBuffer = _api.GetTextBuffer();
    auto attr = textBuffer.GetCurrentAttributes();
    const auto id = textBuffer.GetHyperlinkId(uri, params);
    attr.SetHyperlinkId(id);
    textBuffer.SetCurrentAttributes(attr);
    textBuffer.AddHyperlinkToMap(uri, id);
    return true;
}

// Method Description:
// - Ends a hyperlink
// Return Value:
// - true
bool AdaptDispatch::EndHyperlink()
{
    auto& textBuffer = _api.GetTextBuffer();
    auto attr = textBuffer.GetCurrentAttributes();
    attr.SetHyperlinkId(0);
    textBuffer.SetCurrentAttributes(attr);
    return true;
}

// Method Description:
// - Performs a ConEmu action
//   Currently, the only actions we support are setting the taskbar state/progress
//   and setting the working directory.
// Arguments:
// - string - contains the parameters that define which action we do
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::DoConEmuAction(const std::wstring_view string)
{
    // Return false to forward the operation to the hosting terminal,
    // since ConPTY can't handle this itself.
    if (_api.IsConsolePty())
    {
        return false;
    }

    constexpr size_t TaskbarMaxState{ 4 };
    constexpr size_t TaskbarMaxProgress{ 100 };

    unsigned int state = 0;
    unsigned int progress = 0;

    const auto parts = Utils::SplitString(string, L';');
    unsigned int subParam = 0;

    if (parts.size() < 1 || !Utils::StringToUint(til::at(parts, 0), subParam))
    {
        return false;
    }

    // 4 is SetProgressBar, which sets the taskbar state/progress.
    if (subParam == 4)
    {
        if (parts.size() >= 2)
        {
            // A state parameter is defined, parse it out
            const auto stateSuccess = Utils::StringToUint(til::at(parts, 1), state);
            if (!stateSuccess && !til::at(parts, 1).empty())
            {
                return false;
            }
            if (parts.size() >= 3)
            {
                // A progress parameter is also defined, parse it out
                const auto progressSuccess = Utils::StringToUint(til::at(parts, 2), progress);
                if (!progressSuccess && !til::at(parts, 2).empty())
                {
                    return false;
                }
            }
        }

        if (state > TaskbarMaxState)
        {
            // state is out of bounds, return false
            return false;
        }
        if (progress > TaskbarMaxProgress)
        {
            // progress is greater than the maximum allowed value, clamp it to the max
            progress = TaskbarMaxProgress;
        }
        _api.SetTaskbarProgress(static_cast<DispatchTypes::TaskbarState>(state), progress);
        return true;
    }
    // 9 is SetWorkingDirectory, which informs the terminal about the current working directory.
    else if (subParam == 9)
    {
        if (parts.size() >= 2)
        {
            auto path = til::at(parts, 1);
            // The path should be surrounded with '"' according to the documentation of ConEmu.
            // An example: 9;"D:/"
            // If we fail to find the surrounding quotation marks, we'll give the path a try anyway.
            // ConEmu also does this.
            if (path.size() >= 3 && path.at(0) == L'"' && path.at(path.size() - 1) == L'"')
            {
                path = path.substr(1, path.size() - 2);
            }

            if (!til::is_legal_path(path))
            {
                return false;
            }

            _api.SetWorkingDirectory(path);
            return true;
        }
    }
    // 12: "Let ConEmu treat current cursor position as prompt start"
    //
    // Based on the official conemu docs:
    // * https://conemu.github.io/en/ShellWorkDir.html#connector-ps1
    // * https://conemu.github.io/en/ShellWorkDir.html#PowerShell
    //
    // This seems like basically the same as 133;B - the end of the prompt, the start of the commandline.
    else if (subParam == 12)
    {
        _api.MarkCommandStart();
        return true;
    }

    return false;
}

// Method Description:
// - Performs a iTerm2 action
// - Ascribes to the ITermDispatch interface
// - Currently, the actions we support are:
//   * `OSC1337;SetMark`: mark a line as a prompt line
// - Not actually used in conhost
// Arguments:
// - string: contains the parameters that define which action we do
// Return Value:
// - false in conhost, true for the SetMark action, otherwise false.
bool AdaptDispatch::DoITerm2Action(const std::wstring_view string)
{
    // This is not implemented in conhost.
    if (_api.IsConsolePty())
    {
        // Flush the frame manually, to make sure marks end up on the right line, like the alt buffer sequence.
        _renderer.TriggerFlush(false);
        return false;
    }

    if constexpr (!Feature_ScrollbarMarks::IsEnabled())
    {
        return false;
    }

    const auto parts = Utils::SplitString(string, L';');

    if (parts.size() < 1)
    {
        return false;
    }

    const auto action = til::at(parts, 0);

    if (action == L"SetMark")
    {
        ScrollMark mark;
        mark.category = MarkCategory::Prompt;
        _api.MarkPrompt(mark);
        return true;
    }
    return false;
}

// Method Description:
// - Performs a FinalTerm action
// - Currently, the actions we support are:
//   * `OSC133;A`: mark a line as a prompt line
// - Not actually used in conhost
// - The remainder of the FTCS prompt sequences are tracked in GH#11000
// Arguments:
// - string: contains the parameters that define which action we do
// Return Value:
// - false in conhost, true for the SetMark action, otherwise false.
bool AdaptDispatch::DoFinalTermAction(const std::wstring_view string)
{
    // This is not implemented in conhost.
    if (_api.IsConsolePty())
    {
        // Flush the frame manually, to make sure marks end up on the right line, like the alt buffer sequence.
        _renderer.TriggerFlush(false);
        return false;
    }

    if constexpr (!Feature_ScrollbarMarks::IsEnabled())
    {
        return false;
    }

    const auto parts = Utils::SplitString(string, L';');

    if (parts.size() < 1)
    {
        return false;
    }

    const auto action = til::at(parts, 0);
    if (action.size() == 1)
    {
        switch (til::at(action, 0))
        {
        case L'A': // FTCS_PROMPT
        {
            // Simply just mark this line as a prompt line.
            ScrollMark mark;
            mark.category = MarkCategory::Prompt;
            _api.MarkPrompt(mark);
            return true;
        }
        case L'B': // FTCS_COMMAND_START
        {
            _api.MarkCommandStart();
            return true;
        }
        case L'C': // FTCS_COMMAND_EXECUTED
        {
            _api.MarkOutputStart();
            return true;
        }
        case L'D': // FTCS_COMMAND_FINISHED
        {
            std::optional<unsigned int> error = std::nullopt;
            if (parts.size() >= 2)
            {
                const auto errorString = til::at(parts, 1);

                // If we fail to parse the code, then it was gibberish, or it might
                // have just started with "-". Either way, let's just treat it as an
                // error and move on.
                //
                // We know that "0" will be successfully parsed, and that's close enough.
                unsigned int parsedError = 0;
                error = Utils::StringToUint(errorString, parsedError) ? parsedError :
                                                                        UINT_MAX;
            }
            _api.MarkCommandFinish(error);
            return true;
        }
        default:
        {
            return false;
        }
        }
    }

    // When we add the rest of the FTCS sequences (GH#11000), we should add a
    // simple state machine here to track the most recently emitted mark from
    // this set of sequences, and which sequence was emitted last, so we can
    // modify the state of that mark as we go.
    return false;
}
// Method Description:
// - Performs a VsCode action
// - Currently, the actions we support are:
//   * Completions: An experimental protocol for passing shell completion
//     information from the shell to the terminal. This sequence is still under
//     active development, and subject to change.
// - Not actually used in conhost
// Arguments:
// - string: contains the parameters that define which action we do
// Return Value:
// - false in conhost, true for the SetMark action, otherwise false.
bool AdaptDispatch::DoVsCodeAction(const std::wstring_view string)
{
    // This is not implemented in conhost.
    if (_api.IsConsolePty())
    {
        // Flush the frame manually to make sure this action happens at the right time.
        _renderer.TriggerFlush(false);
        return false;
    }

    if constexpr (!Feature_ShellCompletions::IsEnabled())
    {
        return false;
    }

    const auto parts = Utils::SplitString(string, L';');

    if (parts.size() < 1)
    {
        return false;
    }

    const auto action = til::at(parts, 0);

    if (action == L"Completions")
    {
        // The structure of the message is as follows:
        // `e]633;
        // 0:     Completions;
        // 1:     $($completions.ReplacementIndex);
        // 2:     $($completions.ReplacementLength);
        // 3:     $($cursorIndex);
        // 4:     $completions.CompletionMatches | ConvertTo-Json
        unsigned int replacementIndex = 0;
        unsigned int replacementLength = 0;
        unsigned int cursorIndex = 0;

        bool succeeded = (parts.size() >= 2) &&
                         (Utils::StringToUint(til::at(parts, 1), replacementIndex));
        succeeded &= (parts.size() >= 3) &&
                     (Utils::StringToUint(til::at(parts, 2), replacementLength));
        succeeded &= (parts.size() >= 4) &&
                     (Utils::StringToUint(til::at(parts, 3), cursorIndex));

        // VsCode is using cursorIndex and replacementIndex, but we aren't currently.
        if (succeeded)
        {
            // Get the combined lengths of parts 0-3, plus the semicolons. We
            // need this so that we can just pass the remainder of the string.
            const auto prefixLength = til::at(parts, 0).size() + 1 +
                                      til::at(parts, 1).size() + 1 +
                                      til::at(parts, 2).size() + 1 +
                                      til::at(parts, 3).size() + 1;
            if (prefixLength > string.size())
            {
                return true;
            }
            // Get the remainder of the string
            const auto remainder = string.substr(prefixLength);

            _api.InvokeCompletions(parts.size() < 5 ? L"" : remainder,
                                   replacementLength);
        }

        // If it's poorly formatted, just eat it
        return true;
    }
    return false;
}

// Method Description:
// - DECDLD - Downloads one or more characters of a dynamically redefinable
//   character set (DRCS) with a specified pixel pattern. The pixel array is
//   transmitted in sixel format via the returned StringHandler function.
// Arguments:
// - fontNumber - The buffer number into which the font will be loaded.
// - startChar - The first character in the set that will be replaced.
// - eraseControl - Which characters to erase before loading the new data.
// - cellMatrix - The character cell width (sometimes also height in legacy formats).
// - fontSet - The screen size for which the font is designed.
// - fontUsage - Whether it is a text font or a full-cell font.
// - cellHeight - The character cell height (if not defined by cellMatrix).
// - charsetSize - Whether the character set is 94 or 96 characters.
// Return Value:
// - a function to receive the pixel data or nullptr if parameters are invalid
ITermDispatch::StringHandler AdaptDispatch::DownloadDRCS(const VTInt fontNumber,
                                                         const VTParameter startChar,
                                                         const DispatchTypes::DrcsEraseControl eraseControl,
                                                         const DispatchTypes::DrcsCellMatrix cellMatrix,
                                                         const DispatchTypes::DrcsFontSet fontSet,
                                                         const DispatchTypes::DrcsFontUsage fontUsage,
                                                         const VTParameter cellHeight,
                                                         const DispatchTypes::CharsetSize charsetSize)
{
    // The font buffer is created on demand.
    if (!_fontBuffer)
    {
        _fontBuffer = std::make_unique<FontBuffer>();
    }

    // Only one font buffer is supported, so only 0 (default) and 1 are valid.
    auto success = fontNumber <= 1;
    success = success && _fontBuffer->SetEraseControl(eraseControl);
    success = success && _fontBuffer->SetAttributes(cellMatrix, cellHeight, fontSet, fontUsage);
    success = success && _fontBuffer->SetStartChar(startChar, charsetSize);

    // If any of the parameters are invalid, we return a null handler to let
    // the state machine know we want to ignore the subsequent data string.
    if (!success)
    {
        return nullptr;
    }

    // If we're a conpty, we create a special passthrough handler that will
    // forward the DECDLD sequence to the conpty terminal with a hard-coded ID.
    // That ID is also pre-mapped into the G1 table, so the VT engine can just
    // switch to G1 when it needs to output any DRCS characters. But note that
    // we still need to process the DECDLD sequence locally, so the character
    // set translation is correctly handled on the host side.
    const auto conptyPassthrough = _api.IsConsolePty() ? _CreateDrcsPassthroughHandler(charsetSize) : nullptr;

    return [=](const auto ch) {
        if (conptyPassthrough)
        {
            conptyPassthrough(ch);
        }
        // We pass the data string straight through to the font buffer class
        // until we receive an ESC, indicating the end of the string. At that
        // point we can finalize the buffer, and if valid, update the renderer
        // with the constructed bit pattern.
        if (ch != AsciiChars::ESC)
        {
            _fontBuffer->AddSixelData(ch);
        }
        else if (_fontBuffer->FinalizeSixelData())
        {
            // We also need to inform the character set mapper of the ID that
            // will map to this font (we only support one font buffer so there
            // will only ever be one active dynamic character set).
            if (charsetSize == DispatchTypes::CharsetSize::Size96)
            {
                _termOutput.SetDrcs96Designation(_fontBuffer->GetDesignation());
            }
            else
            {
                _termOutput.SetDrcs94Designation(_fontBuffer->GetDesignation());
            }
            const auto bitPattern = _fontBuffer->GetBitPattern();
            const auto cellSize = _fontBuffer->GetCellSize();
            const auto centeringHint = _fontBuffer->GetTextCenteringHint();
            _renderer.UpdateSoftFont(bitPattern, cellSize, centeringHint);
        }
        return true;
    };
}

// Routine Description:
// - Helper method to create a string handler that can be used to pass through
//   DECDLD sequences when in conpty mode. This patches the original sequence
//   with a hard-coded character set ID, and pre-maps that ID into the G1 table.
// Arguments:
// - <none>
// Return value:
// - a function to receive the data or nullptr if the initial flush fails
ITermDispatch::StringHandler AdaptDispatch::_CreateDrcsPassthroughHandler(const DispatchTypes::CharsetSize charsetSize)
{
    const auto defaultPassthrough = _CreatePassthroughHandler();
    if (defaultPassthrough)
    {
        auto& engine = _api.GetStateMachine().Engine();
        return [=, &engine, gotId = false](const auto ch) mutable {
            // The character set ID is contained in the first characters of the
            // sequence, so we just ignore that initial content until we receive
            // a "final" character (i.e. in range 30 to 7E). At that point we
            // pass through a hard-coded ID of "@".
            if (!gotId)
            {
                if (ch >= 0x30 && ch <= 0x7E)
                {
                    gotId = true;
                    defaultPassthrough('@');
                }
            }
            else if (!defaultPassthrough(ch))
            {
                // Once the DECDLD sequence is finished, we also output an SCS
                // sequence to map the character set into the G1 table.
                const auto charset96 = charsetSize == DispatchTypes::CharsetSize::Size96;
                engine.ActionPassThroughString(charset96 ? L"\033-@" : L"\033)@");
            }
            return true;
        };
    }
    return nullptr;
}

// Method Description:
// - DECRQUPSS - Request the user-preference supplemental character set.
// Arguments:
// - None
// Return Value:
// - True
bool AdaptDispatch::RequestUserPreferenceCharset()
{
    const auto size = _termOutput.GetUserPreferenceCharsetSize();
    const auto id = _termOutput.GetUserPreferenceCharsetId();
    _api.ReturnResponse(fmt::format(FMT_COMPILE(L"\033P{}!u{}\033\\"), (size == 96 ? 1 : 0), id.ToString()));
    return true;
}

// Method Description:
// - DECAUPSS - Assigns the user-preference supplemental character set.
// Arguments:
// - charsetSize - Whether the character set is 94 or 96 characters.
// Return Value:
// - a function to parse the character set ID
ITermDispatch::StringHandler AdaptDispatch::AssignUserPreferenceCharset(const DispatchTypes::CharsetSize charsetSize)
{
    return [this, charsetSize, idBuilder = VTIDBuilder{}](const auto ch) mutable {
        if (ch >= L'\x20' && ch <= L'\x2f')
        {
            idBuilder.AddIntermediate(ch);
        }
        else if (ch >= L'\x30' && ch <= L'\x7e')
        {
            const auto id = idBuilder.Finalize(ch);
            switch (charsetSize)
            {
            case DispatchTypes::CharsetSize::Size94:
                _termOutput.AssignUserPreferenceCharset(id, false);
                break;
            case DispatchTypes::CharsetSize::Size96:
                _termOutput.AssignUserPreferenceCharset(id, true);
                break;
            }
            return false;
        }
        return true;
    };
}

// Method Description:
// - DECDMAC - Defines a string of characters as a macro that can later be
//   invoked with a DECINVM sequence.
// Arguments:
// - macroId - a number to identify the macro when invoked.
// - deleteControl - what gets deleted before loading the new macro data.
// - encoding - whether the data is encoded as plain text or hex digits.
// Return Value:
// - a function to receive the macro data or nullptr if parameters are invalid.
ITermDispatch::StringHandler AdaptDispatch::DefineMacro(const VTInt macroId,
                                                        const DispatchTypes::MacroDeleteControl deleteControl,
                                                        const DispatchTypes::MacroEncoding encoding)
{
    if (!_macroBuffer)
    {
        _macroBuffer = std::make_shared<MacroBuffer>();
    }

    if (_macroBuffer->InitParser(macroId, deleteControl, encoding))
    {
        return [&](const auto ch) {
            return _macroBuffer->ParseDefinition(ch);
        };
    }

    return nullptr;
}

// Method Description:
// - DECINVM - Invokes a previously defined macro, executing the macro content
//   as if it had been received directly from the host.
// Arguments:
// - macroId - the id number of the macro to be invoked.
// Return Value:
// - True
bool AdaptDispatch::InvokeMacro(const VTInt macroId)
{
    if (_macroBuffer)
    {
        // In order to inject our macro sequence into the state machine
        // we need to register a callback that will be executed only
        // once it has finished processing the current operation, and
        // has returned to the ground state. Note that we're capturing
        // a copy of the _macroBuffer pointer here to make sure it won't
        // be deleted (e.g. from an invoked RIS) while still in use.
        const auto macroBuffer = _macroBuffer;
        auto& stateMachine = _api.GetStateMachine();
        stateMachine.OnCsiComplete([=, &stateMachine]() {
            macroBuffer->InvokeMacro(macroId, stateMachine);
        });
    }
    return true;
}

// Method Description:
// - DECRSTS - Restores the terminal state from a stream of data previously
//   saved with a DECRQTSR query.
// Arguments:
// - format - the format of the state report being restored.
// Return Value:
// - a function to receive the data or nullptr if the format is unsupported.
ITermDispatch::StringHandler AdaptDispatch::RestoreTerminalState(const DispatchTypes::ReportFormat format)
{
    switch (format)
    {
    case DispatchTypes::ReportFormat::ColorTableReport:
        return _RestoreColorTable();
    default:
        return nullptr;
    }
}

// Method Description:
// - DECCTR - This is a parser for the Color Table Report received via DECRSTS.
//   The report contains a list of color definitions separated with a slash
//   character. Each definition consists of 5 parameters: Pc;Pu;Px;Py;Pz
//   - Pc is the color number.
//   - Pu is the color model (1 = HLS, 2 = RGB).
//   - Px, Py, and Pz are component values in the color model.
// Arguments:
// - <none>
// Return Value:
// - a function to parse the report data.
ITermDispatch::StringHandler AdaptDispatch::_RestoreColorTable()
{
    // If we're a conpty, we create a passthrough string handler to forward the
    // color report to the connected terminal.
    if (_api.IsConsolePty())
    {
        return _CreatePassthroughHandler();
    }

    return [this, parameter = VTInt{}, parameters = std::vector<VTParameter>{}](const auto ch) mutable {
        if (ch >= L'0' && ch <= L'9')
        {
            parameter *= 10;
            parameter += (ch - L'0');
            parameter = std::min(parameter, MAX_PARAMETER_VALUE);
        }
        else if (ch == L';')
        {
            if (parameters.size() < 5)
            {
                parameters.push_back(parameter);
            }
            parameter = 0;
        }
        else if (ch == L'/' || ch == AsciiChars::ESC)
        {
            parameters.push_back(parameter);
            const auto colorParameters = VTParameters{ parameters.data(), parameters.size() };
            const auto colorNumber = colorParameters.at(0).value_or(0);
            if (colorNumber < TextColor::TABLE_SIZE)
            {
                const auto colorModel = DispatchTypes::ColorModel{ colorParameters.at(1) };
                const auto x = colorParameters.at(2).value_or(0);
                const auto y = colorParameters.at(3).value_or(0);
                const auto z = colorParameters.at(4).value_or(0);
                if (colorModel == DispatchTypes::ColorModel::HLS)
                {
                    SetColorTableEntry(colorNumber, Utils::ColorFromHLS(x, y, z));
                }
                else if (colorModel == DispatchTypes::ColorModel::RGB)
                {
                    SetColorTableEntry(colorNumber, Utils::ColorFromRGB100(x, y, z));
                }
            }
            parameters.clear();
            parameter = 0;
        }
        return (ch != AsciiChars::ESC);
    };
}

// Method Description:
// - DECRQSS - Requests the state of a VT setting. The value being queried is
//   identified by the intermediate and final characters of its control
//   sequence, which are passed to the string handler.
// Arguments:
// - None
// Return Value:
// - a function to receive the VTID of the setting being queried
ITermDispatch::StringHandler AdaptDispatch::RequestSetting()
{
    // We use a VTIDBuilder to parse the characters in the control string into
    // an ID which represents the setting being queried. If the given ID isn't
    // supported, we respond with an error sequence: DCS 0 $ r ST. Note that
    // this is the opposite of what is documented in most DEC manuals, which
    // say that 0 is for a valid response, and 1 is for an error. The correct
    // interpretation is documented in the DEC STD 070 reference.
    return [this, parameter = VTInt{}, idBuilder = VTIDBuilder{}](const auto ch) mutable {
        const auto isFinal = ch >= L'\x40' && ch <= L'\x7e';
        if (isFinal)
        {
            const auto id = idBuilder.Finalize(ch);
            switch (id)
            {
            case VTID("m"):
                _ReportSGRSetting();
                break;
            case VTID("r"):
                _ReportDECSTBMSetting();
                break;
            case VTID("s"):
                _ReportDECSLRMSetting();
                break;
            case VTID("\"q"):
                _ReportDECSCASetting();
                break;
            case VTID("*x"):
                _ReportDECSACESetting();
                break;
            case VTID(",|"):
                _ReportDECACSetting(VTParameter{ parameter });
                break;
            default:
                _api.ReturnResponse(L"\033P0$r\033\\");
                break;
            }
            return false;
        }
        else
        {
            // Although we don't yet support any operations with parameter
            // prefixes, it's important that we still parse the prefix and
            // include it in the ID. Otherwise we'll mistakenly respond to
            // prefixed queries that we don't actually recognise.
            const auto isParameterPrefix = ch >= L'<' && ch <= L'?';
            const auto isParameter = ch >= L'0' && ch < L'9';
            const auto isIntermediate = ch >= L'\x20' && ch <= L'\x2f';
            if (isParameterPrefix || isIntermediate)
            {
                idBuilder.AddIntermediate(ch);
            }
            else if (isParameter)
            {
                parameter *= 10;
                parameter += (ch - L'0');
                parameter = std::min(parameter, MAX_PARAMETER_VALUE);
            }
            return true;
        }
    };
}

// Method Description:
// - Reports the current SGR attributes in response to a DECRQSS query.
// Arguments:
// - None
// Return Value:
// - None
void AdaptDispatch::_ReportSGRSetting() const
{
    using namespace std::string_view_literals;

    // A valid response always starts with DCS 1 $ r.
    // Then the '0' parameter is to reset the SGR attributes to the defaults.
    fmt::basic_memory_buffer<wchar_t, 64> response;
    response.append(L"\033P1$r0"sv);

    const auto& attr = _api.GetTextBuffer().GetCurrentAttributes();
    const auto ulStyle = attr.GetUnderlineStyle();
    // For each boolean attribute that is set, we add the appropriate
    // parameter value to the response string.
    const auto addAttribute = [&](const auto& parameter, const auto enabled) {
        if (enabled)
        {
            response.append(parameter);
        }
    };
    addAttribute(L";1"sv, attr.IsIntense());
    addAttribute(L";2"sv, attr.IsFaint());
    addAttribute(L";3"sv, attr.IsItalic());
    addAttribute(L";4"sv, ulStyle == UnderlineStyle::SinglyUnderlined);
    addAttribute(L";4:3"sv, ulStyle == UnderlineStyle::CurlyUnderlined);
    addAttribute(L";4:4"sv, ulStyle == UnderlineStyle::DottedUnderlined);
    addAttribute(L";4:5"sv, ulStyle == UnderlineStyle::DashedUnderlined);
    addAttribute(L";5"sv, attr.IsBlinking());
    addAttribute(L";7"sv, attr.IsReverseVideo());
    addAttribute(L";8"sv, attr.IsInvisible());
    addAttribute(L";9"sv, attr.IsCrossedOut());
    addAttribute(L";21"sv, ulStyle == UnderlineStyle::DoublyUnderlined);
    addAttribute(L";53"sv, attr.IsOverlined());

    // We also need to add the appropriate color encoding parameters for
    // both the foreground and background colors.
    const auto addColor = [&](const auto base, const auto color) {
        if (color.IsIndex16())
        {
            const auto index = color.GetIndex();
            const auto colorParameter = base + (index >= 8 ? 60 : 0) + (index % 8);
            fmt::format_to(std::back_inserter(response), FMT_COMPILE(L";{}"), colorParameter);
        }
        else if (color.IsIndex256())
        {
            const auto index = color.GetIndex();
            fmt::format_to(std::back_inserter(response), FMT_COMPILE(L";{}:5:{}"), base + 8, index);
        }
        else if (color.IsRgb())
        {
            const auto r = GetRValue(color.GetRGB());
            const auto g = GetGValue(color.GetRGB());
            const auto b = GetBValue(color.GetRGB());
            fmt::format_to(std::back_inserter(response), FMT_COMPILE(L";{}:2::{}:{}:{}"), base + 8, r, g, b);
        }
    };
    addColor(30, attr.GetForeground());
    addColor(40, attr.GetBackground());
    addColor(50, attr.GetUnderlineColor());

    // The 'm' indicates this is an SGR response, and ST ends the sequence.
    response.append(L"m\033\\"sv);
    _api.ReturnResponse({ response.data(), response.size() });
}

// Method Description:
// - Reports the DECSTBM margin range in response to a DECRQSS query.
// Arguments:
// - None
// Return Value:
// - None
void AdaptDispatch::_ReportDECSTBMSetting()
{
    using namespace std::string_view_literals;

    // A valid response always starts with DCS 1 $ r.
    fmt::basic_memory_buffer<wchar_t, 64> response;
    response.append(L"\033P1$r"sv);

    const auto viewport = _api.GetViewport();
    const auto [marginTop, marginBottom] = _GetVerticalMargins(viewport, false);
    // VT origin is at 1,1 so we need to add 1 to these margins.
    fmt::format_to(std::back_inserter(response), FMT_COMPILE(L"{};{}"), marginTop + 1, marginBottom + 1);

    // The 'r' indicates this is an DECSTBM response, and ST ends the sequence.
    response.append(L"r\033\\"sv);
    _api.ReturnResponse({ response.data(), response.size() });
}

// Method Description:
// - Reports the DECSLRM margin range in response to a DECRQSS query.
// Arguments:
// - None
// Return Value:
// - None
void AdaptDispatch::_ReportDECSLRMSetting()
{
    using namespace std::string_view_literals;

    // A valid response always starts with DCS 1 $ r.
    fmt::basic_memory_buffer<wchar_t, 64> response;
    response.append(L"\033P1$r"sv);

    const auto bufferWidth = _api.GetTextBuffer().GetSize().Width();
    const auto [marginLeft, marginRight] = _GetHorizontalMargins(bufferWidth);
    // VT origin is at 1,1 so we need to add 1 to these margins.
    fmt::format_to(std::back_inserter(response), FMT_COMPILE(L"{};{}"), marginLeft + 1, marginRight + 1);

    // The 's' indicates this is an DECSLRM response, and ST ends the sequence.
    response.append(L"s\033\\"sv);
    _api.ReturnResponse({ response.data(), response.size() });
}

// Method Description:
// - Reports the DECSCA protected attribute in response to a DECRQSS query.
// Arguments:
// - None
// Return Value:
// - None
void AdaptDispatch::_ReportDECSCASetting() const
{
    using namespace std::string_view_literals;

    // A valid response always starts with DCS 1 $ r.
    fmt::basic_memory_buffer<wchar_t, 64> response;
    response.append(L"\033P1$r"sv);

    const auto& attr = _api.GetTextBuffer().GetCurrentAttributes();
    response.append(attr.IsProtected() ? L"1"sv : L"0"sv);

    // The '"q' indicates this is an DECSCA response, and ST ends the sequence.
    response.append(L"\"q\033\\"sv);
    _api.ReturnResponse({ response.data(), response.size() });
}

// Method Description:
// - Reports the DECSACE change extent in response to a DECRQSS query.
// Arguments:
// - None
// Return Value:
// - None
void AdaptDispatch::_ReportDECSACESetting() const
{
    using namespace std::string_view_literals;

    // A valid response always starts with DCS 1 $ r.
    fmt::basic_memory_buffer<wchar_t, 64> response;
    response.append(L"\033P1$r"sv);

    response.append(_modes.test(Mode::RectangularChangeExtent) ? L"2"sv : L"1"sv);

    // The '*x' indicates this is an DECSACE response, and ST ends the sequence.
    response.append(L"*x\033\\"sv);
    _api.ReturnResponse({ response.data(), response.size() });
}

// Method Description:
// - Reports the DECAC color assignments in response to a DECRQSS query.
// Arguments:
// - None
// Return Value:
// - None
void AdaptDispatch::_ReportDECACSetting(const VTInt itemNumber) const
{
    using namespace std::string_view_literals;

    size_t fgIndex = 0;
    size_t bgIndex = 0;
    switch (static_cast<DispatchTypes::ColorItem>(itemNumber))
    {
    case DispatchTypes::ColorItem::NormalText:
        fgIndex = _renderSettings.GetColorAliasIndex(ColorAlias::DefaultForeground);
        bgIndex = _renderSettings.GetColorAliasIndex(ColorAlias::DefaultBackground);
        break;
    case DispatchTypes::ColorItem::WindowFrame:
        fgIndex = _renderSettings.GetColorAliasIndex(ColorAlias::FrameForeground);
        bgIndex = _renderSettings.GetColorAliasIndex(ColorAlias::FrameBackground);
        break;
    default:
        _api.ReturnResponse(L"\033P0$r\033\\");
        return;
    }

    // A valid response always starts with DCS 1 $ r.
    fmt::basic_memory_buffer<wchar_t, 64> response;
    response.append(L"\033P1$r"sv);

    fmt::format_to(std::back_inserter(response), FMT_COMPILE(L"{};{};{}"), itemNumber, fgIndex, bgIndex);

    // The ',|' indicates this is a DECAC response, and ST ends the sequence.
    response.append(L",|\033\\"sv);
    _api.ReturnResponse({ response.data(), response.size() });
}

// Routine Description:
// - DECRQPSR - Queries the presentation state of the terminal. This can either
//   be in the form of a cursor information report, or a tabulation stop report,
//   depending on the requested format.
// Arguments:
// - format - the format of the report being requested.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::RequestPresentationStateReport(const DispatchTypes::PresentationReportFormat format)
{
    switch (format)
    {
    case DispatchTypes::PresentationReportFormat::CursorInformationReport:
        _ReportCursorInformation();
        return true;
    case DispatchTypes::PresentationReportFormat::TabulationStopReport:
        _ReportTabStops();
        return true;
    default:
        return false;
    }
}

// Method Description:
// - DECRSPS - Restores the presentation state from a stream of data previously
//   saved with a DECRQPSR query.
// Arguments:
// - format - the format of the report being restored.
// Return Value:
// - a function to receive the data or nullptr if the format is unsupported.
ITermDispatch::StringHandler AdaptDispatch::RestorePresentationState(const DispatchTypes::PresentationReportFormat format)
{
    switch (format)
    {
    case DispatchTypes::PresentationReportFormat::CursorInformationReport:
        return _RestoreCursorInformation();
    case DispatchTypes::PresentationReportFormat::TabulationStopReport:
        return _RestoreTabStops();
    default:
        return nullptr;
    }
}

// Method Description:
// - DECCIR - Returns the Cursor Information Report in response to a DECRQPSR query.
// Arguments:
// - None
// Return Value:
// - None
void AdaptDispatch::_ReportCursorInformation()
{
    const auto viewport = _api.GetViewport();
    const auto& textBuffer = _api.GetTextBuffer();
    const auto& cursor = textBuffer.GetCursor();
    const auto& attributes = textBuffer.GetCurrentAttributes();

    // First pull the cursor position relative to the entire buffer out of the console.
    til::point cursorPosition{ cursor.GetPosition() };

    // Now adjust it for its position in respect to the current viewport top.
    cursorPosition.y -= viewport.top;

    // NOTE: 1,1 is the top-left corner of the viewport in VT-speak, so add 1.
    cursorPosition.x++;
    cursorPosition.y++;

    // If the origin mode is set, the cursor is relative to the margin origin.
    if (_modes.test(Mode::Origin))
    {
        cursorPosition.x -= _GetHorizontalMargins(textBuffer.GetSize().Width()).first;
        cursorPosition.y -= _GetVerticalMargins(viewport, false).first;
    }

    // Paging is not supported yet (GH#13892).
    const auto pageNumber = 1;

    // Only some of the rendition attributes are reported.
    //   Bit    Attribute
    //   1      bold
    //   2      underlined
    //   3      blink
    //   4      reverse video
    //   5      invisible
    //   6      extension indicator
    //   7      Always 1 (on)
    //   8      Always 0 (off)
    auto renditionAttributes = L'@'; // (0100 0000)
    renditionAttributes += (attributes.IsIntense() ? 1 : 0);
    renditionAttributes += (attributes.IsUnderlined() ? 2 : 0);
    renditionAttributes += (attributes.IsBlinking() ? 4 : 0);
    renditionAttributes += (attributes.IsReverseVideo() ? 8 : 0);
    renditionAttributes += (attributes.IsInvisible() ? 16 : 0);

    // There is only one character attribute.
    const auto characterAttributes = attributes.IsProtected() ? L'A' : L'@';

    // Miscellaneous flags and modes.
    auto flags = L'@';
    flags += (_modes.test(Mode::Origin) ? 1 : 0);
    flags += (_termOutput.IsSingleShiftPending(2) ? 2 : 0);
    flags += (_termOutput.IsSingleShiftPending(3) ? 4 : 0);
    flags += (cursor.IsDelayedEOLWrap() ? 8 : 0);

    // Character set designations.
    const auto leftSetNumber = _termOutput.GetLeftSetNumber();
    const auto rightSetNumber = _termOutput.GetRightSetNumber();
    auto charsetSizes = L'@';
    charsetSizes += (_termOutput.GetCharsetSize(0) == 96 ? 1 : 0);
    charsetSizes += (_termOutput.GetCharsetSize(1) == 96 ? 2 : 0);
    charsetSizes += (_termOutput.GetCharsetSize(2) == 96 ? 4 : 0);
    charsetSizes += (_termOutput.GetCharsetSize(3) == 96 ? 8 : 0);
    const auto charset0 = _termOutput.GetCharsetId(0);
    const auto charset1 = _termOutput.GetCharsetId(1);
    const auto charset2 = _termOutput.GetCharsetId(2);
    const auto charset3 = _termOutput.GetCharsetId(3);

    // A valid response always starts with DCS 1 $ u and ends with ST.
    const auto response = fmt::format(
        FMT_COMPILE(L"\033P1$u{};{};{};{};{};{};{};{};{};{}{}{}{}\033\\"),
        cursorPosition.y,
        cursorPosition.x,
        pageNumber,
        renditionAttributes,
        characterAttributes,
        flags,
        leftSetNumber,
        rightSetNumber,
        charsetSizes,
        charset0.ToString(),
        charset1.ToString(),
        charset2.ToString(),
        charset3.ToString());
    _api.ReturnResponse({ response.data(), response.size() });
}

// Method Description:
// - DECCIR - This is a parser for the Cursor Information Report received via DECRSPS.
// Arguments:
// - <none>
// Return Value:
// - a function to parse the report data.
ITermDispatch::StringHandler AdaptDispatch::_RestoreCursorInformation()
{
    // clang-format off
    enum Field { Row, Column, Page, SGR, Attr, Flags, GL, GR, Sizes, G0, G1, G2, G3 };
    // clang-format on
    constexpr til::enumset<Field> numeric{ Field::Row, Field::Column, Field::Page, Field::GL, Field::GR };
    constexpr til::enumset<Field> flags{ Field::SGR, Field::Attr, Field::Flags, Field::Sizes };
    constexpr til::enumset<Field> charset{ Field::G0, Field::G1, Field::G2, Field::G3 };
    struct State
    {
        Field field{ Field::Row };
        VTInt value{ 0 };
        VTIDBuilder charsetId{};
        std::array<bool, 4> charset96{};
        VTParameter row{};
        VTParameter column{};
    };
    auto& textBuffer = _api.GetTextBuffer();
    return [&, state = State{}](const auto ch) mutable {
        if (numeric.test(state.field))
        {
            if (ch >= '0' && ch <= '9')
            {
                state.value *= 10;
                state.value += (ch - L'0');
                state.value = std::min(state.value, MAX_PARAMETER_VALUE);
            }
            else if (ch == L';' || ch == AsciiChars::ESC)
            {
                if (state.field == Field::Row)
                {
                    state.row = state.value;
                }
                else if (state.field == Field::Column)
                {
                    state.column = state.value;
                }
                else if (state.field == Field::Page)
                {
                    // Paging is not supported yet (GH#13892).
                }
                else if (state.field == Field::GL && state.value <= 3)
                {
                    LockingShift(state.value);
                }
                else if (state.field == Field::GR && state.value <= 3)
                {
                    LockingShiftRight(state.value);
                }
                state.value = {};
                state.field = static_cast<Field>(state.field + 1);
            }
        }
        else if (flags.test(state.field))
        {
            // Note that there could potentially be multiple characters in a
            // flag field, so we process the flags as soon as they're received.
            // But for now we're only interested in the first one, so once the
            // state.value is set, we ignore everything else until the `;`.
            if (ch >= L'@' && ch <= '~' && !state.value)
            {
                state.value = ch;
                if (state.field == Field::SGR)
                {
                    auto attr = textBuffer.GetCurrentAttributes();
                    attr.SetIntense(state.value & 1);
                    attr.SetUnderlineStyle(state.value & 2 ? UnderlineStyle::SinglyUnderlined : UnderlineStyle::NoUnderline);
                    attr.SetBlinking(state.value & 4);
                    attr.SetReverseVideo(state.value & 8);
                    attr.SetInvisible(state.value & 16);
                    textBuffer.SetCurrentAttributes(attr);
                }
                else if (state.field == Field::Attr)
                {
                    auto attr = textBuffer.GetCurrentAttributes();
                    attr.SetProtected(state.value & 1);
                    textBuffer.SetCurrentAttributes(attr);
                }
                else if (state.field == Field::Sizes)
                {
                    state.charset96.at(0) = state.value & 1;
                    state.charset96.at(1) = state.value & 2;
                    state.charset96.at(2) = state.value & 4;
                    state.charset96.at(3) = state.value & 8;
                }
                else if (state.field == Field::Flags)
                {
                    const bool originMode = state.value & 1;
                    const bool ss2 = state.value & 2;
                    const bool ss3 = state.value & 4;
                    const bool delayedEOLWrap = state.value & 8;
                    // The cursor position is parsed at the start of the sequence,
                    // but we only set the position once we know the origin mode.
                    _modes.set(Mode::Origin, originMode);
                    CursorPosition(state.row, state.column);
                    // There can only be one single shift applied at a time, so
                    // we'll just apply the last one that is enabled.
                    _termOutput.SingleShift(ss3 ? 3 : (ss2 ? 2 : 0));
                    // The EOL flag will always be reset by the cursor movement
                    // above, so we only need to worry about setting it.
                    if (delayedEOLWrap)
                    {
                        textBuffer.GetCursor().DelayEOLWrap();
                    }
                }
            }
            else if (ch == L';')
            {
                state.value = 0;
                state.field = static_cast<Field>(state.field + 1);
            }
        }
        else if (charset.test(state.field))
        {
            if (ch >= L' ' && ch <= L'/')
            {
                state.charsetId.AddIntermediate(ch);
            }
            else if (ch >= L'0' && ch <= L'~')
            {
                const auto id = state.charsetId.Finalize(ch);
                const auto gset = state.field - Field::G0;
                if (state.charset96.at(gset))
                {
                    Designate96Charset(gset, id);
                }
                else
                {
                    Designate94Charset(gset, id);
                }
                state.charsetId.Clear();
                state.field = static_cast<Field>(state.field + 1);
            }
        }
        return (ch != AsciiChars::ESC);
    };
}

// Method Description:
// - DECTABSR - Returns the Tabulation Stop Report in response to a DECRQPSR query.
// Arguments:
// - None
// Return Value:
// - None
void AdaptDispatch::_ReportTabStops()
{
    // In order to be compatible with the original hardware terminals, we only
    // report tab stops up to the current buffer width, even though there may
    // be positions recorded beyond that limit.
    const auto width = _api.GetTextBuffer().GetSize().Dimensions().width;
    _InitTabStopsForWidth(width);

    using namespace std::string_view_literals;

    // A valid response always starts with DCS 2 $ u.
    fmt::basic_memory_buffer<wchar_t, 64> response;
    response.append(L"\033P2$u"sv);

    auto need_separator = false;
    for (auto column = 0; column < width; column++)
    {
        if (til::at(_tabStopColumns, column))
        {
            response.append(need_separator ? L"/"sv : L""sv);
            fmt::format_to(std::back_inserter(response), FMT_COMPILE(L"{}"), column + 1);
            need_separator = true;
        }
    }

    // An ST ends the sequence.
    response.append(L"\033\\"sv);
    _api.ReturnResponse({ response.data(), response.size() });
}

// Method Description:
// - DECTABSR - This is a parser for the Tabulation Stop Report received via DECRSPS.
// Arguments:
// - <none>
// Return Value:
// - a function to parse the report data.
ITermDispatch::StringHandler AdaptDispatch::_RestoreTabStops()
{
    // In order to be compatible with the original hardware terminals, we need
    // to be able to set tab stops up to at least 132 columns, even though the
    // current buffer width may be less than that.
    const auto width = std::max(_api.GetTextBuffer().GetSize().Dimensions().width, 132);
    _ClearAllTabStops();
    _InitTabStopsForWidth(width);

    return [this, width, column = size_t{}](const auto ch) mutable {
        if (ch >= L'0' && ch <= L'9')
        {
            column *= 10;
            column += (ch - L'0');
            column = std::min<size_t>(column, MAX_PARAMETER_VALUE);
        }
        else if (ch == L'/' || ch == AsciiChars::ESC)
        {
            // Note that column 1 is always a tab stop, so there is no
            // need to record an entry at that offset.
            if (column > 1u && column <= static_cast<size_t>(width))
            {
                _tabStopColumns.at(column - 1) = true;
            }
            column = 0;
        }
        else
        {
            // If we receive an unexpected character, we don't try and
            // process any more of the input - we just abort.
            return false;
        }
        return (ch != AsciiChars::ESC);
    };
}

// Routine Description:
// - DECPS - Plays a sequence of musical notes.
// Arguments:
// - params - The volume, duration, and note values to play.
// Return value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::PlaySounds(const VTParameters parameters)
{
    // If we're a conpty, we return false so the command will be passed on
    // to the connected terminal. But we need to flush the current frame
    // first, otherwise the visual output will lag behind the sound.
    if (_api.IsConsolePty())
    {
        _renderer.TriggerFlush(false);
        return false;
    }

    // First parameter is the volume, in the range 0 to 7. We multiply by
    // 127 / 7 to obtain an equivalent MIDI velocity in the range 0 to 127.
    const auto velocity = std::min(parameters.at(0).value_or(0), 7) * 127 / 7;
    // Second parameter is the duration, in the range 0 to 255. Units are
    // 1/32 of a second, so we multiply by 1000000us/32 to obtain microseconds.
    using namespace std::chrono_literals;
    const auto duration = std::min(parameters.at(1).value_or(0), 255) * 1000000us / 32;
    // The subsequent parameters are notes, in the range 0 to 25.
    return parameters.subspan(2).for_each([=](const auto param) {
        // Values 1 to 25 represent the notes C5 to C7, so we add 71 to
        // obtain the equivalent MIDI note numbers (72 = C5).
        const auto noteNumber = std::min(param.value_or(0), 25) + 71;
        // But value 0 is meant to be silent, so if the note number is 71,
        // we set the velocity to 0 (i.e. no volume).
        _api.PlayMidiNote(noteNumber, noteNumber == 71 ? 0 : velocity, duration);
        return true;
    });
}

// Routine Description:
// - Helper method to create a string handler that can be used to pass through
//   DCS sequences when in conpty mode.
// Arguments:
// - <none>
// Return value:
// - a function to receive the data or nullptr if the initial flush fails
ITermDispatch::StringHandler AdaptDispatch::_CreatePassthroughHandler()
{
    // Before we pass through any more data, we need to flush the current frame
    // first, otherwise it can end up arriving out of sync.
    _renderer.TriggerFlush(false);
    // Then we need to flush the sequence introducer and parameters that have
    // already been parsed by the state machine.
    auto& stateMachine = _api.GetStateMachine();
    if (stateMachine.FlushToTerminal())
    {
        // And finally we create a StringHandler to receive the rest of the
        // sequence data, and pass it through to the connected terminal.
        auto& engine = stateMachine.Engine();
        return [&, buffer = std::wstring{}](const auto ch) mutable {
            // To make things more efficient, we buffer the string data before
            // passing it through, only flushing if the buffer gets too large,
            // or we're dealing with the last character in the current output
            // fragment, or we've reached the end of the string.
            const auto endOfString = ch == AsciiChars::ESC;
            buffer += ch;
            if (buffer.length() >= 4096 || stateMachine.IsProcessingLastCharacter() || endOfString)
            {
                // The end of the string is signaled with an escape, but for it
                // to be a valid string terminator we need to add a backslash.
                if (endOfString)
                {
                    buffer += L'\\';
                }
                engine.ActionPassThroughString(buffer);
                buffer.clear();
            }
            return !endOfString;
        };
    }
    return nullptr;
}
