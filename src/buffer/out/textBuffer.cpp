// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "textBuffer.hpp"
#include "CharRow.hpp"

#include "../types/inc/utils.hpp"
#include "../types/inc/convert.hpp"
#include "../../types/inc/GlyphWidth.hpp"

#pragma hdrstop

using namespace Microsoft::Console;
using namespace Microsoft::Console::Types;

using PointTree = interval_tree::IntervalTree<til::point, size_t>;

// Routine Description:
// - Creates a new instance of TextBuffer
// Arguments:
// - fontInfo - The font to use for this text buffer as specified in the global font cache
// - screenBufferSize - The X by Y dimensions of the new screen buffer
// - fill - Uses the .Attributes property to decide which default color to apply to all text in this buffer
// - cursorSize - The height of the cursor within this buffer
// Return Value:
// - constructed object
// Note: may throw exception
TextBuffer::TextBuffer(const COORD screenBufferSize,
                       const TextAttribute defaultAttributes,
                       const UINT cursorSize,
                       Microsoft::Console::Render::IRenderTarget& renderTarget) :
    _firstRow{ 0 },
    _currentAttributes{ defaultAttributes },
    _cursor{ cursorSize, *this },
    _storage{},
    _unicodeStorage{},
    _renderTarget{ renderTarget },
    _size{},
    _currentHyperlinkId{ 1 },
    _currentPatternId{ 0 }
{
    // initialize ROWs
    _storage.reserve(static_cast<size_t>(screenBufferSize.Y));
    for (size_t i = 0; i < static_cast<size_t>(screenBufferSize.Y); ++i)
    {
        _storage.emplace_back(static_cast<SHORT>(i), screenBufferSize.X, _currentAttributes, this);
    }

    _UpdateSize();
}

// Routine Description:
// - Copies properties from another text buffer into this one.
// - This is primarily to copy properties that would otherwise not be specified during CreateInstance
// Arguments:
// - OtherBuffer - The text buffer to copy properties from
// Return Value:
// - <none>
void TextBuffer::CopyProperties(const TextBuffer& OtherBuffer) noexcept
{
    GetCursor().CopyProperties(OtherBuffer.GetCursor());
}

// Routine Description:
// - Gets the number of rows in the buffer
// Arguments:
// - <none>
// Return Value:
// - Total number of rows in the buffer
UINT TextBuffer::TotalRowCount() const noexcept
{
    return gsl::narrow<UINT>(_storage.size());
}

// Routine Description:
// - Retrieves a row from the buffer by its offset from the first row of the text buffer (what corresponds to
// the top row of the screen buffer)
// Arguments:
// - Number of rows down from the first row of the buffer.
// Return Value:
// - const reference to the requested row. Asserts if out of bounds.
const ROW& TextBuffer::GetRowByOffset(const size_t index) const
{
    const size_t totalRows = TotalRowCount();

    // Rows are stored circularly, so the index you ask for is offset by the start position and mod the total of rows.
    const size_t offsetIndex = (_firstRow + index) % totalRows;
    return _storage.at(offsetIndex);
}

// Routine Description:
// - Retrieves a row from the buffer by its offset from the first row of the text buffer (what corresponds to
// the top row of the screen buffer)
// Arguments:
// - Number of rows down from the first row of the buffer.
// Return Value:
// - reference to the requested row. Asserts if out of bounds.
ROW& TextBuffer::GetRowByOffset(const size_t index)
{
    const size_t totalRows = TotalRowCount();

    // Rows are stored circularly, so the index you ask for is offset by the start position and mod the total of rows.
    const size_t offsetIndex = (_firstRow + index) % totalRows;
    return _storage.at(offsetIndex);
}

// Routine Description:
// - Retrieves read-only text iterator at the given buffer location
// Arguments:
// - at - X,Y position in buffer for iterator start position
// Return Value:
// - Read-only iterator of text data only.
TextBufferTextIterator TextBuffer::GetTextDataAt(const COORD at) const
{
    return TextBufferTextIterator(GetCellDataAt(at));
}

// Routine Description:
// - Retrieves read-only cell iterator at the given buffer location
// Arguments:
// - at - X,Y position in buffer for iterator start position
// Return Value:
// - Read-only iterator of cell data.
TextBufferCellIterator TextBuffer::GetCellDataAt(const COORD at) const
{
    return TextBufferCellIterator(*this, at);
}

// Routine Description:
// - Retrieves read-only text iterator at the given buffer location
//   but restricted to only the specific line (Y coordinate).
// Arguments:
// - at - X,Y position in buffer for iterator start position
// Return Value:
// - Read-only iterator of text data only.
TextBufferTextIterator TextBuffer::GetTextLineDataAt(const COORD at) const
{
    return TextBufferTextIterator(GetCellLineDataAt(at));
}

// Routine Description:
// - Retrieves read-only cell iterator at the given buffer location
//   but restricted to only the specific line (Y coordinate).
// Arguments:
// - at - X,Y position in buffer for iterator start position
// Return Value:
// - Read-only iterator of cell data.
TextBufferCellIterator TextBuffer::GetCellLineDataAt(const COORD at) const
{
    SMALL_RECT limit;
    limit.Top = at.Y;
    limit.Bottom = at.Y;
    limit.Left = 0;
    limit.Right = GetSize().RightInclusive();

    return TextBufferCellIterator(*this, at, Viewport::FromInclusive(limit));
}

// Routine Description:
// - Retrieves read-only text iterator at the given buffer location
//   but restricted to operate only inside the given viewport.
// Arguments:
// - at - X,Y position in buffer for iterator start position
// - limit - boundaries for the iterator to operate within
// Return Value:
// - Read-only iterator of text data only.
TextBufferTextIterator TextBuffer::GetTextDataAt(const COORD at, const Viewport limit) const
{
    return TextBufferTextIterator(GetCellDataAt(at, limit));
}

// Routine Description:
// - Retrieves read-only cell iterator at the given buffer location
//   but restricted to operate only inside the given viewport.
// Arguments:
// - at - X,Y position in buffer for iterator start position
// - limit - boundaries for the iterator to operate within
// Return Value:
// - Read-only iterator of cell data.
TextBufferCellIterator TextBuffer::GetCellDataAt(const COORD at, const Viewport limit) const
{
    return TextBufferCellIterator(*this, at, limit);
}

// Routine Description:
// - Retrieves read-only cell iterator at the given buffer location
//   but restricted to operate only inside the given viewport.
// Arguments:
// - at - X,Y position in buffer for iterator start position
// - limit - boundaries for the iterator to operate within
// - until - X,Y position in buffer for last position for the iterator to read (inclusive)
// Return Value:
// - Read-only iterator of cell data.
TextBufferCellIterator TextBuffer::GetCellDataAt(const COORD at, const Viewport limit, const COORD until) const
{
    return TextBufferCellIterator(*this, at, limit, until);
}

//Routine Description:
// - Corrects and enforces consistent double byte character state (KAttrs line) within a row of the text buffer.
// - This will take the given double byte information and check that it will be consistent when inserted into the buffer
//   at the current cursor position.
// - It will correct the buffer (by erasing the character prior to the cursor) if necessary to make a consistent state.
//Arguments:
// - dbcsAttribute - Double byte information associated with the character about to be inserted into the buffer
//Return Value:
// - True if it is valid to insert a character with the given double byte attributes. False otherwise.
bool TextBuffer::_AssertValidDoubleByteSequence(const DbcsAttribute dbcsAttribute)
{
    // To figure out if the sequence is valid, we have to look at the character that comes before the current one
    const COORD coordPrevPosition = _GetPreviousFromCursor();
    ROW& prevRow = GetRowByOffset(coordPrevPosition.Y);
    DbcsAttribute prevDbcsAttr;
    try
    {
        prevDbcsAttr = prevRow.GetCharRow().DbcsAttrAt(coordPrevPosition.X);
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
        return false;
    }

    bool fValidSequence = true; // Valid until proven otherwise
    bool fCorrectableByErase = false; // Can't be corrected until proven otherwise

    // Here's the matrix of valid items:
    // N = None (single byte)
    // L = Lead (leading byte of double byte sequence
    // T = Trail (trailing byte of double byte sequence
    // Prev Curr    Result
    // N    N       OK.
    // N    L       OK.
    // N    T       Fail, uncorrectable. Trailing byte must have had leading before it.
    // L    N       Fail, OK with erase. Lead needs trailing pair. Can erase lead to correct.
    // L    L       Fail, OK with erase. Lead needs trailing pair. Can erase prev lead to correct.
    // L    T       OK.
    // T    N       OK.
    // T    L       OK.
    // T    T       Fail, uncorrectable. New trailing byte must have had leading before it.

    // Check for only failing portions of the matrix:
    if (prevDbcsAttr.IsSingle() && dbcsAttribute.IsTrailing())
    {
        // N, T failing case (uncorrectable)
        fValidSequence = false;
    }
    else if (prevDbcsAttr.IsLeading())
    {
        if (dbcsAttribute.IsSingle() || dbcsAttribute.IsLeading())
        {
            // L, N and L, L failing cases (correctable)
            fValidSequence = false;
            fCorrectableByErase = true;
        }
    }
    else if (prevDbcsAttr.IsTrailing() && dbcsAttribute.IsTrailing())
    {
        // T, T failing case (uncorrectable)
        fValidSequence = false;
    }

    // If it's correctable by erase, erase the previous character
    if (fCorrectableByErase)
    {
        // Erase previous character into an N type.
        try
        {
            prevRow.ClearColumn(coordPrevPosition.X);
        }
        catch (...)
        {
            LOG_HR(wil::ResultFromCaughtException());
            return false;
        }

        // Sequence is now N N or N L, which are both okay. Set sequence back to valid.
        fValidSequence = true;
    }

    return fValidSequence;
}

//Routine Description:
// - Call before inserting a character into the buffer.
// - This will ensure a consistent double byte state (KAttrs line) within the text buffer
// - It will attempt to correct the buffer if we're inserting an unexpected double byte character type
//   and it will pad out the buffer if we're going to split a double byte sequence across two rows.
//Arguments:
// - dbcsAttribute - Double byte information associated with the character about to be inserted into the buffer
//Return Value:
// - true if we successfully prepared the buffer and moved the cursor
// - false otherwise (out of memory)
bool TextBuffer::_PrepareForDoubleByteSequence(const DbcsAttribute dbcsAttribute)
{
    // This function corrects most errors. If this is false, we had an uncorrectable one which
    // older versions of conhost simply let pass by unflinching.
    LOG_HR_IF(E_NOT_VALID_STATE, !(_AssertValidDoubleByteSequence(dbcsAttribute))); // Shouldn't be uncorrectable sequences unless something is very wrong.

    bool fSuccess = true;
    // Now compensate if we don't have enough space for the upcoming double byte sequence
    // We only need to compensate for leading bytes
    if (dbcsAttribute.IsLeading())
    {
        const auto cursorPosition = GetCursor().GetPosition();
        const auto lineWidth = GetLineWidth(cursorPosition.Y);

        // If we're about to lead on the last column in the row, we need to add a padding space
        if (cursorPosition.X == lineWidth - 1)
        {
            // set that we're wrapping for double byte reasons
            auto& row = GetRowByOffset(cursorPosition.Y);
            row.SetDoubleBytePadded(true);

            // then move the cursor forward and onto the next row
            fSuccess = IncrementCursor();
        }
    }
    return fSuccess;
}

// Routine Description:
// - Writes cells to the output buffer. Writes at the cursor.
// Arguments:
// - givenIt - Iterator representing output cell data to write
// Return Value:
// - The final position of the iterator
OutputCellIterator TextBuffer::Write(const OutputCellIterator givenIt)
{
    const auto& cursor = GetCursor();
    const auto target = cursor.GetPosition();

    const auto finalIt = Write(givenIt, target);

    return finalIt;
}

// Routine Description:
// - Writes cells to the output buffer.
// Arguments:
// - givenIt - Iterator representing output cell data to write
// - target - the row/column to start writing the text to
// - wrap - change the wrap flag if we hit the end of the row while writing and there's still more data
// Return Value:
// - The final position of the iterator
OutputCellIterator TextBuffer::Write(const OutputCellIterator givenIt,
                                     const COORD target,
                                     const std::optional<bool> wrap)
{
    // Make mutable copy so we can walk.
    auto it = givenIt;

    // Make mutable target so we can walk down lines.
    auto lineTarget = target;

    // Get size of the text buffer so we can stay in bounds.
    const auto size = GetSize();

    // While there's still data in the iterator and we're still targeting in bounds...
    while (it && size.IsInBounds(lineTarget))
    {
        // Attempt to write as much data as possible onto this line.
        // NOTE: if wrap = true/false, we want to set the line's wrap to true/false (respectively) if we reach the end of the line
        it = WriteLine(it, lineTarget, wrap);

        // Move to the next line down.
        lineTarget.X = 0;
        ++lineTarget.Y;
    }

    return it;
}

// Routine Description:
// - Writes one line of text to the output buffer.
// Arguments:
// - givenIt - The iterator that will dereference into cell data to insert
// - target - Coordinate targeted within output buffer
// - wrap - change the wrap flag if we hit the end of the row while writing and there's still more data in the iterator.
// - limitRight - Optionally restrict the right boundary for writing (e.g. stop writing earlier than the end of line)
// Return Value:
// - The iterator, but advanced to where we stopped writing. Use to find input consumed length or cells written length.
OutputCellIterator TextBuffer::WriteLine(const OutputCellIterator givenIt,
                                         const COORD target,
                                         const std::optional<bool> wrap,
                                         std::optional<size_t> limitRight)
{
    // If we're not in bounds, exit early.
    if (!GetSize().IsInBounds(target))
    {
        return givenIt;
    }

    //  Get the row and write the cells
    ROW& row = GetRowByOffset(target.Y);
    const auto newIt = row.WriteCells(givenIt, target.X, wrap, limitRight);

    // Take the cell distance written and notify that it needs to be repainted.
    const auto written = newIt.GetCellDistance(givenIt);
    const Viewport paint = Viewport::FromDimensions(target, { gsl::narrow<SHORT>(written), 1 });
    _NotifyPaint(paint);

    return newIt;
}

//Routine Description:
// - Inserts one codepoint into the buffer at the current cursor position and advances the cursor as appropriate.
//Arguments:
// - chars - The codepoint to insert
// - dbcsAttribute - Double byte information associated with the codepoint
// - bAttr - Color data associated with the character
//Return Value:
// - true if we successfully inserted the character
// - false otherwise (out of memory)
bool TextBuffer::InsertCharacter(const std::wstring_view chars,
                                 const DbcsAttribute dbcsAttribute,
                                 const TextAttribute attr)
{
    // Ensure consistent buffer state for double byte characters based on the character type we're about to insert
    bool fSuccess = _PrepareForDoubleByteSequence(dbcsAttribute);

    if (fSuccess)
    {
        // Get the current cursor position
        short const iRow = GetCursor().GetPosition().Y; // row stored as logical position, not array position
        short const iCol = GetCursor().GetPosition().X; // column logical and array positions are equal.

        // Get the row associated with the given logical position
        ROW& Row = GetRowByOffset(iRow);

        // Store character and double byte data
        CharRow& charRow = Row.GetCharRow();
        short const cBufferWidth = GetSize().Width();

        try
        {
            charRow.GlyphAt(iCol) = chars;
            charRow.DbcsAttrAt(iCol) = dbcsAttribute;
        }
        catch (...)
        {
            LOG_HR(wil::ResultFromCaughtException());
            return false;
        }

        // Store color data
        fSuccess = Row.GetAttrRow().SetAttrToEnd(iCol, attr);
        if (fSuccess)
        {
            // Advance the cursor
            fSuccess = IncrementCursor();
        }
    }
    return fSuccess;
}

//Routine Description:
// - Inserts one ucs2 codepoint into the buffer at the current cursor position and advances the cursor as appropriate.
//Arguments:
// - wch - The codepoint to insert
// - dbcsAttribute - Double byte information associated with the codepoint
// - bAttr - Color data associated with the character
//Return Value:
// - true if we successfully inserted the character
// - false otherwise (out of memory)
bool TextBuffer::InsertCharacter(const wchar_t wch, const DbcsAttribute dbcsAttribute, const TextAttribute attr)
{
    return InsertCharacter({ &wch, 1 }, dbcsAttribute, attr);
}

//Routine Description:
// - Finds the current row in the buffer (as indicated by the cursor position)
//   and specifies that we have forced a line wrap on that row
//Arguments:
// - <none> - Always sets to wrap
//Return Value:
// - <none>
void TextBuffer::_SetWrapOnCurrentRow()
{
    _AdjustWrapOnCurrentRow(true);
}

//Routine Description:
// - Finds the current row in the buffer (as indicated by the cursor position)
//   and specifies whether or not it should have a line wrap flag.
//Arguments:
// - fSet - True if this row has a wrap. False otherwise.
//Return Value:
// - <none>
void TextBuffer::_AdjustWrapOnCurrentRow(const bool fSet)
{
    // The vertical position of the cursor represents the current row we're manipulating.
    const UINT uiCurrentRowOffset = GetCursor().GetPosition().Y;

    // Set the wrap status as appropriate
    GetRowByOffset(uiCurrentRowOffset).SetWrapForced(fSet);
}

//Routine Description:
// - Increments the cursor one position in the buffer as if text is being typed into the buffer.
// - NOTE: Will introduce a wrap marker if we run off the end of the current row
//Arguments:
// - <none>
//Return Value:
// - true if we successfully moved the cursor.
// - false otherwise (out of memory)
bool TextBuffer::IncrementCursor()
{
    // Cursor position is stored as logical array indices (starts at 0) for the window
    // Buffer Size is specified as the "length" of the array. It would say 80 for valid values of 0-79.
    // So subtract 1 from buffer size in each direction to find the index of the final column in the buffer
    const short iFinalColumnIndex = GetLineWidth(GetCursor().GetPosition().Y) - 1;

    // Move the cursor one position to the right
    GetCursor().IncrementXPosition(1);

    bool fSuccess = true;
    // If we've passed the final valid column...
    if (GetCursor().GetPosition().X > iFinalColumnIndex)
    {
        // Then mark that we've been forced to wrap
        _SetWrapOnCurrentRow();

        // Then move the cursor to a new line
        fSuccess = NewlineCursor();
    }
    return fSuccess;
}

//Routine Description:
// - Increments the cursor one line down in the buffer and to the beginning of the line
//Arguments:
// - <none>
//Return Value:
// - true if we successfully moved the cursor.
bool TextBuffer::NewlineCursor()
{
    bool fSuccess = false;
    short const iFinalRowIndex = GetSize().BottomInclusive();

    // Reset the cursor position to 0 and move down one line
    GetCursor().SetXPosition(0);
    GetCursor().IncrementYPosition(1);

    // If we've passed the final valid row...
    if (GetCursor().GetPosition().Y > iFinalRowIndex)
    {
        // Stay on the final logical/offset row of the buffer.
        GetCursor().SetYPosition(iFinalRowIndex);

        // Instead increment the circular buffer to move us into the "oldest" row of the backing buffer
        fSuccess = IncrementCircularBuffer();
    }
    else
    {
        fSuccess = true;
    }
    return fSuccess;
}

//Routine Description:
// - Increments the circular buffer by one. Circular buffer is represented by FirstRow variable.
//Arguments:
// - inVtMode - set to true in VT mode, so standard erase attributes are used for the new row.
//Return Value:
// - true if we successfully incremented the buffer.
bool TextBuffer::IncrementCircularBuffer(const bool inVtMode)
{
    // FirstRow is at any given point in time the array index in the circular buffer that corresponds
    // to the logical position 0 in the window (cursor coordinates and all other coordinates).
    _renderTarget.TriggerCircling();

    // Prune hyperlinks to delete obsolete references
    _PruneHyperlinks();

    // Second, clean out the old "first row" as it will become the "last row" of the buffer after the circle is performed.
    auto fillAttributes = _currentAttributes;
    if (inVtMode)
    {
        // The VT standard requires that the new row is initialized with
        // the current background color, but with no meta attributes set.
        fillAttributes.SetStandardErase();
    }
    const bool fSuccess = _storage.at(_firstRow).Reset(fillAttributes);
    if (fSuccess)
    {
        // Now proceed to increment.
        // Incrementing it will cause the next line down to become the new "top" of the window (the new "0" in logical coordinates)
        _firstRow++;

        // If we pass up the height of the buffer, loop back to 0.
        if (_firstRow >= GetSize().Height())
        {
            _firstRow = 0;
        }
    }
    return fSuccess;
}

//Routine Description:
// - Retrieves the position of the last non-space character in the given
//   viewport
// - By default, we search the entire buffer to find the last non-space
//   character.
// - If we know the last character is within the given viewport (so we don't
//   need to check the entire buffer), we can provide a value in viewOptional
//   that we'll use to search for the last character in.
//Arguments:
// - The viewport
//Return value:
// - Coordinate position (relative to the text buffer)
COORD TextBuffer::GetLastNonSpaceCharacter(std::optional<const Microsoft::Console::Types::Viewport> viewOptional) const
{
    const auto viewport = viewOptional.has_value() ? viewOptional.value() : GetSize();

    COORD coordEndOfText = { 0 };
    // Search the given viewport by starting at the bottom.
    coordEndOfText.Y = viewport.BottomInclusive();

    const auto& currRow = GetRowByOffset(coordEndOfText.Y);
    // The X position of the end of the valid text is the Right draw boundary (which is one beyond the final valid character)
    coordEndOfText.X = gsl::narrow<short>(currRow.GetCharRow().MeasureRight()) - 1;

    // If the X coordinate turns out to be -1, the row was empty, we need to search backwards for the real end of text.
    const auto viewportTop = viewport.Top();
    bool fDoBackUp = (coordEndOfText.X < 0 && coordEndOfText.Y > viewportTop); // this row is empty, and we're not at the top
    while (fDoBackUp)
    {
        coordEndOfText.Y--;
        const auto& backupRow = GetRowByOffset(coordEndOfText.Y);
        // We need to back up to the previous row if this line is empty, AND there are more rows

        coordEndOfText.X = gsl::narrow<short>(backupRow.GetCharRow().MeasureRight()) - 1;
        fDoBackUp = (coordEndOfText.X < 0 && coordEndOfText.Y > viewportTop);
    }

    // don't allow negative results
    coordEndOfText.Y = std::max(coordEndOfText.Y, 0i16);
    coordEndOfText.X = std::max(coordEndOfText.X, 0i16);

    return coordEndOfText;
}

// Routine Description:
// - Retrieves the position of the previous character relative to the current cursor position
// Arguments:
// - <none>
// Return Value:
// - Coordinate position in screen coordinates of the character just before the cursor.
// - NOTE: Will return 0,0 if already in the top left corner
COORD TextBuffer::_GetPreviousFromCursor() const
{
    COORD coordPosition = GetCursor().GetPosition();

    // If we're not at the left edge, simply move the cursor to the left by one
    if (coordPosition.X > 0)
    {
        coordPosition.X--;
    }
    else
    {
        // Otherwise, only if we're not on the top row (e.g. we don't move anywhere in the top left corner. there is no previous)
        if (coordPosition.Y > 0)
        {
            // move the cursor up one line
            coordPosition.Y--;

            // and to the right edge
            coordPosition.X = GetLineWidth(coordPosition.Y) - 1;
        }
    }

    return coordPosition;
}

const SHORT TextBuffer::GetFirstRowIndex() const noexcept
{
    return _firstRow;
}

const Viewport TextBuffer::GetSize() const noexcept
{
    return _size;
}

void TextBuffer::_UpdateSize()
{
    _size = Viewport::FromDimensions({ 0, 0 }, { gsl::narrow<SHORT>(_storage.at(0).size()), gsl::narrow<SHORT>(_storage.size()) });
}

void TextBuffer::_SetFirstRowIndex(const SHORT FirstRowIndex) noexcept
{
    _firstRow = FirstRowIndex;
}

void TextBuffer::ScrollRows(const SHORT firstRow, const SHORT size, const SHORT delta)
{
    // If we don't have to move anything, leave early.
    if (delta == 0)
    {
        return;
    }

    // OK. We're about to play games by moving rows around within the deque to
    // scroll a massive region in a faster way than copying things.
    // To make this easier, first correct the circular buffer to have the first row be 0 again.
    if (_firstRow != 0)
    {
        // Rotate the buffer to put the first row at the front.
        std::rotate(_storage.begin(), _storage.begin() + _firstRow, _storage.end());

        // The first row is now at the top.
        _firstRow = 0;
    }

    // Rotate just the subsection specified
    if (delta < 0)
    {
        // The layout is like this:
        // delta is -2, size is 3, firstRow is 5
        // We want 3 rows from 5 (5, 6, and 7) to move up 2 spots.
        // --- (storage) ----
        // | 0 begin
        // | 1
        // | 2
        // | 3 A. begin + firstRow + delta (because delta is negative)
        // | 4
        // | 5 B. begin + firstRow
        // | 6
        // | 7
        // | 8 C. begin + firstRow + size
        // | 9
        // | 10
        // | 11
        // - end
        // We want B to slide up to A (the negative delta) and everything from [B,C) to slide up with it.
        // So the final layout will be
        // --- (storage) ----
        // | 0 begin
        // | 1
        // | 2
        // | 5
        // | 6
        // | 7
        // | 3
        // | 4
        // | 8
        // | 9
        // | 10
        // | 11
        // - end
        std::rotate(_storage.begin() + firstRow + delta, _storage.begin() + firstRow, _storage.begin() + firstRow + size);
    }
    else
    {
        // The layout is like this:
        // delta is 2, size is 3, firstRow is 5
        // We want 3 rows from 5 (5, 6, and 7) to move down 2 spots.
        // --- (storage) ----
        // | 0 begin
        // | 1
        // | 2
        // | 3
        // | 4
        // | 5 A. begin + firstRow
        // | 6
        // | 7
        // | 8 B. begin + firstRow + size
        // | 9
        // | 10 C. begin + firstRow + size + delta
        // | 11
        // - end
        // We want B-1 to slide down to C-1 (the positive delta) and everything from [A, B) to slide down with it.
        // So the final layout will be
        // --- (storage) ----
        // | 0 begin
        // | 1
        // | 2
        // | 3
        // | 4
        // | 8
        // | 9
        // | 5
        // | 6
        // | 7
        // | 10
        // | 11
        // - end
        std::rotate(_storage.begin() + firstRow, _storage.begin() + firstRow + size, _storage.begin() + firstRow + size + delta);
    }

    // Renumber the IDs now that we've rearranged where the rows sit within the buffer.
    // Refreshing should also delegate to the UnicodeStorage to re-key all the stored unicode sequences (where applicable).
    _RefreshRowIDs(std::nullopt);
}

Cursor& TextBuffer::GetCursor() noexcept
{
    return _cursor;
}

const Cursor& TextBuffer::GetCursor() const noexcept
{
    return _cursor;
}

[[nodiscard]] TextAttribute TextBuffer::GetCurrentAttributes() const noexcept
{
    return _currentAttributes;
}

void TextBuffer::SetCurrentAttributes(const TextAttribute& currentAttributes) noexcept
{
    _currentAttributes = currentAttributes;
}

void TextBuffer::SetCurrentLineRendition(const LineRendition lineRendition)
{
    const auto cursorPosition = GetCursor().GetPosition();
    const auto rowIndex = cursorPosition.Y;
    auto& row = GetRowByOffset(rowIndex);
    if (row.GetLineRendition() != lineRendition)
    {
        row.SetLineRendition(lineRendition);
        // If the line rendition has changed, the row can no longer be wrapped.
        row.SetWrapForced(false);
        // And if it's no longer single width, the right half of the row should be erased.
        if (lineRendition != LineRendition::SingleWidth)
        {
            const auto fillChar = L' ';
            auto fillAttrs = GetCurrentAttributes();
            fillAttrs.SetStandardErase();
            const size_t fillOffset = GetLineWidth(rowIndex);
            const size_t fillLength = GetSize().Width() - fillOffset;
            const auto fillData = OutputCellIterator{ fillChar, fillAttrs, fillLength };
            row.WriteCells(fillData, fillOffset, false);
            // We also need to make sure the cursor is clamped within the new width.
            GetCursor().SetPosition(ClampPositionWithinLine(cursorPosition));
        }
        _NotifyPaint(Viewport::FromDimensions({ 0, rowIndex }, { GetSize().Width(), 1 }));
    }
}

void TextBuffer::ResetLineRenditionRange(const size_t startRow, const size_t endRow)
{
    for (auto row = startRow; row < endRow; row++)
    {
        GetRowByOffset(row).SetLineRendition(LineRendition::SingleWidth);
    }
}

LineRendition TextBuffer::GetLineRendition(const size_t row) const
{
    return GetRowByOffset(row).GetLineRendition();
}

bool TextBuffer::IsDoubleWidthLine(const size_t row) const
{
    return GetLineRendition(row) != LineRendition::SingleWidth;
}

SHORT TextBuffer::GetLineWidth(const size_t row) const
{
    // Use shift right to quickly divide the width by 2 for double width lines.
    const SHORT scale = IsDoubleWidthLine(row) ? 1 : 0;
    return GetSize().Width() >> scale;
}

COORD TextBuffer::ClampPositionWithinLine(const COORD position) const
{
    const SHORT rightmostColumn = GetLineWidth(position.Y) - 1;
    return { std::min(position.X, rightmostColumn), position.Y };
}

COORD TextBuffer::ScreenToBufferPosition(const COORD position) const
{
    // Use shift right to quickly divide the X pos by 2 for double width lines.
    const SHORT scale = IsDoubleWidthLine(position.Y) ? 1 : 0;
    return { position.X >> scale, position.Y };
}

COORD TextBuffer::BufferToScreenPosition(const COORD position) const
{
    // Use shift left to quickly multiply the X pos by 2 for double width lines.
    const SHORT scale = IsDoubleWidthLine(position.Y) ? 1 : 0;
    return { position.X << scale, position.Y };
}

// Routine Description:
// - Resets the text contents of this buffer with the default character
//   and the default current color attributes
void TextBuffer::Reset()
{
    const auto attr = GetCurrentAttributes();

    for (auto& row : _storage)
    {
        row.Reset(attr);
    }
}

// Routine Description:
// - This is the legacy screen resize with minimal changes
// Arguments:
// - newSize - new size of screen.
// Return Value:
// - Success if successful. Invalid parameter if screen buffer size is unexpected. No memory if allocation failed.
[[nodiscard]] NTSTATUS TextBuffer::ResizeTraditional(const COORD newSize) noexcept
{
    RETURN_HR_IF(E_INVALIDARG, newSize.X < 0 || newSize.Y < 0);

    try
    {
        const auto currentSize = GetSize().Dimensions();
        const auto attributes = GetCurrentAttributes();

        SHORT TopRow = 0; // new top row of the screen buffer
        if (newSize.Y <= GetCursor().GetPosition().Y)
        {
            TopRow = GetCursor().GetPosition().Y - newSize.Y + 1;
        }
        const SHORT TopRowIndex = (GetFirstRowIndex() + TopRow) % currentSize.Y;

        // rotate rows until the top row is at index 0
        for (int i = 0; i < TopRowIndex; i++)
        {
            _storage.emplace_back(std::move(_storage.front()));
            _storage.erase(_storage.begin());
        }

        _SetFirstRowIndex(0);

        // realloc in the Y direction
        // remove rows if we're shrinking
        while (_storage.size() > static_cast<size_t>(newSize.Y))
        {
            _storage.pop_back();
        }
        // add rows if we're growing
        while (_storage.size() < static_cast<size_t>(newSize.Y))
        {
            _storage.emplace_back(static_cast<short>(_storage.size()), newSize.X, attributes, this);
        }

        // Now that we've tampered with the row placement, refresh all the row IDs.
        // Also take advantage of the row ID refresh loop to resize the rows in the X dimension
        // and cleanup the UnicodeStorage characters that might fall outside the resized buffer.
        _RefreshRowIDs(newSize.X);

        // Update the cached size value
        _UpdateSize();
    }
    CATCH_RETURN();

    return S_OK;
}

const UnicodeStorage& TextBuffer::GetUnicodeStorage() const noexcept
{
    return _unicodeStorage;
}

UnicodeStorage& TextBuffer::GetUnicodeStorage() noexcept
{
    return _unicodeStorage;
}

// Routine Description:
// - Method to help refresh all the Row IDs after manipulating the row
//   by shuffling pointers around.
// - This will also update parent pointers that are stored in depth within the buffer
//   (e.g. it will update CharRow parents pointing at Rows that might have been moved around)
// - Optionally takes a new row width if we're resizing to perform a resize operation and cleanup
//   any high unicode (UnicodeStorage) runs while we're already looping through the rows.
// Arguments:
// - newRowWidth - Optional new value for the row width.
void TextBuffer::_RefreshRowIDs(std::optional<SHORT> newRowWidth)
{
    std::unordered_map<SHORT, SHORT> rowMap;
    SHORT i = 0;
    for (auto& it : _storage)
    {
        // Build a map so we can update Unicode Storage
        rowMap.emplace(it.GetId(), i);

        // Update the IDs
        it.SetId(i++);

        // Also update the char row parent pointers as they can get shuffled up in the rotates.
        it.GetCharRow().UpdateParent(&it);

        // Resize the rows in the X dimension if we have a new width
        if (newRowWidth.has_value())
        {
            // Realloc in the X direction
            THROW_IF_FAILED(it.Resize(newRowWidth.value()));
        }
    }

    // Give the new mapping to Unicode Storage
    _unicodeStorage.Remap(rowMap, newRowWidth);
}

void TextBuffer::_NotifyPaint(const Viewport& viewport) const
{
    _renderTarget.TriggerRedraw(viewport);
}

// Routine Description:
// - Retrieves the first row from the underlying buffer.
// Arguments:
// - <none>
// Return Value:
//  - reference to the first row.
ROW& TextBuffer::_GetFirstRow()
{
    return GetRowByOffset(0);
}

// Routine Description:
// - Retrieves the row that comes before the given row.
// - Does not wrap around the screen buffer.
// Arguments:
// - The current row.
// Return Value:
// - reference to the previous row
// Note:
// - will throw exception if called with the first row of the text buffer
ROW& TextBuffer::_GetPrevRowNoWrap(const ROW& Row)
{
    int prevRowIndex = Row.GetId() - 1;
    if (prevRowIndex < 0)
    {
        prevRowIndex = TotalRowCount() - 1;
    }

    THROW_HR_IF(E_FAIL, Row.GetId() == _firstRow);
    return _storage.at(prevRowIndex);
}

// Method Description:
// - Retrieves this buffer's current render target.
// Arguments:
// - <none>
// Return Value:
// - This buffer's current render target.
Microsoft::Console::Render::IRenderTarget& TextBuffer::GetRenderTarget() noexcept
{
    return _renderTarget;
}

// Method Description:
// - get delimiter class for buffer cell position
// - used for double click selection and uia word navigation
// Arguments:
// - pos: the buffer cell under observation
// - wordDelimiters: the delimiters defined as a part of the DelimiterClass::DelimiterChar
// Return Value:
// - the delimiter class for the given char
const DelimiterClass TextBuffer::_GetDelimiterClassAt(const COORD pos, const std::wstring_view wordDelimiters) const
{
    return GetRowByOffset(pos.Y).GetCharRow().DelimiterClassAt(pos.X, wordDelimiters);
}

// Method Description:
// - Get the COORD for the beginning of the word you are on
// Arguments:
// - target - a COORD on the word you are currently on
// - wordDelimiters - what characters are we considering for the separation of words
// - accessibilityMode - when enabled, we continue expanding left until we are at the beginning of a readable word.
//                        Otherwise, expand left until a character of a new delimiter class is found
//                        (or a row boundary is encountered)
// Return Value:
// - The COORD for the first character on the "word" (inclusive)
const COORD TextBuffer::GetWordStart(const COORD target, const std::wstring_view wordDelimiters, bool accessibilityMode) const
{
    // Consider a buffer with this text in it:
    // "  word   other  "
    // In selection (accessibilityMode = false),
    //  a "word" is defined as the range between two delimiters
    //  so the words in the example include ["  ", "word", "   ", "other", "  "]
    // In accessibility (accessibilityMode = true),
    //  a "word" includes the delimiters after a range of readable characters
    //  so the words in the example include ["word   ", "other  "]
    // NOTE: the start anchor (this one) is inclusive, whereas the end anchor (GetWordEnd) is exclusive

#pragma warning(suppress : 26496)
    // GH#7664: Treat EndExclusive as EndInclusive so
    // that it actually points to a space in the buffer
    auto copy{ target };
    const auto bufferSize{ GetSize() };
    if (target == bufferSize.Origin())
    {
        // can't expand left
        return target;
    }
    else if (target == bufferSize.EndExclusive())
    {
        // treat EndExclusive as EndInclusive
        copy = { bufferSize.RightInclusive(), bufferSize.BottomInclusive() };
    }

    if (accessibilityMode)
    {
        return _GetWordStartForAccessibility(copy, wordDelimiters);
    }
    else
    {
        return _GetWordStartForSelection(copy, wordDelimiters);
    }
}

// Method Description:
// - Helper method for GetWordStart(). Get the COORD for the beginning of the word (accessibility definition) you are on
// Arguments:
// - target - a COORD on the word you are currently on
// - wordDelimiters - what characters are we considering for the separation of words
// Return Value:
// - The COORD for the first character on the current/previous READABLE "word" (inclusive)
const COORD TextBuffer::_GetWordStartForAccessibility(const COORD target, const std::wstring_view wordDelimiters) const
{
    COORD result = target;
    const auto bufferSize = GetSize();
    bool stayAtOrigin = false;

    // ignore left boundary. Continue until readable text found
    while (_GetDelimiterClassAt(result, wordDelimiters) != DelimiterClass::RegularChar)
    {
        if (!bufferSize.DecrementInBounds(result))
        {
            // first char in buffer is a DelimiterChar or ControlChar
            // we can't move any further back
            stayAtOrigin = true;
            break;
        }
    }

    // make sure we expand to the left boundary or the beginning of the word
    while (_GetDelimiterClassAt(result, wordDelimiters) == DelimiterClass::RegularChar)
    {
        if (!bufferSize.DecrementInBounds(result))
        {
            // first char in buffer is a RegularChar
            // we can't move any further back
            break;
        }
    }

    // move off of delimiter and onto word start
    if (!stayAtOrigin && _GetDelimiterClassAt(result, wordDelimiters) != DelimiterClass::RegularChar)
    {
        bufferSize.IncrementInBounds(result);
    }

    return result;
}

// Method Description:
// - Helper method for GetWordStart(). Get the COORD for the beginning of the word (selection definition) you are on
// Arguments:
// - target - a COORD on the word you are currently on
// - wordDelimiters - what characters are we considering for the separation of words
// Return Value:
// - The COORD for the first character on the current word or delimiter run (stopped by the left margin)
const COORD TextBuffer::_GetWordStartForSelection(const COORD target, const std::wstring_view wordDelimiters) const
{
    COORD result = target;
    const auto bufferSize = GetSize();

    const auto initialDelimiter = _GetDelimiterClassAt(result, wordDelimiters);

    // expand left until we hit the left boundary or a different delimiter class
    while (result.X > bufferSize.Left() && (_GetDelimiterClassAt(result, wordDelimiters) == initialDelimiter))
    {
        bufferSize.DecrementInBounds(result);
    }

    if (_GetDelimiterClassAt(result, wordDelimiters) != initialDelimiter)
    {
        // move off of delimiter
        bufferSize.IncrementInBounds(result);
    }

    return result;
}

// Method Description:
// - Get the COORD for the beginning of the NEXT word
// Arguments:
// - target - a COORD on the word you are currently on
// - wordDelimiters - what characters are we considering for the separation of words
// - accessibilityMode - when enabled, we continue expanding right until we are at the beginning of the next READABLE word
//                        Otherwise, expand right until a character of a new delimiter class is found
//                        (or a row boundary is encountered)
// Return Value:
// - The COORD for the last character on the "word" (inclusive)
const COORD TextBuffer::GetWordEnd(const COORD target, const std::wstring_view wordDelimiters, bool accessibilityMode) const
{
    // Consider a buffer with this text in it:
    // "  word   other  "
    // In selection (accessibilityMode = false),
    //  a "word" is defined as the range between two delimiters
    //  so the words in the example include ["  ", "word", "   ", "other", "  "]
    // In accessibility (accessibilityMode = true),
    //  a "word" includes the delimiters after a range of readable characters
    //  so the words in the example include ["word   ", "other  "]
    // NOTE: the end anchor (this one) is exclusive, whereas the start anchor (GetWordStart) is inclusive

    // Already at the end. Can't move forward.
    if (target == GetSize().EndExclusive())
    {
        return target;
    }

    if (accessibilityMode)
    {
        const auto lastCharPos{ GetLastNonSpaceCharacter() };
        return _GetWordEndForAccessibility(target, wordDelimiters, lastCharPos);
    }
    else
    {
        return _GetWordEndForSelection(target, wordDelimiters);
    }
}

// Method Description:
// - Helper method for GetWordEnd(). Get the COORD for the beginning of the next READABLE word
// Arguments:
// - target - a COORD on the word you are currently on
// - wordDelimiters - what characters are we considering for the separation of words
// - lastCharPos - the position of the last nonspace character in the text buffer (to improve performance)
// Return Value:
// - The COORD for the first character of the next readable "word". If no next word, return one past the end of the buffer
const COORD TextBuffer::_GetWordEndForAccessibility(const COORD target, const std::wstring_view wordDelimiters, const COORD lastCharPos) const
{
    const auto bufferSize = GetSize();
    COORD result = target;

    // Check if we're already on/past the last RegularChar
    if (bufferSize.CompareInBounds(result, lastCharPos, true) >= 0)
    {
        return bufferSize.EndExclusive();
    }

    // ignore right boundary. Continue through readable text found
    while (_GetDelimiterClassAt(result, wordDelimiters) == DelimiterClass::RegularChar)
    {
        if (!bufferSize.IncrementInBounds(result, true))
        {
            break;
        }
    }

    // we are already on/past the last RegularChar
    if (bufferSize.CompareInBounds(result, lastCharPos, true) >= 0)
    {
        return bufferSize.EndExclusive();
    }

    // make sure we expand to the beginning of the NEXT word
    while (_GetDelimiterClassAt(result, wordDelimiters) != DelimiterClass::RegularChar)
    {
        if (!bufferSize.IncrementInBounds(result, true))
        {
            // we are at the EndInclusive COORD
            // this signifies that we must include the last char in the buffer
            // but the position of the COORD points to nothing
            break;
        }
    }

    return result;
}

// Method Description:
// - Helper method for GetWordEnd(). Get the COORD for the beginning of the NEXT word
// Arguments:
// - target - a COORD on the word you are currently on
// - wordDelimiters - what characters are we considering for the separation of words
// Return Value:
// - The COORD for the last character of the current word or delimiter run (stopped by right margin)
const COORD TextBuffer::_GetWordEndForSelection(const COORD target, const std::wstring_view wordDelimiters) const
{
    const auto bufferSize = GetSize();

    // can't expand right
    if (target.X == bufferSize.RightInclusive())
    {
        return target;
    }

    COORD result = target;
    const auto initialDelimiter = _GetDelimiterClassAt(result, wordDelimiters);

    // expand right until we hit the right boundary or a different delimiter class
    while (result.X < bufferSize.RightInclusive() && (_GetDelimiterClassAt(result, wordDelimiters) == initialDelimiter))
    {
        bufferSize.IncrementInBounds(result);
    }

    if (_GetDelimiterClassAt(result, wordDelimiters) != initialDelimiter)
    {
        // move off of delimiter
        bufferSize.DecrementInBounds(result);
    }

    return result;
}

void TextBuffer::_PruneHyperlinks()
{
    // Check the old first row for hyperlink references
    // If there are any, search the entire buffer for the same reference
    // If the buffer does not contain the same reference, we can remove that hyperlink from our map
    // This way, obsolete hyperlink references are cleared from our hyperlink map instead of hanging around
    // Get all the hyperlink references in the row we're erasing
    const auto hyperlinks = _storage.at(_firstRow).GetAttrRow().GetHyperlinks();

    if (!hyperlinks.empty())
    {
        // Move to unordered set so we can use hashed lookup of IDs instead of linear search.
        // Only make it an unordered set now because set always heap allocates but vector
        // doesn't when the set is empty (saving an allocation in the common case of no links.)
        std::unordered_set<uint16_t> firstRowRefs{ hyperlinks.cbegin(), hyperlinks.cend() };

        const auto total = TotalRowCount();
        // Loop through all the rows in the buffer except the first row -
        // we have found all hyperlink references in the first row and put them in refs,
        // now we need to search the rest of the buffer (i.e. all the rows except the first)
        // to see if those references are anywhere else
        for (size_t i = 1; i != total; ++i)
        {
            const auto nextRowRefs = GetRowByOffset(i).GetAttrRow().GetHyperlinks();
            for (auto id : nextRowRefs)
            {
                if (firstRowRefs.find(id) != firstRowRefs.end())
                {
                    firstRowRefs.erase(id);
                }
            }
            if (firstRowRefs.empty())
            {
                // No more hyperlink references left to search for, terminate early
                break;
            }
        }

        // Now delete obsolete references from our map
        for (auto hyperlinkReference : firstRowRefs)
        {
            RemoveHyperlinkFromMap(hyperlinkReference);
        }
    }
}

// Method Description:
// - Update pos to be the position of the first character of the next word. This is used for accessibility
// Arguments:
// - pos - a COORD on the word you are currently on
// - wordDelimiters - what characters are we considering for the separation of words
// - lastCharPos - the position of the last nonspace character in the text buffer (to improve performance)
// Return Value:
// - true, if successfully updated pos. False, if we are unable to move (usually due to a buffer boundary)
// - pos - The COORD for the first character on the "word" (inclusive)
bool TextBuffer::MoveToNextWord(COORD& pos, const std::wstring_view wordDelimiters, COORD lastCharPos) const
{
    // move to the beginning of the next word
    // NOTE: _GetWordEnd...() returns the exclusive position of the "end of the word"
    //       This is also the inclusive start of the next word.
    auto copy{ _GetWordEndForAccessibility(pos, wordDelimiters, lastCharPos) };

    if (copy == GetSize().EndExclusive())
    {
        return false;
    }

    pos = copy;
    return true;
}

// Method Description:
// - Update pos to be the position of the first character of the previous word. This is used for accessibility
// Arguments:
// - pos - a COORD on the word you are currently on
// - wordDelimiters - what characters are we considering for the separation of words
// Return Value:
// - true, if successfully updated pos. False, if we are unable to move (usually due to a buffer boundary)
// - pos - The COORD for the first character on the "word" (inclusive)
bool TextBuffer::MoveToPreviousWord(COORD& pos, std::wstring_view wordDelimiters) const
{
    // move to the beginning of the current word
    auto copy{ GetWordStart(pos, wordDelimiters, true) };

    if (!GetSize().DecrementInBounds(copy, true))
    {
        // can't move behind current word
        return false;
    }

    // move to the beginning of the previous word
    pos = GetWordStart(copy, wordDelimiters, true);
    return true;
}

// Method Description:
// - Update pos to be the beginning of the current glyph/character. This is used for accessibility
// Arguments:
// - pos - a COORD on the word you are currently on
// Return Value:
// - pos - The COORD for the first cell of the current glyph (inclusive)
const til::point TextBuffer::GetGlyphStart(const til::point pos) const
{
    COORD resultPos = pos;
    const auto bufferSize = GetSize();

    if (resultPos == bufferSize.EndExclusive())
    {
        bufferSize.DecrementInBounds(resultPos, true);
    }

    if (resultPos != bufferSize.EndExclusive() && GetCellDataAt(resultPos)->DbcsAttr().IsTrailing())
    {
        bufferSize.DecrementInBounds(resultPos, true);
    }

    return resultPos;
}

// Method Description:
// - Update pos to be the end of the current glyph/character. This is used for accessibility
// Arguments:
// - pos - a COORD on the word you are currently on
// Return Value:
// - pos - The COORD for the last cell of the current glyph (exclusive)
const til::point TextBuffer::GetGlyphEnd(const til::point pos) const
{
    COORD resultPos = pos;

    const auto bufferSize = GetSize();
    if (resultPos != bufferSize.EndExclusive() && GetCellDataAt(resultPos)->DbcsAttr().IsLeading())
    {
        bufferSize.IncrementInBounds(resultPos, true);
    }

    // increment one more time to become exclusive
    bufferSize.IncrementInBounds(resultPos, true);
    return resultPos;
}

// Method Description:
// - Update pos to be the beginning of the next glyph/character. This is used for accessibility
// Arguments:
// - pos - a COORD on the word you are currently on
// - allowBottomExclusive - allow the nonexistent end-of-buffer cell to be encountered
// Return Value:
// - true, if successfully updated pos. False, if we are unable to move (usually due to a buffer boundary)
// - pos - The COORD for the first cell of the current glyph (inclusive)
bool TextBuffer::MoveToNextGlyph(til::point& pos, bool allowBottomExclusive) const
{
    COORD resultPos = pos;
    const auto bufferSize = GetSize();

    if (resultPos == GetSize().EndExclusive())
    {
        // we're already at the end
        return false;
    }

    // try to move. If we can't, we're done.
    const bool success = bufferSize.IncrementInBounds(resultPos, allowBottomExclusive);
    if (resultPos != bufferSize.EndExclusive() && GetCellDataAt(resultPos)->DbcsAttr().IsTrailing())
    {
        bufferSize.IncrementInBounds(resultPos, allowBottomExclusive);
    }

    pos = resultPos;
    return success;
}

// Method Description:
// - Update pos to be the beginning of the previous glyph/character. This is used for accessibility
// Arguments:
// - pos - a COORD on the word you are currently on
// Return Value:
// - true, if successfully updated pos. False, if we are unable to move (usually due to a buffer boundary)
// - pos - The COORD for the first cell of the previous glyph (inclusive)
bool TextBuffer::MoveToPreviousGlyph(til::point& pos) const
{
    COORD resultPos = pos;

    // try to move. If we can't, we're done.
    const auto bufferSize = GetSize();
    const bool success = bufferSize.DecrementInBounds(resultPos, true);
    if (resultPos != bufferSize.EndExclusive() && GetCellDataAt(resultPos)->DbcsAttr().IsLeading())
    {
        bufferSize.DecrementInBounds(resultPos, true);
    }

    pos = resultPos;
    return success;
}

// Method Description:
// - Determines the line-by-line rectangles based on two COORDs
// - expands the rectangles to support wide glyphs
// - used for selection rects and UIA bounding rects
// Arguments:
// - start: a corner of the text region of interest (inclusive)
// - end: the other corner of the text region of interest (inclusive)
// - blockSelection: when enabled, only get the rectangular text region,
//                   as opposed to the text extending to the left/right
//                   buffer margins
// - bufferCoordinates: when enabled, treat the coordinates as relative to
//                      the buffer rather than the screen.
// Return Value:
// - the delimiter class for the given char
const std::vector<SMALL_RECT> TextBuffer::GetTextRects(COORD start, COORD end, bool blockSelection, bool bufferCoordinates) const
{
    std::vector<SMALL_RECT> textRects;

    const auto bufferSize = GetSize();

    // (0,0) is the top-left of the screen
    // the physically "higher" coordinate is closer to the top-left
    // the physically "lower" coordinate is closer to the bottom-right
    const auto [higherCoord, lowerCoord] = bufferSize.CompareInBounds(start, end) <= 0 ?
                                               std::make_tuple(start, end) :
                                               std::make_tuple(end, start);

    const auto textRectSize = base::ClampedNumeric<short>(1) + lowerCoord.Y - higherCoord.Y;
    textRects.reserve(textRectSize);
    for (auto row = higherCoord.Y; row <= lowerCoord.Y; row++)
    {
        SMALL_RECT textRow;

        textRow.Top = row;
        textRow.Bottom = row;

        if (blockSelection || higherCoord.Y == lowerCoord.Y)
        {
            // set the left and right margin to the left-/right-most respectively
            textRow.Left = std::min(higherCoord.X, lowerCoord.X);
            textRow.Right = std::max(higherCoord.X, lowerCoord.X);
        }
        else
        {
            textRow.Left = (row == higherCoord.Y) ? higherCoord.X : bufferSize.Left();
            textRow.Right = (row == lowerCoord.Y) ? lowerCoord.X : bufferSize.RightInclusive();
        }

        // If we were passed screen coordinates, convert the given range into
        // equivalent buffer offsets, taking line rendition into account.
        if (!bufferCoordinates)
        {
            textRow = ScreenToBufferLine(textRow, GetLineRendition(row));
        }

        _ExpandTextRow(textRow);
        textRects.emplace_back(textRow);
    }

    return textRects;
}

// Method Description:
// - Expand the selection row according to include wide glyphs fully
// - this is particularly useful for box selections (ALT + selection)
// Arguments:
// - selectionRow: the selection row to be expanded
// Return Value:
// - modifies selectionRow's Left and Right values to expand properly
void TextBuffer::_ExpandTextRow(SMALL_RECT& textRow) const
{
    const auto bufferSize = GetSize();

    // expand left side of rect
    COORD targetPoint{ textRow.Left, textRow.Top };
    if (GetCellDataAt(targetPoint)->DbcsAttr().IsTrailing())
    {
        if (targetPoint.X == bufferSize.Left())
        {
            bufferSize.IncrementInBounds(targetPoint);
        }
        else
        {
            bufferSize.DecrementInBounds(targetPoint);
        }
        textRow.Left = targetPoint.X;
    }

    // expand right side of rect
    targetPoint = { textRow.Right, textRow.Bottom };
    if (GetCellDataAt(targetPoint)->DbcsAttr().IsLeading())
    {
        if (targetPoint.X == bufferSize.RightInclusive())
        {
            bufferSize.DecrementInBounds(targetPoint);
        }
        else
        {
            bufferSize.IncrementInBounds(targetPoint);
        }
        textRow.Right = targetPoint.X;
    }
}

// Routine Description:
// - Retrieves the text data from the selected region and presents it in a clipboard-ready format (given little post-processing).
// Arguments:
// - includeCRLF - inject CRLF pairs to the end of each line
// - trimTrailingWhitespace - remove the trailing whitespace at the end of each line
// - textRects - the rectangular regions from which the data will be extracted from the buffer (i.e.: selection rects)
// - GetAttributeColors - function used to map TextAttribute to RGB COLORREFs. If null, only extract the text.
// - formatWrappedRows - if set we will apply formatting (CRLF inclusion and whitespace trimming) on wrapped rows
// Return Value:
// - The text, background color, and foreground color data of the selected region of the text buffer.
const TextBuffer::TextAndColor TextBuffer::GetText(const bool includeCRLF,
                                                   const bool trimTrailingWhitespace,
                                                   const std::vector<SMALL_RECT>& selectionRects,
                                                   std::function<std::pair<COLORREF, COLORREF>(const TextAttribute&)> GetAttributeColors,
                                                   const bool formatWrappedRows) const
{
    TextAndColor data;
    const bool copyTextColor = GetAttributeColors != nullptr;

    // preallocate our vectors to reduce reallocs
    size_t const rows = selectionRects.size();
    data.text.reserve(rows);
    if (copyTextColor)
    {
        data.FgAttr.reserve(rows);
        data.BkAttr.reserve(rows);
    }

    // for each row in the selection
    for (UINT i = 0; i < rows; i++)
    {
        const UINT iRow = selectionRects.at(i).Top;

        const Viewport highlight = Viewport::FromInclusive(selectionRects.at(i));

        // retrieve the data from the screen buffer
        auto it = GetCellDataAt(highlight.Origin(), highlight);

        // allocate a string buffer
        std::wstring selectionText;
        std::vector<COLORREF> selectionFgAttr;
        std::vector<COLORREF> selectionBkAttr;

        // preallocate to avoid reallocs
        selectionText.reserve(gsl::narrow<size_t>(highlight.Width()) + 2); // + 2 for \r\n if we munged it
        if (copyTextColor)
        {
            selectionFgAttr.reserve(gsl::narrow<size_t>(highlight.Width()) + 2);
            selectionBkAttr.reserve(gsl::narrow<size_t>(highlight.Width()) + 2);
        }

        // copy char data into the string buffer, skipping trailing bytes
        while (it)
        {
            const auto& cell = *it;

            if (!cell.DbcsAttr().IsTrailing())
            {
                selectionText.append(cell.Chars());

                if (copyTextColor)
                {
                    const auto cellData = cell.TextAttr();
                    const auto [CellFgAttr, CellBkAttr] = GetAttributeColors(cellData);
                    for (const wchar_t wch : cell.Chars())
                    {
                        selectionFgAttr.push_back(CellFgAttr);
                        selectionBkAttr.push_back(CellBkAttr);
                    }
                }
            }
#pragma warning(suppress : 26444)
            // TODO GH 2675: figure out why there's custom construction/destruction happening here
            it++;
        }

        // We apply formatting to rows if the row was NOT wrapped or formatting of wrapped rows is allowed
        const bool shouldFormatRow = formatWrappedRows || !GetRowByOffset(iRow).WasWrapForced();

        if (trimTrailingWhitespace)
        {
            if (shouldFormatRow)
            {
                // remove the spaces at the end (aka trim the trailing whitespace)
                while (!selectionText.empty() && selectionText.back() == UNICODE_SPACE)
                {
                    selectionText.pop_back();
                    if (copyTextColor)
                    {
                        selectionFgAttr.pop_back();
                        selectionBkAttr.pop_back();
                    }
                }
            }
        }

        // apply CR/LF to the end of the final string, unless we're the last line.
        // a.k.a if we're earlier than the bottom, then apply CR/LF.
        if (includeCRLF && i < selectionRects.size() - 1)
        {
            if (shouldFormatRow)
            {
                // then we can assume a CR/LF is proper
                selectionText.push_back(UNICODE_CARRIAGERETURN);
                selectionText.push_back(UNICODE_LINEFEED);

                if (copyTextColor)
                {
                    // cant see CR/LF so just use black FG & BK
                    COLORREF const Blackness = RGB(0x00, 0x00, 0x00);
                    selectionFgAttr.push_back(Blackness);
                    selectionFgAttr.push_back(Blackness);
                    selectionBkAttr.push_back(Blackness);
                    selectionBkAttr.push_back(Blackness);
                }
            }
        }

        data.text.emplace_back(std::move(selectionText));
        if (copyTextColor)
        {
            data.FgAttr.emplace_back(std::move(selectionFgAttr));
            data.BkAttr.emplace_back(std::move(selectionBkAttr));
        }
    }

    return data;
}

// Routine Description:
// - Generates a CF_HTML compliant structure based on the passed in text and color data
// Arguments:
// - rows - the text and color data we will format & encapsulate
// - backgroundColor - default background color for characters, also used in padding
// - fontHeightPoints - the unscaled font height
// - fontFaceName - the name of the font used
// Return Value:
// - string containing the generated HTML
std::string TextBuffer::GenHTML(const TextAndColor& rows,
                                const int fontHeightPoints,
                                const std::wstring_view fontFaceName,
                                const COLORREF backgroundColor)
{
    try
    {
        std::ostringstream htmlBuilder;

        // First we have to add some standard
        // HTML boiler plate required for CF_HTML
        // as part of the HTML Clipboard format
        const std::string htmlHeader =
            "<!DOCTYPE><HTML><HEAD></HEAD><BODY>";
        htmlBuilder << htmlHeader;

        htmlBuilder << "<!--StartFragment -->";

        // apply global style in div element
        {
            htmlBuilder << "<DIV STYLE=\"";
            htmlBuilder << "display:inline-block;";
            htmlBuilder << "white-space:pre;";

            htmlBuilder << "background-color:";
            htmlBuilder << Utils::ColorToHexString(backgroundColor);
            htmlBuilder << ";";

            htmlBuilder << "font-family:";
            htmlBuilder << "'";
            htmlBuilder << ConvertToA(CP_UTF8, fontFaceName);
            htmlBuilder << "',";
            // even with different font, add monospace as fallback
            htmlBuilder << "monospace;";

            htmlBuilder << "font-size:";
            htmlBuilder << fontHeightPoints;
            htmlBuilder << "pt;";

            // note: MS Word doesn't support padding (in this way at least)
            htmlBuilder << "padding:";
            htmlBuilder << 4; // todo: customizable padding
            htmlBuilder << "px;";

            htmlBuilder << "\">";
        }

        // copy text and info color from buffer
        bool hasWrittenAnyText = false;
        std::optional<COLORREF> fgColor = std::nullopt;
        std::optional<COLORREF> bkColor = std::nullopt;
        for (size_t row = 0; row < rows.text.size(); row++)
        {
            size_t startOffset = 0;

            if (row != 0)
            {
                htmlBuilder << "<BR>";
            }

            for (size_t col = 0; col < rows.text.at(row).length(); col++)
            {
                const auto writeAccumulatedChars = [&](bool includeCurrent) {
                    if (col >= startOffset)
                    {
                        const auto unescapedText = ConvertToA(CP_UTF8, std::wstring_view(rows.text.at(row)).substr(startOffset, col - startOffset + includeCurrent));
                        for (const auto c : unescapedText)
                        {
                            switch (c)
                            {
                            case '<':
                                htmlBuilder << "&lt;";
                                break;
                            case '>':
                                htmlBuilder << "&gt;";
                                break;
                            case '&':
                                htmlBuilder << "&amp;";
                                break;
                            default:
                                htmlBuilder << c;
                            }
                        }

                        startOffset = col;
                    }
                };

                if (rows.text.at(row).at(col) == '\r' || rows.text.at(row).at(col) == '\n')
                {
                    // do not include \r nor \n as they don't have color attributes
                    // and are not HTML friendly. For line break use '<BR>' instead.
                    writeAccumulatedChars(false);
                    break;
                }

                bool colorChanged = false;
                if (!fgColor.has_value() || rows.FgAttr.at(row).at(col) != fgColor.value())
                {
                    fgColor = rows.FgAttr.at(row).at(col);
                    colorChanged = true;
                }

                if (!bkColor.has_value() || rows.BkAttr.at(row).at(col) != bkColor.value())
                {
                    bkColor = rows.BkAttr.at(row).at(col);
                    colorChanged = true;
                }

                if (colorChanged)
                {
                    writeAccumulatedChars(false);

                    if (hasWrittenAnyText)
                    {
                        htmlBuilder << "</SPAN>";
                    }

                    htmlBuilder << "<SPAN STYLE=\"";
                    htmlBuilder << "color:";
                    htmlBuilder << Utils::ColorToHexString(fgColor.value());
                    htmlBuilder << ";";
                    htmlBuilder << "background-color:";
                    htmlBuilder << Utils::ColorToHexString(bkColor.value());
                    htmlBuilder << ";";
                    htmlBuilder << "\">";
                }

                hasWrittenAnyText = true;

                // if this is the last character in the row, flush the whole row
                if (col == rows.text.at(row).length() - 1)
                {
                    writeAccumulatedChars(true);
                }
            }
        }

        if (hasWrittenAnyText)
        {
            // last opened span wasn't closed in loop above, so close it now
            htmlBuilder << "</SPAN>";
        }

        htmlBuilder << "</DIV>";

        htmlBuilder << "<!--EndFragment -->";

        constexpr std::string_view HtmlFooter = "</BODY></HTML>";
        htmlBuilder << HtmlFooter;

        // once filled with values, there will be exactly 157 bytes in the clipboard header
        constexpr size_t ClipboardHeaderSize = 157;

        // these values are byte offsets from start of clipboard
        const size_t htmlStartPos = ClipboardHeaderSize;
        const size_t htmlEndPos = ClipboardHeaderSize + gsl::narrow<size_t>(htmlBuilder.tellp());
        const size_t fragStartPos = ClipboardHeaderSize + gsl::narrow<size_t>(htmlHeader.length());
        const size_t fragEndPos = htmlEndPos - HtmlFooter.length();

        // header required by HTML 0.9 format
        std::ostringstream clipHeaderBuilder;
        clipHeaderBuilder << "Version:0.9\r\n";
        clipHeaderBuilder << std::setfill('0');
        clipHeaderBuilder << "StartHTML:" << std::setw(10) << htmlStartPos << "\r\n";
        clipHeaderBuilder << "EndHTML:" << std::setw(10) << htmlEndPos << "\r\n";
        clipHeaderBuilder << "StartFragment:" << std::setw(10) << fragStartPos << "\r\n";
        clipHeaderBuilder << "EndFragment:" << std::setw(10) << fragEndPos << "\r\n";
        clipHeaderBuilder << "StartSelection:" << std::setw(10) << fragStartPos << "\r\n";
        clipHeaderBuilder << "EndSelection:" << std::setw(10) << fragEndPos << "\r\n";

        return clipHeaderBuilder.str() + htmlBuilder.str();
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
        return {};
    }
}

// Routine Description:
// - Generates an RTF document based on the passed in text and color data
//   RTF 1.5 Spec: https://www.biblioscape.com/rtf15_spec.htm
// Arguments:
// - rows - the text and color data we will format & encapsulate
// - backgroundColor - default background color for characters, also used in padding
// - fontHeightPoints - the unscaled font height
// - fontFaceName - the name of the font used
// - htmlTitle - value used in title tag of html header. Used to name the application
// Return Value:
// - string containing the generated RTF
std::string TextBuffer::GenRTF(const TextAndColor& rows, const int fontHeightPoints, const std::wstring_view fontFaceName, const COLORREF backgroundColor)
{
    try
    {
        std::ostringstream rtfBuilder;

        // start rtf
        rtfBuilder << "{";

        // Standard RTF header.
        // This is similar to the header generated by WordPad.
        // \ansi - specifies that the ANSI char set is used in the current doc
        // \ansicpg1252 - represents the ANSI code page which is used to perform the Unicode to ANSI conversion when writing RTF text
        // \deff0 - specifies that the default font for the document is the one at index 0 in the font table
        // \nouicompat - ?
        rtfBuilder << "\\rtf1\\ansi\\ansicpg1252\\deff0\\nouicompat";

        // font table
        rtfBuilder << "{\\fonttbl{\\f0\\fmodern\\fcharset0 " << ConvertToA(CP_UTF8, fontFaceName) << ";}}";

        // map to keep track of colors:
        // keys are colors represented by COLORREF
        // values are indices of the corresponding colors in the color table
        std::unordered_map<COLORREF, int> colorMap;
        int nextColorIndex = 1; // leave 0 for the default color and start from 1.

        // RTF color table
        std::ostringstream colorTableBuilder;
        colorTableBuilder << "{\\colortbl ;";
        colorTableBuilder << "\\red" << static_cast<int>(GetRValue(backgroundColor))
                          << "\\green" << static_cast<int>(GetGValue(backgroundColor))
                          << "\\blue" << static_cast<int>(GetBValue(backgroundColor))
                          << ";";
        colorMap[backgroundColor] = nextColorIndex++;

        // content
        std::ostringstream contentBuilder;
        contentBuilder << "\\viewkind4\\uc4";

        // paragraph styles
        // \fs specifies font size in half-points i.e. \fs20 results in a font size
        // of 10 pts. That's why, font size is multiplied by 2 here.
        contentBuilder << "\\pard\\slmult1\\f0\\fs" << std::to_string(2 * fontHeightPoints)
                       << "\\highlight1"
                       << " ";

        std::optional<COLORREF> fgColor = std::nullopt;
        std::optional<COLORREF> bkColor = std::nullopt;
        for (size_t row = 0; row < rows.text.size(); ++row)
        {
            size_t startOffset = 0;

            if (row != 0)
            {
                contentBuilder << "\\line "; // new line
            }

            for (size_t col = 0; col < rows.text.at(row).length(); ++col)
            {
                const auto writeAccumulatedChars = [&](bool includeCurrent) {
                    if (col >= startOffset)
                    {
                        const auto unescapedText = ConvertToA(CP_UTF8, std::wstring_view(rows.text.at(row)).substr(startOffset, col - startOffset + includeCurrent));
                        for (const auto c : unescapedText)
                        {
                            switch (c)
                            {
                            case '\\':
                            case '{':
                            case '}':
                                contentBuilder << "\\" << c;
                                break;
                            default:
                                contentBuilder << c;
                            }
                        }

                        startOffset = col;
                    }
                };

                if (rows.text.at(row).at(col) == '\r' || rows.text.at(row).at(col) == '\n')
                {
                    // do not include \r nor \n as they don't have color attributes.
                    // For line break use \line instead.
                    writeAccumulatedChars(false);
                    break;
                }

                bool colorChanged = false;
                if (!fgColor.has_value() || rows.FgAttr.at(row).at(col) != fgColor.value())
                {
                    fgColor = rows.FgAttr.at(row).at(col);
                    colorChanged = true;
                }

                if (!bkColor.has_value() || rows.BkAttr.at(row).at(col) != bkColor.value())
                {
                    bkColor = rows.BkAttr.at(row).at(col);
                    colorChanged = true;
                }

                if (colorChanged)
                {
                    writeAccumulatedChars(false);

                    int bkColorIndex = 0;
                    if (colorMap.find(bkColor.value()) != colorMap.end())
                    {
                        // color already exists in the map, just retrieve the index
                        bkColorIndex = colorMap[bkColor.value()];
                    }
                    else
                    {
                        // color not present in the map, so add it
                        colorTableBuilder << "\\red" << static_cast<int>(GetRValue(bkColor.value()))
                                          << "\\green" << static_cast<int>(GetGValue(bkColor.value()))
                                          << "\\blue" << static_cast<int>(GetBValue(bkColor.value()))
                                          << ";";
                        colorMap[bkColor.value()] = nextColorIndex;
                        bkColorIndex = nextColorIndex++;
                    }

                    int fgColorIndex = 0;
                    if (colorMap.find(fgColor.value()) != colorMap.end())
                    {
                        // color already exists in the map, just retrieve the index
                        fgColorIndex = colorMap[fgColor.value()];
                    }
                    else
                    {
                        // color not present in the map, so add it
                        colorTableBuilder << "\\red" << static_cast<int>(GetRValue(fgColor.value()))
                                          << "\\green" << static_cast<int>(GetGValue(fgColor.value()))
                                          << "\\blue" << static_cast<int>(GetBValue(fgColor.value()))
                                          << ";";
                        colorMap[fgColor.value()] = nextColorIndex;
                        fgColorIndex = nextColorIndex++;
                    }

                    contentBuilder << "\\highlight" << bkColorIndex
                                   << "\\cf" << fgColorIndex
                                   << " ";
                }

                // if this is the last character in the row, flush the whole row
                if (col == rows.text.at(row).length() - 1)
                {
                    writeAccumulatedChars(true);
                }
            }
        }

        // end colortbl
        colorTableBuilder << "}";

        // add color table to the final RTF
        rtfBuilder << colorTableBuilder.str();

        // add the text content to the final RTF
        rtfBuilder << contentBuilder.str();

        // end rtf
        rtfBuilder << "}";

        return rtfBuilder.str();
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
        return {};
    }
}

// Function Description:
// - Reflow the contents from the old buffer into the new buffer. The new buffer
//   can have different dimensions than the old buffer. If it does, then this
//   function will attempt to maintain the logical contents of the old buffer,
//   by continuing wrapped lines onto the next line in the new buffer.
// Arguments:
// - oldBuffer - the text buffer to copy the contents FROM
// - newBuffer - the text buffer to copy the contents TO
// - lastCharacterViewport - Optional. If the caller knows that the last
//   nonspace character is in a particular Viewport, the caller can provide this
//   parameter as an optimization, as opposed to searching the entire buffer.
// - positionInfo - Optional. The caller can provide a pair of rows in this
//   parameter and we'll calculate the position of the _end_ of those rows in
//   the new buffer. The rows's new value is placed back into this parameter.
// Return Value:
// - S_OK if we successfully copied the contents to the new buffer, otherwise an appropriate HRESULT.
HRESULT TextBuffer::Reflow(TextBuffer& oldBuffer,
                           TextBuffer& newBuffer,
                           const std::optional<Viewport> lastCharacterViewport,
                           std::optional<std::reference_wrapper<PositionInformation>> positionInfo)
{
    const Cursor& oldCursor = oldBuffer.GetCursor();
    Cursor& newCursor = newBuffer.GetCursor();

    // We need to save the old cursor position so that we can
    // place the new cursor back on the equivalent character in
    // the new buffer.
    const COORD cOldCursorPos = oldCursor.GetPosition();
    const COORD cOldLastChar = oldBuffer.GetLastNonSpaceCharacter(lastCharacterViewport);

    const short cOldRowsTotal = cOldLastChar.Y + 1;

    COORD cNewCursorPos = { 0 };
    bool fFoundCursorPos = false;
    bool foundOldMutable = false;
    bool foundOldVisible = false;
    HRESULT hr = S_OK;
    // Loop through all the rows of the old buffer and reprint them into the new buffer
    for (short iOldRow = 0; iOldRow < cOldRowsTotal; iOldRow++)
    {
        // Fetch the row and its "right" which is the last printable character.
        const ROW& row = oldBuffer.GetRowByOffset(iOldRow);
        const short cOldColsTotal = oldBuffer.GetLineWidth(iOldRow);
        const CharRow& charRow = row.GetCharRow();
        short iRight = gsl::narrow_cast<short>(charRow.MeasureRight());

        // If we're starting a new row, try and preserve the line rendition
        // from the row in the original buffer.
        const auto newBufferPos = newBuffer.GetCursor().GetPosition();
        if (newBufferPos.X == 0)
        {
            auto& newRow = newBuffer.GetRowByOffset(newBufferPos.Y);
            newRow.SetLineRendition(row.GetLineRendition());
        }

        // There is a special case here. If the row has a "wrap"
        // flag on it, but the right isn't equal to the width (one
        // index past the final valid index in the row) then there
        // were a bunch trailing of spaces in the row.
        // (But the measuring functions for each row Left/Right do
        // not count spaces as "displayable" so they're not
        // included.)
        // As such, adjust the "right" to be the width of the row
        // to capture all these spaces
        if (row.WasWrapForced())
        {
            iRight = cOldColsTotal;

            // And a combined special case.
            // If we wrapped off the end of the row by adding a
            // piece of padding because of a double byte LEADING
            // character, then remove one from the "right" to
            // leave this padding out of the copy process.
            if (row.WasDoubleBytePadded())
            {
                iRight--;
            }
        }

        // Loop through every character in the current row (up to
        // the "right" boundary, which is one past the final valid
        // character)
        for (short iOldCol = 0; iOldCol < iRight; iOldCol++)
        {
            if (iOldCol == cOldCursorPos.X && iOldRow == cOldCursorPos.Y)
            {
                cNewCursorPos = newCursor.GetPosition();
                fFoundCursorPos = true;
            }

            try
            {
                // TODO: MSFT: 19446208 - this should just use an iterator and the inserter...
                const auto glyph = row.GetCharRow().GlyphAt(iOldCol);
                const auto dbcsAttr = row.GetCharRow().DbcsAttrAt(iOldCol);
                const auto textAttr = row.GetAttrRow().GetAttrByColumn(iOldCol);

                if (!newBuffer.InsertCharacter(glyph, dbcsAttr, textAttr))
                {
                    hr = E_OUTOFMEMORY;
                    break;
                }
            }
            CATCH_RETURN();
        }

        // If we found the old row that the caller was interested in, set the
        // out value of that parameter to the cursor's current Y position (the
        // new location of the _end_ of that row in the buffer).
        if (positionInfo.has_value())
        {
            if (!foundOldMutable)
            {
                if (iOldRow >= positionInfo.value().get().mutableViewportTop)
                {
                    positionInfo.value().get().mutableViewportTop = newCursor.GetPosition().Y;
                    foundOldMutable = true;
                }
            }

            if (!foundOldVisible)
            {
                if (iOldRow >= positionInfo.value().get().visibleViewportTop)
                {
                    positionInfo.value().get().visibleViewportTop = newCursor.GetPosition().Y;
                    foundOldVisible = true;
                }
            }
        }

        if (SUCCEEDED(hr))
        {
            // If we didn't have a full row to copy, insert a new
            // line into the new buffer.
            // Only do so if we were not forced to wrap. If we did
            // force a word wrap, then the existing line break was
            // only because we ran out of space.
            if (iRight < cOldColsTotal && !row.WasWrapForced())
            {
                if (iRight == cOldCursorPos.X && iOldRow == cOldCursorPos.Y)
                {
                    cNewCursorPos = newCursor.GetPosition();
                    fFoundCursorPos = true;
                }
                // Only do this if it's not the final line in the buffer.
                // On the final line, we want the cursor to sit
                // where it is done printing for the cursor
                // adjustment to follow.
                if (iOldRow < cOldRowsTotal - 1)
                {
                    hr = newBuffer.NewlineCursor() ? hr : E_OUTOFMEMORY;
                }
                else
                {
                    // If we are on the final line of the buffer, we have one more check.
                    // We got into this code path because we are at the right most column of a row in the old buffer
                    // that had a hard return (no wrap was forced).
                    // However, as we're inserting, the old row might have just barely fit into the new buffer and
                    // caused a new soft return (wrap was forced) putting the cursor at x=0 on the line just below.
                    // We need to preserve the memory of the hard return at this point by inserting one additional
                    // hard newline, otherwise we've lost that information.
                    // We only do this when the cursor has just barely poured over onto the next line so the hard return
                    // isn't covered by the soft one.
                    // e.g.
                    // The old line was:
                    // |aaaaaaaaaaaaaaaaaaa | with no wrap which means there was a newline after that final a.
                    // The cursor was here ^
                    // And the new line will be:
                    // |aaaaaaaaaaaaaaaaaaa| and show a wrap at the end
                    // |                   |
                    //  ^ and the cursor is now there.
                    // If we leave it like this, we've lost the newline information.
                    // So we insert one more newline so a continued reflow of this buffer by resizing larger will
                    // continue to look as the original output intended with the newline data.
                    // After this fix, it looks like this:
                    // |aaaaaaaaaaaaaaaaaaa| no wrap at the end (preserved hard newline)
                    // |                   |
                    //  ^ and the cursor is now here.
                    const COORD coordNewCursor = newCursor.GetPosition();
                    if (coordNewCursor.X == 0 && coordNewCursor.Y > 0)
                    {
                        if (newBuffer.GetRowByOffset(gsl::narrow_cast<size_t>(coordNewCursor.Y) - 1).WasWrapForced())
                        {
                            hr = newBuffer.NewlineCursor() ? hr : E_OUTOFMEMORY;
                        }
                    }
                }
            }
        }
    }
    if (SUCCEEDED(hr))
    {
        // Finish copying remaining parameters from the old text buffer to the new one
        newBuffer.CopyProperties(oldBuffer);
        newBuffer.CopyHyperlinkMaps(oldBuffer);
        newBuffer.CopyPatterns(oldBuffer);

        // If we found where to put the cursor while placing characters into the buffer,
        //   just put the cursor there. Otherwise we have to advance manually.
        if (fFoundCursorPos)
        {
            newCursor.SetPosition(cNewCursorPos);
        }
        else
        {
            // Advance the cursor to the same offset as before
            // get the number of newlines and spaces between the old end of text and the old cursor,
            //   then advance that many newlines and chars
            int iNewlines = cOldCursorPos.Y - cOldLastChar.Y;
            const int iIncrements = cOldCursorPos.X - cOldLastChar.X;
            const COORD cNewLastChar = newBuffer.GetLastNonSpaceCharacter();

            // If the last row of the new buffer wrapped, there's going to be one less newline needed,
            //   because the cursor is already on the next line
            if (newBuffer.GetRowByOffset(cNewLastChar.Y).WasWrapForced())
            {
                iNewlines = std::max(iNewlines - 1, 0);
            }
            else
            {
                // if this buffer didn't wrap, but the old one DID, then the d(columns) of the
                //   old buffer will be one more than in this buffer, so new need one LESS.
                if (oldBuffer.GetRowByOffset(cOldLastChar.Y).WasWrapForced())
                {
                    iNewlines = std::max(iNewlines - 1, 0);
                }
            }

            for (int r = 0; r < iNewlines; r++)
            {
                if (!newBuffer.NewlineCursor())
                {
                    hr = E_OUTOFMEMORY;
                    break;
                }
            }
            if (SUCCEEDED(hr))
            {
                for (int c = 0; c < iIncrements - 1; c++)
                {
                    if (!newBuffer.IncrementCursor())
                    {
                        hr = E_OUTOFMEMORY;
                        break;
                    }
                }
            }
        }
    }

    if (SUCCEEDED(hr))
    {
        // Save old cursor size before we delete it
        ULONG const ulSize = oldCursor.GetSize();

        // Set size back to real size as it will be taking over the rendering duties.
        newCursor.SetSize(ulSize);
    }

    return hr;
}

// Method Description:
// - Adds or updates a hyperlink in our hyperlink table
// Arguments:
// - The hyperlink URI, the hyperlink id (could be new or old)
void TextBuffer::AddHyperlinkToMap(std::wstring_view uri, uint16_t id)
{
    _hyperlinkMap[id] = uri;
}

// Method Description:
// - Retrieves the URI associated with a particular hyperlink ID
// Arguments:
// - The hyperlink ID
// Return Value:
// - The URI
std::wstring TextBuffer::GetHyperlinkUriFromId(uint16_t id) const
{
    return _hyperlinkMap.at(id);
}

// Method description:
// - Provides the hyperlink ID to be assigned as a text attribute, based on the optional custom id provided
// Arguments:
// - The user-defined id
// Return value:
// - The internal hyperlink ID
uint16_t TextBuffer::GetHyperlinkId(std::wstring_view uri, std::wstring_view id)
{
    uint16_t numericId = 0;
    if (id.empty())
    {
        // no custom id specified, return our internal count
        numericId = _currentHyperlinkId;
        ++_currentHyperlinkId;
    }
    else
    {
        // assign _currentHyperlinkId if the custom id does not already exist
        std::wstring newId{ id };
        // hash the URL and add it to the custom ID - GH#7698
        newId += L"%" + std::to_wstring(std::hash<std::wstring_view>{}(uri));
        const auto result = _hyperlinkCustomIdMap.emplace(newId, _currentHyperlinkId);
        if (result.second)
        {
            // the custom id did not already exist
            ++_currentHyperlinkId;
        }
        numericId = (*(result.first)).second;
    }
    // _currentHyperlinkId could overflow, make sure its not 0
    if (_currentHyperlinkId == 0)
    {
        ++_currentHyperlinkId;
    }
    return numericId;
}

// Method Description:
// - Removes a hyperlink from the hyperlink map and the associated
//   user defined id from the custom id map (if there is one)
// Arguments:
// - The ID of the hyperlink to be removed
void TextBuffer::RemoveHyperlinkFromMap(uint16_t id) noexcept
{
    _hyperlinkMap.erase(id);
    for (const auto& customIdPair : _hyperlinkCustomIdMap)
    {
        if (customIdPair.second == id)
        {
            _hyperlinkCustomIdMap.erase(customIdPair.first);
            break;
        }
    }
}

// Method Description:
// - Obtains the custom ID, if there was one, associated with the
//   uint16_t id of a hyperlink
// Arguments:
// - The uint16_t id of the hyperlink
// Return Value:
// - The custom ID if there was one, empty string otherwise
std::wstring TextBuffer::GetCustomIdFromId(uint16_t id) const
{
    for (auto customIdPair : _hyperlinkCustomIdMap)
    {
        if (customIdPair.second == id)
        {
            return customIdPair.first;
        }
    }
    return {};
}

// Method Description:
// - Copies the hyperlink/customID maps of the old buffer into this one,
//   also copies currentHyperlinkId
// Arguments:
// - The other buffer
void TextBuffer::CopyHyperlinkMaps(const TextBuffer& other)
{
    _hyperlinkMap = other._hyperlinkMap;
    _hyperlinkCustomIdMap = other._hyperlinkCustomIdMap;
    _currentHyperlinkId = other._currentHyperlinkId;
}

// Method Description:
// - Adds a regex pattern we should search for
// - The searching does not happen here, we only search when asked to by TerminalCore
// Arguments:
// - The regex pattern
// Return value:
// - An ID that the caller should associate with the given pattern
const size_t TextBuffer::AddPatternRecognizer(const std::wstring_view regexString)
{
    ++_currentPatternId;
    _idsAndPatterns.emplace(std::make_pair(_currentPatternId, regexString));
    return _currentPatternId;
}

// Method Description:
// - Clears the patterns we know of and resets the pattern ID counter
void TextBuffer::ClearPatternRecognizers() noexcept
{
    _idsAndPatterns.clear();
    _currentPatternId = 0;
}

// Method Description:
// - Copies the patterns the other buffer knows about into this one
// Arguments:
// - The other buffer
void TextBuffer::CopyPatterns(const TextBuffer& OtherBuffer)
{
    _idsAndPatterns = OtherBuffer._idsAndPatterns;
    _currentPatternId = OtherBuffer._currentPatternId;
}

// Method Description:
// - Finds patterns within the requested region of the text buffer
// Arguments:
// - The firstRow to start searching from
// - The lastRow to search
// Return value:
// - An interval tree containing the patterns found
PointTree TextBuffer::GetPatterns(const size_t firstRow, const size_t lastRow) const
{
    PointTree::interval_vector intervals;

    std::wstring concatAll;
    const auto rowSize = GetRowByOffset(0).size();
    concatAll.reserve(rowSize * (lastRow - firstRow + 1));

    // to deal with text that spans multiple lines, we will first concatenate
    // all the text into one string and find the patterns in that string
    for (auto i = firstRow; i <= lastRow; ++i)
    {
        auto& row = GetRowByOffset(i);
        concatAll += row.GetText();
    }

    // for each pattern we know of, iterate through the string
    for (const auto& idAndPattern : _idsAndPatterns)
    {
        std::wregex regexObj{ idAndPattern.second };

        // search through the run with our regex object
        auto words_begin = std::wsregex_iterator(concatAll.begin(), concatAll.end(), regexObj);
        auto words_end = std::wsregex_iterator();

        size_t lenUpToThis = 0;
        for (auto i = words_begin; i != words_end; ++i)
        {
            // record the locations -
            // when we find a match, the prefix is text that is between this
            // match and the previous match, so we use the size of the prefix
            // along with the size of the match to determine the locations
            size_t prefixSize = 0;

            for (const auto ch : i->prefix().str())
            {
                prefixSize += IsGlyphFullWidth(ch) ? 2 : 1;
            }
            const auto start = lenUpToThis + prefixSize;
            size_t matchSize = 0;
            for (const auto ch : i->str())
            {
                matchSize += IsGlyphFullWidth(ch) ? 2 : 1;
            }
            const auto end = start + matchSize;
            lenUpToThis = end;

            const til::point startCoord{ gsl::narrow<SHORT>(start % rowSize), gsl::narrow<SHORT>(start / rowSize) };
            const til::point endCoord{ gsl::narrow<SHORT>(end % rowSize), gsl::narrow<SHORT>(end / rowSize) };

            // store the intervals
            // NOTE: these intervals are relative to the VIEWPORT not the buffer
            // Keeping these relative to the viewport for now because its the renderer
            // that actually uses these locations and the renderer works relative to
            // the viewport
            intervals.push_back(PointTree::interval(startCoord, endCoord, idAndPattern.first));
        }
    }
    PointTree result(std::move(intervals));
    return result;
}
