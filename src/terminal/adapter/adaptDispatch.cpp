// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "adaptDispatch.hpp"
#include "conGetSet.hpp"
#include "../../types/inc/Viewport.hpp"
#include "../../types/inc/utils.hpp"
#include "../../inc/unicode.hpp"
#include "../parser/ascii.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::VirtualTerminal;

// Routine Description:
// - No Operation helper. It's just here to make sure they're always all the same.
// Arguments:
// - <none>
// Return Value:
// - Always false to signify we didn't handle it.
bool NoOp() noexcept
{
    return false;
}

// Note: AdaptDispatch will take ownership of pConApi and pDefaults
AdaptDispatch::AdaptDispatch(std::unique_ptr<ConGetSet> pConApi,
                             std::unique_ptr<AdaptDefaults> pDefaults) :
    _pConApi{ std::move(pConApi) },
    _pDefaults{ std::move(pDefaults) },
    _usingAltBuffer(false),
    _isOriginModeRelative(false), // by default, the DECOM origin mode is absolute.
    _isDECCOLMAllowed(false), // by default, DECCOLM is not allowed.
    _termOutput()
{
    THROW_HR_IF_NULL(E_INVALIDARG, _pConApi.get());
    THROW_HR_IF_NULL(E_INVALIDARG, _pDefaults.get());
    _scrollMargins = { 0 }; // initially, there are no scroll margins.
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
        _pDefaults->Print(wchTranslated);
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
    try
    {
        if (_termOutput.NeedToTranslate())
        {
            std::wstring buffer;
            buffer.reserve(string.size());
            for (auto& wch : string)
            {
                buffer.push_back(_termOutput.TranslateKey(wch));
            }
            _pDefaults->PrintString(buffer);
        }
        else
        {
            _pDefaults->PrintString(string);
        }
    }
    CATCH_LOG();
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
// - True if handled successfully. False otherwise.
bool AdaptDispatch::CursorUp(const size_t distance)
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
// - True if handled successfully. False otherwise.
bool AdaptDispatch::CursorDown(const size_t distance)
{
    return _CursorMovePosition(Offset::Forward(distance), Offset::Unchanged(), true);
}

// Routine Description:
// - CUF - Handles cursor forward movement by given distance
// Arguments:
// - distance - Distance to move
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::CursorForward(const size_t distance)
{
    return _CursorMovePosition(Offset::Unchanged(), Offset::Forward(distance), true);
}

// Routine Description:
// - CUB - Handles cursor backward movement by given distance
// Arguments:
// - distance - Distance to move
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::CursorBackward(const size_t distance)
{
    return _CursorMovePosition(Offset::Unchanged(), Offset::Backward(distance), true);
}

// Routine Description:
// - CNL - Handles cursor movement to the following line (or N lines down)
// - Moves to the beginning X/Column position of the line.
// Arguments:
// - distance - Distance to move
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::CursorNextLine(const size_t distance)
{
    return _CursorMovePosition(Offset::Forward(distance), Offset::Absolute(1), true);
}

// Routine Description:
// - CPL - Handles cursor movement to the previous line (or N lines up)
// - Moves to the beginning X/Column position of the line.
// Arguments:
// - distance - Distance to move
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::CursorPrevLine(const size_t distance)
{
    return _CursorMovePosition(Offset::Backward(distance), Offset::Absolute(1), true);
}

// Routine Description:
// - Generalizes cursor movement to a specific position, which can be absolute or relative.
// Arguments:
// - rowOffset - The row to move to
// - colOffset - The column to move to
// - clampInMargins - Should the position be clamped within the scrolling margins
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::_CursorMovePosition(const Offset rowOffset, const Offset colOffset, const bool clampInMargins) const
{
    bool success = true;

    // First retrieve some information about the buffer
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    // Make sure to reset the viewport (with MoveToBottom )to where it was
    //      before the user scrolled the console output
    success = (_pConApi->MoveToBottom() && _pConApi->GetConsoleScreenBufferInfoEx(csbiex));

    if (success)
    {
        // Calculate the viewport boundaries as inclusive values.
        // srWindow is exclusive so we need to subtract 1 from the bottom.
        const int viewportTop = csbiex.srWindow.Top;
        const int viewportBottom = csbiex.srWindow.Bottom - 1;

        // Calculate the absolute margins of the scrolling area.
        const int topMargin = viewportTop + _scrollMargins.Top;
        const int bottomMargin = viewportTop + _scrollMargins.Bottom;
        const bool marginsSet = topMargin < bottomMargin;

        // For relative movement, the given offsets will be relative to
        // the current cursor position.
        int row = csbiex.dwCursorPosition.Y;
        int col = csbiex.dwCursorPosition.X;

        // But if the row is absolute, it will be relative to the top of the
        // viewport, or the top margin, depending on the origin mode.
        if (rowOffset.IsAbsolute)
        {
            row = _isOriginModeRelative ? topMargin : viewportTop;
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
        row = std::clamp(row + rowOffset.Value, viewportTop, viewportBottom);
        col = std::clamp(col + colOffset.Value, 0, csbiex.dwSize.X - 1);

        // If the operation needs to be clamped inside the margins, or the origin
        // mode is relative (which always requires margin clamping), then the row
        // may need to be adjusted further.
        if (marginsSet && (clampInMargins || _isOriginModeRelative))
        {
            // See microsoft/terminal#2929 - If the cursor is _below_ the top
            // margin, it should stay below the top margin. If it's _above_ the
            // bottom, it should stay above the bottom. Cursor movements that stay
            // outside the margins shouldn't necessarily be affected. For example,
            // moving up while below the bottom margin shouldn't just jump straight
            // to the bottom margin. See
            // ScreenBufferTests::CursorUpDownOutsideMargins for a test of that
            // behavior.
            if (csbiex.dwCursorPosition.Y >= topMargin)
            {
                row = std::max(row, topMargin);
            }
            if (csbiex.dwCursorPosition.Y <= bottomMargin)
            {
                row = std::min(row, bottomMargin);
            }
        }

        // Finally, attempt to set the adjusted cursor position back into the console.
        const COORD newPos = { gsl::narrow_cast<SHORT>(col), gsl::narrow_cast<SHORT>(row) };
        success = _pConApi->SetConsoleCursorPosition(newPos);
    }

    return success;
}

// Routine Description:
// - CHA - Moves the cursor to an exact X/Column position on the current line.
// Arguments:
// - column - Specific X/Column position to move to
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::CursorHorizontalPositionAbsolute(const size_t column)
{
    return _CursorMovePosition(Offset::Unchanged(), Offset::Absolute(column), false);
}

// Routine Description:
// - VPA - Moves the cursor to an exact Y/row position on the current column.
// Arguments:
// - line - Specific Y/Row position to move to
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::VerticalLinePositionAbsolute(const size_t line)
{
    return _CursorMovePosition(Offset::Absolute(line), Offset::Unchanged(), false);
}

// Routine Description:
// - HPR - Handles cursor forward movement by given distance
// - Unlike CUF, this is not constrained by margin settings.
// Arguments:
// - distance - Distance to move
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::HorizontalPositionRelative(const size_t distance)
{
    return _CursorMovePosition(Offset::Unchanged(), Offset::Forward(distance), false);
}

// Routine Description:
// - VPR - Handles cursor downward movement by given distance
// - Unlike CUD, this is not constrained by margin settings.
// Arguments:
// - distance - Distance to move
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::VerticalPositionRelative(const size_t distance)
{
    return _CursorMovePosition(Offset::Forward(distance), Offset::Unchanged(), false);
}

// Routine Description:
// - CUP - Moves the cursor to an exact X/Column and Y/Row/Line coordinate position.
// Arguments:
// - line - Specific Y/Row/Line position to move to
// - column - Specific X/Column position to move to
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::CursorPosition(const size_t line, const size_t column)
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
// - True if handled successfully. False otherwise.
bool AdaptDispatch::CursorSaveState()
{
    // First retrieve some information about the buffer
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    // Make sure to reset the viewport (with MoveToBottom )to where it was
    //      before the user scrolled the console output
    bool success = (_pConApi->MoveToBottom() && _pConApi->GetConsoleScreenBufferInfoEx(csbiex));

    TextAttribute attributes;
    success = success && (_pConApi->PrivateGetTextAttributes(attributes));

    if (success)
    {
        // The cursor is given to us by the API as relative to the whole buffer.
        // But in VT speak, the cursor row should be relative to the current viewport top.
        COORD coordCursor = csbiex.dwCursorPosition;
        coordCursor.Y -= csbiex.srWindow.Top;

        // VT is also 1 based, not 0 based, so correct by 1.
        auto& savedCursorState = _savedCursorState.at(_usingAltBuffer);
        savedCursorState.Column = coordCursor.X + 1;
        savedCursorState.Row = coordCursor.Y + 1;
        savedCursorState.IsOriginModeRelative = _isOriginModeRelative;
        savedCursorState.Attributes = attributes;
        savedCursorState.TermOutput = _termOutput;
        _pConApi->GetConsoleOutputCP(savedCursorState.CodePage);
    }

    return success;
}

// Routine Description:
// - DECRC - Restores a saved "cursor state" from the DECSC command back into
//   the console state. This includes the cursor position, origin mode, graphic
//   rendition, and active character set.
// Arguments:
// - <none>
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::CursorRestoreState()
{
    auto& savedCursorState = _savedCursorState.at(_usingAltBuffer);

    auto row = savedCursorState.Row;
    const auto col = savedCursorState.Column;

    // If the origin mode is relative, and the scrolling region is set (the bottom is non-zero),
    // we need to make sure the restored position is clamped within the margins.
    if (savedCursorState.IsOriginModeRelative && _scrollMargins.Bottom != 0)
    {
        // VT origin is at 1,1 so we need to add 1 to these margins.
        row = std::clamp(row, _scrollMargins.Top + 1u, _scrollMargins.Bottom + 1u);
    }

    // The saved coordinates are always absolute, so we need reset the origin mode temporarily.
    _isOriginModeRelative = false;
    bool success = CursorPosition(row, col);

    // Once the cursor position is restored, we can then restore the actual origin mode.
    _isOriginModeRelative = savedCursorState.IsOriginModeRelative;

    // Restore text attributes.
    success = (_pConApi->PrivateSetTextAttributes(savedCursorState.Attributes)) && success;

    // Restore designated character set.
    _termOutput = savedCursorState.TermOutput;

    // Restore the code page if it was previously saved.
    if (savedCursorState.CodePage != 0)
    {
        success = _pConApi->SetConsoleOutputCP(savedCursorState.CodePage);
    }

    return success;
}

// Routine Description:
// - DECTCEM - Sets the show/hide visibility status of the cursor.
// Arguments:
// - fIsVisible - Turns the cursor rendering on (TRUE) or off (FALSE).
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::CursorVisibility(const bool fIsVisible)
{
    // This uses a private API instead of the public one, because the public API
    //      will set the cursor shape back to legacy.
    return _pConApi->PrivateShowCursor(fIsVisible);
}

// Routine Description:
// - This helper will do the work of performing an insert or delete character operation
// - Both operations are similar in that they cut text and move it left or right in the buffer, padding the leftover area with spaces.
// Arguments:
// - count - The number of characters to insert
// - isInsert - TRUE if insert mode (cut and paste to the right, away from the cursor). FALSE if delete mode (cut and paste to the left, toward the cursor)
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::_InsertDeleteHelper(const size_t count, const bool isInsert) const
{
    // We'll be doing short math on the distance since all console APIs use shorts. So check that we can successfully convert the uint into a short first.
    SHORT distance;
    RETURN_BOOL_IF_FALSE(SUCCEEDED(SizeTToShort(count, &distance)));

    // get current cursor, attributes
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    // Make sure to reset the viewport (with MoveToBottom )to where it was
    //      before the user scrolled the console output
    RETURN_BOOL_IF_FALSE(_pConApi->MoveToBottom());
    RETURN_BOOL_IF_FALSE(_pConApi->GetConsoleScreenBufferInfoEx(csbiex));

    const auto cursor = csbiex.dwCursorPosition;
    // Rectangle to cut out of the existing buffer. This is inclusive.
    // It will be clipped to the buffer boundaries so SHORT_MAX gives us the full buffer width.
    SMALL_RECT srScroll;
    srScroll.Left = cursor.X;
    srScroll.Right = SHORT_MAX;
    srScroll.Top = cursor.Y;
    srScroll.Bottom = srScroll.Top;

    // Paste coordinate for cut text above
    COORD coordDestination;
    coordDestination.Y = cursor.Y;
    coordDestination.X = cursor.X;

    bool success = false;
    if (isInsert)
    {
        // Insert makes space by moving characters out to the right. So move the destination of the cut/paste region.
        success = SUCCEEDED(ShortAdd(coordDestination.X, distance, &coordDestination.X));
    }
    else
    {
        // Delete scrolls the affected region to the left, relying on the clipping rect to actually delete the characters.
        success = SUCCEEDED(ShortSub(coordDestination.X, distance, &coordDestination.X));
    }

    if (success)
    {
        // Note the revealed characters are filled with the standard erase attributes.
        success = _pConApi->PrivateScrollRegion(srScroll, srScroll, coordDestination, true);
    }

    return success;
}

// Routine Description:
// ICH - Insert Character - Blank/default attribute characters will be inserted at the current cursor position.
//     - Each inserted character will push all text in the row to the right.
// Arguments:
// - count - The number of characters to insert
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::InsertCharacter(const size_t count)
{
    return _InsertDeleteHelper(count, true);
}

// Routine Description:
// DCH - Delete Character - The character at the cursor position will be deleted. Blank/attribute characters will
//       be inserted from the right edge of the current line.
// Arguments:
// - count - The number of characters to delete
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::DeleteCharacter(const size_t count)
{
    return _InsertDeleteHelper(count, false);
}

// Routine Description:
// - Internal helper to erase one particular line of the buffer. Either from beginning to the cursor, from the cursor to the end, or the entire line.
// - Used by both erase line (used just once) and by erase screen (used in a loop) to erase a portion of the buffer.
// Arguments:
// - csbiex - Pointer to the console screen buffer that we will be erasing (and getting cursor data from within)
// - eraseType - Enumeration mode of which kind of erase to perform: beginning to cursor, cursor to end, or entire line.
// - lineId - The line number (array index value, starts at 0) of the line to operate on within the buffer.
//           - This is not aware of circular buffer. Line 0 is always the top visible line if you scrolled the whole way up the window.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::_EraseSingleLineHelper(const CONSOLE_SCREEN_BUFFER_INFOEX& csbiex,
                                           const DispatchTypes::EraseType eraseType,
                                           const size_t lineId) const
{
    COORD coordStartPosition = { 0 };
    if (FAILED(SizeTToShort(lineId, &coordStartPosition.Y)))
    {
        return false;
    }

    // determine start position from the erase type
    // remember that erases are inclusive of the current cursor position.
    switch (eraseType)
    {
    case DispatchTypes::EraseType::FromBeginning:
    case DispatchTypes::EraseType::All:
        coordStartPosition.X = 0; // from beginning and the whole line start from the left most edge of the buffer.
        break;
    case DispatchTypes::EraseType::ToEnd:
        coordStartPosition.X = csbiex.dwCursorPosition.X; // from the current cursor position (including it)
        break;
    }

    DWORD nLength = 0;

    // determine length of erase from erase type
    switch (eraseType)
    {
    case DispatchTypes::EraseType::FromBeginning:
        // +1 because if cursor were at the left edge, the length would be 0 and we want to paint at least the 1 character the cursor is on.
        nLength = csbiex.dwCursorPosition.X + 1;
        break;
    case DispatchTypes::EraseType::ToEnd:
    case DispatchTypes::EraseType::All:
        // Remember the .X value is 1 farther than the right most column in the buffer. Therefore no +1.
        nLength = csbiex.dwSize.X - coordStartPosition.X;
        break;
    }

    // Note that the region is filled with the standard erase attributes.
    return _pConApi->PrivateFillRegion(coordStartPosition, nLength, L' ', true);
}

// Routine Description:
// - ECH - Erase Characters from the current cursor position, by replacing
//     them with a space. This will only erase characters in the current line,
//     and won't wrap to the next. The attributes of any erased positions
//     receive the currently selected attributes.
// Arguments:
// - numChars - The number of characters to erase.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::EraseCharacters(const size_t numChars)
{
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    bool success = _pConApi->GetConsoleScreenBufferInfoEx(csbiex);

    if (success)
    {
        const COORD startPosition = csbiex.dwCursorPosition;

        const SHORT remainingSpaces = csbiex.dwSize.X - startPosition.X;
        const size_t actualRemaining = gsl::narrow_cast<size_t>((remainingSpaces < 0) ? 0 : remainingSpaces);
        // erase at max the number of characters remaining in the line from the current position.
        const auto eraseLength = (numChars <= actualRemaining) ? numChars : actualRemaining;

        // Note that the region is filled with the standard erase attributes.
        success = _pConApi->PrivateFillRegion(startPosition, eraseLength, L' ', true);
    }
    return success;
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
    // First things first. If this is a "Scrollback" clear, then just do that.
    // Scrollback clears erase everything in the "scrollback" of a *nix terminal
    //      Everything that's scrolled off the screen so far.
    // Or if it's an Erase All, then we also need to handle that specially
    //      by moving the current contents of the viewport into the scrollback.
    if (eraseType == DispatchTypes::EraseType::Scrollback)
    {
        const bool eraseScrollbackResult = _EraseScrollback();
        // GH#2715 - If this succeeded, but we're in a conpty, return `false` to
        // make the state machine propagate this ED sequence to the connected
        // terminal application. While we're in conpty mode, we don't really
        // have a scrollback, but the attached terminal might.
        const bool isPty = _pConApi->IsConsolePty();
        return eraseScrollbackResult && (!isPty);
    }
    else if (eraseType == DispatchTypes::EraseType::All)
    {
        // GH#5683 - If this succeeded, but we're in a conpty, return `false` to
        // make the state machine propagate this ED sequence to the connected
        // terminal application. While we're in conpty mode, when the client
        // requests a Erase All operation, we need to manually tell the
        // connected terminal to do the same thing, so that the terminal will
        // move it's own buffer contents into the scrollback.
        const bool eraseAllResult = _EraseAll();
        const bool isPty = _pConApi->IsConsolePty();
        return eraseAllResult && (!isPty);
    }

    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    // Make sure to reset the viewport (with MoveToBottom )to where it was
    //      before the user scrolled the console output
    bool success = (_pConApi->MoveToBottom() && _pConApi->GetConsoleScreenBufferInfoEx(csbiex));

    if (success)
    {
        // What we need to erase is grouped into 3 types:
        // 1. Lines before cursor
        // 2. Cursor Line
        // 3. Lines after cursor
        // We erase one or more of these based on the erase type:
        // A. FromBeginning - Erase 1 and Some of 2.
        // B. ToEnd - Erase some of 2 and 3.
        // C. All - Erase 1, 2, and 3.

        // 1. Lines before cursor line
        if (eraseType == DispatchTypes::EraseType::FromBeginning)
        {
            // For beginning and all, erase all complete lines before (above vertically) from the cursor position.
            for (SHORT startLine = csbiex.srWindow.Top; startLine < csbiex.dwCursorPosition.Y; startLine++)
            {
                success = _EraseSingleLineHelper(csbiex, DispatchTypes::EraseType::All, startLine);

                if (!success)
                {
                    break;
                }
            }
        }

        if (success)
        {
            // 2. Cursor Line
            success = _EraseSingleLineHelper(csbiex, eraseType, csbiex.dwCursorPosition.Y);
        }

        if (success)
        {
            // 3. Lines after cursor line
            if (eraseType == DispatchTypes::EraseType::ToEnd)
            {
                // For beginning and all, erase all complete lines after (below vertically) the cursor position.
                // Remember that the viewport bottom value is 1 beyond the viewable area of the viewport.
                for (SHORT startLine = csbiex.dwCursorPosition.Y + 1; startLine < csbiex.srWindow.Bottom; startLine++)
                {
                    success = _EraseSingleLineHelper(csbiex, DispatchTypes::EraseType::All, startLine);

                    if (!success)
                    {
                        break;
                    }
                }
            }
        }
    }

    return success;
}

// Routine Description:
// - EL - Erases the line that the cursor is currently on.
// Arguments:
// - eraseType - Determines whether to erase: From beginning (left edge) to the cursor, from cursor to end (right edge), or the entire line.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::EraseInLine(const DispatchTypes::EraseType eraseType)
{
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    bool success = _pConApi->GetConsoleScreenBufferInfoEx(csbiex);

    if (success)
    {
        success = _EraseSingleLineHelper(csbiex, eraseType, csbiex.dwCursorPosition.Y);
    }

    return success;
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
    bool success = false;

    switch (statusType)
    {
    case DispatchTypes::AnsiStatusType::OS_OperatingStatus:
        success = _OperatingStatus();
        break;
    case DispatchTypes::AnsiStatusType::CPR_CursorPositionReport:
        success = _CursorPositionReport();
        break;
    }

    return success;
}

// Routine Description:
// - DA - Reports the identity of this Virtual Terminal machine to the caller.
//      - In our case, we'll report back to acknowledge we understand, but reveal no "hardware" upgrades like physical terminals of old.
// Arguments:
// - <none>
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::DeviceAttributes()
{
    // See: http://vt100.net/docs/vt100-ug/chapter3.html#DA
    return _WriteResponse(L"\x1b[?1;0c");
}

// Routine Description:
// - VT52 Identify - Reports the identity of the terminal in VT52 emulation mode.
//   An actual VT52 terminal would typically identify itself with ESC / K.
//   But for a terminal that is emulating a VT52, the sequence should be ESC / Z.
// Arguments:
// - <none>
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::Vt52DeviceAttributes()
{
    return _WriteResponse(L"\x1b/Z");
}

// Routine Description:
// - DSR-OS - Reports the operating status back to the input channel
// Arguments:
// - <none>
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::_OperatingStatus() const
{
    // We always report a good operating condition.
    return _WriteResponse(L"\x1b[0n");
}

// Routine Description:
// - DSR-CPR - Reports the current cursor position within the viewport back to the input channel
// Arguments:
// - <none>
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::_CursorPositionReport() const
{
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    // Make sure to reset the viewport (with MoveToBottom )to where it was
    //      before the user scrolled the console output
    bool success = (_pConApi->MoveToBottom() && _pConApi->GetConsoleScreenBufferInfoEx(csbiex));

    if (success)
    {
        // First pull the cursor position relative to the entire buffer out of the console.
        COORD coordCursorPos = csbiex.dwCursorPosition;

        // Now adjust it for its position in respect to the current viewport top.
        coordCursorPos.Y -= csbiex.srWindow.Top;

        // NOTE: 1,1 is the top-left corner of the viewport in VT-speak, so add 1.
        coordCursorPos.X++;
        coordCursorPos.Y++;

        // If the origin mode is relative, line numbers start at top margin of the scrolling region.
        if (_isOriginModeRelative)
        {
            coordCursorPos.Y -= _scrollMargins.Top;
        }

        // Now send it back into the input channel of the console.
        // First format the response string.
        const auto response = wil::str_printf<std::wstring>(L"\x1b[%d;%dR", coordCursorPos.Y, coordCursorPos.X);
        success = _WriteResponse(response);
    }

    return success;
}

// Routine Description:
// - Helper to send a string reply to the input stream of the console.
// - Used by various commands where the program attached would like a reply to one of the commands issued.
// - This will generate two "key presses" (one down, one up) for every character in the string and place them into the head of the console's input stream.
// Arguments:
// - reply - The reply string to transmit back to the input stream
// Return Value:
// - True if the string was converted to input events and placed into the console input buffer successfully. False otherwise.
bool AdaptDispatch::_WriteResponse(const std::wstring_view reply) const
{
    bool success = false;
    std::deque<std::unique_ptr<IInputEvent>> inEvents;
    try
    {
        // generate a paired key down and key up event for every
        // character to be sent into the console's input buffer
        for (const auto& wch : reply)
        {
            // This wasn't from a real keyboard, so we're leaving key/scan codes blank.
            KeyEvent keyEvent{ TRUE, 1, 0, 0, wch, 0 };

            inEvents.push_back(std::make_unique<KeyEvent>(keyEvent));
            keyEvent.SetKeyDown(false);
            inEvents.push_back(std::make_unique<KeyEvent>(keyEvent));
        }
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
        return false;
    }

    size_t eventsWritten;
    success = _pConApi->PrivatePrependConsoleInput(inEvents, eventsWritten);

    return success;
}

// Routine Description:
// - Generalizes scrolling movement for up/down
// Arguments:
// - scrollDirection - Specific direction to move
// - distance - Magnitude of the move
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::_ScrollMovement(const ScrollDirection scrollDirection, const size_t distance) const
{
    // We'll be doing short math on the distance since all console APIs use shorts. So check that we can successfully convert the size_t into a short first.
    SHORT dist;
    bool success = SUCCEEDED(SizeTToShort(distance, &dist));

    if (success)
    {
        // get current cursor
        CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
        csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
        // Make sure to reset the viewport (with MoveToBottom )to where it was
        //      before the user scrolled the console output
        success = (_pConApi->MoveToBottom() && _pConApi->GetConsoleScreenBufferInfoEx(csbiex));

        if (success)
        {
            // Rectangle to cut out of the existing buffer. This is inclusive.
            // It will be clipped to the buffer boundaries so SHORT_MAX gives us the full buffer width.
            SMALL_RECT srScreen;
            srScreen.Left = 0;
            srScreen.Right = SHORT_MAX;
            srScreen.Top = csbiex.srWindow.Top;
            srScreen.Bottom = csbiex.srWindow.Bottom - 1; // srWindow is exclusive, hence the - 1
            // Clip to the DECSTBM margin boundaries
            if (_scrollMargins.Top < _scrollMargins.Bottom)
            {
                srScreen.Top = csbiex.srWindow.Top + _scrollMargins.Top;
                srScreen.Bottom = csbiex.srWindow.Top + _scrollMargins.Bottom;
            }

            // Paste coordinate for cut text above
            COORD coordDestination;
            coordDestination.X = srScreen.Left;
            coordDestination.Y = srScreen.Top + dist * (scrollDirection == ScrollDirection::Up ? -1 : 1);

            // Note the revealed lines are filled with the standard erase attributes.
            success = _pConApi->PrivateScrollRegion(srScreen, srScreen, coordDestination, true);
        }
    }

    return success;
}

// Routine Description:
// - SU - Pans the window DOWN by given distance (distance new lines appear at the bottom of the screen)
// Arguments:
// - distance - Distance to move
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::ScrollUp(const size_t uiDistance)
{
    return _ScrollMovement(ScrollDirection::Up, uiDistance);
}

// Routine Description:
// - SD - Pans the window UP by given distance (distance new lines appear at the top of the screen)
// Arguments:
// - distance - Distance to move
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::ScrollDown(const size_t uiDistance)
{
    return _ScrollMovement(ScrollDirection::Down, uiDistance);
}

// Routine Description:
// - DECSCPP / DECCOLM Sets the number of columns "per page" AKA sets the console width.
// DECCOLM also clear the screen (like a CSI 2 J sequence), while DECSCPP just sets the width.
// (DECCOLM will do this separately of this function)
// Arguments:
// - columns - Number of columns
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::SetColumns(const size_t columns)
{
    SHORT col;
    bool success = SUCCEEDED(SizeTToShort(columns, &col));
    if (success)
    {
        CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
        csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
        success = _pConApi->GetConsoleScreenBufferInfoEx(csbiex);

        if (success)
        {
            csbiex.dwSize.X = col;
            success = _pConApi->SetConsoleScreenBufferInfoEx(csbiex);
        }
    }
    return success;
}

// Routine Description:
// - DECCOLM not only sets the number of columns, but also clears the screen buffer,
//    resets the page margins and origin mode, and places the cursor at 1,1
// Arguments:
// - columns - Number of columns
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::_DoDECCOLMHelper(const size_t columns)
{
    if (!_isDECCOLMAllowed)
    {
        // Only proceed if DECCOLM is allowed. Return true, as this is technically a successful handling.
        return true;
    }

    bool success = SetColumns(columns);
    if (success)
    {
        success = SetOriginMode(false);
        if (success)
        {
            success = CursorPosition(1, 1);
            if (success)
            {
                success = EraseInDisplay(DispatchTypes::EraseType::All);
                if (success)
                {
                    success = _DoSetTopBottomScrollingMargins(0, 0);
                }
            }
        }
    }
    return success;
}

// Routine Description:
// - Support routine for routing private mode parameters to be set/reset as flags
// Arguments:
// - params - array of params to set/reset
// - enable - True for set, false for unset.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::_PrivateModeParamsHelper(const DispatchTypes::PrivateModeParams param, const bool enable)
{
    bool success = false;
    switch (param)
    {
    case DispatchTypes::PrivateModeParams::DECCKM_CursorKeysMode:
        // set - Enable Application Mode, reset - Normal mode
        success = SetCursorKeysMode(enable);
        break;
    case DispatchTypes::PrivateModeParams::DECANM_AnsiMode:
        success = SetAnsiMode(enable);
        break;
    case DispatchTypes::PrivateModeParams::DECCOLM_SetNumberOfColumns:
        success = _DoDECCOLMHelper(enable ? DispatchTypes::s_sDECCOLMSetColumns : DispatchTypes::s_sDECCOLMResetColumns);
        break;
    case DispatchTypes::PrivateModeParams::DECSCNM_ScreenMode:
        success = SetScreenMode(enable);
        break;
    case DispatchTypes::PrivateModeParams::DECOM_OriginMode:
        // The cursor is also moved to the new home position when the origin mode is set or reset.
        success = SetOriginMode(enable) && CursorPosition(1, 1);
        break;
    case DispatchTypes::PrivateModeParams::DECAWM_AutoWrapMode:
        success = SetAutoWrapMode(enable);
        break;
    case DispatchTypes::PrivateModeParams::ATT610_StartCursorBlink:
        success = EnableCursorBlinking(enable);
        break;
    case DispatchTypes::PrivateModeParams::DECTCEM_TextCursorEnableMode:
        success = CursorVisibility(enable);
        break;
    case DispatchTypes::PrivateModeParams::XTERM_EnableDECCOLMSupport:
        success = EnableDECCOLMSupport(enable);
        break;
    case DispatchTypes::PrivateModeParams::VT200_MOUSE_MODE:
        success = EnableVT200MouseMode(enable);
        break;
    case DispatchTypes::PrivateModeParams::BUTTON_EVENT_MOUSE_MODE:
        success = EnableButtonEventMouseMode(enable);
        break;
    case DispatchTypes::PrivateModeParams::ANY_EVENT_MOUSE_MODE:
        success = EnableAnyEventMouseMode(enable);
        break;
    case DispatchTypes::PrivateModeParams::UTF8_EXTENDED_MODE:
        success = EnableUTF8ExtendedMouseMode(enable);
        break;
    case DispatchTypes::PrivateModeParams::SGR_EXTENDED_MODE:
        success = EnableSGRExtendedMouseMode(enable);
        break;
    case DispatchTypes::PrivateModeParams::ALTERNATE_SCROLL:
        success = EnableAlternateScroll(enable);
        break;
    case DispatchTypes::PrivateModeParams::ASB_AlternateScreenBuffer:
        success = enable ? UseAlternateScreenBuffer() : UseMainScreenBuffer();
        break;
    case DispatchTypes::PrivateModeParams::W32IM_Win32InputMode:
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
// - Generalized handler for the setting/resetting of DECSET/DECRST parameters.
//     All params in the rgParams will attempt to be executed, even if one
//     fails, to allow us to successfully re/set params that are chained with
//     params we don't yet support.
// Arguments:
// - params - array of params to set/reset
// - enable - True for set, false for unset.
// Return Value:
// - True if ALL params were handled successfully. False otherwise.
bool AdaptDispatch::_SetResetPrivateModes(const std::basic_string_view<DispatchTypes::PrivateModeParams> params, const bool enable)
{
    // because the user might chain together params we don't support with params we DO support, execute all
    // params in the sequence, and only return failure if we failed at least one of them
    size_t failures = 0;
    for (const auto& p : params)
    {
        failures += _PrivateModeParamsHelper(p, enable) ? 0 : 1; // increment the number of failures if we fail.
    }
    return failures == 0;
}

// Routine Description:
// - DECSET - Enables the given DEC private mode params.
// Arguments:
// - params - array of params to set
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::SetPrivateModes(const std::basic_string_view<DispatchTypes::PrivateModeParams> params)
{
    return _SetResetPrivateModes(params, true);
}

// Routine Description:
// - DECRST - Disables the given DEC private mode params.
// Arguments:
// - params - array of params to reset
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::ResetPrivateModes(const std::basic_string_view<DispatchTypes::PrivateModeParams> params)
{
    return _SetResetPrivateModes(params, false);
}

// - DECKPAM, DECKPNM - Sets the keypad input mode to either Application mode or Numeric mode (true, false respectively)
// Arguments:
// - applicationMode - set to true to enable Application Mode Input, false for Numeric Mode Input.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::SetKeypadMode(const bool fApplicationMode)
{
    bool success = true;
    success = _pConApi->PrivateSetKeypadMode(fApplicationMode);

    if (_ShouldPassThroughInputModeChange())
    {
        return false;
    }

    return success;
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
    bool success = true;
    success = _pConApi->PrivateEnableWin32InputMode(win32InputMode);

    if (_ShouldPassThroughInputModeChange())
    {
        return false;
    }

    return success;
}

// - DECCKM - Sets the cursor keys input mode to either Application mode or Normal mode (true, false respectively)
// Arguments:
// - applicationMode - set to true to enable Application Mode Input, false for Normal Mode Input.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::SetCursorKeysMode(const bool applicationMode)
{
    bool success = true;
    success = _pConApi->PrivateSetCursorKeysMode(applicationMode);

    if (_ShouldPassThroughInputModeChange())
    {
        return false;
    }

    return success;
}

// - att610 - Enables or disables the cursor blinking.
// Arguments:
// - enable - set to true to enable blinking, false to disable
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::EnableCursorBlinking(const bool enable)
{
    return _pConApi->PrivateAllowCursorBlinking(enable);
}

// Routine Description:
// - IL - This control function inserts one or more blank lines, starting at the cursor.
//    As lines are inserted, lines below the cursor and in the scrolling region move down.
//    Lines scrolled off the page are lost. IL has no effect outside the page margins.
// Arguments:
// - distance - number of lines to insert
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::InsertLine(const size_t distance)
{
    return _pConApi->InsertLines(distance);
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
// - True if handled successfully. False otherwise.
bool AdaptDispatch::DeleteLine(const size_t distance)
{
    return _pConApi->DeleteLines(distance);
}

// - DECANM - Sets the terminal emulation mode to either ANSI-compatible or VT52.
// Arguments:
// - ansiMode - set to true to enable the ANSI mode, false for VT52 mode.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::SetAnsiMode(const bool ansiMode)
{
    // When an attempt is made to update the mode, the designated character sets
    // need to be reset to defaults, even if the mode doesn't actually change.
    _termOutput = {};

    return _pConApi->PrivateSetAnsiMode(ansiMode);
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
    return _pConApi->PrivateSetScreenMode(reverseMode);
}

// Routine Description:
// - DECOM - Sets the cursor addressing origin mode to relative or absolute.
//    When relative, line numbers start at top margin of the user-defined scrolling region.
//    When absolute, line numbers are independent of the scrolling region.
// Arguments:
// - relativeMode - set to true to use relative addressing, false for absolute addressing.
// Return Value:
// - True if handled successfully. False otherwise.
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
// - True if handled successfully. False otherwise.
bool AdaptDispatch::SetAutoWrapMode(const bool wrapAtEOL)
{
    return _pConApi->PrivateSetAutoWrapMode(wrapAtEOL);
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
// - True if handled successfully. False otherwise.
bool AdaptDispatch::_DoSetTopBottomScrollingMargins(const size_t topMargin,
                                                    const size_t bottomMargin)
{
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    // Make sure to reset the viewport (with MoveToBottom )to where it was
    //      before the user scrolled the console output
    bool success = (_pConApi->MoveToBottom() && _pConApi->GetConsoleScreenBufferInfoEx(csbiex));

    // so notes time: (input -> state machine out -> adapter out -> conhost internal)
    // having only a top param is legal         ([3;r   -> 3,0   -> 3,h  -> 3,h,true)
    // having only a bottom param is legal      ([;3r   -> 0,3   -> 1,3  -> 1,3,true)
    // having neither uses the defaults         ([;r [r -> 0,0   -> 0,0  -> 0,0,false)
    // an illegal combo (eg, 3;2r) is ignored
    if (success)
    {
        SHORT actualTop = 0;
        SHORT actualBottom = 0;
        success = SUCCEEDED(SizeTToShort(topMargin, &actualTop)) && SUCCEEDED(SizeTToShort(bottomMargin, &actualBottom));
        if (success)
        {
            const SHORT screenHeight = csbiex.srWindow.Bottom - csbiex.srWindow.Top;
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
            success = (actualTop < actualBottom && actualBottom <= screenHeight);
            if (success)
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
                success = _pConApi->PrivateSetScrollingRegion(_scrollMargins);
            }
        }
    }
    return success;
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
// - True if handled successfully. False otherwise.
bool AdaptDispatch::SetTopBottomScrollingMargins(const size_t topMargin,
                                                 const size_t bottomMargin)
{
    // When this is called, the cursor should also be moved to home.
    // Other functions that only need to set/reset the margins should call _DoSetTopBottomScrollingMargins
    return _DoSetTopBottomScrollingMargins(topMargin, bottomMargin) && CursorPosition(1, 1);
}

// Routine Description:
// - BEL - Rings the warning bell.
//    Causes the terminal to emit an audible tone of brief duration.
// Arguments:
// - None
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::WarningBell()
{
    return _pConApi->PrivateWarningBell();
}

// Routine Description:
// - CR - Performs a carriage return.
//    Moves the cursor to the leftmost column.
// Arguments:
// - None
// Return Value:
// - True if handled successfully. False otherwise.
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
        return _pConApi->PrivateLineFeed(_pConApi->PrivateGetLineFeedMode());
    case DispatchTypes::LineFeedType::WithoutReturn:
        return _pConApi->PrivateLineFeed(false);
    case DispatchTypes::LineFeedType::WithReturn:
        return _pConApi->PrivateLineFeed(true);
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
// - True if handled successfully. False otherwise.
bool AdaptDispatch::ReverseLineFeed()
{
    return _pConApi->PrivateReverseLineFeed();
}

// Routine Description:
// - OSC Set Window Title - Sets the title of the window
// Arguments:
// - title - The string to set the title to. Must be null terminated.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::SetWindowTitle(std::wstring_view title)
{
    return _pConApi->SetConsoleTitleW(title);
}

// - ASBSET - Creates and swaps to the alternate screen buffer. In virtual terminals, there exists both a "main"
//     screen buffer and an alternate. ASBSET creates a new alternate, and switches to it. If there is an already
//     existing alternate, it is discarded.
// Arguments:
// - None
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::UseAlternateScreenBuffer()
{
    bool success = CursorSaveState();
    if (success)
    {
        success = _pConApi->PrivateUseAlternateScreenBuffer();
        if (success)
        {
            _usingAltBuffer = true;
        }
    }
    return success;
}

// Routine Description:
// - ASBRST - From the alternate buffer, returns to the main screen buffer.
//     From the main screen buffer, does nothing. The alternate is discarded.
// Arguments:
// - None
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::UseMainScreenBuffer()
{
    bool success = _pConApi->PrivateUseMainScreenBuffer();
    if (success)
    {
        _usingAltBuffer = false;
        if (success)
        {
            success = CursorRestoreState();
        }
    }
    return success;
}

//Routine Description:
// HTS - sets a VT tab stop in the cursor's current column.
//Arguments:
// - None
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::HorizontalTabSet()
{
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    const bool success = _pConApi->GetConsoleScreenBufferInfoEx(csbiex);
    if (success)
    {
        const auto width = csbiex.dwSize.X;
        const auto column = csbiex.dwCursorPosition.X;

        _InitTabStopsForWidth(width);
        _tabStopColumns.at(column) = true;
    }
    return success;
}

//Routine Description:
// CHT - performing a forwards tab. This will take the
//     cursor to the tab stop following its current location. If there are no
//     more tabs in this row, it will take it to the right side of the window.
//     If it's already in the last column of the row, it will move it to the next line.
//Arguments:
// - numTabs - the number of tabs to perform
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::ForwardTab(const size_t numTabs)
{
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    bool success = _pConApi->GetConsoleScreenBufferInfoEx(csbiex);
    if (success)
    {
        const auto width = csbiex.dwSize.X;
        const auto row = csbiex.dwCursorPosition.Y;
        auto column = csbiex.dwCursorPosition.X;
        auto tabsPerformed = 0u;

        _InitTabStopsForWidth(width);
        while (column + 1 < width && tabsPerformed < numTabs)
        {
            column++;
            if (til::at(_tabStopColumns, column))
            {
                tabsPerformed++;
            }
        }

        success = _pConApi->SetConsoleCursorPosition({ column, row });
    }
    return success;
}

//Routine Description:
// CBT - performing a backwards tab. This will take the cursor to the tab stop
//     previous to its current location. It will not reverse line feed.
//Arguments:
// - numTabs - the number of tabs to perform
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::BackwardsTab(const size_t numTabs)
{
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    bool success = _pConApi->GetConsoleScreenBufferInfoEx(csbiex);
    if (success)
    {
        const auto width = csbiex.dwSize.X;
        const auto row = csbiex.dwCursorPosition.Y;
        auto column = csbiex.dwCursorPosition.X;
        auto tabsPerformed = 0u;

        _InitTabStopsForWidth(width);
        while (column > 0 && tabsPerformed < numTabs)
        {
            column--;
            if (til::at(_tabStopColumns, column))
            {
                tabsPerformed++;
            }
        }

        success = _pConApi->SetConsoleCursorPosition({ column, row });
    }
    return success;
}

//Routine Description:
// TBC - Used to clear set tab stops. ClearType ClearCurrentColumn (0) results
//     in clearing only the tab stop in the cursor's current column, if there
//     is one. ClearAllColumns (3) results in resetting all set tab stops.
//Arguments:
// - clearType - Whether to clear the current column, or all columns, defined in DispatchTypes::TabClearType
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::TabClear(const size_t clearType)
{
    bool success = false;
    switch (clearType)
    {
    case DispatchTypes::TabClearType::ClearCurrentColumn:
        success = _ClearSingleTabStop();
        break;
    case DispatchTypes::TabClearType::ClearAllColumns:
        success = _ClearAllTabStops();
        break;
    }
    return success;
}

// Routine Description:
// - Clears the tab stop in the cursor's current column, if there is one.
// Arguments:
// - <none>
// Return value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::_ClearSingleTabStop()
{
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    const bool success = _pConApi->GetConsoleScreenBufferInfoEx(csbiex);
    if (success)
    {
        const auto width = csbiex.dwSize.X;
        const auto column = csbiex.dwCursorPosition.X;

        _InitTabStopsForWidth(width);
        _tabStopColumns.at(column) = false;
    }
    return success;
}

// Routine Description:
// - Clears all tab stops and resets the _initDefaultTabStops flag to indicate
//    that they shouldn't be reinitialized at the default positions.
// Arguments:
// - <none>
// Return value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::_ClearAllTabStops() noexcept
{
    _tabStopColumns.clear();
    _initDefaultTabStops = false;
    return true;
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
// - width - the width of the screen buffer that we need to accomodate
// Return value:
// - <none>
void AdaptDispatch::_InitTabStopsForWidth(const size_t width)
{
    const auto initialWidth = _tabStopColumns.size();
    if (width > initialWidth)
    {
        _tabStopColumns.resize(width);
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
//     When ISO2022 is selected, the code page is set to ISO-8859-1, and both
//     GL and GR areas of the code table can be remapped. When UTF8 is selected,
//     the code page is set to UTF-8, and only the GL area can be remapped.
//Arguments:
// - codingSystem - The coding system that will be selected.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::DesignateCodingSystem(const wchar_t codingSystem)
{
    // If we haven't previously saved the initial code page, do so now.
    // This will be used to restore the code page in response to a reset.
    if (!_initialCodePage.has_value())
    {
        unsigned int currentCodePage;
        _pConApi->GetConsoleOutputCP(currentCodePage);
        _initialCodePage = currentCodePage;
    }

    bool success = false;
    switch (codingSystem)
    {
    case DispatchTypes::CodingSystem::ISO2022:
        success = _pConApi->SetConsoleOutputCP(28591);
        if (success)
        {
            _termOutput.EnableGrTranslation(true);
        }
        break;
    case DispatchTypes::CodingSystem::UTF8:
        success = _pConApi->SetConsoleOutputCP(CP_UTF8);
        if (success)
        {
            _termOutput.EnableGrTranslation(false);
        }
        break;
    }
    return success;
}

//Routine Description:
// Designate Charset - Selects a specific 94-character set into one of the four G-sets.
//     See http://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Controls-beginning-with-ESC
//       for a list of all charsets and their codes.
//     If the specified charset is unsupported, we do nothing (remain on the current one)
//Arguments:
// - gsetNumber - The G-set into which the charset will be selected.
// - charset - The characters indicating the charset that will be used.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::Designate94Charset(const size_t gsetNumber, const std::pair<wchar_t, wchar_t> charset)
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
// - charset - The characters indicating the charset that will be used.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::Designate96Charset(const size_t gsetNumber, const std::pair<wchar_t, wchar_t> charset)
{
    return _termOutput.Designate96Charset(gsetNumber, charset);
}

//Routine Description:
// Locking Shift - Invoke one of the G-sets into the left half of the code table.
//Arguments:
// - gsetNumber - The G-set that will be invoked.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::LockingShift(const size_t gsetNumber)
{
    return _termOutput.LockingShift(gsetNumber);
}

//Routine Description:
// Locking Shift Right - Invoke one of the G-sets into the right half of the code table.
//Arguments:
// - gsetNumber - The G-set that will be invoked.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::LockingShiftRight(const size_t gsetNumber)
{
    return _termOutput.LockingShiftRight(gsetNumber);
}

//Routine Description:
// Single Shift - Temporarily invoke one of the G-sets into the code table.
//Arguments:
// - gsetNumber - The G-set that will be invoked.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::SingleShift(const size_t gsetNumber)
{
    return _termOutput.SingleShift(gsetNumber);
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
    const bool isPty = _pConApi->IsConsolePty();

    bool success = CursorVisibility(true); // Cursor enabled.
    if (success)
    {
        success = SetOriginMode(false); // Absolute cursor addressing.
    }
    if (success)
    {
        success = SetAutoWrapMode(true); // Wrap at end of line.
    }
    if (success)
    {
        success = SetCursorKeysMode(false); // Normal characters.
    }
    // SetCursorKeysMode will return false if we're in conpty mode, as to
    // trigger a passthrough. If that's the case, just power through here.
    if (success || isPty)
    {
        success = SetKeypadMode(false); // Numeric characters.
    }
    // SetKeypadMode will return false if we're in conpty mode, as to trigger a
    // passthrough. If that's the case, just power through here.
    if (success || isPty)
    {
        // Top margin = 1; bottom margin = page length.
        success = _DoSetTopBottomScrollingMargins(0, 0);
    }
    if (success)
    {
        _termOutput = {}; // Reset all character set designations.
        if (_initialCodePage.has_value())
        {
            // Restore initial code page if previously changed by a DOCS sequence.
            success = _pConApi->SetConsoleOutputCP(_initialCodePage.value());
        }
    }
    if (success)
    {
        const auto opt = DispatchTypes::GraphicsOptions::Off;
        success = SetGraphicsRendition({ &opt, 1 }); // Normal rendition.
    }
    if (success)
    {
        // Reset the saved cursor state.
        // Note that XTerm only resets the main buffer state, but that
        // seems likely to be a bug. Most other terminals reset both.
        _savedCursorState.at(0) = {}; // Main buffer
        _savedCursorState.at(1) = {}; // Alt buffer
    }

    return success;
}

//Routine Description:
// Full Reset - Perform a hard reset of the terminal. http://vt100.net/docs/vt220-rm/chapter4.html
//  RIS performs the following actions: (Items with sub-bullets are supported)
//   - Switches to the main screen buffer if in the alt buffer.
//      * This matches the XTerm behaviour, which is the de facto standard for the alt buffer.
//   - Performs a communications line disconnect.
//   - Clears UDKs.
//   - Clears a down-line-loaded character set.
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
    bool success = true;

    // If in the alt buffer, switch back to main before doing anything else.
    if (_usingAltBuffer)
    {
        success = _pConApi->PrivateUseMainScreenBuffer();
        _usingAltBuffer = !success;
    }

    // Sets the SGR state to normal - this must be done before EraseInDisplay
    //      to ensure that it clears with the default background color.
    if (success)
    {
        success = SoftReset();
    }

    // Clears the screen - Needs to be done in two operations.
    if (success)
    {
        success = EraseInDisplay(DispatchTypes::EraseType::All);
    }
    if (success)
    {
        success = _EraseScrollback();
    }

    // Set the DECSCNM screen mode back to normal.
    if (success)
    {
        success = SetScreenMode(false);
    }

    // Cursor to 1,1 - the Soft Reset guarantees this is absolute
    if (success)
    {
        success = CursorPosition(1, 1);
    }

    // delete all current tab stops and reapply
    _ResetTabStops();

    // GH#2715 - If all this succeeded, but we're in a conpty, return `false` to
    // make the state machine propagate this RIS sequence to the connected
    // terminal application. We've reset our state, but the connected terminal
    // might need to do more.
    if (_pConApi->IsConsolePty())
    {
        return false;
    }

    return success;
}

// Routine Description:
// - DECALN - Fills the entire screen with a test pattern of uppercase Es,
//    resets the margins and rendition attributes, and moves the cursor to
//    the home position.
// Arguments:
// - None
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::ScreenAlignmentPattern()
{
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    // Make sure to reset the viewport (with MoveToBottom )to where it was
    //      before the user scrolled the console output
    bool success = _pConApi->MoveToBottom() && _pConApi->GetConsoleScreenBufferInfoEx(csbiex);

    if (success)
    {
        // Fill the screen with the letter E using the default attributes.
        auto fillPosition = COORD{ 0, csbiex.srWindow.Top };
        const auto fillLength = (csbiex.srWindow.Bottom - csbiex.srWindow.Top) * csbiex.dwSize.X;
        success = _pConApi->PrivateFillRegion(fillPosition, fillLength, L'E', false);
        // Reset the meta/extended attributes (but leave the colors unchanged).
        TextAttribute attr;
        if (_pConApi->PrivateGetTextAttributes(attr))
        {
            attr.SetStandardErase();
            success = success && _pConApi->PrivateSetTextAttributes(attr);
        }
        // Reset the origin mode to absolute.
        success = success && SetOriginMode(false);
        // Clear the scrolling margins.
        success = success && _DoSetTopBottomScrollingMargins(0, 0);
        // Set the cursor position to home.
        success = success && CursorPosition(1, 1);
    }

    return success;
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
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(csbiex);
    // Make sure to reset the viewport (with MoveToBottom )to where it was
    //      before the user scrolled the console output
    bool success = (_pConApi->GetConsoleScreenBufferInfoEx(csbiex) && _pConApi->MoveToBottom());
    if (success)
    {
        const SMALL_RECT screen = csbiex.srWindow;
        const SHORT height = screen.Bottom - screen.Top;
        FAIL_FAST_IF(!(height > 0));
        const COORD cursor = csbiex.dwCursorPosition;

        // Rectangle to cut out of the existing buffer
        // It will be clipped to the buffer boundaries so SHORT_MAX gives us the full buffer width.
        SMALL_RECT scroll = screen;
        scroll.Left = 0;
        scroll.Right = SHORT_MAX;
        // Paste coordinate for cut text above
        COORD destination;
        destination.X = 0;
        destination.Y = 0;

        // Typically a scroll operation should fill with standard erase attributes, but in
        // this case we need to use the default attributes, hence standardFillAttrs is false.
        success = _pConApi->PrivateScrollRegion(scroll, std::nullopt, destination, false);
        if (success)
        {
            // Clear everything after the viewport.
            const DWORD totalAreaBelow = csbiex.dwSize.X * (csbiex.dwSize.Y - height);
            const COORD coordBelowStartPosition = { 0, height };
            // Again we need to use the default attributes, hence standardFillAttrs is false.
            success = _pConApi->PrivateFillRegion(coordBelowStartPosition, totalAreaBelow, L' ', false);

            if (success)
            {
                // Move the viewport (CAN'T be done in one call with SetConsolescreenBufferInfoEx, because legacy)
                SMALL_RECT newViewport;
                newViewport.Left = screen.Left;
                newViewport.Top = 0;
                // SetConsoleWindowInfo uses an inclusive rect, while GetConsolescreenBufferInfo is exclusive
                newViewport.Right = screen.Right - 1;
                newViewport.Bottom = height - 1;
                success = _pConApi->SetConsoleWindowInfo(true, newViewport);

                if (success)
                {
                    // Move the cursor to the same relative location.
                    const COORD newcursor = { cursor.X, cursor.Y - screen.Top };
                    success = _pConApi->SetConsoleCursorPosition(newcursor);
                }
            }
        }
    }
    return success;
}

//Routine Description:
// Erase All (^[[2J - ED)
//  Erase the current contents of the viewport. In most terminals, because they
//      only have a scrollback (and not a buffer per-se), they implement this
//      by scrolling the current contents of the buffer off of the screen.
//  We can't properly replicate this behavior with only the public API, because
//      we need to know where the last character in the buffer is. (it may be below the viewport)
//Arguments:
// <none>
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::_EraseAll()
{
    return _pConApi->PrivateEraseAll();
}

// Routine Description:
// - Enables or disables support for the DECCOLM escape sequence.
// Arguments:
// - enabled - set to true to allow DECCOLM to be used, false to disallow.
// Return Value:
// - True if handled successfully. False otherwise.
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
    bool success = true;
    success = _pConApi->PrivateEnableVT200MouseMode(enabled);

    if (_ShouldPassThroughInputModeChange())
    {
        return false;
    }

    return success;
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
    bool success = true;
    success = _pConApi->PrivateEnableUTF8ExtendedMouseMode(enabled);

    if (_ShouldPassThroughInputModeChange())
    {
        return false;
    }

    return success;
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
    bool success = true;
    success = _pConApi->PrivateEnableSGRExtendedMouseMode(enabled);

    if (_ShouldPassThroughInputModeChange())
    {
        return false;
    }

    return success;
}

//Routine Description:
// Enable Button Event mode - send mouse move events WITH A BUTTON PRESSED to the input.
//Arguments:
// - enabled - true to enable, false to disable.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::EnableButtonEventMouseMode(const bool enabled)
{
    bool success = true;
    success = _pConApi->PrivateEnableButtonEventMouseMode(enabled);

    if (_ShouldPassThroughInputModeChange())
    {
        return false;
    }

    return success;
}

//Routine Description:
// Enable Any Event mode - send all mouse events to the input.

//Arguments:
// - enabled - true to enable, false to disable.
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::EnableAnyEventMouseMode(const bool enabled)
{
    bool success = true;
    success = _pConApi->PrivateEnableAnyEventMouseMode(enabled);

    if (_ShouldPassThroughInputModeChange())
    {
        return false;
    }

    return success;
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
    bool success = true;
    success = _pConApi->PrivateEnableAlternateScroll(enabled);

    if (_ShouldPassThroughInputModeChange())
    {
        return false;
    }

    return success;
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
    CursorType actualType = CursorType::Legacy;
    bool fEnableBlinking = false;

    switch (cursorStyle)
    {
    case DispatchTypes::CursorStyle::BlinkingBlock:
    case DispatchTypes::CursorStyle::BlinkingBlockDefault:
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
    }

    bool success = _pConApi->SetCursorStyle(actualType);
    if (success)
    {
        success = _pConApi->PrivateAllowCursorBlinking(fEnableBlinking);
    }

    // If we're a conpty, always return false, so that this cursor state will be
    // sent to the connected terminal
    if (_pConApi->IsConsolePty())
    {
        return false;
    }

    return success;
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
    if (_pConApi->IsConsolePty())
    {
        return false;
    }

    return _pConApi->SetCursorColor(cursorColor);
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
    const bool success = _pConApi->PrivateSetColorTableEntry(tableIndex, dwColor);

    // If we're a conpty, always return false, so that we send the updated color
    //      value to the terminal. Still handle the sequence so apps that use
    //      the API or VT to query the values of the color table still read the
    //      correct color.
    if (_pConApi->IsConsolePty())
    {
        return false;
    }

    return success;
}

// Method Description:
// - Sets the default foreground color to a new value
// Arguments:
// - dwColor: The new RGB color value to use, as a COLORREF, format 0x00BBGGRR.
// Return Value:
// True if handled successfully. False otherwise.
bool Microsoft::Console::VirtualTerminal::AdaptDispatch::SetDefaultForeground(const DWORD dwColor)
{
    bool success = true;
    success = _pConApi->PrivateSetDefaultForeground(dwColor);

    // If we're a conpty, always return false, so that we send the updated color
    //      value to the terminal. Still handle the sequence so apps that use
    //      the API or VT to query the values of the color table still read the
    //      correct color.
    if (_pConApi->IsConsolePty())
    {
        return false;
    }

    return success;
}

// Method Description:
// - Sets the default background color to a new value
// Arguments:
// - dwColor: The new RGB color value to use, as a COLORREF, format 0x00BBGGRR.
// Return Value:
// True if handled successfully. False otherwise.
bool Microsoft::Console::VirtualTerminal::AdaptDispatch::SetDefaultBackground(const DWORD dwColor)
{
    bool success = true;
    success = _pConApi->PrivateSetDefaultBackground(dwColor);

    // If we're a conpty, always return false, so that we send the updated color
    //      value to the terminal. Still handle the sequence so apps that use
    //      the API or VT to query the values of the color table still read the
    //      correct color.
    if (_pConApi->IsConsolePty())
    {
        return false;
    }

    return success;
}

//Routine Description:
// Window Manipulation - Performs a variety of actions relating to the window,
//      such as moving the window position, resizing the window, querying
//      window state, forcing the window to repaint, etc.
//  This is kept separate from the input version, as there may be
//      codes that are supported in one direction but not the other.
//Arguments:
// - function - An identifier of the WindowManipulation function to perform
// - parameters - Additional parameters to pass to the function
// Return value:
// True if handled successfully. False otherwise.
bool AdaptDispatch::WindowManipulation(const DispatchTypes::WindowManipulationType function,
                                       const std::basic_string_view<size_t> parameters)
{
    bool success = false;
    // Other Window Manipulation functions:
    //  MSFT:13271098 - QueryViewport
    //  MSFT:13271146 - QueryScreenSize
    switch (function)
    {
    case DispatchTypes::WindowManipulationType::RefreshWindow:
        if (parameters.empty())
        {
            success = DispatchCommon::s_RefreshWindow(*_pConApi);
        }
        break;
    case DispatchTypes::WindowManipulationType::ResizeWindowInCharacters:
        if (parameters.size() == 2)
        {
            success = DispatchCommon::s_ResizeWindow(*_pConApi, til::at(parameters, 1), til::at(parameters, 0));
        }
        break;
    default:
        success = false;
    }

    return success;
}

// Routine Description:
// - Determines whether we should pass any sequence that manipulates
//   TerminalInput's input generator through the PTY. It encapsulates
//   a check for whether the PTY is in use.
// Return value:
// True if the request should be passed.
bool AdaptDispatch::_ShouldPassThroughInputModeChange() const
{
    // If we're a conpty, AND WE'RE IN VT INPUT MODE, always pass input mode requests
    // The VT Input mode check is to work around ssh.exe v7.7, which uses VT
    // output, but not Input.
    // The original comment said, "Once the conpty supports these types of input,
    // this check can be removed. See GH#4911". Unfortunately, time has shown
    // us that SSH 7.7 _also_ requests mouse input and that can have a user interface
    // impact on the actual connected terminal. We can't remove this check,
    // because SSH <=7.7 is out in the wild on all versions of Windows <=2004.
    return _pConApi->IsConsolePty() && _pConApi->PrivateIsVtInputEnabled();
}
