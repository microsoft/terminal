// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "adaptDispatch.hpp"
#include "SixelParser.hpp"
#include "../../inc/unicode.hpp"
#include "../../renderer/base/renderer.hpp"
#include "../../types/inc/CodepointWidthDetector.hpp"
#include "../../types/inc/utils.hpp"
#include "../../types/inc/Viewport.hpp"
#include "../parser/ascii.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::VirtualTerminal;

static constexpr std::wstring_view whitespace{ L" " };

struct XtermResourceColorTableEntry
{
    int ColorTableIndex;
    int AliasIndex;
};

static constexpr std::array<XtermResourceColorTableEntry, 10> XtermResourceColorTableMappings{ {
    /* 10 */ { TextColor::DEFAULT_FOREGROUND, static_cast<int>(ColorAlias::DefaultForeground) },
    /* 11 */ { TextColor::DEFAULT_BACKGROUND, static_cast<int>(ColorAlias::DefaultBackground) },
    /* 12 */ { TextColor::CURSOR_COLOR, -1 },
    /* 13 */ { -1, -1 },
    /* 14 */ { -1, -1 },
    /* 15 */ { -1, -1 },
    /* 16 */ { -1, -1 },
    /* 17 */ { TextColor::SELECTION_BACKGROUND, -1 },
    /* 18 */ { -1, -1 },
    /* 19 */ { -1, -1 },
} };

AdaptDispatch::AdaptDispatch(ITerminalApi& api, Renderer* renderer, RenderSettings& renderSettings, TerminalInput& terminalInput) noexcept :
    _api{ api },
    _renderer{ renderer },
    _renderSettings{ renderSettings },
    _terminalInput{ terminalInput },
    _usingAltBuffer(false),
    _termOutput(),
    _pages{ api, renderer }
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
    auto page = _pages.ActivePage();
    auto& textBuffer = page.Buffer();
    auto& cursor = page.Cursor();
    auto cursorPosition = cursor.GetPosition();
    const auto wrapAtEOL = _api.GetSystemMode(ITerminalApi::Mode::AutoWrap);
    const auto& attributes = page.Attributes();

    auto [topMargin, bottomMargin] = _GetVerticalMargins(page, true);
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(page.Width());

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
                if (_DoLineFeed(page, true, true))
                {
                    // If the line feed caused the viewport to move down, we
                    // need to adjust the page viewport and margins to match.
                    page.MoveViewportDown();
                    std::tie(topMargin, bottomMargin) = _GetVerticalMargins(page, true);
                }

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

        state.columnBegin = cursorPosition.x;

        const auto textPositionBefore = state.text.data();
        if (_modes.test(Mode::InsertReplace))
        {
            textBuffer.Insert(cursorPosition.y, attributes, state);
        }
        else
        {
            textBuffer.Replace(cursorPosition.y, attributes, state);
        }
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

    // Notify terminal and UIA of new text.
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
void AdaptDispatch::CursorUp(const VTInt distance)
{
    _CursorMovePosition(Offset::Backward(distance), Offset::Unchanged(), true);
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
void AdaptDispatch::CursorDown(const VTInt distance)
{
    _CursorMovePosition(Offset::Forward(distance), Offset::Unchanged(), true);
}

// Routine Description:
// - CUF - Handles cursor forward movement by given distance
// Arguments:
// - distance - Distance to move
void AdaptDispatch::CursorForward(const VTInt distance)
{
    _CursorMovePosition(Offset::Unchanged(), Offset::Forward(distance), true);
}

// Routine Description:
// - CUB - Handles cursor backward movement by given distance
// Arguments:
// - distance - Distance to move
void AdaptDispatch::CursorBackward(const VTInt distance)
{
    _CursorMovePosition(Offset::Unchanged(), Offset::Backward(distance), true);
}

// Routine Description:
// - CNL - Handles cursor movement to the following line (or N lines down)
// - Moves to the beginning X/Column position of the line.
// Arguments:
// - distance - Distance to move
void AdaptDispatch::CursorNextLine(const VTInt distance)
{
    _CursorMovePosition(Offset::Forward(distance), Offset::Absolute(1), true);
}

// Routine Description:
// - CPL - Handles cursor movement to the previous line (or N lines up)
// - Moves to the beginning X/Column position of the line.
// Arguments:
// - distance - Distance to move
void AdaptDispatch::CursorPrevLine(const VTInt distance)
{
    _CursorMovePosition(Offset::Backward(distance), Offset::Absolute(1), true);
}

// Routine Description:
// - Returns the coordinates of the vertical scroll margins.
// Arguments:
// - page - The page that the margins will apply to.
// - absolute - Should coordinates be absolute or relative to the page top.
// Return Value:
// - A std::pair containing the top and bottom coordinates (inclusive).
std::pair<int, int> AdaptDispatch::_GetVerticalMargins(const Page& page, const bool absolute) noexcept
{
    // If the top is out of range, reset the margins completely.
    const auto bottommostRow = page.Height() - 1;
    if (_scrollMargins.top >= bottommostRow)
    {
        _scrollMargins.top = _scrollMargins.bottom = 0;
    }
    // If margins aren't set, use the full extent of the page.
    const auto marginsSet = _scrollMargins.top < _scrollMargins.bottom;
    auto topMargin = marginsSet ? _scrollMargins.top : 0;
    auto bottomMargin = marginsSet ? _scrollMargins.bottom : bottommostRow;
    // If the bottom is out of range, clamp it to the bottommost row.
    bottomMargin = std::min(bottomMargin, bottommostRow);
    if (absolute)
    {
        topMargin += page.Top();
        bottomMargin += page.Top();
    }
    return { topMargin, bottomMargin };
}

// Routine Description:
// - Returns the coordinates of the horizontal scroll margins.
// Arguments:
// - pageWidth - The width of the page
// Return Value:
// - A std::pair containing the left and right coordinates (inclusive).
std::pair<int, int> AdaptDispatch::_GetHorizontalMargins(const til::CoordType pageWidth) noexcept
{
    // If the left is out of range, reset the margins completely.
    const auto rightmostColumn = pageWidth - 1;
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
void AdaptDispatch::_CursorMovePosition(const Offset rowOffset, const Offset colOffset, const bool clampInMargins)
{
    // First retrieve some information about the buffer
    const auto page = _pages.ActivePage();
    auto& cursor = page.Cursor();
    const auto pageWidth = page.Width();
    const auto cursorPosition = cursor.GetPosition();
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(page, true);
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(pageWidth);

    // For relative movement, the given offsets will be relative to
    // the current cursor position.
    auto row = cursorPosition.y;
    auto col = cursorPosition.x;

    // But if the row is absolute, it will be relative to the top of the
    // page, or the top margin, depending on the origin mode.
    if (rowOffset.IsAbsolute)
    {
        row = _modes.test(Mode::Origin) ? topMargin : page.Top();
    }

    // And if the column is absolute, it'll be relative to column 0,
    // or the left margin, depending on the origin mode.
    // Horizontal positions are not affected by the viewport.
    if (colOffset.IsAbsolute)
    {
        col = _modes.test(Mode::Origin) ? leftMargin : 0;
    }

    // Adjust the base position by the given offsets and clamp the results.
    // The row is constrained within the page's vertical boundaries,
    // while the column is constrained by the buffer width.
    row = std::clamp(row + rowOffset.Value, page.Top(), page.Bottom() - 1);
    col = std::clamp(col + colOffset.Value, 0, pageWidth - 1);

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
    cursor.SetPosition(page.Buffer().ClampPositionWithinLine({ col, row }));
    _ApplyCursorMovementFlags(cursor);
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
void AdaptDispatch::CursorHorizontalPositionAbsolute(const VTInt column)
{
    _CursorMovePosition(Offset::Unchanged(), Offset::Absolute(column), false);
}

// Routine Description:
// - VPA - Moves the cursor to an exact Y/row position on the current column.
// Arguments:
// - line - Specific Y/Row position to move to
void AdaptDispatch::VerticalLinePositionAbsolute(const VTInt line)
{
    _CursorMovePosition(Offset::Absolute(line), Offset::Unchanged(), false);
}

// Routine Description:
// - HPR - Handles cursor forward movement by given distance
// - Unlike CUF, this is not constrained by margin settings.
// Arguments:
// - distance - Distance to move
void AdaptDispatch::HorizontalPositionRelative(const VTInt distance)
{
    _CursorMovePosition(Offset::Unchanged(), Offset::Forward(distance), false);
}

// Routine Description:
// - VPR - Handles cursor downward movement by given distance
// - Unlike CUD, this is not constrained by margin settings.
// Arguments:
// - distance - Distance to move
void AdaptDispatch::VerticalPositionRelative(const VTInt distance)
{
    _CursorMovePosition(Offset::Forward(distance), Offset::Unchanged(), false);
}

// Routine Description:
// - CUP - Moves the cursor to an exact X/Column and Y/Row/Line coordinate position.
// Arguments:
// - line - Specific Y/Row/Line position to move to
// - column - Specific X/Column position to move to
void AdaptDispatch::CursorPosition(const VTInt line, const VTInt column)
{
    _CursorMovePosition(Offset::Absolute(line), Offset::Absolute(column), false);
}

// Routine Description:
// - DECSC - Saves the current "cursor state" into a memory buffer. This
//   includes the cursor position, origin mode, graphic rendition, and
//   active character set.
// Arguments:
// - <none>
void AdaptDispatch::CursorSaveState()
{
    // First retrieve some information about the buffer
    const auto page = _pages.ActivePage();

    // The cursor is given to us by the API as relative to the whole buffer.
    // But in VT speak, the cursor row should be relative to the current page top.
    auto cursorPosition = page.Cursor().GetPosition();
    cursorPosition.y -= page.Top();

    // Although if origin mode is set, the cursor is relative to the margin origin.
    if (_modes.test(Mode::Origin))
    {
        cursorPosition.x -= _GetHorizontalMargins(page.Width()).first;
        cursorPosition.y -= _GetVerticalMargins(page, false).first;
    }

    // VT is also 1 based, not 0 based, so correct by 1.
    auto& savedCursorState = _savedCursorState.at(_usingAltBuffer);
    savedCursorState.Column = cursorPosition.x + 1;
    savedCursorState.Row = cursorPosition.y + 1;
    savedCursorState.Page = page.Number();
    savedCursorState.IsDelayedEOLWrap = page.Cursor().IsDelayedEOLWrap();
    savedCursorState.IsOriginModeRelative = _modes.test(Mode::Origin);
    savedCursorState.Attributes = page.Attributes();
    savedCursorState.TermOutput = _termOutput;
}

// Routine Description:
// - DECRC - Restores a saved "cursor state" from the DECSC command back into
//   the console state. This includes the cursor position, origin mode, graphic
//   rendition, and active character set.
// Arguments:
// - <none>
void AdaptDispatch::CursorRestoreState()
{
    auto& savedCursorState = _savedCursorState.at(_usingAltBuffer);

    // Restore the origin mode first, since the cursor coordinates may be relative.
    _modes.set(Mode::Origin, savedCursorState.IsOriginModeRelative);

    // Restore the page number.
    PagePositionAbsolute(savedCursorState.Page);

    // We can then restore the position with a standard CUP operation.
    CursorPosition(savedCursorState.Row, savedCursorState.Column);

    // If the delayed wrap flag was set when the cursor was saved, we need to restore that now.
    const auto page = _pages.ActivePage();
    if (savedCursorState.IsDelayedEOLWrap)
    {
        page.Cursor().DelayEOLWrap();
    }

    // Restore text attributes.
    page.SetAttributes(savedCursorState.Attributes);

    // Restore designated character sets.
    _termOutput.RestoreFrom(savedCursorState.TermOutput);
}

// Routine Description:
// - Returns the attributes that should be used when erasing the buffer. When
//   the Erase Color mode is set, we use the default attributes, but when reset,
//   we use the active color attributes with the character attributes cleared.
// Arguments:
// - page - Target page that is being erased.
// Return Value:
// - The erase TextAttribute value.
TextAttribute AdaptDispatch::_GetEraseAttributes(const Page& page) const noexcept
{
    if (_modes.test(Mode::EraseColor))
    {
        return {};
    }
    else
    {
        auto eraseAttributes = page.Attributes();
        eraseAttributes.SetStandardErase();
        return eraseAttributes;
    }
}

// Routine Description:
// - Scrolls an area of the buffer in a vertical direction.
// Arguments:
// - page - Target page to be scrolled.
// - fillRect - Area of the page that will be affected.
// - delta - Distance to move (positive is down, negative is up).
// Return Value:
// - <none>
void AdaptDispatch::_ScrollRectVertically(const Page& page, const til::rect& scrollRect, const VTInt delta)
{
    auto& textBuffer = page.Buffer();
    const auto absoluteDelta = std::min(std::abs(delta), scrollRect.height());
    if (absoluteDelta < scrollRect.height())
    {
        const auto top = delta > 0 ? scrollRect.top : scrollRect.top + absoluteDelta;
        const auto width = scrollRect.width();
        const auto height = scrollRect.height() - absoluteDelta;
        const auto actualDelta = delta > 0 ? absoluteDelta : -absoluteDelta;
        if (width == page.Width())
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
            const auto srcView = Viewport::FromDimensions(srcOrigin, { width, height });
            const auto dstView = Viewport::FromDimensions(dstOrigin, { width, height });
            const auto walkDirection = Viewport::DetermineWalkDirection(srcView, dstView);
            auto srcPos = srcView.GetWalkOrigin(walkDirection);
            auto dstPos = dstView.GetWalkOrigin(walkDirection);
            do
            {
                const auto current = OutputCell(*textBuffer.GetCellDataAt(srcPos));
                textBuffer.WriteLine(OutputCellIterator({ &current, 1 }), dstPos);
                srcView.WalkInBounds(srcPos, walkDirection);
            } while (dstView.WalkInBounds(dstPos, walkDirection));
            // Copy any image content in the affected area.
            ImageSlice::CopyBlock(textBuffer, srcView.ToExclusive(), textBuffer, dstView.ToExclusive());
        }
    }

    // Rows revealed by the scroll are filled with standard erase attributes.
    auto eraseRect = scrollRect;
    eraseRect.top = delta > 0 ? scrollRect.top : (scrollRect.bottom - absoluteDelta);
    eraseRect.bottom = eraseRect.top + absoluteDelta;
    const auto eraseAttributes = _GetEraseAttributes(page);
    _FillRect(page, eraseRect, whitespace, eraseAttributes);

    // Also reset the line rendition for the erased rows.
    textBuffer.ResetLineRenditionRange(eraseRect.top, eraseRect.bottom);
}

// Routine Description:
// - Scrolls an area of the buffer in a horizontal direction.
// Arguments:
// - page - Target page to be scrolled.
// - fillRect - Area of the page that will be affected.
// - delta - Distance to move (positive is right, negative is left).
// Return Value:
// - <none>
void AdaptDispatch::_ScrollRectHorizontally(const Page& page, const til::rect& scrollRect, const VTInt delta)
{
    auto& textBuffer = page.Buffer();
    const auto absoluteDelta = std::min(std::abs(delta), scrollRect.width());
    if (absoluteDelta < scrollRect.width())
    {
        const auto left = delta > 0 ? scrollRect.left : (scrollRect.left + absoluteDelta);
        const auto top = scrollRect.top;
        const auto width = scrollRect.width() - absoluteDelta;
        const auto height = scrollRect.height();
        const auto actualDelta = delta > 0 ? absoluteDelta : -absoluteDelta;

        const auto source = Viewport::FromDimensions({ left, top }, { width, height });
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
        // Copy any image content in the affected area.
        ImageSlice::CopyBlock(textBuffer, source.ToExclusive(), textBuffer, target.ToExclusive());
    }

    // Columns revealed by the scroll are filled with standard erase attributes.
    auto eraseRect = scrollRect;
    eraseRect.left = delta > 0 ? scrollRect.left : (scrollRect.right - absoluteDelta);
    eraseRect.right = eraseRect.left + absoluteDelta;
    const auto eraseAttributes = _GetEraseAttributes(page);
    _FillRect(page, eraseRect, whitespace, eraseAttributes);
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
    const auto page = _pages.ActivePage();
    const auto row = page.Cursor().GetPosition().y;
    const auto col = page.Cursor().GetPosition().x;
    const auto lineWidth = page.Buffer().GetLineWidth(row);
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(page, true);
    const auto [leftMargin, rightMargin] = (row >= topMargin && row <= bottomMargin) ?
                                               _GetHorizontalMargins(lineWidth) :
                                               std::make_pair(0, lineWidth - 1);
    if (col >= leftMargin && col <= rightMargin)
    {
        _ScrollRectHorizontally(page, { col, row, rightMargin + 1, row + 1 }, delta);
        // The ICH and DCH controls are expected to reset the delayed wrap flag.
        page.Cursor().ResetDelayEOLWrap();
    }
}

// Routine Description:
// ICH - Insert Character - Blank/default attribute characters will be inserted at the current cursor position.
//     - Each inserted character will push all text in the row to the right.
// Arguments:
// - count - The number of characters to insert
void AdaptDispatch::InsertCharacter(const VTInt count)
{
    _InsertDeleteCharacterHelper(count);
}

// Routine Description:
// DCH - Delete Character - The character at the cursor position will be deleted. Blank/attribute characters will
//       be inserted from the right edge of the current line.
// Arguments:
// - count - The number of characters to delete
void AdaptDispatch::DeleteCharacter(const VTInt count)
{
    _InsertDeleteCharacterHelper(-count);
}

// Routine Description:
// - Fills an area of the buffer with a given character and attributes.
// Arguments:
// - page - Target page to be filled.
// - fillRect - Area of the page that will be affected.
// - fillChar - Character to be written to the buffer.
// - fillAttrs - Attributes to be written to the buffer.
// Return Value:
// - <none>
void AdaptDispatch::_FillRect(const Page& page, const til::rect& fillRect, const std::wstring_view& fillChar, const TextAttribute& fillAttrs) const
{
    page.Buffer().FillRect(fillRect, fillChar, fillAttrs);
    _api.NotifyAccessibilityChange(fillRect);
}

// Routine Description:
// - ECH - Erase Characters from the current cursor position, by replacing
//     them with a space. This will only erase characters in the current line,
//     and won't wrap to the next. The attributes of any erased positions
//     receive the currently selected attributes.
// Arguments:
// - numChars - The number of characters to erase.
void AdaptDispatch::EraseCharacters(const VTInt numChars)
{
    const auto page = _pages.ActivePage();
    const auto row = page.Cursor().GetPosition().y;
    const auto startCol = page.Cursor().GetPosition().x;
    const auto endCol = std::min<VTInt>(startCol + numChars, page.Buffer().GetLineWidth(row));

    // The ECH control is expected to reset the delayed wrap flag.
    page.Cursor().ResetDelayEOLWrap();

    const auto eraseAttributes = _GetEraseAttributes(page);
    _FillRect(page, { startCol, row, endCol, row + 1 }, whitespace, eraseAttributes);
}

// Routine Description:
// - ED - Erases a portion of the current page of the console.
// Arguments:
// - eraseType - Determines whether to erase:
//      From beginning (top-left corner) to the cursor
//      From cursor to end (bottom-right corner)
//      The entire page
//      The scrollback (outside the page area)
void AdaptDispatch::EraseInDisplay(const DispatchTypes::EraseType eraseType)
{
    if (eraseType > DispatchTypes::EraseType::Scrollback)
    {
        return;
    }

    // First things first. If this is a "Scrollback" clear, then just do that.
    // Scrollback clears erase everything in the "scrollback" of a *nix terminal
    //      Everything that's scrolled off the screen so far.
    // Or if it's an Erase All, then we also need to handle that specially
    //      by moving the current contents of the page into the scrollback.
    if (eraseType == DispatchTypes::EraseType::Scrollback)
    {
        return _EraseScrollback();
    }
    else if (eraseType == DispatchTypes::EraseType::All)
    {
        return _EraseAll();
    }

    const auto page = _pages.ActivePage();
    auto& textBuffer = page.Buffer();
    const auto pageWidth = page.Width();
    const auto row = page.Cursor().GetPosition().y;
    const auto col = page.Cursor().GetPosition().x;

    // The ED control is expected to reset the delayed wrap flag.
    // The special case variants above ("erase all" and "erase scrollback")
    // take care of that themselves when they set the cursor position.
    page.Cursor().ResetDelayEOLWrap();

    const auto eraseAttributes = _GetEraseAttributes(page);

    // When erasing the display, every line that is erased in full should be
    // reset to single width. When erasing to the end, this could include
    // the current line, if the cursor is in the first column. When erasing
    // from the beginning, though, the current line would never be included,
    // because the cursor could never be in the rightmost column (assuming
    // the line is double width).
    if (eraseType == DispatchTypes::EraseType::FromBeginning)
    {
        textBuffer.ResetLineRenditionRange(page.Top(), row);
        _FillRect(page, { 0, page.Top(), pageWidth, row }, whitespace, eraseAttributes);
        _FillRect(page, { 0, row, col + 1, row + 1 }, whitespace, eraseAttributes);
    }
    if (eraseType == DispatchTypes::EraseType::ToEnd)
    {
        textBuffer.ResetLineRenditionRange(col > 0 ? row + 1 : row, page.Bottom());
        _FillRect(page, { col, row, pageWidth, row + 1 }, whitespace, eraseAttributes);
        _FillRect(page, { 0, row + 1, pageWidth, page.Bottom() }, whitespace, eraseAttributes);
    }
}

// Routine Description:
// - EL - Erases the line that the cursor is currently on.
// Arguments:
// - eraseType - Determines whether to erase: From beginning (left edge) to the cursor, from cursor to end (right edge), or the entire line.
void AdaptDispatch::EraseInLine(const DispatchTypes::EraseType eraseType)
{
    const auto page = _pages.ActivePage();
    const auto& textBuffer = page.Buffer();
    const auto row = page.Cursor().GetPosition().y;
    const auto col = page.Cursor().GetPosition().x;

    // The EL control is expected to reset the delayed wrap flag.
    page.Cursor().ResetDelayEOLWrap();

    const auto eraseAttributes = _GetEraseAttributes(page);
    switch (eraseType)
    {
    case DispatchTypes::EraseType::FromBeginning:
        _FillRect(page, { 0, row, col + 1, row + 1 }, whitespace, eraseAttributes);
        break;
    case DispatchTypes::EraseType::ToEnd:
        _FillRect(page, { col, row, textBuffer.GetLineWidth(row), row + 1 }, whitespace, eraseAttributes);
        break;
    case DispatchTypes::EraseType::All:
        _FillRect(page, { 0, row, textBuffer.GetLineWidth(row), row + 1 }, whitespace, eraseAttributes);
        break;
    default:
        break;
    }
}

// Routine Description:
// - Selectively erases unprotected cells in an area of the buffer.
// Arguments:
// - page - Target page to be erased.
// - eraseRect - Area of the page that will be affected.
// Return Value:
// - <none>
void AdaptDispatch::_SelectiveEraseRect(const Page& page, const til::rect& eraseRect)
{
    if (eraseRect)
    {
        for (auto row = eraseRect.top; row < eraseRect.bottom; row++)
        {
            auto& rowBuffer = page.Buffer().GetMutableRowByOffset(row);
            for (auto col = eraseRect.left; col < eraseRect.right; col++)
            {
                // Only unprotected cells are affected.
                if (!rowBuffer.GetAttrByColumn(col).IsProtected())
                {
                    // The text is cleared but the attributes are left as is.
                    rowBuffer.ClearCell(col);
                    // Any image content also needs to be erased.
                    ImageSlice::EraseCells(rowBuffer, col, col + 1);
                    page.Buffer().TriggerRedraw(Viewport::FromDimensions({ col, row }, { 1, 1 }));
                }
            }
        }
        _api.NotifyAccessibilityChange(eraseRect);
    }
}

// Routine Description:
// - DECSED - Selectively erases unprotected cells in a portion of the page.
// Arguments:
// - eraseType - Determines whether to erase:
//      From beginning (top-left corner) to the cursor
//      From cursor to end (bottom-right corner)
//      The entire page area
void AdaptDispatch::SelectiveEraseInDisplay(const DispatchTypes::EraseType eraseType)
{
    const auto page = _pages.ActivePage();
    const auto pageWidth = page.Width();
    const auto row = page.Cursor().GetPosition().y;
    const auto col = page.Cursor().GetPosition().x;

    // The DECSED control is expected to reset the delayed wrap flag.
    page.Cursor().ResetDelayEOLWrap();

    switch (eraseType)
    {
    case DispatchTypes::EraseType::FromBeginning:
        _SelectiveEraseRect(page, { 0, page.Top(), pageWidth, row });
        _SelectiveEraseRect(page, { 0, row, col + 1, row + 1 });
        break;
    case DispatchTypes::EraseType::ToEnd:
        _SelectiveEraseRect(page, { col, row, pageWidth, row + 1 });
        _SelectiveEraseRect(page, { 0, row + 1, pageWidth, page.Bottom() });
        break;
    case DispatchTypes::EraseType::All:
        _SelectiveEraseRect(page, { 0, page.Top(), pageWidth, page.Bottom() });
        break;
    default:
        break;
    }
}

// Routine Description:
// - DECSEL - Selectively erases unprotected cells on line with the cursor.
// Arguments:
// - eraseType - Determines whether to erase:
//      From beginning (left edge) to the cursor
//      From cursor to end (right edge)
//      The entire line.
void AdaptDispatch::SelectiveEraseInLine(const DispatchTypes::EraseType eraseType)
{
    const auto page = _pages.ActivePage();
    const auto& textBuffer = page.Buffer();
    const auto row = page.Cursor().GetPosition().y;
    const auto col = page.Cursor().GetPosition().x;

    // The DECSEL control is expected to reset the delayed wrap flag.
    page.Cursor().ResetDelayEOLWrap();

    switch (eraseType)
    {
    case DispatchTypes::EraseType::FromBeginning:
        _SelectiveEraseRect(page, { 0, row, col + 1, row + 1 });
        break;
    case DispatchTypes::EraseType::ToEnd:
        _SelectiveEraseRect(page, { col, row, textBuffer.GetLineWidth(row), row + 1 });
        break;
    case DispatchTypes::EraseType::All:
        _SelectiveEraseRect(page, { 0, row, textBuffer.GetLineWidth(row), row + 1 });
        break;
    default:
        break;
    }
}

// Routine Description:
// - Changes the attributes of each cell in a rectangular area of the buffer.
// Arguments:
// - page - Target page to be changed.
// - changeRect - A rectangular area of the page that will be affected.
// - changeOps - Changes that will be applied to each of the attributes.
// Return Value:
// - <none>
void AdaptDispatch::_ChangeRectAttributes(const Page& page, const til::rect& changeRect, const ChangeOps& changeOps)
{
    if (changeRect)
    {
        for (auto row = changeRect.top; row < changeRect.bottom; row++)
        {
            auto& rowBuffer = page.Buffer().GetMutableRowByOffset(row);
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
        page.Buffer().TriggerRedraw(Viewport::FromExclusive(changeRect));
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
    const auto page = _pages.ActivePage();
    const auto changeRect = _CalculateRectArea(page, changeArea.top, changeArea.left, changeArea.bottom, changeArea.right);
    const auto lineCount = changeRect.height();

    // If the change extent is rectangular, we can apply the change with a
    // single call. The same is true for a stream extent that is only one line.
    if (_modes.test(Mode::RectangularChangeExtent) || lineCount == 1)
    {
        _ChangeRectAttributes(page, changeRect, changeOps);
    }
    // If the stream extent is more than one line we require three passes. The
    // top line is altered from the left offset up to the end of the line. The
    // bottom line is altered from the start up to the right offset. All the
    // lines in-between have their entire length altered. The right coordinate
    // must be greater than the left, otherwise the operation is ignored.
    else if (lineCount > 1 && changeRect.right > changeRect.left)
    {
        const auto pageWidth = page.Width();
        _ChangeRectAttributes(page, { changeRect.origin(), til::size{ pageWidth - changeRect.left, 1 } }, changeOps);
        _ChangeRectAttributes(page, { { 0, changeRect.top + 1 }, til::size{ pageWidth, lineCount - 2 } }, changeOps);
        _ChangeRectAttributes(page, { { 0, changeRect.bottom - 1 }, til::size{ changeRect.right, 1 } }, changeOps);
    }
}

// Routine Description:
// - Helper method to calculate the applicable buffer coordinates for use with
//   the various rectangular area operations.
// Arguments:
// - page - The target page.
// - top - The first row of the area.
// - left - The first column of the area.
// - bottom - The last row of the area (inclusive).
// - right - The last column of the area (inclusive).
// Return value:
// - An exclusive rect with the absolute buffer coordinates.
til::rect AdaptDispatch::_CalculateRectArea(const Page& page, const VTInt top, const VTInt left, const VTInt bottom, const VTInt right)
{
    const auto pageWidth = page.Width();
    const auto pageHeight = page.Height();

    // We start by calculating the margin offsets and maximum dimensions.
    // If the origin mode isn't set, we use the page extent.
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(page, false);
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(pageWidth);
    const auto yOffset = _modes.test(Mode::Origin) ? topMargin : 0;
    const auto yMaximum = _modes.test(Mode::Origin) ? bottomMargin + 1 : pageHeight;
    const auto xOffset = _modes.test(Mode::Origin) ? leftMargin : 0;
    const auto xMaximum = _modes.test(Mode::Origin) ? rightMargin + 1 : pageWidth;

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

    // To get absolute coordinates we offset with the page top.
    fillRect.top += page.Top();
    fillRect.bottom += page.Top();

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
void AdaptDispatch::ChangeAttributesRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right, const VTParameters attrs)
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
void AdaptDispatch::ReverseAttributesRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right, const VTParameters attrs)
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
}

// Routine Description:
// - DECCRA - Copies a rectangular area from one part of the buffer to another.
// Arguments:
// - top - The first row of the source area.
// - left - The first column of the source area.
// - bottom - The last row of the source area (inclusive).
// - right - The last column of the source area (inclusive).
// - page - The source page number.
// - dstTop - The first row of the destination.
// - dstLeft - The first column of the destination.
// - dstPage - The destination page number.
void AdaptDispatch::CopyRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right, const VTInt page, const VTInt dstTop, const VTInt dstLeft, const VTInt dstPage)
{
    const auto src = _pages.Get(page);
    const auto dst = _pages.Get(dstPage);
    const auto srcRect = _CalculateRectArea(src, top, left, bottom, right);
    const auto dstBottom = dstTop + srcRect.height() - 1;
    const auto dstRight = dstLeft + srcRect.width() - 1;
    const auto dstRect = _CalculateRectArea(dst, dstTop, dstLeft, dstBottom, dstRight);

    if (dstRect && (dstRect.origin() != srcRect.origin() || src.Number() != dst.Number()))
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
        auto next = OutputCell(*src.Buffer().GetCellDataAt(srcPos));
        do
        {
            const auto current = next;
            const auto currentSrcPos = srcPos;
            srcView.WalkInBounds(srcPos, walkDirection);
            next = OutputCell(*src.Buffer().GetCellDataAt(srcPos));
            // If the source position is offscreen (which can occur on double
            // width lines), then we shouldn't copy anything to the destination.
            if (currentSrcPos.x < src.Buffer().GetLineWidth(currentSrcPos.y))
            {
                dst.Buffer().WriteLine(OutputCellIterator({ &current, 1 }), dstPos);
            }
        } while (dstView.WalkInBounds(dstPos, walkDirection));
        // Copy any image content in the affected area.
        ImageSlice::CopyBlock(src.Buffer(), srcView.ToExclusive(), dst.Buffer(), dstView.ToExclusive());
        _api.NotifyAccessibilityChange(dstRect);
    }
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
void AdaptDispatch::FillRectangularArea(const VTParameter ch, const VTInt top, const VTInt left, const VTInt bottom, const VTInt right)
{
    const auto page = _pages.ActivePage();
    const auto fillRect = _CalculateRectArea(page, top, left, bottom, right);

    // The standard only allows for characters in the range of the GL and GR
    // character set tables, but we also support additional Unicode characters
    // from the BMP if the code page is UTF-8. Default and 0 are treated as 32.
    const auto charValue = ch.value_or(0) == 0 ? 32 : ch.value();
    const auto glChar = (charValue >= 32 && charValue <= 126);
    const auto grChar = (charValue >= 160 && charValue <= 255);
    const auto unicodeChar = (charValue >= 256 && charValue <= 65535 && _api.GetOutputCodePage() == CP_UTF8);
    if (glChar || grChar || unicodeChar)
    {
        const auto fillChar = _termOutput.TranslateKey(gsl::narrow_cast<wchar_t>(charValue));
        const auto& fillAttributes = page.Attributes();
        _FillRect(page, fillRect, { &fillChar, 1 }, fillAttributes);
    }
}

// Routine Description:
// - DECERA - Erases a rectangular area, replacing all cells with a space
//     character and the default rendition attributes.
// Arguments:
// - top - The first row of the area.
// - left - The first column of the area.
// - bottom - The last row of the area (inclusive).
// - right - The last column of the area (inclusive).
void AdaptDispatch::EraseRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right)
{
    const auto page = _pages.ActivePage();
    const auto eraseRect = _CalculateRectArea(page, top, left, bottom, right);
    const auto eraseAttributes = _GetEraseAttributes(page);
    _FillRect(page, eraseRect, whitespace, eraseAttributes);
}

// Routine Description:
// - DECSERA - Selectively erases a rectangular area, replacing unprotected
//     cells with a space character, but retaining the rendition attributes.
// Arguments:
// - top - The first row of the area.
// - left - The first column of the area.
// - bottom - The last row of the area (inclusive).
// - right - The last column of the area (inclusive).
void AdaptDispatch::SelectiveEraseRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right)
{
    const auto page = _pages.ActivePage();
    const auto eraseRect = _CalculateRectArea(page, top, left, bottom, right);
    _SelectiveEraseRect(page, eraseRect);
}

// Routine Description:
// - DECSACE - Selects the format of the character range that will be affected
//   by the DECCARA and DECRARA attribute operations.
// Arguments:
// - changeExtent - Whether the character range is a stream or a rectangle.
void AdaptDispatch::SelectAttributeChangeExtent(const DispatchTypes::ChangeExtent changeExtent) noexcept
{
    switch (changeExtent)
    {
    case DispatchTypes::ChangeExtent::Default:
    case DispatchTypes::ChangeExtent::Stream:
        _modes.reset(Mode::RectangularChangeExtent);
        break;
    case DispatchTypes::ChangeExtent::Rectangle:
        _modes.set(Mode::RectangularChangeExtent);
        break;
    default:
        break;
    }
}

void AdaptDispatch::SetVtChecksumReportSupport(const bool enabled) noexcept
{
    _vtChecksumReportEnabled = enabled;
}

// Routine Description:
// - DECRQCRA - Computes and reports a checksum of the specified area of
//   the buffer memory.
// Arguments:
// - id - a numeric label used to identify the request.
// - page - The page number.
// - top - The first row of the area.
// - left - The first column of the area.
// - bottom - The last row of the area (inclusive).
// - right - The last column of the area (inclusive).
void AdaptDispatch::RequestChecksumRectangularArea(const VTInt id, const VTInt page, const VTInt top, const VTInt left, const VTInt bottom, const VTInt right)
{
    uint16_t checksum = 0;
    // If this feature is not enabled, we'll just report a zero checksum.
    if constexpr (Feature_VtChecksumReport::IsEnabled())
    {
        if (_vtChecksumReportEnabled)
        {
            // If the page number is 0, then we're meant to return a checksum of all
            // of the pages, but we have no need for that, so we'll just return 0.
            if (page != 0)
            {
                // As part of the checksum, we need to include the color indices of each
                // cell, and in the case of default colors, those indices come from the
                // color alias table. But if they're not in the bottom 16 range, we just
                // fallback to using white on black (7 and 0).
                auto defaultFgIndex = _renderSettings.GetColorAliasIndex(ColorAlias::DefaultForeground);
                auto defaultBgIndex = _renderSettings.GetColorAliasIndex(ColorAlias::DefaultBackground);
                defaultFgIndex = defaultFgIndex < 16 ? defaultFgIndex : 7;
                defaultBgIndex = defaultBgIndex < 16 ? defaultBgIndex : 0;

                const auto target = _pages.Get(page);
                const auto eraseRect = _CalculateRectArea(target, top, left, bottom, right);
                for (auto row = eraseRect.top; row < eraseRect.bottom; row++)
                {
                    for (auto col = eraseRect.left; col < eraseRect.right; col++)
                    {
                        // The algorithm we're using here should match the DEC terminals
                        // for the ASCII and Latin-1 range. Their other character sets
                        // predate Unicode, though, so we'd need a custom mapping table
                        // to lookup the correct checksums. Considering this is only for
                        // testing at the moment, that doesn't seem worth the effort.
                        const auto cell = target.Buffer().GetCellDataAt({ col, row });
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
    }
    _ReturnDcsResponse(wil::str_printf<std::wstring>(L"%d!~%04X", id, checksum));
}

// Routine Description:
// - DECSWL/DECDWL/DECDHL - Sets the line rendition attribute for the current line.
// Arguments:
// - rendition - Determines whether the line will be rendered as single width, double
//   width, or as one half of a double height line.
void AdaptDispatch::SetLineRendition(const LineRendition rendition)
{
    // The line rendition can't be changed if left/right margins are allowed.
    if (!_modes.test(Mode::AllowDECSLRM))
    {
        const auto page = _pages.ActivePage();
        const auto eraseAttributes = _GetEraseAttributes(page);
        page.Buffer().SetCurrentLineRendition(rendition, eraseAttributes);
        // There is some variation in how this was handled by the different DEC
        // terminals, but the STD 070 reference (on page D-13) makes it clear that
        // the delayed wrap (aka the Last Column Flag) was expected to be reset when
        // line rendition controls were executed.
        page.Cursor().ResetDelayEOLWrap();
    }
}

// Routine Description:
// - DSR - Reports status of a console property back to the STDIN based on the type of status requested.
// Arguments:
// - statusType - status type indicating what property we should report back
// - id - a numeric label used to identify the request in DECCKSR reports
void AdaptDispatch::DeviceStatusReport(const DispatchTypes::StatusType statusType, const VTParameter id)
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
        break;
    case DispatchTypes::StatusType::CursorPositionReport:
        _CursorPositionReport(false);
        break;
    case DispatchTypes::StatusType::ExtendedCursorPositionReport:
        _CursorPositionReport(true);
        break;
    case DispatchTypes::StatusType::PrinterStatus:
        _DeviceStatusReport(PrinterNotConnected);
        break;
    case DispatchTypes::StatusType::UserDefinedKeys:
        _DeviceStatusReport(UserDefinedKeysNotSupported);
        break;
    case DispatchTypes::StatusType::KeyboardStatus:
        _DeviceStatusReport(UnknownPcKeyboard);
        break;
    case DispatchTypes::StatusType::LocatorStatus:
        _DeviceStatusReport(LocatorNotConnected);
        break;
    case DispatchTypes::StatusType::LocatorIdentity:
        _DeviceStatusReport(UnknownLocatorDevice);
        break;
    case DispatchTypes::StatusType::MacroSpaceReport:
        _MacroSpaceReport();
        break;
    case DispatchTypes::StatusType::MemoryChecksum:
        _MacroChecksumReport(id);
        break;
    case DispatchTypes::StatusType::DataIntegrity:
        _DeviceStatusReport(TerminalReady);
        break;
    case DispatchTypes::StatusType::MultipleSessionStatus:
        _DeviceStatusReport(MultipleSessionsNotSupported);
        break;
    default:
        break;
    }
}

// Routine Description:
// - DA - Reports the service class or conformance level that the terminal
//   supports, and the set of implemented extensions.
// Arguments:
// - <none>
void AdaptDispatch::DeviceAttributes()
{
    // This first parameter of the response is 61, representing a conformance
    // level of 1. The subsequent parameters identify the supported feature
    // extensions.
    //
    // 4 = Sixel Graphics
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

    _ReturnCsiResponse(L"?61;4;6;7;14;21;22;23;24;28;32;42c");
}

// Routine Description:
// - DA2 - Reports the terminal type, firmware version, and hardware options.
//   For now we're following the XTerm practice of using 0 to represent a VT100
//   terminal, the version is hard-coded as 10 (1.0), and the hardware option
//   is set to 1 (indicating a PC Keyboard).
// Arguments:
// - <none>
void AdaptDispatch::SecondaryDeviceAttributes()
{
    _ReturnCsiResponse(L">0;10;1c");
}

// Routine Description:
// - DA3 - Reports the terminal unit identification code. Terminal emulators
//   typically return a hard-coded value, the most common being all zeros.
// Arguments:
// - <none>
void AdaptDispatch::TertiaryDeviceAttributes()
{
    _ReturnDcsResponse(L"!|00000000");
}

// Routine Description:
// - VT52 Identify - Reports the identity of the terminal in VT52 emulation mode.
//   An actual VT52 terminal would typically identify itself with ESC / K.
//   But for a terminal that is emulating a VT52, the sequence should be ESC / Z.
// Arguments:
// - <none>
void AdaptDispatch::Vt52DeviceAttributes()
{
    _api.ReturnResponse(L"\x1b/Z");
}

// Routine Description:
// - DECREQTPARM - This sequence was originally used on the VT100 terminal to
//   report the serial communication parameters (baud rate, data bits, parity,
//   etc.). On modern terminal emulators, the response is simply hard-coded.
// Arguments:
// - permission - This would originally have determined whether the terminal
//   was allowed to send unsolicited reports or not.
void AdaptDispatch::RequestTerminalParameters(const DispatchTypes::ReportingPermission permission)
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
        _ReturnCsiResponse(L"2;1;1;128;128;1;0x");
        break;
    case DispatchTypes::ReportingPermission::Solicited:
        _ReturnCsiResponse(L"3;1;1;128;128;1;0x");
        break;
    default:
        break;
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
    _ReturnCsiResponse(fmt::format(FMT_COMPILE(L"{}n"), parameters));
}

// Routine Description:
// - CPR and DECXCPR- Reports the current cursor position within the page,
//   as well as the current page number if this is an extended report.
// Arguments:
// - extendedReport - Set to true if the report should include the page number
// Return Value:
// - <none>
void AdaptDispatch::_CursorPositionReport(const bool extendedReport)
{
    const auto page = _pages.ActivePage();

    // First pull the cursor position relative to the entire buffer out of the console.
    til::point cursorPosition{ page.Cursor().GetPosition() };

    // Now adjust it for its position in respect to the current page top.
    cursorPosition.y -= page.Top();

    // NOTE: 1,1 is the top-left corner of the page in VT-speak, so add 1.
    cursorPosition.x++;
    cursorPosition.y++;

    // If the origin mode is set, the cursor is relative to the margin origin.
    if (_modes.test(Mode::Origin))
    {
        cursorPosition.x -= _GetHorizontalMargins(page.Width()).first;
        cursorPosition.y -= _GetVerticalMargins(page, false).first;
    }

    // Now send it back into the input channel of the console.
    if (extendedReport)
    {
        // An extended report also includes the page number.
        const auto pageNumber = page.Number();
        _ReturnCsiResponse(wil::str_printf<std::wstring>(L"?%d;%d;%dR", cursorPosition.y, cursorPosition.x, pageNumber));
    }
    else
    {
        // The standard report only returns the cursor position.
        _ReturnCsiResponse(wil::str_printf<std::wstring>(L"%d;%dR", cursorPosition.y, cursorPosition.x));
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
    _ReturnCsiResponse(wil::str_printf<std::wstring>(L"%zu*{", spaceInBytes / 16));
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
    _ReturnDcsResponse(wil::str_printf<std::wstring>(L"%d!~%04X", requestId, checksum));
}

// Routine Description:
// - Generalizes scrolling movement for up/down
// Arguments:
// - delta - Distance to move (positive is down, negative is up)
// Return Value:
// - <none>
void AdaptDispatch::_ScrollMovement(const VTInt delta)
{
    const auto page = _pages.ActivePage();
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(page, true);
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(page.Width());
    _ScrollRectVertically(page, { leftMargin, topMargin, rightMargin + 1, bottomMargin + 1 }, delta);
}

// Routine Description:
// - SU - Pans the window DOWN by given distance (distance new lines appear at the bottom of the screen)
// Arguments:
// - distance - Distance to move
void AdaptDispatch::ScrollUp(const VTInt uiDistance)
{
    _ScrollMovement(-uiDistance);
}

// Routine Description:
// - SD - Pans the window UP by given distance (distance new lines appear at the top of the screen)
// Arguments:
// - distance - Distance to move
void AdaptDispatch::ScrollDown(const VTInt uiDistance)
{
    _ScrollMovement(uiDistance);
}

// Routine Description:
// - NP - Moves the active position one or more pages ahead, and moves the
//   cursor to home.
// Arguments:
// - pageCount - Number of pages to move
void AdaptDispatch::NextPage(const VTInt pageCount)
{
    PagePositionRelative(pageCount);
    CursorPosition(1, 1);
}

// Routine Description:
// - PP - Moves the active position one or more pages back, and moves the
//   cursor to home.
// Arguments:
// - pageCount - Number of pages to move
void AdaptDispatch::PrecedingPage(const VTInt pageCount)
{
    PagePositionBack(pageCount);
    CursorPosition(1, 1);
}

// Routine Description:
// - PPA - Moves the active position to the specified page number, without
//   altering the cursor coordinates.
// Arguments:
// - page - Destination page
void AdaptDispatch::PagePositionAbsolute(const VTInt page)
{
    _pages.MoveTo(page, _modes.test(Mode::PageCursorCoupling));
}

// Routine Description:
// - PPR - Moves the active position one or more pages ahead, without altering
//   the cursor coordinates.
// Arguments:
// - pageCount - Number of pages to move
void AdaptDispatch::PagePositionRelative(const VTInt pageCount)
{
    _pages.MoveRelative(pageCount, _modes.test(Mode::PageCursorCoupling));
}

// Routine Description:
// - PPB - Moves the active position one or more pages back, without altering
//   the cursor coordinates.
// Arguments:
// - pageCount - Number of pages to move
void AdaptDispatch::PagePositionBack(const VTInt pageCount)
{
    _pages.MoveRelative(-pageCount, _modes.test(Mode::PageCursorCoupling));
}

// Routine Description:
// - DECRQDE - Requests the area of page memory that is currently visible.
// Arguments:
// - None
void AdaptDispatch::RequestDisplayedExtent()
{
    const auto page = _pages.VisiblePage();
    const auto width = page.Viewport().width();
    const auto height = page.Viewport().height();
    const auto left = page.XPanOffset() + 1;
    const auto top = page.YPanOffset() + 1;
    _ReturnCsiResponse(fmt::format(FMT_COMPILE(L"{};{};{};{};{}\"w"), height, width, left, top, page.Number()));
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
    if (_modes.test(Mode::AllowDECCOLM))
    {
        const auto page = _pages.VisiblePage();
        const auto pageHeight = page.Height();
        const auto pageWidth = (enable ? DispatchTypes::s_sDECCOLMSetColumns : DispatchTypes::s_sDECCOLMResetColumns);
        _api.ResizeWindow(pageWidth, pageHeight);
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
        const auto page = _pages.ActivePage();
        _api.UseAlternateScreenBuffer(_GetEraseAttributes(page));
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
// - Support routine for routing mode parameters to be set/reset as flags
// Arguments:
// - param - mode parameter to set/reset
// - enable - True for set, false for unset.
void AdaptDispatch::_ModeParamsHelper(const DispatchTypes::ModeParams param, const bool enable)
{
    switch (param)
    {
    case DispatchTypes::ModeParams::IRM_InsertReplaceMode:
        _modes.set(Mode::InsertReplace, enable);
        break;
    case DispatchTypes::ModeParams::LNM_LineFeedNewLineMode:
        // VT apps expect that the system and input modes are the same, so if
        // they become out of sync, we just act as if LNM mode isn't supported.
        if (_api.GetSystemMode(ITerminalApi::Mode::LineFeed) == _terminalInput.GetInputMode(TerminalInput::Mode::LineFeed))
        {
            _api.SetSystemMode(ITerminalApi::Mode::LineFeed, enable);
            _terminalInput.SetInputMode(TerminalInput::Mode::LineFeed, enable);
        }
        break;
    case DispatchTypes::ModeParams::DECCKM_CursorKeysMode:
        _terminalInput.SetInputMode(TerminalInput::Mode::CursorKey, enable);
        break;
    case DispatchTypes::ModeParams::DECANM_AnsiMode:
        return SetAnsiMode(enable);
    case DispatchTypes::ModeParams::DECCOLM_SetNumberOfColumns:
        _SetColumnMode(enable);
        break;
    case DispatchTypes::ModeParams::DECSCNM_ScreenMode:
        _renderSettings.SetRenderMode(RenderSettings::Mode::ScreenReversed, enable);
        if (_renderer)
        {
            _renderer->TriggerRedrawAll();
        }
        break;
    case DispatchTypes::ModeParams::DECOM_OriginMode:
        _modes.set(Mode::Origin, enable);
        // The cursor is also moved to the new home position when the origin mode is set or reset.
        CursorPosition(1, 1);
        break;
    case DispatchTypes::ModeParams::DECAWM_AutoWrapMode:
        _api.SetSystemMode(ITerminalApi::Mode::AutoWrap, enable);
        // Resetting DECAWM should also reset the delayed wrap flag.
        if (!enable)
        {
            _pages.ActivePage().Cursor().ResetDelayEOLWrap();
        }
        break;
    case DispatchTypes::ModeParams::DECARM_AutoRepeatMode:
        _terminalInput.SetInputMode(TerminalInput::Mode::AutoRepeat, enable);
        break;
    case DispatchTypes::ModeParams::ATT610_StartCursorBlink:
        _pages.ActivePage().Cursor().SetBlinkingAllowed(enable);
        break;
    case DispatchTypes::ModeParams::DECTCEM_TextCursorEnableMode:
        _pages.ActivePage().Cursor().SetIsVisible(enable);
        break;
    case DispatchTypes::ModeParams::XTERM_EnableDECCOLMSupport:
        _modes.set(Mode::AllowDECCOLM, enable);
        break;
    case DispatchTypes::ModeParams::DECPCCM_PageCursorCouplingMode:
        _modes.set(Mode::PageCursorCoupling, enable);
        if (enable)
        {
            _pages.MakeActivePageVisible();
        }
        break;
    case DispatchTypes::ModeParams::DECNKM_NumericKeypadMode:
        _terminalInput.SetInputMode(TerminalInput::Mode::Keypad, enable);
        break;
    case DispatchTypes::ModeParams::DECBKM_BackarrowKeyMode:
        _terminalInput.SetInputMode(TerminalInput::Mode::BackarrowKey, enable);
        break;
    case DispatchTypes::ModeParams::DECLRMM_LeftRightMarginMode:
        _modes.set(Mode::AllowDECSLRM, enable);
        _DoSetLeftRightScrollingMargins(0, 0);
        if (enable)
        {
            // If we've allowed left/right margins, we can't have line renditions.
            const auto page = _pages.ActivePage();
            page.Buffer().ResetLineRenditionRange(page.Top(), page.Bottom());
        }
        break;
    case DispatchTypes::ModeParams::DECSDM_SixelDisplayMode:
        _modes.set(Mode::SixelDisplay, enable);
        if (_sixelParser)
        {
            _sixelParser->SetDisplayMode(enable);
        }
        break;
    case DispatchTypes::ModeParams::DECECM_EraseColorMode:
        _modes.set(Mode::EraseColor, enable);
        break;
    case DispatchTypes::ModeParams::VT200_MOUSE_MODE:
        _terminalInput.SetInputMode(TerminalInput::Mode::DefaultMouseTracking, enable);
        break;
    case DispatchTypes::ModeParams::BUTTON_EVENT_MOUSE_MODE:
        _terminalInput.SetInputMode(TerminalInput::Mode::ButtonEventMouseTracking, enable);
        break;
    case DispatchTypes::ModeParams::ANY_EVENT_MOUSE_MODE:
        _terminalInput.SetInputMode(TerminalInput::Mode::AnyEventMouseTracking, enable);
        break;
    case DispatchTypes::ModeParams::UTF8_EXTENDED_MODE:
        _terminalInput.SetInputMode(TerminalInput::Mode::Utf8MouseEncoding, enable);
        break;
    case DispatchTypes::ModeParams::SGR_EXTENDED_MODE:
        _terminalInput.SetInputMode(TerminalInput::Mode::SgrMouseEncoding, enable);
        break;
    case DispatchTypes::ModeParams::FOCUS_EVENT_MODE:
        _terminalInput.SetInputMode(TerminalInput::Mode::FocusEvent, enable);
        // ConPTY always wants to know about focus events, so let it know that it needs to re-enable this mode.
        if (!enable)
        {
            _api.GetStateMachine().InjectSequence(InjectionType::DECSET_FOCUS);
        }
        break;
    case DispatchTypes::ModeParams::ALTERNATE_SCROLL:
        _terminalInput.SetInputMode(TerminalInput::Mode::AlternateScroll, enable);
        break;
    case DispatchTypes::ModeParams::ASB_AlternateScreenBuffer:
        _SetAlternateScreenBufferMode(enable);
        break;
    case DispatchTypes::ModeParams::XTERM_BracketedPasteMode:
        _api.SetSystemMode(ITerminalApi::Mode::BracketedPaste, enable);
        break;
    case DispatchTypes::ModeParams::GCM_GraphemeClusterMode:
        break;
    case DispatchTypes::ModeParams::W32IM_Win32InputMode:
        _terminalInput.SetInputMode(TerminalInput::Mode::Win32, enable);
        // ConPTY requests the Win32InputMode on startup and disables it on shutdown. When nesting ConPTY inside
        // ConPTY then this should not bubble up. Otherwise, when the inner ConPTY exits and the outer ConPTY
        // passes the disable sequence up to the hosting terminal, we'd stop getting Win32InputMode entirely!
        // It also makes more sense to not bubble it up, because this mode is specifically for INPUT_RECORD interop
        // and thus entirely between a PTY's input records and its INPUT_RECORD-aware VT-aware console clients.
        // Returning true here will mark this as being handled and avoid this.
        if (!enable)
        {
            _api.GetStateMachine().InjectSequence(InjectionType::W32IM);
        }
        break;
    default:
        break;
    }
}

// Routine Description:
// - SM/DECSET - Enables the given mode parameter (both ANSI and private).
// Arguments:
// - param - mode parameter to set
void AdaptDispatch::SetMode(const DispatchTypes::ModeParams param)
{
    _ModeParamsHelper(param, true);
}

// Routine Description:
// - RM/DECRST - Disables the given mode parameter (both ANSI and private).
// Arguments:
// - param - mode parameter to reset
void AdaptDispatch::ResetMode(const DispatchTypes::ModeParams param)
{
    _ModeParamsHelper(param, false);
}

// Routine Description:
// - DECRQM - Requests the current state of a given mode number. The result
//   is reported back with a DECRPM escape sequence.
// Arguments:
// - param - the mode number being queried
void AdaptDispatch::RequestMode(const DispatchTypes::ModeParams param)
{
    static constexpr auto mapTemp = [](const bool b) { return b ? DispatchTypes::DECRPM_Enabled : DispatchTypes::DECRPM_Disabled; };
    static constexpr auto mapPerm = [](const bool b) { return b ? DispatchTypes::DECRPM_PermanentlyEnabled : DispatchTypes::DECRPM_PermanentlyDisabled; };

    VTInt state = DispatchTypes::DECRPM_Unsupported;

    switch (param)
    {
    case DispatchTypes::ModeParams::IRM_InsertReplaceMode:
        state = mapTemp(_modes.test(Mode::InsertReplace));
        break;
    case DispatchTypes::ModeParams::LNM_LineFeedNewLineMode:
        // VT apps expect that the system and input modes are the same, so if
        // they become out of sync, we just act as if LNM mode isn't supported.
        if (_api.GetSystemMode(ITerminalApi::Mode::LineFeed) == _terminalInput.GetInputMode(TerminalInput::Mode::LineFeed))
        {
            state = mapTemp(_terminalInput.GetInputMode(TerminalInput::Mode::LineFeed));
        }
        break;
    case DispatchTypes::ModeParams::DECCKM_CursorKeysMode:
        state = mapTemp(_terminalInput.GetInputMode(TerminalInput::Mode::CursorKey));
        break;
    case DispatchTypes::ModeParams::DECANM_AnsiMode:
        state = mapTemp(_api.GetStateMachine().GetParserMode(StateMachine::Mode::Ansi));
        break;
    case DispatchTypes::ModeParams::DECCOLM_SetNumberOfColumns:
        state = mapTemp(_modes.test(Mode::Column));
        break;
    case DispatchTypes::ModeParams::DECSCNM_ScreenMode:
        state = mapTemp(_renderSettings.GetRenderMode(RenderSettings::Mode::ScreenReversed));
        break;
    case DispatchTypes::ModeParams::DECOM_OriginMode:
        state = mapTemp(_modes.test(Mode::Origin));
        break;
    case DispatchTypes::ModeParams::DECAWM_AutoWrapMode:
        state = mapTemp(_api.GetSystemMode(ITerminalApi::Mode::AutoWrap));
        break;
    case DispatchTypes::ModeParams::DECARM_AutoRepeatMode:
        state = mapTemp(_terminalInput.GetInputMode(TerminalInput::Mode::AutoRepeat));
        break;
    case DispatchTypes::ModeParams::ATT610_StartCursorBlink:
        state = mapTemp(_pages.ActivePage().Cursor().IsBlinkingAllowed());
        break;
    case DispatchTypes::ModeParams::DECTCEM_TextCursorEnableMode:
        state = mapTemp(_pages.ActivePage().Cursor().IsVisible());
        break;
    case DispatchTypes::ModeParams::XTERM_EnableDECCOLMSupport:
        state = mapTemp(_modes.test(Mode::AllowDECCOLM));
        break;
    case DispatchTypes::ModeParams::DECPCCM_PageCursorCouplingMode:
        state = mapTemp(_modes.test(Mode::PageCursorCoupling));
        break;
    case DispatchTypes::ModeParams::DECNKM_NumericKeypadMode:
        state = mapTemp(_terminalInput.GetInputMode(TerminalInput::Mode::Keypad));
        break;
    case DispatchTypes::ModeParams::DECBKM_BackarrowKeyMode:
        state = mapTemp(_terminalInput.GetInputMode(TerminalInput::Mode::BackarrowKey));
        break;
    case DispatchTypes::ModeParams::DECLRMM_LeftRightMarginMode:
        state = mapTemp(_modes.test(Mode::AllowDECSLRM));
        break;
    case DispatchTypes::ModeParams::DECSDM_SixelDisplayMode:
        state = mapTemp(_modes.test(Mode::SixelDisplay));
        break;
    case DispatchTypes::ModeParams::DECECM_EraseColorMode:
        state = mapTemp(_modes.test(Mode::EraseColor));
        break;
    case DispatchTypes::ModeParams::VT200_MOUSE_MODE:
        state = mapTemp(_terminalInput.GetInputMode(TerminalInput::Mode::DefaultMouseTracking));
        break;
    case DispatchTypes::ModeParams::BUTTON_EVENT_MOUSE_MODE:
        state = mapTemp(_terminalInput.GetInputMode(TerminalInput::Mode::ButtonEventMouseTracking));
        break;
    case DispatchTypes::ModeParams::ANY_EVENT_MOUSE_MODE:
        state = mapTemp(_terminalInput.GetInputMode(TerminalInput::Mode::AnyEventMouseTracking));
        break;
    case DispatchTypes::ModeParams::UTF8_EXTENDED_MODE:
        state = mapTemp(_terminalInput.GetInputMode(TerminalInput::Mode::Utf8MouseEncoding));
        break;
    case DispatchTypes::ModeParams::SGR_EXTENDED_MODE:
        state = mapTemp(_terminalInput.GetInputMode(TerminalInput::Mode::SgrMouseEncoding));
        break;
    case DispatchTypes::ModeParams::FOCUS_EVENT_MODE:
        state = mapTemp(_terminalInput.GetInputMode(TerminalInput::Mode::FocusEvent));
        break;
    case DispatchTypes::ModeParams::ALTERNATE_SCROLL:
        state = mapTemp(_terminalInput.GetInputMode(TerminalInput::Mode::AlternateScroll));
        break;
    case DispatchTypes::ModeParams::ASB_AlternateScreenBuffer:
        state = mapTemp(_usingAltBuffer);
        break;
    case DispatchTypes::ModeParams::XTERM_BracketedPasteMode:
        state = mapTemp(_api.GetSystemMode(ITerminalApi::Mode::BracketedPaste));
        break;
    case DispatchTypes::ModeParams::GCM_GraphemeClusterMode:
        state = mapPerm(CodepointWidthDetector::Singleton().GetMode() == TextMeasurementMode::Graphemes);
        break;
    case DispatchTypes::ModeParams::W32IM_Win32InputMode:
        state = mapTemp(_terminalInput.GetInputMode(TerminalInput::Mode::Win32));
        break;
    default:
        break;
    }

    VTInt mode = param;
    std::wstring_view prefix;

    if (mode >= DispatchTypes::DECPrivateMode(0))
    {
        mode -= DispatchTypes::DECPrivateMode(0);
        prefix = L"?";
    }

    _ReturnCsiResponse(fmt::format(FMT_COMPILE(L"{}{};{}$y"), prefix, mode, state));
}

// - DECKPAM, DECKPNM - Sets the keypad input mode to either Application mode or Numeric mode (true, false respectively)
// Arguments:
// - applicationMode - set to true to enable Application Mode Input, false for Numeric Mode Input.
void AdaptDispatch::SetKeypadMode(const bool fApplicationMode) noexcept
{
    _terminalInput.SetInputMode(TerminalInput::Mode::Keypad, fApplicationMode);
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
    const auto page = _pages.ActivePage();
    auto& cursor = page.Cursor();
    const auto col = cursor.GetPosition().x;
    const auto row = cursor.GetPosition().y;

    const auto [topMargin, bottomMargin] = _GetVerticalMargins(page, true);
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(page.Width());
    if (row >= topMargin && row <= bottomMargin && col >= leftMargin && col <= rightMargin)
    {
        // We emulate inserting and deleting by scrolling the area between the cursor and the bottom margin.
        _ScrollRectVertically(page, { leftMargin, row, rightMargin + 1, bottomMargin + 1 }, delta);

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
void AdaptDispatch::InsertLine(const VTInt distance)
{
    _InsertDeleteLineHelper(distance);
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
void AdaptDispatch::DeleteLine(const VTInt distance)
{
    _InsertDeleteLineHelper(-distance);
}

// Routine Description:
// - Internal logic for adding or removing columns in the active screen buffer.
// Parameters:
// - delta - Number of columns to modify (positive if inserting, negative if deleting).
// Return Value:
// - <none>
void AdaptDispatch::_InsertDeleteColumnHelper(const VTInt delta)
{
    const auto page = _pages.ActivePage();
    const auto& cursor = page.Cursor();
    const auto col = cursor.GetPosition().x;
    const auto row = cursor.GetPosition().y;

    const auto [topMargin, bottomMargin] = _GetVerticalMargins(page, true);
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(page.Width());
    if (row >= topMargin && row <= bottomMargin && col >= leftMargin && col <= rightMargin)
    {
        // We emulate inserting and deleting by scrolling the area between the cursor and the right margin.
        _ScrollRectHorizontally(page, { col, topMargin, rightMargin + 1, bottomMargin + 1 }, delta);
    }
}

// Routine Description:
// - DECIC - This control function inserts one or more blank columns in the
//    scrolling region, starting at the column that has the cursor.
// Arguments:
// - distance - number of columns to insert
void AdaptDispatch::InsertColumn(const VTInt distance)
{
    _InsertDeleteColumnHelper(distance);
}

// Routine Description:
// - DECDC - This control function deletes one or more columns in the scrolling
//    region, starting with the column that has the cursor.
// Arguments:
// - distance - number of columns to delete
void AdaptDispatch::DeleteColumn(const VTInt distance)
{
    _InsertDeleteColumnHelper(-distance);
}

// - DECANM - Sets the terminal emulation mode to either ANSI-compatible or VT52.
// Arguments:
// - ansiMode - set to true to enable the ANSI mode, false for VT52 mode.
void AdaptDispatch::SetAnsiMode(const bool ansiMode)
{
    // When an attempt is made to update the mode, the designated character sets
    // need to be reset to defaults, even if the mode doesn't actually change.
    _termOutput.SoftReset();

    _api.GetStateMachine().SetParserMode(StateMachine::Mode::Ansi, ansiMode);
    _terminalInput.SetInputMode(TerminalInput::Mode::Ansi, ansiMode);

    // While input mode changes are often forwarded over conpty, we never want
    // to do that for the DECANM mode.
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

    const auto page = _pages.ActivePage();
    const auto pageHeight = page.Height();
    // The default top margin is line 1
    if (actualTop == 0)
    {
        actualTop = 1;
    }
    // The default bottom margin is the page height
    if (actualBottom == 0)
    {
        actualBottom = pageHeight;
    }
    // The top margin must be less than the bottom margin, and the
    // bottom margin must be less than or equal to the page height
    if (actualTop < actualBottom && actualBottom <= pageHeight)
    {
        if (actualTop == 1 && actualBottom == pageHeight)
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
void AdaptDispatch::SetTopBottomScrollingMargins(const VTInt topMargin,
                                                 const VTInt bottomMargin)
{
    _DoSetTopBottomScrollingMargins(topMargin, bottomMargin, true);
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

    const auto page = _pages.ActivePage();
    const auto pageWidth = page.Width();
    // The default left margin is column 1
    if (actualLeft == 0)
    {
        actualLeft = 1;
    }
    // The default right margin is the page width
    if (actualRight == 0)
    {
        actualRight = pageWidth;
    }
    // The left margin must be less than the right margin, and the
    // right margin must be less than or equal to the buffer width
    if (actualLeft < actualRight && actualRight <= pageWidth)
    {
        if (actualLeft == 1 && actualRight == pageWidth)
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
void AdaptDispatch::SetLeftRightScrollingMargins(const VTInt leftMargin,
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
}

// Routine Description:
// - ENQ - Directs the terminal to send the answerback message.
// Arguments:
// - None
void AdaptDispatch::EnquireAnswerback()
{
    _api.ReturnAnswerback();
}

// Routine Description:
// - BEL - Rings the warning bell.
//    Causes the terminal to emit an audible tone of brief duration.
// Arguments:
// - None
void AdaptDispatch::WarningBell()
{
    _api.WarningBell();
}

// Routine Description:
// - CR - Performs a carriage return.
//    Moves the cursor to the leftmost column.
// Arguments:
// - None
void AdaptDispatch::CarriageReturn()
{
    _CursorMovePosition(Offset::Unchanged(), Offset::Absolute(1), true);
}

// Routine Description:
// - Helper method for executing a line feed, possibly preceded by carriage return.
// Arguments:
// - page - Target page on which the line feed is executed.
// - withReturn - Set to true if a carriage return should be performed as well.
// - wrapForced - Set to true is the line feed was the result of the line wrapping. if the viewport panned down. False if not.
bool AdaptDispatch::_DoLineFeed(const Page& page, const bool withReturn, const bool wrapForced)
{
    auto& textBuffer = page.Buffer();
    const auto pageWidth = page.Width();
    const auto bufferHeight = page.BufferHeight();
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(page, true);
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(pageWidth);
    auto viewportMoved = false;

    auto& cursor = page.Cursor();
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
        // bottom of the page.
        newPosition.y = std::min(currentPosition.y + 1, page.Bottom() - 1);
        newPosition = textBuffer.ClampPositionWithinLine(newPosition);
    }
    else if (topMargin > page.Top() || leftMargin > 0 || rightMargin < pageWidth - 1)
    {
        // If the top margin isn't at the top of the page, or the
        // horizontal margins are set, then we're just scrolling the margin
        // area and the cursor stays where it is.
        _ScrollRectVertically(page, { leftMargin, topMargin, rightMargin + 1, bottomMargin + 1 }, -1);
    }
    else if (page.Bottom() < bufferHeight)
    {
        // If the top margin is at the top of the page, then we'll scroll
        // the content up by panning the viewport down, and also move the cursor
        // down a row. But we only do this if the viewport hasn't yet reached
        // the end of the buffer.
        _api.SetViewportPosition({ page.XPanOffset(), page.Top() + 1 });
        newPosition.y++;
        viewportMoved = true;

        // And if the bottom margin didn't cover the full page, we copy the
        // lower part of the page down so it remains static. But for a full
        // pan we reset the newly revealed row with the erase attributes.
        if (bottomMargin < page.Bottom() - 1)
        {
            _ScrollRectVertically(page, { 0, bottomMargin + 1, pageWidth, page.Bottom() + 1 }, 1);
        }
        else
        {
            const auto eraseAttributes = _GetEraseAttributes(page);
            textBuffer.GetMutableRowByOffset(newPosition.y).Reset(eraseAttributes);
        }
    }
    else
    {
        // If the viewport has reached the end of the buffer, we can't pan down,
        // so we cycle the row coordinates, which effectively scrolls the buffer
        // content up. In this case we don't need to move the cursor down.
        const auto eraseAttributes = _GetEraseAttributes(page);
        textBuffer.IncrementCircularBuffer(eraseAttributes);
        _api.NotifyBufferRotation(1);

        // We trigger a scroll rather than a redraw, since that's more efficient,
        // but we need to turn the cursor off before doing so, otherwise a ghost
        // cursor can be left behind in the previous position.
        cursor.SetIsOn(false);
        textBuffer.TriggerScroll({ 0, -1 });

        // And again, if the bottom margin didn't cover the full page, we
        // copy the lower part of the page down so it remains static.
        if (bottomMargin < page.Bottom() - 1)
        {
            _ScrollRectVertically(page, { 0, bottomMargin, pageWidth, bufferHeight }, 1);
        }
    }

    cursor.SetPosition(newPosition);
    _ApplyCursorMovementFlags(cursor);
    return viewportMoved;
}

// Routine Description:
// - IND/NEL - Performs a line feed, possibly preceded by carriage return.
//    Moves the cursor down one line, and possibly also to the leftmost column.
// Arguments:
// - lineFeedType - Specify whether a carriage return should be performed as well.
void AdaptDispatch::LineFeed(const DispatchTypes::LineFeedType lineFeedType)
{
    const auto page = _pages.ActivePage();
    switch (lineFeedType)
    {
    case DispatchTypes::LineFeedType::DependsOnMode:
        _DoLineFeed(page, _api.GetSystemMode(ITerminalApi::Mode::LineFeed), false);
        break;
    case DispatchTypes::LineFeedType::WithoutReturn:
        _DoLineFeed(page, false, false);
        break;
    case DispatchTypes::LineFeedType::WithReturn:
        _DoLineFeed(page, true, false);
        break;
    default:
        break;
    }
}

// Routine Description:
// - RI - Performs a "Reverse line feed", essentially, the opposite of '\n'.
//    Moves the cursor up one line, and tries to keep its position in the line
// Arguments:
// - None
void AdaptDispatch::ReverseLineFeed()
{
    const auto page = _pages.ActivePage();
    const auto& textBuffer = page.Buffer();
    auto& cursor = page.Cursor();
    const auto cursorPosition = cursor.GetPosition();
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(page.Width());
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(page, true);

    // If the cursor is at the top of the margin area, we shift the buffer
    // contents down, to emulate inserting a line at that point.
    if (cursorPosition.y == topMargin && cursorPosition.x >= leftMargin && cursorPosition.x <= rightMargin)
    {
        _ScrollRectVertically(page, { leftMargin, topMargin, rightMargin + 1, bottomMargin + 1 }, 1);
    }
    else if (cursorPosition.y > page.Top())
    {
        // Otherwise we move the cursor up, but not past the top of the page.
        cursor.SetPosition(textBuffer.ClampPositionWithinLine({ cursorPosition.x, cursorPosition.y - 1 }));
        _ApplyCursorMovementFlags(cursor);
    }
}

// Routine Description:
// - DECBI - Moves the cursor back one column, scrolling the screen
//    horizontally if it reaches the left margin.
// Arguments:
// - None
void AdaptDispatch::BackIndex()
{
    const auto page = _pages.ActivePage();
    auto& cursor = page.Cursor();
    const auto cursorPosition = cursor.GetPosition();
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(page.Width());
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(page, true);

    // If the cursor is at the left of the margin area, we shift the buffer right.
    if (cursorPosition.x == leftMargin && cursorPosition.y >= topMargin && cursorPosition.y <= bottomMargin)
    {
        _ScrollRectHorizontally(page, { leftMargin, topMargin, rightMargin + 1, bottomMargin + 1 }, 1);
    }
    // Otherwise we move the cursor left, but not past the start of the line.
    else if (cursorPosition.x > 0)
    {
        cursor.SetXPosition(cursorPosition.x - 1);
        _ApplyCursorMovementFlags(cursor);
    }
}

// Routine Description:
// - DECFI - Moves the cursor forward one column, scrolling the screen
//    horizontally if it reaches the right margin.
// Arguments:
// - None
void AdaptDispatch::ForwardIndex()
{
    const auto page = _pages.ActivePage();
    auto& cursor = page.Cursor();
    const auto cursorPosition = cursor.GetPosition();
    const auto [leftMargin, rightMargin] = _GetHorizontalMargins(page.Width());
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(page, true);

    // If the cursor is at the right of the margin area, we shift the buffer left.
    if (cursorPosition.x == rightMargin && cursorPosition.y >= topMargin && cursorPosition.y <= bottomMargin)
    {
        _ScrollRectHorizontally(page, { leftMargin, topMargin, rightMargin + 1, bottomMargin + 1 }, -1);
    }
    // Otherwise we move the cursor right, but not past the end of the line.
    else if (cursorPosition.x < page.Buffer().GetLineWidth(cursorPosition.y) - 1)
    {
        cursor.SetXPosition(cursorPosition.x + 1);
        _ApplyCursorMovementFlags(cursor);
    }
}

// Routine Description:
// - OSC Set Window Title - Sets the title of the window
// Arguments:
// - title - The string to set the title to.
void AdaptDispatch::SetWindowTitle(std::wstring_view title)
{
    _api.SetWindowTitle(title);
}

//Routine Description:
// HTS - sets a VT tab stop in the cursor's current column.
//Arguments:
// - None
void AdaptDispatch::HorizontalTabSet()
{
    const auto page = _pages.ActivePage();
    const auto column = page.Cursor().GetPosition().x;

    _InitTabStopsForWidth(page.Width());
    _tabStopColumns.at(column) = true;
}

//Routine Description:
// CHT - performing a forwards tab. This will take the
//     cursor to the tab stop following its current location. If there are no
//     more tabs in this row, it will take it to the right side of the window.
//     If it's already in the last column of the row, it will move it to the next line.
//Arguments:
// - numTabs - the number of tabs to perform
void AdaptDispatch::ForwardTab(const VTInt numTabs)
{
    const auto page = _pages.ActivePage();
    auto& cursor = page.Cursor();
    auto column = cursor.GetPosition().x;
    const auto row = cursor.GetPosition().y;
    const auto width = page.Buffer().GetLineWidth(row);
    auto tabsPerformed = 0;

    const auto [topMargin, bottomMargin] = _GetVerticalMargins(page, true);
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
}

//Routine Description:
// CBT - performing a backwards tab. This will take the cursor to the tab stop
//     previous to its current location. It will not reverse line feed.
//Arguments:
// - numTabs - the number of tabs to perform
void AdaptDispatch::BackwardsTab(const VTInt numTabs)
{
    const auto page = _pages.ActivePage();
    auto& cursor = page.Cursor();
    auto column = cursor.GetPosition().x;
    const auto row = cursor.GetPosition().y;
    const auto width = page.Buffer().GetLineWidth(row);
    auto tabsPerformed = 0;

    const auto [topMargin, bottomMargin] = _GetVerticalMargins(page, true);
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
}

//Routine Description:
// TBC - Used to clear set tab stops. ClearType ClearCurrentColumn (0) results
//     in clearing only the tab stop in the cursor's current column, if there
//     is one. ClearAllColumns (3) results in resetting all set tab stops.
//Arguments:
// - clearType - Whether to clear the current column, or all columns, defined in DispatchTypes::TabClearType
void AdaptDispatch::TabClear(const DispatchTypes::TabClearType clearType)
{
    switch (clearType)
    {
    case DispatchTypes::TabClearType::ClearCurrentColumn:
        _ClearSingleTabStop();
        break;
    case DispatchTypes::TabClearType::ClearAllColumns:
        _ClearAllTabStops();
        break;
    default:
        break;
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
    const auto page = _pages.ActivePage();
    const auto column = page.Cursor().GetPosition().x;

    _InitTabStopsForWidth(page.Width());
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
void AdaptDispatch::TabSet(const VTParameter setType) noexcept
{
    constexpr auto SetEvery8Columns = DispatchTypes::TabSetType::SetEvery8Columns;
    if (setType.value_or(SetEvery8Columns) == SetEvery8Columns)
    {
        _tabStopColumns.clear();
        _initDefaultTabStops = true;
    }
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
void AdaptDispatch::DesignateCodingSystem(const VTID codingSystem)
{
    switch (codingSystem)
    {
    case DispatchTypes::CodingSystem::ISO2022:
        _api.SetCodePage(28591);
        AcceptC1Controls(true);
        _termOutput.EnableGrTranslation(true);
        break;
    case DispatchTypes::CodingSystem::UTF8:
        _api.SetCodePage(CP_UTF8);
        AcceptC1Controls(false);
        _termOutput.EnableGrTranslation(false);
        break;
    default:
        break;
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
void AdaptDispatch::Designate94Charset(const VTInt gsetNumber, const VTID charset)
{
    _termOutput.Designate94Charset(gsetNumber, charset);
}

//Routine Description:
// Designate Charset - Selects a specific 96-character set into one of the four G-sets.
//     See http://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Controls-beginning-with-ESC
//       for a list of all charsets and their codes.
//     If the specified charset is unsupported, we do nothing (remain on the current one)
//Arguments:
// - gsetNumber - The G-set into which the charset will be selected.
// - charset - The identifier indicating the charset that will be used.
void AdaptDispatch::Designate96Charset(const VTInt gsetNumber, const VTID charset)
{
    _termOutput.Designate96Charset(gsetNumber, charset);
}

//Routine Description:
// Locking Shift - Invoke one of the G-sets into the left half of the code table.
//Arguments:
// - gsetNumber - The G-set that will be invoked.
void AdaptDispatch::LockingShift(const VTInt gsetNumber)
{
    _termOutput.LockingShift(gsetNumber);
}

//Routine Description:
// Locking Shift Right - Invoke one of the G-sets into the right half of the code table.
//Arguments:
// - gsetNumber - The G-set that will be invoked.
void AdaptDispatch::LockingShiftRight(const VTInt gsetNumber)
{
    _termOutput.LockingShiftRight(gsetNumber);
}

//Routine Description:
// Single Shift - Temporarily invoke one of the G-sets into the code table.
//Arguments:
// - gsetNumber - The G-set that will be invoked.
void AdaptDispatch::SingleShift(const VTInt gsetNumber) noexcept
{
    _termOutput.SingleShift(gsetNumber);
}

//Routine Description:
// DECAC1 - Enable or disable the reception of C1 control codes in the parser.
//Arguments:
// - enabled - true to allow C1 controls to be used, false to disallow.
void AdaptDispatch::AcceptC1Controls(const bool enabled)
{
    _api.GetStateMachine().SetParserMode(StateMachine::Mode::AcceptC1, enabled);
}

//Routine Description:
// S8C1T/S7C1T - Enable or disable the sending of C1 controls in key sequences
//   and query responses. When this is enabled, C1 controls are sent as a single
//   codepoint. When disabled, they're sent as a two character escape sequence.
//Arguments:
// - enabled - true to send C1 controls, false to send escape sequences.
void AdaptDispatch::SendC1Controls(const bool enabled)
{
    // If this is an attempt to enable C1 controls, the input code page must be
    // one of the DOCS choices (UTF-8 or ISO-8859-1), otherwise there's a risk
    // that those controls won't have a valid encoding.
    const auto codepage = _api.GetInputCodePage();
    if (enabled == false || codepage == CP_UTF8 || codepage == 28591)
    {
        _terminalInput.SetInputMode(TerminalInput::Mode::SendC1, enabled);
    }
}

//Routine Description:
// ACS - Announces the ANSI conformance level for subsequent data exchange.
//  This requires certain character sets to be mapped into the terminal's
//  G-sets and in-use tables.
//Arguments:
// - ansiLevel - the expected conformance level
void AdaptDispatch::AnnounceCodeStructure(const VTInt ansiLevel)
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
        break;
    default:
        break;
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
void AdaptDispatch::SoftReset()
{
    _pages.ActivePage().Cursor().SetIsVisible(true); // Cursor enabled.

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

    // Soft reset the Sixel parser if in use.
    if (_sixelParser)
    {
        _sixelParser->SoftReset();
    }
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
void AdaptDispatch::HardReset()
{
    // If in the alt buffer, switch back to main before doing anything else.
    if (_usingAltBuffer)
    {
        _api.UseMainScreenBuffer();
        _usingAltBuffer = false;
    }

    // Reset all page buffers.
    _pages.Reset();

    // Reset the Sixel parser.
    _sixelParser = nullptr;

    // Completely reset the TerminalOutput state.
    _termOutput = {};
    // Reset the code page to the default value.
    _api.ResetCodePage();
    // Disable parsing and sending of C1 control codes.
    AcceptC1Controls(false);
    SendC1Controls(false);

    // Sets the SGR state to normal - this must be done before EraseInDisplay
    //      to ensure that it clears with the default background color.
    SoftReset();

    // Clears the screen - Needs to be done in two operations.
    EraseInDisplay(DispatchTypes::EraseType::All);
    EraseInDisplay(DispatchTypes::EraseType::Scrollback);

    // Set the color table and render modes back to their initial startup values.
    _renderSettings.RestoreDefaultSettings();
    // Let the renderer know that the background and frame colors may have changed.
    if (_renderer)
    {
        _renderer->TriggerRedrawAll(true, true);
    }

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
    _pages.ActivePage().Cursor().SetBlinkingAllowed(true);

    // Delete all current tab stops and reapply
    TabSet(DispatchTypes::TabSetType::SetEvery8Columns);

    // Clear the soft font in the renderer and delete the font buffer.
    if (_renderer)
    {
        _renderer->UpdateSoftFont({}, {}, false);
    }
    _fontBuffer = nullptr;

    // Reset internal modes to their initial state
    _modes = { Mode::PageCursorCoupling };

    // Clear and release the macro buffer.
    if (_macroBuffer)
    {
        _macroBuffer->ClearMacrosIfInUse();
        _macroBuffer = nullptr;
    }

    // A hard reset will disable all the modes that ConPTY relies on,
    // so let it know that it needs to re-enable those modes.
    _api.GetStateMachine().InjectSequence(InjectionType::RIS);
}

// Routine Description:
// - DECALN - Fills the entire screen with a test pattern of uppercase Es,
//    resets the margins and rendition attributes, and moves the cursor to
//    the home position.
// Arguments:
// - None
void AdaptDispatch::ScreenAlignmentPattern()
{
    const auto page = _pages.ActivePage();

    // Fill the screen with the letter E using the default attributes.
    _FillRect(page, { 0, page.Top(), page.Width(), page.Bottom() }, L"E", {});
    // Reset the line rendition for all of these rows.
    page.Buffer().ResetLineRenditionRange(page.Top(), page.Bottom());
    // Reset the meta/extended attributes (but leave the colors unchanged).
    auto attr = page.Attributes();
    attr.SetStandardErase();
    page.SetAttributes(attr);
    // Reset the origin mode to absolute, and disallow left/right margins.
    _modes.reset(Mode::Origin, Mode::AllowDECSLRM);
    // Clear the scrolling margins.
    _DoSetTopBottomScrollingMargins(0, 0);
    _DoSetLeftRightScrollingMargins(0, 0);
    // Set the cursor position to home.
    CursorPosition(1, 1);
}

//Routine Description:
//  - Erase Scrollback (^[[3J - ED extension by xterm)
//    Because conhost doesn't exactly have a scrollback, We have to be tricky here.
//    We need to move the entire page to 0,0, and clear everything outside
//      (0, 0, pageWidth, pageHeight) To give the appearance that
//      everything above the viewport was cleared.
//    We don't want to save the text BELOW the viewport, because in *nix, there isn't anything there
//      (There isn't a scroll-forward, only a scrollback)
// Arguments:
// - <none>
void AdaptDispatch::_EraseScrollback()
{
    const auto page = _pages.VisiblePage();
    auto& cursor = page.Cursor();
    const auto row = cursor.GetPosition().y;

    page.Buffer().ClearScrollback(page.Top(), page.Height());
    // Move the viewport
    _api.SetViewportPosition({ page.XPanOffset(), 0 });
    // Move the cursor to the same relative location.
    cursor.SetYPosition(row - page.Top());
    cursor.SetHasMoved(true);
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
void AdaptDispatch::_EraseAll()
{
    const auto page = _pages.ActivePage();
    const auto pageWidth = page.Width();
    const auto pageHeight = page.Height();
    const auto bufferHeight = page.BufferHeight();
    auto& textBuffer = page.Buffer();

    // Stash away the current position of the cursor within the page.
    // We'll need to restore the cursor to that same relative position, after
    //      we move the viewport.
    auto& cursor = page.Cursor();
    const auto row = cursor.GetPosition().y - page.Top();

    // Calculate new page position. Typically we want to move one line below
    // the last non-space row, but if the last non-space character is the very
    // start of the buffer, then we shouldn't move down at all.
    const auto lastChar = textBuffer.GetLastNonSpaceCharacter();
    auto newPageTop = lastChar == til::point{} ? 0 : lastChar.y + 1;
    auto newPageBottom = newPageTop + pageHeight;
    const auto delta = newPageBottom - bufferHeight;
    if (delta > 0)
    {
        for (auto i = 0; i < delta; i++)
        {
            textBuffer.IncrementCircularBuffer();
        }
        _api.NotifyBufferRotation(delta);
        newPageTop -= delta;
        newPageBottom -= delta;
        textBuffer.TriggerScroll({ 0, -delta });
    }
    // Move the viewport if necessary.
    if (newPageTop != page.Top())
    {
        _api.SetViewportPosition({ page.XPanOffset(), newPageTop });
    }
    // Restore the relative cursor position
    cursor.SetYPosition(row + newPageTop);
    cursor.SetHasMoved(true);

    // Erase all the rows in the current page.
    const auto eraseAttributes = _GetEraseAttributes(page);
    _FillRect(page, { 0, newPageTop, pageWidth, newPageBottom }, whitespace, eraseAttributes);

    // Also reset the line rendition for the erased rows.
    textBuffer.ResetLineRenditionRange(newPageTop, newPageBottom);
}

//Routine Description:
// Set Cursor Style - Changes the cursor's style to match the given Dispatch
//      cursor style. Unix styles are a combination of the shape and the blinking state.
//Arguments:
// - cursorStyle - The unix-like cursor style to apply to the cursor
void AdaptDispatch::SetCursorStyle(const DispatchTypes::CursorStyle cursorStyle)
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
        return;
    }

    auto& cursor = _pages.ActivePage().Cursor();
    cursor.SetType(actualType);
    cursor.SetBlinkingAllowed(fEnableBlinking);
}

// Routine Description:
// - OSC Copy to Clipboard
// Arguments:
// - content - The content to copy to clipboard. Must be null terminated.
void AdaptDispatch::SetClipboard(const wil::zwstring_view content)
{
    _api.CopyToClipboard(content);
}

// Method Description:
// - Sets a single entry of the colortable to a new value
// Arguments:
// - tableIndex: The VT color table index
// - dwColor: The new RGB color value to use.
void AdaptDispatch::SetColorTableEntry(const size_t tableIndex, const DWORD dwColor)
{
    _renderSettings.SetColorTableEntry(tableIndex, dwColor);

    if (_renderer)
    {
        // If we're updating the background color, we need to let the renderer
        // know, since it may want to repaint the window background to match.
        const auto backgroundIndex = _renderSettings.GetColorAliasIndex(ColorAlias::DefaultBackground);
        const auto backgroundChanged = (tableIndex == backgroundIndex);

        // Similarly for the frame color, the tab may need to be repainted.
        const auto frameIndex = _renderSettings.GetColorAliasIndex(ColorAlias::FrameBackground);
        const auto frameChanged = (tableIndex == frameIndex);

        _renderer->TriggerRedrawAll(backgroundChanged, frameChanged);
    }
}

void AdaptDispatch::RequestColorTableEntry(const size_t tableIndex)
{
    const auto color = _renderSettings.GetColorTableEntry(tableIndex);
    if (color != INVALID_COLOR)
    {
        const til::color c{ color };
        // Scale values up to match xterm's 16-bit color report format.
        _ReturnOscResponse(fmt::format(FMT_COMPILE(L"4;{};rgb:{:04x}/{:04x}/{:04x}"), tableIndex, c.r * 0x0101, c.g * 0x0101, c.b * 0x0101));
    }
}

// Method Description:
// - Sets one Xterm Color Resource such as Default Foreground, Background, Cursor
void AdaptDispatch::SetXtermColorResource(const size_t resource, const DWORD color)
{
    assert(resource >= 10);
    const auto mappingIndex = resource - 10;
    const auto& oscMapping = XtermResourceColorTableMappings.at(mappingIndex);
    if (oscMapping.ColorTableIndex > 0)
    {
        if (oscMapping.AliasIndex >= 0) [[unlikely]]
        {
            // If this color change applies to an aliased color, point the alias at the new color
            _renderSettings.SetColorAliasIndex(static_cast<ColorAlias>(oscMapping.AliasIndex), oscMapping.ColorTableIndex);
        }
        return SetColorTableEntry(oscMapping.ColorTableIndex, color);
    }
}

// Method Description:
// - Reports the value of one Xterm Color Resource, if it is set.
// Return Value:
// True if handled successfully. False otherwise.
void AdaptDispatch::RequestXtermColorResource(const size_t resource)
{
    assert(resource >= 10);
    const auto mappingIndex = resource - 10;
    const auto& oscMapping = XtermResourceColorTableMappings.at(mappingIndex);
    if (oscMapping.ColorTableIndex > 0)
    {
        size_t finalColorIndex = oscMapping.ColorTableIndex;

        if (oscMapping.AliasIndex >= 0) [[unlikely]]
        {
            finalColorIndex = _renderSettings.GetColorAliasIndex(static_cast<ColorAlias>(oscMapping.AliasIndex));
        }

        const auto color = _renderSettings.GetColorTableEntry(finalColorIndex);
        if (color != INVALID_COLOR)
        {
            const til::color c{ color };
            // Scale values up to match xterm's 16-bit color report format.
            _ReturnOscResponse(fmt::format(FMT_COMPILE(L"{};rgb:{:04x}/{:04x}/{:04x}"), resource, c.r * 0x0101, c.g * 0x0101, c.b * 0x0101));
        }
    }
}

// Method Description:
// DECAC - Assigns the foreground and background color indexes that should be
//   used for a given aspect of the user interface.
// Arguments:
// - item: The aspect of the interface that will have its colors altered.
// - fgIndex: The color table index to be used for the foreground.
// - bgIndex: The color table index to be used for the background.
void AdaptDispatch::AssignColor(const DispatchTypes::ColorItem item, const VTInt fgIndex, const VTInt bgIndex)
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
        return;
    }

    if (_renderer)
    {
        const auto backgroundChanged = item == DispatchTypes::ColorItem::NormalText;
        const auto frameChanged = item == DispatchTypes::ColorItem::WindowFrame;
        _renderer->TriggerRedrawAll(backgroundChanged, frameChanged);
    }
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
void AdaptDispatch::WindowManipulation(const DispatchTypes::WindowManipulationType function,
                                       const VTParameter parameter1,
                                       const VTParameter parameter2)
{
    // Other Window Manipulation functions:
    //  MSFT:13271098 - QueryViewport
    //  MSFT:13271146 - QueryScreenSize

    const auto reportSize = [&](const auto size) {
        const auto reportType = function - 10;
        _ReturnCsiResponse(fmt::format(FMT_COMPILE(L"{};{};{}t"), reportType, size.height, size.width));
    };

    switch (function)
    {
    case DispatchTypes::WindowManipulationType::DeIconifyWindow:
        _api.ShowWindow(true);
        break;
    case DispatchTypes::WindowManipulationType::IconifyWindow:
        _api.ShowWindow(false);
        break;
    case DispatchTypes::WindowManipulationType::RefreshWindow:
        _pages.VisiblePage().Buffer().TriggerRedrawAll();
        break;
    case DispatchTypes::WindowManipulationType::ResizeWindowInCharacters:
        _api.ResizeWindow(parameter2.value_or(0), parameter1.value_or(0));
        break;
    case DispatchTypes::WindowManipulationType::ReportTextSizeInCharacters:
        reportSize(_pages.VisiblePage().Size());
        break;
    case DispatchTypes::WindowManipulationType::ReportTextSizeInPixels:
        // Prior to the existence of the character cell size query, Sixel applications
        // that wanted to know the cell size would request the text area in pixels and
        // divide that by the text area in characters. But for this to work, we need to
        // return the virtual pixel size, as used in the Sixel graphics emulation, and
        // not the physical pixel size (which should be of no concern to applications).
        reportSize(_pages.VisiblePage().Size() * SixelParser::CellSizeForLevel());
        break;
    case DispatchTypes::WindowManipulationType::ReportCharacterCellSize:
        reportSize(SixelParser::CellSizeForLevel());
        break;
    default:
        break;
    }
}

// Method Description:
// - Starts a hyperlink
// Arguments:
// - The hyperlink URI, optional additional parameters
void AdaptDispatch::AddHyperlink(const std::wstring_view uri, const std::wstring_view params)
{
    const auto page = _pages.ActivePage();
    auto attr = page.Attributes();
    const auto id = page.Buffer().GetHyperlinkId(uri, params);
    attr.SetHyperlinkId(id);
    page.SetAttributes(attr);
    page.Buffer().AddHyperlinkToMap(uri, id);
}

// Method Description:
// - Ends a hyperlink
void AdaptDispatch::EndHyperlink()
{
    const auto page = _pages.ActivePage();
    auto attr = page.Attributes();
    attr.SetHyperlinkId(0);
    page.SetAttributes(attr);
}

// Method Description:
// - Performs a ConEmu action
//   Currently, the only actions we support are setting the taskbar state/progress
//   and setting the working directory.
// Arguments:
// - string - contains the parameters that define which action we do
void AdaptDispatch::DoConEmuAction(const std::wstring_view string)
{
    constexpr size_t TaskbarMaxState{ 4 };
    constexpr size_t TaskbarMaxProgress{ 100 };

    unsigned int state = 0;
    unsigned int progress = 0;

    const auto parts = Utils::SplitString(string, L';');
    unsigned int subParam = 0;

    if (parts.size() < 1 || !Utils::StringToUint(til::at(parts, 0), subParam))
    {
        return;
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
                return;
            }
            if (parts.size() >= 3)
            {
                // A progress parameter is also defined, parse it out
                const auto progressSuccess = Utils::StringToUint(til::at(parts, 2), progress);
                if (!progressSuccess && !til::at(parts, 2).empty())
                {
                    return;
                }
            }
        }

        if (state > TaskbarMaxState)
        {
            // state is out of bounds, return false
            return;
        }
        if (progress > TaskbarMaxProgress)
        {
            // progress is greater than the maximum allowed value, clamp it to the max
            progress = TaskbarMaxProgress;
        }
        _api.SetTaskbarProgress(static_cast<DispatchTypes::TaskbarState>(state), progress);
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
                return;
            }

            _api.SetWorkingDirectory(path);
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
        _pages.ActivePage().Buffer().StartCommand();
    }
}

// Method Description:
// - Performs a iTerm2 action
// - Ascribes to the ITermDispatch interface
// - Currently, the actions we support are:
//   * `OSC1337;SetMark`: mark a line as a prompt line
// - Not actually used in conhost
// Arguments:
// - string: contains the parameters that define which action we do
void AdaptDispatch::DoITerm2Action(const std::wstring_view string)
{
    if constexpr (!Feature_ScrollbarMarks::IsEnabled())
    {
        return;
    }

    const auto parts = Utils::SplitString(string, L';');

    if (parts.size() < 1)
    {
        return;
    }

    const auto action = til::at(parts, 0);

    if (action == L"SetMark")
    {
        _pages.ActivePage().Buffer().StartPrompt();
    }
}

// Method Description:
// - Performs a FinalTerm action
// - Currently, the actions we support are:
//   * `OSC133;A`: mark a line as a prompt line
// - Not actually used in conhost
// - The remainder of the FTCS prompt sequences are tracked in GH#11000
// Arguments:
// - string: contains the parameters that define which action we do
void AdaptDispatch::DoFinalTermAction(const std::wstring_view string)
{
    if constexpr (!Feature_ScrollbarMarks::IsEnabled())
    {
        return;
    }

    const auto parts = Utils::SplitString(string, L';');

    if (parts.size() < 1)
    {
        return;
    }

    const auto action = til::at(parts, 0);
    if (action.size() == 1)
    {
        switch (til::at(action, 0))
        {
        case L'A': // FTCS_PROMPT
        {
            _pages.ActivePage().Buffer().StartPrompt();
            break;
        }
        case L'B': // FTCS_COMMAND_START
        {
            _pages.ActivePage().Buffer().StartCommand();
            break;
        }
        case L'C': // FTCS_COMMAND_EXECUTED
        {
            _pages.ActivePage().Buffer().StartOutput();
            break;
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

            _pages.ActivePage().Buffer().EndCurrentCommand(error);

            break;
        }
        default:
            break;
        }
    }

    // When we add the rest of the FTCS sequences (GH#11000), we should add a
    // simple state machine here to track the most recently emitted mark from
    // this set of sequences, and which sequence was emitted last, so we can
    // modify the state of that mark as we go.
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
void AdaptDispatch::DoVsCodeAction(const std::wstring_view string)
{
    if constexpr (!Feature_ShellCompletions::IsEnabled())
    {
        return;
    }

    const auto parts = Utils::SplitString(string, L';');

    if (parts.size() < 1)
    {
        return;
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
                return;
            }
            // Get the remainder of the string
            const auto remainder = string.substr(prefixLength);

            _api.InvokeCompletions(parts.size() < 5 ? L"" : remainder,
                                   replacementLength);
        }

        // If it's poorly formatted, just eat it
    }
}

// Method Description:
// - Performs a Windows Terminal action
// - Currently, the actions we support are:
//   * CmdNotFound: A protocol for passing commands that the shell couldn't resolve
//     to the terminal. The command is then shared with WinGet to see if it can
//     find a package that provides that command, which is then displayed to the
//     user.
// - Not actually used in conhost
// Arguments:
// - string: contains the parameters that define which action we do
void AdaptDispatch::DoWTAction(const std::wstring_view string)
{
    const auto parts = Utils::SplitString(string, L';');

    if (parts.size() < 1)
    {
        return;
    }

    const auto action = til::at(parts, 0);

    if (action == L"CmdNotFound")
    {
        // The structure of the message is as follows:
        // `e]9001;
        // 0:     CmdNotFound;
        // 1:     $($cmdNotFound.missingCmd);
        if (parts.size() >= 2)
        {
            const std::wstring_view missingCmd = til::at(parts, 1);
            _api.SearchMissingCommand(missingCmd);
        }
    }
}

// Method Description:
// - SIXEL - Defines an image transmitted in sixel format via the returned
//   StringHandler function.
// Arguments:
// - macroParameter - Selects one a of set of predefined aspect ratios.
// - backgroundSelect - Whether the background should be transparent or opaque.
// - backgroundColor - The color number used for the background (VT240).
// Return Value:
// - a function to receive the pixel data or nullptr if parameters are invalid
ITermDispatch::StringHandler AdaptDispatch::DefineSixelImage(const VTInt macroParameter,
                                                             const DispatchTypes::SixelBackground backgroundSelect,
                                                             const VTParameter backgroundColor)
{
    // The sixel parser is created on demand.
    if (!_sixelParser)
    {
        _sixelParser = std::make_unique<SixelParser>(*this, _api.GetStateMachine());
        _sixelParser->SetDisplayMode(_modes.test(Mode::SixelDisplay));
    }
    return _sixelParser->DefineImage(macroParameter, backgroundSelect, backgroundColor);
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

    return [=](const auto ch) {
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
            if (_renderer)
            {
                const auto bitPattern = _fontBuffer->GetBitPattern();
                const auto cellSize = _fontBuffer->GetCellSize();
                const auto centeringHint = _fontBuffer->GetTextCenteringHint();
                _renderer->UpdateSoftFont(bitPattern, cellSize, centeringHint);
            }
        }
        return true;
    };
}

// Method Description:
// - DECRQUPSS - Request the user-preference supplemental character set.
// Arguments:
// - None
void AdaptDispatch::RequestUserPreferenceCharset()
{
    const auto size = _termOutput.GetUserPreferenceCharsetSize();
    const auto id = _termOutput.GetUserPreferenceCharsetId();
    _ReturnDcsResponse(fmt::format(FMT_COMPILE(L"{}!u{}"), (size == 96 ? 1 : 0), id));
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
void AdaptDispatch::InvokeMacro(const VTInt macroId)
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
}

// Routine Description:
// - DECRQTSR - Queries the state of the terminal. This can either be a terminal
//   state report, generally covering all settable state in the terminal (with
//   the exception of large data items), or a color table report.
// Arguments:
// - format - the format of the report being requested.
// - formatOption - a format-specific option.
void AdaptDispatch::RequestTerminalStateReport(const DispatchTypes::ReportFormat format, const VTParameter formatOption)
{
    switch (format)
    {
    case DispatchTypes::ReportFormat::ColorTableReport:
        _ReportColorTable(formatOption);
        break;
    default:
        break;
    }
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
// - DECCTR - Returns the Color Table Report in response to a DECRQTSR query.
// Arguments:
// - colorModel - the color model to use in the report (1 = HLS, 2 = RGB).
// Return Value:
// - None
void AdaptDispatch::_ReportColorTable(const DispatchTypes::ColorModel colorModel) const
{
    using namespace std::string_view_literals;

    // A valid response always starts with 2 $ s.
    fmt::basic_memory_buffer<wchar_t, TextColor::TABLE_SIZE * 18> response;
    response.append(L"2$s"sv);

    const auto modelNumber = static_cast<int>(colorModel);
    for (size_t colorNumber = 0; colorNumber < TextColor::TABLE_SIZE; colorNumber++)
    {
        const auto color = _renderSettings.GetColorTableEntry(colorNumber);
        if (color != INVALID_COLOR)
        {
            response.append(colorNumber > 0 ? L"/"sv : L""sv);
            auto x = 0, y = 0, z = 0;
            switch (colorModel)
            {
            case DispatchTypes::ColorModel::HLS:
                std::tie(x, y, z) = Utils::ColorToHLS(color);
                break;
            case DispatchTypes::ColorModel::RGB:
                std::tie(x, y, z) = Utils::ColorToRGB100(color);
                break;
            default:
                return;
            }
            fmt::format_to(std::back_inserter(response), FMT_COMPILE(L"{};{};{};{};{}"), colorNumber, modelNumber, x, y, z);
        }
    }

    _ReturnDcsResponse({ response.data(), response.size() });
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
            case VTID(" q"):
                _ReportDECSCUSRSetting();
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
                _ReturnDcsResponse(L"0$r");
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

    // A valid response always starts with 1 $ r.
    // Then the '0' parameter is to reset the SGR attributes to the defaults.
    fmt::basic_memory_buffer<wchar_t, 64> response;
    response.append(L"1$r0"sv);

    const auto& attr = _pages.ActivePage().Attributes();
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

    // The 'm' indicates this is an SGR response.
    response.append(L"m"sv);
    _ReturnDcsResponse({ response.data(), response.size() });
}

// Method Description:
// - Reports the DECSTBM margin range in response to a DECRQSS query.
// Arguments:
// - None
// Return Value:
// - None
void AdaptDispatch::_ReportDECSTBMSetting()
{
    const auto page = _pages.ActivePage();
    const auto [marginTop, marginBottom] = _GetVerticalMargins(page, false);
    // A valid response always starts with 1 $ r and the final 'r' indicates this is a DECSTBM response.
    // VT origin is at 1,1 so we need to add 1 to these margins.
    _ReturnDcsResponse(fmt::format(FMT_COMPILE(L"1$r{};{}r"), marginTop + 1, marginBottom + 1));
}

// Method Description:
// - Reports the DECSLRM margin range in response to a DECRQSS query.
// Arguments:
// - None
// Return Value:
// - None
void AdaptDispatch::_ReportDECSLRMSetting()
{
    const auto pageWidth = _pages.ActivePage().Width();
    const auto [marginLeft, marginRight] = _GetHorizontalMargins(pageWidth);
    // A valid response always starts with 1 $ r and the 's' indicates this is a DECSLRM response.
    // VT origin is at 1,1 so we need to add 1 to these margins.
    _ReturnDcsResponse(fmt::format(FMT_COMPILE(L"1$r{};{}s"), marginLeft + 1, marginRight + 1));
}

// Method Description:
// - Reports the DECSCUSR cursor style in response to a DECRQSS query.
// Arguments:
// - None
// Return Value:
// - None
void AdaptDispatch::_ReportDECSCUSRSetting() const
{
    const auto& cursor = _pages.ActivePage().Cursor();
    const auto blinking = cursor.IsBlinkingAllowed();
    // A valid response always starts with 1 $ r. This is followed by a
    // number from 1 to 6 representing the cursor style. The ' q' indicates
    // this is a DECSCUSR response.
    switch (cursor.GetType())
    {
    case CursorType::FullBox:
        _ReturnDcsResponse(blinking ? L"1$r1 q" : L"1$r2 q");
        break;
    case CursorType::Underscore:
        _ReturnDcsResponse(blinking ? L"1$r3 q" : L"1$r4 q");
        break;
    case CursorType::VerticalBar:
        _ReturnDcsResponse(blinking ? L"1$r5 q" : L"1$r6 q");
        break;
    default:
        // If we have a non-standard style, this is likely because it's the
        // user's chosen default style, so we report a default value of 0.
        // That way, if an application later tries to restore the cursor with
        // the returned value, it should be reset to its original state.
        _ReturnDcsResponse(L"1$r0 q");
        break;
    }
}

// Method Description:
// - Reports the DECSCA protected attribute in response to a DECRQSS query.
// Arguments:
// - None
// Return Value:
// - None
void AdaptDispatch::_ReportDECSCASetting() const
{
    const auto isProtected = _pages.ActivePage().Attributes().IsProtected();
    // A valid response always starts with 1 $ r. This is followed by '1' if the
    // protected attribute is set, or '0' if not. The '"q' indicates this is a
    // DECSCA response.
    _ReturnDcsResponse(isProtected ? L"1$r1\"q" : L"1$r0\"q");
}

// Method Description:
// - Reports the DECSACE change extent in response to a DECRQSS query.
// Arguments:
// - None
// Return Value:
// - None
void AdaptDispatch::_ReportDECSACESetting() const
{
    const auto rectangularExtent = _modes.test(Mode::RectangularChangeExtent);
    // A valid response always starts with 1 $ r. This is followed by '2' if
    // the extent is rectangular, or '1' if it's a stream. The '*x' indicates
    // this is a DECSACE response.
    _ReturnDcsResponse(rectangularExtent ? L"1$r2*x" : L"1$r1*x");
}

// Method Description:
// - Reports the DECAC color assignments in response to a DECRQSS query.
// Arguments:
// - None
// Return Value:
// - None
void AdaptDispatch::_ReportDECACSetting(const VTInt itemNumber) const
{
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
        _ReturnDcsResponse(L"0$r");
        return;
    }
    // A valid response always starts with 1 $ r and the ',|' indicates this is a DECAC response.
    _ReturnDcsResponse(fmt::format(FMT_COMPILE(L"1$r{};{};{},|"), itemNumber, fgIndex, bgIndex));
}

// Routine Description:
// - DECRQPSR - Queries the presentation state of the terminal. This can either
//   be in the form of a cursor information report, or a tabulation stop report,
//   depending on the requested format.
// Arguments:
// - format - the format of the report being requested.
void AdaptDispatch::RequestPresentationStateReport(const DispatchTypes::PresentationReportFormat format)
{
    switch (format)
    {
    case DispatchTypes::PresentationReportFormat::CursorInformationReport:
        _ReportCursorInformation();
        break;
    case DispatchTypes::PresentationReportFormat::TabulationStopReport:
        _ReportTabStops();
        break;
    default:
        break;
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
    const auto page = _pages.ActivePage();
    const auto& cursor = page.Cursor();
    const auto& attributes = page.Attributes();

    // First pull the cursor position relative to the entire buffer out of the console.
    til::point cursorPosition{ cursor.GetPosition() };

    // Now adjust it for its position in respect to the current page top.
    cursorPosition.y -= page.Top();

    // NOTE: 1,1 is the top-left corner of the page in VT-speak, so add 1.
    cursorPosition.x++;
    cursorPosition.y++;

    // If the origin mode is set, the cursor is relative to the margin origin.
    if (_modes.test(Mode::Origin))
    {
        cursorPosition.x -= _GetHorizontalMargins(page.Width()).first;
        cursorPosition.y -= _GetVerticalMargins(page, false).first;
    }

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

    // A valid response always starts with 1 $ u.
    const auto response = fmt::format(
        FMT_COMPILE(L"1$u{};{};{};{};{};{};{};{};{};{}{}{}{}"),
        cursorPosition.y,
        cursorPosition.x,
        page.Number(),
        renditionAttributes,
        characterAttributes,
        flags,
        leftSetNumber,
        rightSetNumber,
        charsetSizes,
        charset0,
        charset1,
        charset2,
        charset3);
    _ReturnDcsResponse({ response.data(), response.size() });
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
                    PagePositionAbsolute(state.value);
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
                    const auto page = _pages.ActivePage();
                    auto attr = page.Attributes();
                    attr.SetIntense(state.value & 1);
                    attr.SetUnderlineStyle(state.value & 2 ? UnderlineStyle::SinglyUnderlined : UnderlineStyle::NoUnderline);
                    attr.SetBlinking(state.value & 4);
                    attr.SetReverseVideo(state.value & 8);
                    attr.SetInvisible(state.value & 16);
                    page.SetAttributes(attr);
                }
                else if (state.field == Field::Attr)
                {
                    const auto page = _pages.ActivePage();
                    auto attr = page.Attributes();
                    attr.SetProtected(state.value & 1);
                    page.SetAttributes(attr);
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
                        const auto page = _pages.ActivePage();
                        page.Cursor().DelayEOLWrap();
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
    const auto width = _pages.ActivePage().Width();
    _InitTabStopsForWidth(width);

    using namespace std::string_view_literals;

    // A valid response always starts with 2 $ u.
    fmt::basic_memory_buffer<wchar_t, 64> response;
    response.append(L"2$u"sv);

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

    _ReturnDcsResponse({ response.data(), response.size() });
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
    const auto width = std::max(_pages.ActivePage().Width(), 132);
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

void AdaptDispatch::_ReturnCsiResponse(const std::wstring_view response) const
{
    const auto csi = _terminalInput.GetInputMode(TerminalInput::Mode::SendC1) ? L"\x9B" : L"\x1B[";
    _api.ReturnResponse(fmt::format(FMT_COMPILE(L"{}{}"), csi, response));
}

void AdaptDispatch::_ReturnDcsResponse(const std::wstring_view response) const
{
    const auto dcs = _terminalInput.GetInputMode(TerminalInput::Mode::SendC1) ? L"\x90" : L"\x1BP";
    const auto st = _terminalInput.GetInputMode(TerminalInput::Mode::SendC1) ? L"\x9C" : L"\x1B\\";
    _api.ReturnResponse(fmt::format(FMT_COMPILE(L"{}{}{}"), dcs, response, st));
}

void AdaptDispatch::_ReturnOscResponse(const std::wstring_view response) const
{
    const auto osc = _terminalInput.GetInputMode(TerminalInput::Mode::SendC1) ? L"\x9D" : L"\x1B]";
    const auto st = _terminalInput.GetInputMode(TerminalInput::Mode::SendC1) ? L"\x9C" : L"\x1B\\";
    _api.ReturnResponse(fmt::format(FMT_COMPILE(L"{}{}{}"), osc, response, st));
}

// Routine Description:
// - DECPS - Plays a sequence of musical notes.
// Arguments:
// - params - The volume, duration, and note values to play.
void AdaptDispatch::PlaySounds(const VTParameters parameters)
{
    // First parameter is the volume, in the range 0 to 7. We multiply by
    // 127 / 7 to obtain an equivalent MIDI velocity in the range 0 to 127.
    const auto velocity = std::min(parameters.at(0).value_or(0), 7) * 127 / 7;
    // Second parameter is the duration, in the range 0 to 255. Units are
    // 1/32 of a second, so we multiply by 1000000us/32 to obtain microseconds.
    using namespace std::chrono_literals;
    const auto duration = std::min(parameters.at(1).value_or(0), 255) * 1000000us / 32;
    // The subsequent parameters are notes, in the range 0 to 25.
    parameters.subspan(2).for_each([=](const auto param) {
        // Values 1 to 25 represent the notes C5 to C7, so we add 71 to
        // obtain the equivalent MIDI note numbers (72 = C5).
        const auto noteNumber = std::min(param.value_or(0), 25) + 71;
        // But value 0 is meant to be silent, so if the note number is 71,
        // we set the velocity to 0 (i.e. no volume).
        _api.PlayMidiNote(noteNumber, noteNumber == 71 ? 0 : velocity, duration);
    });
}
