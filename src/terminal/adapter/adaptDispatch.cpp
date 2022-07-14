// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "adaptDispatch.hpp"
#include "../../renderer/base/renderer.hpp"
#include "../../types/inc/Viewport.hpp"
#include "../../types/inc/utils.hpp"
#include "../../inc/unicode.hpp"
#include "../parser/ascii.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::VirtualTerminal;

AdaptDispatch::AdaptDispatch(ITerminalApi& api, Renderer& renderer, RenderSettings& renderSettings, TerminalInput& terminalInput) :
    _api{ api },
    _renderer{ renderer },
    _renderSettings{ renderSettings },
    _terminalInput{ terminalInput },
    _usingAltBuffer(false),
    _isOriginModeRelative(false), // by default, the DECOM origin mode is absolute.
    _isDECCOLMAllowed(false), // by default, DECCOLM is not allowed.
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
        _api.PrintString({ &wchTranslated, 1 });
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
        _api.PrintString(buffer);
    }
    else
    {
        _api.PrintString(string);
    }
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
std::pair<int, int> AdaptDispatch::_GetVerticalMargins(const til::rect& viewport, const bool absolute)
{
    // If the top is out of range, reset the margins completely.
    const auto bottommostRow = viewport.bottom - viewport.top - 1;
    if (_scrollMargins.Top >= bottommostRow)
    {
        _scrollMargins.Top = _scrollMargins.Bottom = 0;
        _api.SetScrollingRegion(_scrollMargins);
    }
    // If margins aren't set, use the full extent of the viewport.
    const auto marginsSet = _scrollMargins.Top < _scrollMargins.Bottom;
    auto topMargin = marginsSet ? _scrollMargins.Top : 0;
    auto bottomMargin = marginsSet ? _scrollMargins.Bottom : bottommostRow;
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
    const auto cursorPosition = cursor.GetPosition();
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(viewport, true);

    // For relative movement, the given offsets will be relative to
    // the current cursor position.
    auto row = cursorPosition.Y;
    auto col = cursorPosition.X;

    // But if the row is absolute, it will be relative to the top of the
    // viewport, or the top margin, depending on the origin mode.
    if (rowOffset.IsAbsolute)
    {
        row = _isOriginModeRelative ? topMargin : viewport.top;
    }

    // And if the column is absolute, it'll be relative to column 0.
    // Horizontal positions are not affected by the viewport.
    if (colOffset.IsAbsolute)
    {
        col = 0;
    }

    // Adjust the base position by the given offsets and clamp the results.
    // The row is constrained within the viewport's vertical boundaries,
    // while the column is constrained by the buffer width.
    row = std::clamp(row + rowOffset.Value, viewport.top, viewport.bottom - 1);
    col = std::clamp(col + colOffset.Value, 0, textBuffer.GetSize().Width() - 1);

    // If the operation needs to be clamped inside the margins, or the origin
    // mode is relative (which always requires margin clamping), then the row
    // may need to be adjusted further.
    if (clampInMargins || _isOriginModeRelative)
    {
        // See microsoft/terminal#2929 - If the cursor is _below_ the top
        // margin, it should stay below the top margin. If it's _above_ the
        // bottom, it should stay above the bottom. Cursor movements that stay
        // outside the margins shouldn't necessarily be affected. For example,
        // moving up while below the bottom margin shouldn't just jump straight
        // to the bottom margin. See
        // ScreenBufferTests::CursorUpDownOutsideMargins for a test of that
        // behavior.
        if (cursorPosition.Y >= topMargin)
        {
            row = std::max(row, topMargin);
        }
        if (cursorPosition.Y <= bottomMargin)
        {
            row = std::min(row, bottomMargin);
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
    const auto attributes = textBuffer.GetCurrentAttributes();

    // The cursor is given to us by the API as relative to the whole buffer.
    // But in VT speak, the cursor row should be relative to the current viewport top.
    auto cursorPosition = textBuffer.GetCursor().GetPosition();
    cursorPosition.Y -= viewport.top;

    // VT is also 1 based, not 0 based, so correct by 1.
    auto& savedCursorState = _savedCursorState.at(_usingAltBuffer);
    savedCursorState.Column = cursorPosition.X + 1;
    savedCursorState.Row = cursorPosition.Y + 1;
    savedCursorState.IsOriginModeRelative = _isOriginModeRelative;
    savedCursorState.Attributes = attributes;
    savedCursorState.TermOutput = _termOutput;
    savedCursorState.C1ControlsAccepted = _GetParserMode(StateMachine::Mode::AcceptC1);
    savedCursorState.CodePage = _api.GetConsoleOutputCP();

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

    auto row = savedCursorState.Row;
    const auto col = savedCursorState.Column;

    // If the origin mode is relative, we need to make sure the restored
    // position is clamped within the margins.
    if (savedCursorState.IsOriginModeRelative)
    {
        const auto viewport = _api.GetViewport();
        const auto [topMargin, bottomMargin] = _GetVerticalMargins(viewport, false);
        // VT origin is at 1,1 so we need to add 1 to these margins.
        row = std::clamp(row, topMargin + 1, bottomMargin + 1);
    }

    // The saved coordinates are always absolute, so we need reset the origin mode temporarily.
    _isOriginModeRelative = false;
    CursorPosition(row, col);

    // Once the cursor position is restored, we can then restore the actual origin mode.
    _isOriginModeRelative = savedCursorState.IsOriginModeRelative;

    // Restore text attributes.
    _api.SetTextAttributes(savedCursorState.Attributes);

    // Restore designated character set.
    _termOutput = savedCursorState.TermOutput;

    // Restore the parsing state of C1 control codes.
    AcceptC1Controls(savedCursorState.C1ControlsAccepted);

    // Restore the code page if it was previously saved.
    if (savedCursorState.CodePage != 0)
    {
        _api.SetConsoleOutputCP(savedCursorState.CodePage);
    }

    return true;
}

// Routine Description:
// - DECTCEM - Sets the show/hide visibility status of the cursor.
// Arguments:
// - fIsVisible - Turns the cursor rendering on (TRUE) or off (FALSE).
// Return Value:
// - True.
bool AdaptDispatch::CursorVisibility(const bool fIsVisible)
{
    _api.GetTextBuffer().GetCursor().SetIsVisible(fIsVisible);
    return true;
}

// Routine Description:
// - Scrolls an area of the buffer in a vertical direction.
// Arguments:
// - textBuffer - Target buffer to be scrolled.
// - fillRect - Area of the buffer that will be affected.
// - delta - Distance to move (positive is down, negative is up).
// Return Value:
// - <none>
void AdaptDispatch::_ScrollRectVertically(TextBuffer& textBuffer, const til::rect& scrollRect, const int32_t delta)
{
    const auto absoluteDelta = std::min(std::abs(delta), scrollRect.height());
    if (absoluteDelta < scrollRect.height())
    {
        // For now we're assuming the scrollRect is always the full width of the
        // buffer, but this will likely need to be extended to support scrolling
        // of arbitrary widths at some point in the future.
        const auto top = delta > 0 ? scrollRect.top : scrollRect.top + absoluteDelta;
        const auto height = scrollRect.height() - absoluteDelta;
        const auto actualDelta = delta > 0 ? absoluteDelta : -absoluteDelta;
        textBuffer.ScrollRows(top, height, actualDelta);
        textBuffer.TriggerRedraw(Viewport::FromExclusive(scrollRect));
    }

    // Rows revealed by the scroll are filled with standard erase attributes.
    auto eraseRect = scrollRect;
    eraseRect.top = delta > 0 ? scrollRect.top : (scrollRect.bottom - absoluteDelta);
    eraseRect.bottom = eraseRect.top + absoluteDelta;
    auto eraseAttributes = textBuffer.GetCurrentAttributes();
    eraseAttributes.SetStandardErase();
    _FillRect(textBuffer, eraseRect, L' ', eraseAttributes);

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
void AdaptDispatch::_ScrollRectHorizontally(TextBuffer& textBuffer, const til::rect& scrollRect, const int32_t delta)
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
        do
        {
            const auto data = OutputCell(*textBuffer.GetCellDataAt(sourcePos));
            textBuffer.Write(OutputCellIterator({ &data, 1 }), targetPos);
            source.WalkInBounds(sourcePos, walkDirection);
        } while (target.WalkInBounds(targetPos, walkDirection));
    }

    // Columns revealed by the scroll are filled with standard erase attributes.
    auto eraseRect = scrollRect;
    eraseRect.left = delta > 0 ? scrollRect.left : (scrollRect.right - absoluteDelta);
    eraseRect.right = eraseRect.left + absoluteDelta;
    auto eraseAttributes = textBuffer.GetCurrentAttributes();
    eraseAttributes.SetStandardErase();
    _FillRect(textBuffer, eraseRect, L' ', eraseAttributes);
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
    auto& textBuffer = _api.GetTextBuffer();
    const auto row = textBuffer.GetCursor().GetPosition().Y;
    const auto startCol = textBuffer.GetCursor().GetPosition().X;
    const auto endCol = textBuffer.GetLineWidth(row);
    _ScrollRectHorizontally(textBuffer, { startCol, row, endCol, row + 1 }, delta);
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
void AdaptDispatch::_FillRect(TextBuffer& textBuffer, const til::rect& fillRect, const wchar_t fillChar, const TextAttribute fillAttrs)
{
    if (fillRect.left < fillRect.right && fillRect.top < fillRect.bottom)
    {
        const auto fillWidth = gsl::narrow_cast<size_t>(fillRect.right - fillRect.left);
        const auto fillData = OutputCellIterator{ fillChar, fillAttrs, fillWidth };
        const auto col = fillRect.left;
        for (auto row = fillRect.top; row < fillRect.bottom; row++)
        {
            textBuffer.WriteLine(fillData, { col, row }, false);
        }
        _api.NotifyAccessibilityChange(fillRect);
    }
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
    const auto row = textBuffer.GetCursor().GetPosition().Y;
    const auto startCol = textBuffer.GetCursor().GetPosition().X;
    const auto endCol = std::min<VTInt>(startCol + numChars, textBuffer.GetLineWidth(row));

    auto eraseAttributes = textBuffer.GetCurrentAttributes();
    eraseAttributes.SetStandardErase();
    _FillRect(textBuffer, { startCol, row, endCol, row + 1 }, L' ', eraseAttributes);

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
        _EraseScrollback();
        // GH#2715 - If this succeeded, but we're in a conpty, return `false` to
        // make the state machine propagate this ED sequence to the connected
        // terminal application. While we're in conpty mode, we don't really
        // have a scrollback, but the attached terminal might.
        return !_api.IsConsolePty();
    }
    else if (eraseType == DispatchTypes::EraseType::All)
    {
        // GH#5683 - If this succeeded, but we're in a conpty, return `false` to
        // make the state machine propagate this ED sequence to the connected
        // terminal application. While we're in conpty mode, when the client
        // requests a Erase All operation, we need to manually tell the
        // connected terminal to do the same thing, so that the terminal will
        // move it's own buffer contents into the scrollback.
        _EraseAll();
        return !_api.IsConsolePty();
    }

    const auto viewport = _api.GetViewport();
    auto& textBuffer = _api.GetTextBuffer();
    const auto bufferWidth = textBuffer.GetSize().Width();
    const auto row = textBuffer.GetCursor().GetPosition().Y;
    const auto col = textBuffer.GetCursor().GetPosition().X;

    auto eraseAttributes = textBuffer.GetCurrentAttributes();
    eraseAttributes.SetStandardErase();

    // When erasing the display, every line that is erased in full should be
    // reset to single width. When erasing to the end, this could include
    // the current line, if the cursor is in the first column. When erasing
    // from the beginning, though, the current line would never be included,
    // because the cursor could never be in the rightmost column (assuming
    // the line is double width).
    if (eraseType == DispatchTypes::EraseType::FromBeginning)
    {
        textBuffer.ResetLineRenditionRange(viewport.top, row);
        _FillRect(textBuffer, { 0, viewport.top, bufferWidth, row }, L' ', eraseAttributes);
        _FillRect(textBuffer, { 0, row, col + 1, row + 1 }, L' ', eraseAttributes);
    }
    if (eraseType == DispatchTypes::EraseType::ToEnd)
    {
        textBuffer.ResetLineRenditionRange(col > 0 ? row + 1 : row, viewport.bottom);
        _FillRect(textBuffer, { col, row, bufferWidth, row + 1 }, L' ', eraseAttributes);
        _FillRect(textBuffer, { 0, row + 1, bufferWidth, viewport.bottom }, L' ', eraseAttributes);
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
    const auto row = textBuffer.GetCursor().GetPosition().Y;
    const auto col = textBuffer.GetCursor().GetPosition().X;

    auto eraseAttributes = textBuffer.GetCurrentAttributes();
    eraseAttributes.SetStandardErase();
    switch (eraseType)
    {
    case DispatchTypes::EraseType::FromBeginning:
        _FillRect(textBuffer, { 0, row, col + 1, row + 1 }, L' ', eraseAttributes);
        return true;
    case DispatchTypes::EraseType::ToEnd:
        _FillRect(textBuffer, { col, row, textBuffer.GetLineWidth(row), row + 1 }, L' ', eraseAttributes);
        return true;
    case DispatchTypes::EraseType::All:
        _FillRect(textBuffer, { 0, row, textBuffer.GetLineWidth(row), row + 1 }, L' ', eraseAttributes);
        return true;
    default:
        return false;
    }
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
    _api.GetTextBuffer().SetCurrentLineRendition(rendition);
    return true;
}

// Routine Description:
// - DSR - Reports status of a console property back to the STDIN based on the type of status requested.
//       - This particular routine responds to ANSI status patterns only (CSI # n), not the DEC format (CSI ? # n)
// Arguments:
// - statusType - ANSI status type indicating what property we should report back
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::DeviceStatusReport(const DispatchTypes::AnsiStatusType statusType)
{
    switch (statusType)
    {
    case DispatchTypes::AnsiStatusType::OS_OperatingStatus:
        _OperatingStatus();
        return true;
    case DispatchTypes::AnsiStatusType::CPR_CursorPositionReport:
        _CursorPositionReport();
        return true;
    default:
        return false;
    }
}

// Routine Description:
// - DA - Reports the identity of this Virtual Terminal machine to the caller.
//      - In our case, we'll report back to acknowledge we understand, but reveal no "hardware" upgrades like physical terminals of old.
// Arguments:
// - <none>
// Return Value:
// - True.
bool AdaptDispatch::DeviceAttributes()
{
    // See: http://vt100.net/docs/vt100-ug/chapter3.html#DA
    _api.ReturnResponse(L"\x1b[?1;0c");
    return true;
}

// Routine Description:
// - DA2 - Reports the terminal type, firmware version, and hardware options.
//   For now we're following the XTerm practice of using 0 to represent a VT100
//   terminal, the version is hardcoded as 10 (1.0), and the hardware option
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
//   typically return a hardcoded value, the most common being all zeros.
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
//   etc.). On modern terminal emulators, the response is simply hardcoded.
// Arguments:
// - permission - This would originally have determined whether the terminal
//   was allowed to send unsolicited reports or not.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::RequestTerminalParameters(const DispatchTypes::ReportingPermission permission)
{
    // We don't care whether unsolicited reports are allowed or not, but the
    // requested permission does determine the value of the first response
    // parameter. The remaining parameters are just hardcoded to indicate a
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
// - DSR-OS - Reports the operating status back to the input channel
// Arguments:
// - <none>
// Return Value:
// - <none>
void AdaptDispatch::_OperatingStatus() const
{
    // We always report a good operating condition.
    _api.ReturnResponse(L"\x1b[0n");
}

// Routine Description:
// - DSR-CPR - Reports the current cursor position within the viewport back to the input channel
// Arguments:
// - <none>
// Return Value:
// - <none>
void AdaptDispatch::_CursorPositionReport()
{
    const auto viewport = _api.GetViewport();
    const auto& textBuffer = _api.GetTextBuffer();

    // First pull the cursor position relative to the entire buffer out of the console.
    til::point cursorPosition{ textBuffer.GetCursor().GetPosition() };

    // Now adjust it for its position in respect to the current viewport top.
    cursorPosition.Y -= viewport.top;

    // NOTE: 1,1 is the top-left corner of the viewport in VT-speak, so add 1.
    cursorPosition.X++;
    cursorPosition.Y++;

    // If the origin mode is relative, line numbers start at top margin of the scrolling region.
    if (_isOriginModeRelative)
    {
        const auto topMargin = _GetVerticalMargins(viewport, false).first;
        cursorPosition.Y -= topMargin;
    }

    // Now send it back into the input channel of the console.
    // First format the response string.
    const auto response = wil::str_printf<std::wstring>(L"\x1b[%d;%dR", cursorPosition.Y, cursorPosition.X);
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
    _ScrollRectVertically(textBuffer, { 0, topMargin, bufferWidth, bottomMargin + 1 }, delta);
}

// Routine Description:
// - SU - Pans the window DOWN by given distance (distance new lines appear at the bottom of the screen)
// Arguments:
// - distance - Distance to move
// Return Value:
// - True.
bool AdaptDispatch::ScrollUp(const VTInt uiDistance)
{
    _ScrollMovement(-gsl::narrow_cast<int32_t>(uiDistance));
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
    _ScrollMovement(gsl::narrow_cast<int32_t>(uiDistance));
    return true;
}

// Routine Description:
// - DECSCPP / DECCOLM Sets the number of columns "per page" AKA sets the console width.
// DECCOLM also clear the screen (like a CSI 2 J sequence), while DECSCPP just sets the width.
// (DECCOLM will do this separately of this function)
// Arguments:
// - columns - Number of columns
// Return Value:
// - True.
bool AdaptDispatch::SetColumns(const VTInt columns)
{
    const auto viewport = _api.GetViewport();
    const auto viewportHeight = viewport.bottom - viewport.top;
    _api.ResizeWindow(columns, viewportHeight);
    return true;
}

// Routine Description:
// - DECCOLM not only sets the number of columns, but also clears the screen buffer,
//    resets the page margins and origin mode, and places the cursor at 1,1
// Arguments:
// - columns - Number of columns
// Return Value:
// - True.
bool AdaptDispatch::_DoDECCOLMHelper(const VTInt columns)
{
    // Only proceed if DECCOLM is allowed. Return true, as this is technically a successful handling.
    if (_isDECCOLMAllowed)
    {
        SetColumns(columns);
        SetOriginMode(false);
        CursorPosition(1, 1);
        EraseInDisplay(DispatchTypes::EraseType::All);
        _DoSetTopBottomScrollingMargins(0, 0);
    }
    return true;
}

// Routine Description :
// - Retrieves the various StateMachine parser modes.
// Arguments:
// - mode - the parser mode to query.
// Return Value:
// - true if the mode is enabled. false if disabled.
bool AdaptDispatch::_GetParserMode(const StateMachine::Mode mode) const
{
    return _api.GetStateMachine().GetParserMode(mode);
}

// Routine Description:
// - Sets the various StateMachine parser modes.
// Arguments:
// - mode - the parser mode to change.
// - enable - set to true to enable the mode, false to disable it.
// Return Value:
// - <none>
void AdaptDispatch::_SetParserMode(const StateMachine::Mode mode, const bool enable)
{
    _api.GetStateMachine().SetParserMode(mode, enable);
}

// Routine Description:
// - Sets the various terminal input modes.
// Arguments:
// - mode - the input mode to change.
// - enable - set to true to enable the mode, false to disable it.
// Return Value:
// - true if successful. false otherwise.
bool AdaptDispatch::_SetInputMode(const TerminalInput::Mode mode, const bool enable)
{
    _terminalInput.SetInputMode(mode, enable);

    // If we're a conpty, AND WE'RE IN VT INPUT MODE, always pass input mode requests
    // The VT Input mode check is to work around ssh.exe v7.7, which uses VT
    // output, but not Input.
    // The original comment said, "Once the conpty supports these types of input,
    // this check can be removed. See GH#4911". Unfortunately, time has shown
    // us that SSH 7.7 _also_ requests mouse input and that can have a user interface
    // impact on the actual connected terminal. We can't remove this check,
    // because SSH <=7.7 is out in the wild on all versions of Windows <=2004.

    // GH#12799 - If the app requested that we disable focus events, DON'T pass
    // that through. ConPTY would _always_ like to know about focus events.

    return !_api.IsConsolePty() ||
           !_api.IsVtInputEnabled() ||
           (!enable && mode == TerminalInput::Mode::FocusEvent);

    // Another way of writing the above statement is:
    //
    // const bool inConpty = _api.IsConsolePty();
    // const bool shouldPassthrough = inConpty && _api.IsVtInputEnabled();
    // const bool disabledFocusEvents = inConpty && (!enable && mode == TerminalInput::Mode::FocusEvent);
    // return !shouldPassthrough || disabledFocusEvents;
    //
    // It's like a "filter" left to right. Due to the early return via
    // !IsConsolePty, once you're at the !enable part, IsConsolePty can only be
    // true anymore.
}

// Routine Description:
// - Support routine for routing private mode parameters to be set/reset as flags
// Arguments:
// - param - mode parameter to set/reset
// - enable - True for set, false for unset.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::_ModeParamsHelper(const DispatchTypes::ModeParams param, const bool enable)
{
    auto success = false;
    switch (param)
    {
    case DispatchTypes::ModeParams::DECCKM_CursorKeysMode:
        // set - Enable Application Mode, reset - Normal mode
        success = SetCursorKeysMode(enable);
        break;
    case DispatchTypes::ModeParams::DECANM_AnsiMode:
        success = SetAnsiMode(enable);
        break;
    case DispatchTypes::ModeParams::DECCOLM_SetNumberOfColumns:
        success = _DoDECCOLMHelper(enable ? DispatchTypes::s_sDECCOLMSetColumns : DispatchTypes::s_sDECCOLMResetColumns);
        break;
    case DispatchTypes::ModeParams::DECSCNM_ScreenMode:
        success = SetScreenMode(enable);
        break;
    case DispatchTypes::ModeParams::DECOM_OriginMode:
        // The cursor is also moved to the new home position when the origin mode is set or reset.
        success = SetOriginMode(enable) && CursorPosition(1, 1);
        break;
    case DispatchTypes::ModeParams::DECAWM_AutoWrapMode:
        success = SetAutoWrapMode(enable);
        break;
    case DispatchTypes::ModeParams::ATT610_StartCursorBlink:
        success = EnableCursorBlinking(enable);
        break;
    case DispatchTypes::ModeParams::DECTCEM_TextCursorEnableMode:
        success = CursorVisibility(enable);
        break;
    case DispatchTypes::ModeParams::XTERM_EnableDECCOLMSupport:
        success = EnableDECCOLMSupport(enable);
        break;
    case DispatchTypes::ModeParams::VT200_MOUSE_MODE:
        success = EnableVT200MouseMode(enable);
        break;
    case DispatchTypes::ModeParams::BUTTON_EVENT_MOUSE_MODE:
        success = EnableButtonEventMouseMode(enable);
        break;
    case DispatchTypes::ModeParams::ANY_EVENT_MOUSE_MODE:
        success = EnableAnyEventMouseMode(enable);
        break;
    case DispatchTypes::ModeParams::UTF8_EXTENDED_MODE:
        success = EnableUTF8ExtendedMouseMode(enable);
        break;
    case DispatchTypes::ModeParams::SGR_EXTENDED_MODE:
        success = EnableSGRExtendedMouseMode(enable);
        break;
    case DispatchTypes::ModeParams::FOCUS_EVENT_MODE:
        success = EnableFocusEventMode(enable);
        break;
    case DispatchTypes::ModeParams::ALTERNATE_SCROLL:
        success = EnableAlternateScroll(enable);
        break;
    case DispatchTypes::ModeParams::ASB_AlternateScreenBuffer:
        success = enable ? UseAlternateScreenBuffer() : UseMainScreenBuffer();
        break;
    case DispatchTypes::ModeParams::XTERM_BracketedPasteMode:
        success = EnableXtermBracketedPasteMode(enable);
        break;
    case DispatchTypes::ModeParams::W32IM_Win32InputMode:
        success = EnableWin32InputMode(enable);
        break;
    default:
        // If no functions to call, overall dispatch was a failure.
        success = false;
        break;
    }
    return success;
}

// Routine Description:
// - DECSET - Enables the given DEC private mode params.
// Arguments:
// - param - mode parameter to set
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::SetMode(const DispatchTypes::ModeParams param)
{
    return _ModeParamsHelper(param, true);
}

// Routine Description:
// - DECRST - Disables the given DEC private mode params.
// Arguments:
// - param - mode parameter to reset
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::ResetMode(const DispatchTypes::ModeParams param)
{
    return _ModeParamsHelper(param, false);
}

// - DECKPAM, DECKPNM - Sets the keypad input mode to either Application mode or Numeric mode (true, false respectively)
// Arguments:
// - applicationMode - set to true to enable Application Mode Input, false for Numeric Mode Input.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::SetKeypadMode(const bool fApplicationMode)
{
    return _SetInputMode(TerminalInput::Mode::Keypad, fApplicationMode);
}

// Method Description:
// - win32-input-mode: Enable sending full input records encoded as a string of
//   characters to the client application.
// Arguments:
// - win32InputMode - set to true to enable win32-input-mode, false to disable.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::EnableWin32InputMode(const bool win32InputMode)
{
    return _SetInputMode(TerminalInput::Mode::Win32, win32InputMode);
}

// - DECCKM - Sets the cursor keys input mode to either Application mode or Normal mode (true, false respectively)
// Arguments:
// - applicationMode - set to true to enable Application Mode Input, false for Normal Mode Input.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::SetCursorKeysMode(const bool applicationMode)
{
    return _SetInputMode(TerminalInput::Mode::CursorKey, applicationMode);
}

// - att610 - Enables or disables the cursor blinking.
// Arguments:
// - enable - set to true to enable blinking, false to disable
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::EnableCursorBlinking(const bool enable)
{
    auto& cursor = _api.GetTextBuffer().GetCursor();
    cursor.SetBlinkingAllowed(enable);

    // GH#2642 - From what we've gathered from other terminals, when blinking is
    // disabled, the cursor should remain On always, and have the visibility
    // controlled by the IsVisible property. So when you do a printf "\e[?12l"
    // to disable blinking, the cursor stays stuck On. At this point, only the
    // cursor visibility property controls whether the user can see it or not.
    // (Yes, the cursor can be On and NOT Visible)
    cursor.SetIsOn(true);

    return !_api.IsConsolePty();
}

// Routine Description:
// - Internal logic for adding or removing lines in the active screen buffer.
//   This also moves the cursor to the left margin, which is expected behavior for IL and DL.
// Parameters:
// - delta - Number of lines to modify (positive if inserting, negative if deleting).
// Return Value:
// - <none>
void AdaptDispatch::_InsertDeleteLineHelper(const int32_t delta)
{
    const auto viewport = _api.GetViewport();
    auto& textBuffer = _api.GetTextBuffer();
    const auto bufferWidth = textBuffer.GetSize().Width();

    auto& cursor = textBuffer.GetCursor();
    const auto row = cursor.GetPosition().Y;

    const auto [topMargin, bottomMargin] = _GetVerticalMargins(viewport, true);
    if (row >= topMargin && row <= bottomMargin)
    {
        // We emulate inserting and deleting by scrolling the area between the cursor and the bottom margin.
        _ScrollRectVertically(textBuffer, { 0, row, bufferWidth, bottomMargin + 1 }, delta);

        // The IL and DL controls are also expected to move the cursor to the left margin.
        // For now this is just column 0, since we don't yet support DECSLRM.
        cursor.SetXPosition(0);
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
    _InsertDeleteLineHelper(gsl::narrow_cast<int32_t>(distance));
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
    _InsertDeleteLineHelper(-gsl::narrow_cast<int32_t>(distance));
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
    _termOutput = {};

    _SetParserMode(StateMachine::Mode::Ansi, ansiMode);
    _SetInputMode(TerminalInput::Mode::Ansi, ansiMode);

    // We don't check the _SetInputMode return value, because we'll never want
    // to forward a DECANM mode change over conpty.
    return true;
}

// Routine Description:
// - DECSCNM - Sets the screen mode to either normal or reverse.
//    When in reverse screen mode, the background and foreground colors are switched.
// Arguments:
// - reverseMode - set to true to enable reverse screen mode, false for normal mode.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::SetScreenMode(const bool reverseMode)
{
    // If we're a conpty, always return false
    if (_api.IsConsolePty())
    {
        return false;
    }
    _renderSettings.SetRenderMode(RenderSettings::Mode::ScreenReversed, reverseMode);
    _renderer.TriggerRedrawAll();
    return true;
}

// Routine Description:
// - DECOM - Sets the cursor addressing origin mode to relative or absolute.
//    When relative, line numbers start at top margin of the user-defined scrolling region.
//    When absolute, line numbers are independent of the scrolling region.
// Arguments:
// - relativeMode - set to true to use relative addressing, false for absolute addressing.
// Return Value:
// - True.
bool AdaptDispatch::SetOriginMode(const bool relativeMode) noexcept
{
    _isOriginModeRelative = relativeMode;
    return true;
}

// Routine Description:
// - DECAWM - Sets the Auto Wrap Mode.
//    This controls whether the cursor moves to the beginning of the next row
//    when it reaches the end of the current row.
// Arguments:
// - wrapAtEOL - set to true to wrap, false to overwrite the last character.
// Return Value:
// - True.
bool AdaptDispatch::SetAutoWrapMode(const bool wrapAtEOL)
{
    _api.SetAutoWrapMode(wrapAtEOL);
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
// Return Value:
// - <none>
void AdaptDispatch::_DoSetTopBottomScrollingMargins(const VTInt topMargin,
                                                    const VTInt bottomMargin)
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
        _scrollMargins.Top = actualTop;
        _scrollMargins.Bottom = actualBottom;
        _api.SetScrollingRegion(_scrollMargins);
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
    // When this is called, the cursor should also be moved to home.
    // Other functions that only need to set/reset the margins should call _DoSetTopBottomScrollingMargins
    _DoSetTopBottomScrollingMargins(topMargin, bottomMargin);
    CursorPosition(1, 1);
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
// - IND/NEL - Performs a line feed, possibly preceded by carriage return.
//    Moves the cursor down one line, and possibly also to the leftmost column.
// Arguments:
// - lineFeedType - Specify whether a carriage return should be performed as well.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::LineFeed(const DispatchTypes::LineFeedType lineFeedType)
{
    switch (lineFeedType)
    {
    case DispatchTypes::LineFeedType::DependsOnMode:
        _api.LineFeed(_api.GetLineFeedMode());
        return true;
    case DispatchTypes::LineFeedType::WithoutReturn:
        _api.LineFeed(false);
        return true;
    case DispatchTypes::LineFeedType::WithReturn:
        _api.LineFeed(true);
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
    const auto [topMargin, bottomMargin] = _GetVerticalMargins(viewport, true);

    // If the cursor is at the top of the margin area, we shift the buffer
    // contents down, to emulate inserting a line at that point.
    if (cursorPosition.Y == topMargin)
    {
        const auto bufferWidth = textBuffer.GetSize().Width();
        _ScrollRectVertically(textBuffer, { 0, topMargin, bufferWidth, bottomMargin + 1 }, 1);
    }
    else if (cursorPosition.Y > viewport.top)
    {
        // Otherwise we move the cursor up, but not past the top of the viewport.
        cursor.SetPosition(textBuffer.ClampPositionWithinLine({ cursorPosition.X, cursorPosition.Y - 1 }));
        _ApplyCursorMovementFlags(cursor);
    }
    return true;
}

// Routine Description:
// - OSC Set Window Title - Sets the title of the window
// Arguments:
// - title - The string to set the title to. Must be null terminated.
// Return Value:
// - True.
bool AdaptDispatch::SetWindowTitle(std::wstring_view title)
{
    _api.SetWindowTitle(title);
    return true;
}

// - ASBSET - Creates and swaps to the alternate screen buffer. In virtual terminals, there exists both a "main"
//     screen buffer and an alternate. ASBSET creates a new alternate, and switches to it. If there is an already
//     existing alternate, it is discarded.
// Arguments:
// - None
// Return Value:
// - True.
bool AdaptDispatch::UseAlternateScreenBuffer()
{
    CursorSaveState();
    _api.UseAlternateScreenBuffer();
    _usingAltBuffer = true;
    return true;
}

// Routine Description:
// - ASBRST - From the alternate buffer, returns to the main screen buffer.
//     From the main screen buffer, does nothing. The alternate is discarded.
// Arguments:
// - None
// Return Value:
// - True.
bool AdaptDispatch::UseMainScreenBuffer()
{
    _api.UseMainScreenBuffer();
    _usingAltBuffer = false;
    CursorRestoreState();
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
    const auto width = textBuffer.GetSize().Dimensions().X;
    const auto column = textBuffer.GetCursor().GetPosition().X;

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
    const auto width = textBuffer.GetLineWidth(cursor.GetPosition().Y);
    auto column = cursor.GetPosition().X;
    auto tabsPerformed = 0;

    _InitTabStopsForWidth(width);
    while (column + 1 < width && tabsPerformed < numTabs)
    {
        column++;
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
    const auto width = textBuffer.GetLineWidth(cursor.GetPosition().Y);
    auto column = cursor.GetPosition().X;
    auto tabsPerformed = 0;

    _InitTabStopsForWidth(width);
    while (column > 0 && tabsPerformed < numTabs)
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
    const auto width = textBuffer.GetSize().Dimensions().X;
    const auto column = textBuffer.GetCursor().GetPosition().X;

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
// - Clears all tab stops and sets the _initDefaultTabStops flag to indicate
//    that the default positions should be reinitialized when needed.
// Arguments:
// - <none>
// Return value:
// - <none>
void AdaptDispatch::_ResetTabStops() noexcept
{
    _tabStopColumns.clear();
    _initDefaultTabStops = true;
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
bool AdaptDispatch::SingleShift(const VTInt gsetNumber)
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
    _SetParserMode(StateMachine::Mode::AcceptC1, enabled);
    return true;
}

//Routine Description:
// Soft Reset - Perform a soft reset. See http://www.vt100.net/docs/vt510-rm/DECSTR.html
// The following table lists everything that should be done, 'X's indicate the ones that
//   we actually perform. As the appropriate functionality is added to our ANSI support,
//   we should update this.
//  X Text cursor enable          DECTCEM     Cursor enabled.
//    Insert/replace              IRM         Replace mode.
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
//    Select character attribute  DECSCA      Normal (erasable by DECSEL and DECSED).
//  X Save cursor state           DECSC       Home position.
//    Assign user preference      DECAUPSS    Set selected in Set-Up.
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
    CursorVisibility(true); // Cursor enabled.
    SetOriginMode(false); // Absolute cursor addressing.
    SetAutoWrapMode(true); // Wrap at end of line.
    SetCursorKeysMode(false); // Normal characters.
    SetKeypadMode(false); // Numeric characters.

    // Top margin = 1; bottom margin = page length.
    _DoSetTopBottomScrollingMargins(0, 0);

    _termOutput = {}; // Reset all character set designations.
    if (_initialCodePage.has_value())
    {
        // Restore initial code page if previously changed by a DOCS sequence.
        _api.SetConsoleOutputCP(_initialCodePage.value());
    }
    // Disable parsing of C1 control codes.
    AcceptC1Controls(false);

    SetGraphicsRendition({}); // Normal rendition.

    // Reset the saved cursor state.
    // Note that XTerm only resets the main buffer state, but that
    // seems likely to be a bug. Most other terminals reset both.
    _savedCursorState.at(0) = {}; // Main buffer
    _savedCursorState.at(1) = {}; // Alt buffer

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

    // Sets the SGR state to normal - this must be done before EraseInDisplay
    //      to ensure that it clears with the default background color.
    SoftReset();

    // Clears the screen - Needs to be done in two operations.
    EraseInDisplay(DispatchTypes::EraseType::All);
    EraseInDisplay(DispatchTypes::EraseType::Scrollback);

    // Set the DECSCNM screen mode back to normal.
    SetScreenMode(false);

    // Cursor to 1,1 - the Soft Reset guarantees this is absolute
    CursorPosition(1, 1);

    // Reset the mouse mode
    EnableSGRExtendedMouseMode(false);
    EnableAnyEventMouseMode(false);

    // Delete all current tab stops and reapply
    _ResetTabStops();

    // Clear the soft font in the renderer and delete the font buffer.
    _renderer.UpdateSoftFont({}, {}, false);
    _fontBuffer = nullptr;

    // GH#2715 - If all this succeeded, but we're in a conpty, return `false` to
    // make the state machine propagate this RIS sequence to the connected
    // terminal application. We've reset our state, but the connected terminal
    // might need to do more.
    return !_api.IsConsolePty();
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
    const auto bufferWidth = textBuffer.GetSize().Dimensions().X;

    // Fill the screen with the letter E using the default attributes.
    _FillRect(textBuffer, { 0, viewport.top, bufferWidth, viewport.bottom }, L'E', {});
    // Reset the line rendition for all of these rows.
    textBuffer.ResetLineRenditionRange(viewport.top, viewport.bottom);
    // Reset the meta/extended attributes (but leave the colors unchanged).
    auto attr = textBuffer.GetCurrentAttributes();
    attr.SetStandardErase();
    _api.SetTextAttributes(attr);
    // Reset the origin mode to absolute.
    SetOriginMode(false);
    // Clear the scrolling margins.
    _DoSetTopBottomScrollingMargins(0, 0);
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
// - <none>
void AdaptDispatch::_EraseScrollback()
{
    const auto viewport = _api.GetViewport();
    const auto top = viewport.top;
    const auto height = viewport.bottom - viewport.top;
    auto& textBuffer = _api.GetTextBuffer();
    const auto bufferSize = textBuffer.GetSize().Dimensions();
    auto& cursor = textBuffer.GetCursor();
    const auto row = cursor.GetPosition().Y;

    // Scroll the viewport content to the top of the buffer.
    textBuffer.ScrollRows(top, height, -top);
    // Clear everything after the viewport.
    _FillRect(textBuffer, { 0, height, bufferSize.X, bufferSize.Y }, L' ', {});
    // Also reset the line rendition for all of the cleared rows.
    textBuffer.ResetLineRenditionRange(height, bufferSize.Y);
    // Move the viewport
    _api.SetViewportPosition({ viewport.left, 0 });
    // Move the cursor to the same relative location.
    cursor.SetYPosition(row - top);
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
// Return value:
// - <none>
void AdaptDispatch::_EraseAll()
{
    const auto viewport = _api.GetViewport();
    const auto viewportHeight = viewport.bottom - viewport.top;
    auto& textBuffer = _api.GetTextBuffer();
    const auto bufferSize = textBuffer.GetSize();

    // Stash away the current position of the cursor within the viewport.
    // We'll need to restore the cursor to that same relative position, after
    //      we move the viewport.
    auto& cursor = textBuffer.GetCursor();
    const auto row = cursor.GetPosition().Y - viewport.top;

    // Calculate new viewport position. Typically we want to move one line below
    // the last non-space row, but if the last non-space character is the very
    // start of the buffer, then we shouldn't move down at all.
    const auto lastChar = textBuffer.GetLastNonSpaceCharacter();
    auto newViewportTop = lastChar == til::point{} ? 0 : lastChar.Y + 1;
    const auto newViewportBottom = newViewportTop + viewportHeight;
    const auto delta = newViewportBottom - (bufferSize.Height());
    for (auto i = 0; i < delta; i++)
    {
        textBuffer.IncrementCircularBuffer();
        newViewportTop--;
    }
    // Move the viewport
    _api.SetViewportPosition({ viewport.left, newViewportTop });
    // Restore the relative cursor position
    cursor.SetYPosition(row + newViewportTop);
    cursor.SetHasMoved(true);

    // Erase all the rows in the current viewport.
    auto eraseAttributes = textBuffer.GetCurrentAttributes();
    eraseAttributes.SetStandardErase();
    _FillRect(textBuffer, { 0, newViewportTop, bufferSize.Width(), newViewportBottom }, L' ', eraseAttributes);

    // Also reset the line rendition for the erased rows.
    textBuffer.ResetLineRenditionRange(newViewportTop, newViewportBottom);
}

// Routine Description:
// - Enables or disables support for the DECCOLM escape sequence.
// Arguments:
// - enabled - set to true to allow DECCOLM to be used, false to disallow.
// Return Value:
// - True.
bool AdaptDispatch::EnableDECCOLMSupport(const bool enabled) noexcept
{
    _isDECCOLMAllowed = enabled;
    return true;
}

//Routine Description:
// Enable VT200 Mouse Mode - Enables/disables the mouse input handler in default tracking mode.
//Arguments:
// - enabled - true to enable, false to disable.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::EnableVT200MouseMode(const bool enabled)
{
    return _SetInputMode(TerminalInput::Mode::DefaultMouseTracking, enabled);
}

//Routine Description:
// Enable UTF-8 Extended Encoding - this changes the encoding scheme for sequences
//      emitted by the mouse input handler. Does not enable/disable mouse mode on its own.
//Arguments:
// - enabled - true to enable, false to disable.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::EnableUTF8ExtendedMouseMode(const bool enabled)
{
    return _SetInputMode(TerminalInput::Mode::Utf8MouseEncoding, enabled);
}

//Routine Description:
// Enable SGR Extended Encoding - this changes the encoding scheme for sequences
//      emitted by the mouse input handler. Does not enable/disable mouse mode on its own.
//Arguments:
// - enabled - true to enable, false to disable.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::EnableSGRExtendedMouseMode(const bool enabled)
{
    return _SetInputMode(TerminalInput::Mode::SgrMouseEncoding, enabled);
}

//Routine Description:
// Enable Button Event mode - send mouse move events WITH A BUTTON PRESSED to the input.
//Arguments:
// - enabled - true to enable, false to disable.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::EnableButtonEventMouseMode(const bool enabled)
{
    return _SetInputMode(TerminalInput::Mode::ButtonEventMouseTracking, enabled);
}

//Routine Description:
// Enable Any Event mode - send all mouse events to the input.
//Arguments:
// - enabled - true to enable, false to disable.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::EnableAnyEventMouseMode(const bool enabled)
{
    return _SetInputMode(TerminalInput::Mode::AnyEventMouseTracking, enabled);
}

// Method Description:
// - Enables/disables focus event mode. A client may enable this if they want to
//   receive focus events.
// - ConPTY always enables this mode and never disables it. Internally, we'll
//   always set this mode, but conpty will never request this to be disabled by
//   the hosting terminal.
// Arguments:
// - enabled - true to enable, false to disable.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::EnableFocusEventMode(const bool enabled)
{
    return _SetInputMode(TerminalInput::Mode::FocusEvent, enabled);
}

//Routine Description:
// Enable Alternate Scroll Mode - When in the Alt Buffer, send CUP and CUD on
//      scroll up/down events instead of the usual sequences
//Arguments:
// - enabled - true to enable, false to disable.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::EnableAlternateScroll(const bool enabled)
{
    return _SetInputMode(TerminalInput::Mode::AlternateScroll, enabled);
}

//Routine Description:
// Enable "bracketed paste mode".
//Arguments:
// - enabled - true to enable, false to disable.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::EnableXtermBracketedPasteMode(const bool enabled)
{
    // Return false to forward the operation to the hosting terminal,
    // since ConPTY can't handle this itself.
    if (_api.IsConsolePty())
    {
        return false;
    }
    _api.EnableXtermBracketedPasteMode(enabled);
    return true;
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
    cursor.SetIsOn(true);

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
    if (_renderSettings.GetRenderMode(RenderSettings::Mode::DistinguishableColors))
    {
        // Re-calculate the adjusted colors now that one of the entries has been changed
        _renderSettings.MakeAdjustedColorArray();
    }

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
        if (_renderSettings.GetRenderMode(RenderSettings::Mode::DistinguishableColors))
        {
            // Re-calculate the adjusted colors now that these aliases have been changed
            _renderSettings.MakeAdjustedColorArray();
        }
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
            const auto path = til::at(parts, 1);
            // The path should be surrounded with '"' according to the documentation of ConEmu.
            // An example: 9;"D:/"
            if (path.at(0) == L'"' && path.at(path.size() - 1) == L'"' && path.size() >= 3)
            {
                _api.SetWorkingDirectory(path.substr(1, path.size() - 2));
            }
            else
            {
                // If we fail to find the surrounding quotation marks, we'll give the path a try anyway.
                // ConEmu also does this.
                _api.SetWorkingDirectory(path);
            }
            return true;
        }
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
        DispatchTypes::ScrollMark mark;
        mark.category = DispatchTypes::MarkCategory::Prompt;
        _api.AddMark(mark);
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

    if (action == L"A") // FTCS_PROMPT
    {
        // Simply just mark this line as a prompt line.
        DispatchTypes::ScrollMark mark;
        mark.category = DispatchTypes::MarkCategory::Prompt;
        _api.AddMark(mark);
        return true;
    }

    // When we add the rest of the FTCS sequences (GH#11000), we should add a
    // simple state machine here to track the most recently emitted mark from
    // this set of sequences, and which sequence was emitted last, so we can
    // modify the state of that mark as we go.
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
                                                         const DispatchTypes::DrcsCharsetSize charsetSize)
{
    // If we're a conpty, we're just going to ignore the operation for now.
    // There's no point in trying to pass it through without also being able
    // to pass through the character set designations.
    if (_api.IsConsolePty())
    {
        return nullptr;
    }

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
            if (charsetSize == DispatchTypes::DrcsCharsetSize::Size96)
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
    const auto idBuilder = std::make_shared<VTIDBuilder>();
    return [=](const auto ch) {
        if (ch >= '\x40' && ch <= '\x7e')
        {
            const auto id = idBuilder->Finalize(ch);
            switch (id)
            {
            case VTID('m'):
                _ReportSGRSetting();
                break;
            case VTID('r'):
                _ReportDECSTBMSetting();
                break;
            default:
                _api.ReturnResponse(L"\033P0$r\033\\");
                break;
            }
            return false;
        }
        else
        {
            if (ch >= '\x20' && ch <= '\x2f')
            {
                idBuilder->AddIntermediate(ch);
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

    const auto attr = _api.GetTextBuffer().GetCurrentAttributes();
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
    addAttribute(L";4"sv, attr.IsUnderlined());
    addAttribute(L";5"sv, attr.IsBlinking());
    addAttribute(L";7"sv, attr.IsReverseVideo());
    addAttribute(L";8"sv, attr.IsInvisible());
    addAttribute(L";9"sv, attr.IsCrossedOut());
    addAttribute(L";21"sv, attr.IsDoublyUnderlined());
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
            fmt::format_to(std::back_inserter(response), FMT_COMPILE(L";{};5;{}"), base + 8, index);
        }
        else if (color.IsRgb())
        {
            const auto r = GetRValue(color.GetRGB());
            const auto g = GetGValue(color.GetRGB());
            const auto b = GetBValue(color.GetRGB());
            fmt::format_to(std::back_inserter(response), FMT_COMPILE(L";{};2;{};{};{}"), base + 8, r, g, b);
        }
    };
    addColor(30, attr.GetForeground());
    addColor(40, attr.GetBackground());

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
