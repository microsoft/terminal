// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "textBuffer.hpp"
#include "CharRow.hpp"

#include "../types/inc/utils.hpp"
#include "../types/inc/convert.hpp"

#pragma hdrstop

using namespace Microsoft::Console;
using namespace Microsoft::Console::Types;

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
    _renderTarget{ renderTarget }
{
    // initialize ROWs
    for (size_t i = 0; i < static_cast<size_t>(screenBufferSize.Y); ++i)
    {
        _storage.emplace_back(static_cast<SHORT>(i), screenBufferSize.X, _currentAttributes, this);
    }
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
            prevRow.GetCharRow().ClearCell(coordPrevPosition.X);
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
    // Assert the buffer state is ready for this character
    // This function corrects most errors. If this is false, we had an uncorrectable one.
    FAIL_FAST_IF(!(_AssertValidDoubleByteSequence(dbcsAttribute))); // Shouldn't be uncorrectable sequences unless something is very wrong.

    bool fSuccess = true;
    // Now compensate if we don't have enough space for the upcoming double byte sequence
    // We only need to compensate for leading bytes
    if (dbcsAttribute.IsLeading())
    {
        short const sBufferWidth = GetSize().Width();

        // If we're about to lead on the last column in the row, we need to add a padding space
        if (GetCursor().GetPosition().X == sBufferWidth - 1)
        {
            // set that we're wrapping for double byte reasons
            CharRow& charRow = GetRowByOffset(GetCursor().GetPosition().Y).GetCharRow();
            charRow.SetDoubleBytePadded(true);

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
    GetRowByOffset(uiCurrentRowOffset).GetCharRow().SetWrapForced(fSet);
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
    const short iFinalColumnIndex = GetSize().RightInclusive();

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
// - <none>
//Return Value:
// - true if we successfully incremented the buffer.
bool TextBuffer::IncrementCircularBuffer()
{
    // FirstRow is at any given point in time the array index in the circular buffer that corresponds
    // to the logical position 0 in the window (cursor coordinates and all other coordinates).
    _renderTarget.TriggerCircling();

    // First, clean out the old "first row" as it will become the "last row" of the buffer after the circle is performed.
    const bool fSuccess = _storage.at(_firstRow).Reset(_currentAttributes);
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
// - Retrieves the position of the last non-space character on the final line of the text buffer.
// - By default, we search the entire buffer to find the last non-space character
//Arguments:
// - <none>
//Return Value:
// - Coordinate position in screen coordinates (offset coordinates, not array index coordinates).
COORD TextBuffer::GetLastNonSpaceCharacter() const
{
    return GetLastNonSpaceCharacter(GetSize());
}

//Routine Description:
// - Retrieves the position of the last non-space character in the given viewport
// - This is basically an optimized version of GetLastNonSpaceCharacter(), and can be called when
// - we know the last character is within the given viewport (so we don't need to check the entire buffer)
//Arguments:
// - The viewport
//Return value:
// - Coordinate position (relative to the text buffer)
COORD TextBuffer::GetLastNonSpaceCharacter(const Microsoft::Console::Types::Viewport viewport) const
{
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
            // move the cursor to the right edge
            coordPosition.X = GetSize().RightInclusive();

            // and up one line
            coordPosition.Y--;
        }
    }

    return coordPosition;
}

const SHORT TextBuffer::GetFirstRowIndex() const noexcept
{
    return _firstRow;
}
const Viewport TextBuffer::GetSize() const
{
    return Viewport::FromDimensions({ 0, 0 }, { gsl::narrow<SHORT>(_storage.at(0).size()), gsl::narrow<SHORT>(_storage.size()) });
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

void TextBuffer::SetCurrentAttributes(const TextAttribute currentAttributes) noexcept
{
    _currentAttributes = currentAttributes;
}

// Routine Description:
// - Resets the text contents of this buffer with the default character
//   and the default current color attributes
void TextBuffer::Reset()
{
    const auto attr = GetCurrentAttributes();

    for (auto& row : _storage)
    {
        row.GetCharRow().Reset();
        row.GetAttrRow().Reset(attr);
    }
}

// Routine Description:
// - This is the legacy screen resize with minimal changes
// Arguments:
// - newSize - new size of screen.
// Return Value:
// - Success if successful. Invalid parameter if screen buffer size is unexpected. No memory if allocation failed.
[[nodiscard]] NTSTATUS TextBuffer::ResizeTraditional(const COORD newSize)
{
    RETURN_HR_IF(E_INVALIDARG, newSize.X < 0 || newSize.Y < 0);

    const auto currentSize = GetSize().Dimensions();
    const auto attributes = GetCurrentAttributes();

    SHORT TopRow = 0; // new top row of the screen buffer
    if (newSize.Y <= GetCursor().GetPosition().Y)
    {
        TopRow = GetCursor().GetPosition().Y - newSize.Y + 1;
    }
    const SHORT TopRowIndex = (GetFirstRowIndex() + TopRow) % currentSize.Y;

    // rotate rows until the top row is at index 0
    try
    {
        const ROW& newTopRow = _storage.at(TopRowIndex);
        while (&newTopRow != &_storage.front())
        {
            _storage.push_back(std::move(_storage.front()));
            _storage.pop_front();
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
    std::map<SHORT, SHORT> rowMap;
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

// Routine Description:
// - Retrieves the text data from the selected region and presents it in a clipboard-ready format (given little post-processing).
// Arguments:
// - lineSelection - true if entire line is being selected. False otherwise (box selection)
// - trimTrailingWhitespace - setting flag removes trailing whitespace at the end of each row in selection
// - selectionRects - the selection regions from which the data will be extracted from the buffer
// - GetForegroundColor - function used to map TextAttribute to RGB COLORREF for foreground color
// - GetBackgroundColor - function used to map TextAttribute to RGB COLORREF for foreground color
// Return Value:
// - The text, background color, and foreground color data of the selected region of the text buffer.
const TextBuffer::TextAndColor TextBuffer::GetTextForClipboard(const bool lineSelection,
                                                               const bool trimTrailingWhitespace,
                                                               const std::vector<SMALL_RECT>& selectionRects,
                                                               std::function<COLORREF(TextAttribute&)> GetForegroundColor,
                                                               std::function<COLORREF(TextAttribute&)> GetBackgroundColor) const
{
    TextAndColor data;

    // preallocate our vectors to reduce reallocs
    size_t const rows = selectionRects.size();
    data.text.reserve(rows);
    data.FgAttr.reserve(rows);
    data.BkAttr.reserve(rows);

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
        selectionFgAttr.reserve(gsl::narrow<size_t>(highlight.Width()) + 2);
        selectionBkAttr.reserve(gsl::narrow<size_t>(highlight.Width()) + 2);

        // copy char data into the string buffer, skipping trailing bytes
        while (it)
        {
            const auto& cell = *it;
            auto cellData = cell.TextAttr();
            COLORREF const CellFgAttr = GetForegroundColor(cellData);
            COLORREF const CellBkAttr = GetBackgroundColor(cellData);

            if (!cell.DbcsAttr().IsTrailing())
            {
                selectionText.append(cell.Chars());
                for (const wchar_t wch : cell.Chars())
                {
                    selectionFgAttr.push_back(CellFgAttr);
                    selectionBkAttr.push_back(CellBkAttr);
                }
            }
#pragma warning(suppress : 26444)
            // TODO GH 2675: figure out why there's custom construction/destruction happening here
            it++;
        }

        // trim trailing spaces if SHIFT key not held
        if (trimTrailingWhitespace)
        {
            const ROW& Row = GetRowByOffset(iRow);

            // FOR LINE SELECTION ONLY: if the row was wrapped, don't remove the spaces at the end.
            if (!lineSelection || !Row.GetCharRow().WasWrapForced())
            {
                while (!selectionText.empty() && selectionText.back() == UNICODE_SPACE)
                {
                    selectionText.pop_back();
                    selectionFgAttr.pop_back();
                    selectionBkAttr.pop_back();
                }
            }

            // apply CR/LF to the end of the final string, unless we're the last line.
            // a.k.a if we're earlier than the bottom, then apply CR/LF.
            if (i < selectionRects.size() - 1)
            {
                // FOR LINE SELECTION ONLY: if the row was wrapped, do not apply CR/LF.
                // a.k.a. if the row was NOT wrapped, then we can assume a CR/LF is proper
                // always apply \r\n for box selection
                if (!lineSelection || !GetRowByOffset(iRow).GetCharRow().WasWrapForced())
                {
                    COLORREF const Blackness = RGB(0x00, 0x00, 0x00); // cant see CR/LF so just use black FG & BK

                    selectionText.push_back(UNICODE_CARRIAGERETURN);
                    selectionText.push_back(UNICODE_LINEFEED);
                    selectionFgAttr.push_back(Blackness);
                    selectionFgAttr.push_back(Blackness);
                    selectionBkAttr.push_back(Blackness);
                    selectionBkAttr.push_back(Blackness);
                }
            }
        }

        data.text.emplace_back(std::move(selectionText));
        data.FgAttr.emplace_back(std::move(selectionFgAttr));
        data.BkAttr.emplace_back(std::move(selectionBkAttr));
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
// - htmlTitle - value used in title tag of html header. Used to name the application
// Return Value:
// - string containing the generated HTML
std::string TextBuffer::GenHTML(const TextAndColor& rows, const int fontHeightPoints, const std::wstring_view fontFaceName, const COLORREF backgroundColor, const std::string& htmlTitle)
{
    try
    {
        std::ostringstream htmlBuilder;

        // First we have to add some standard
        // HTML boiler plate required for CF_HTML
        // as part of the HTML Clipboard format
        const std::string htmlHeader =
            "<!DOCTYPE><HTML><HEAD><TITLE>" + htmlTitle + "</TITLE></HEAD><BODY>";
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
                // do not include \r nor \n as they don't have attributes
                // and are not HTML friendly. For line break use '<BR>' instead.
                const bool isLastCharInRow =
                    col == rows.text.at(row).length() - 1 ||
                    rows.text.at(row).at(col + 1) == '\r' ||
                    rows.text.at(row).at(col + 1) == '\n';

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

                if (isLastCharInRow)
                {
                    writeAccumulatedChars(true);
                    break;
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
        // This is similar to the header gnerated by WordPad.
        // \ansi - specifies that the ANSI char set is used in the current doc
        // \ansicpg1252 - represents the ANSI code page which is used to perform the Unicode to ANSI conversion when writing RTF text
        // \deff0 - specifes that the default font for the document is the one at index 0 in the font table
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
        // \fs specificies font size in half-points i.e. \fs20 results in a font size
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
                const bool isLastCharInRow =
                    col == rows.text.at(row).length() - 1 ||
                    rows.text.at(row).at(col + 1) == '\r' ||
                    rows.text.at(row).at(col + 1) == '\n';

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

                    contentBuilder << "\\highglight" << bkColorIndex
                                   << "\\cf" << fgColorIndex
                                   << " ";
                }

                if (isLastCharInRow)
                {
                    writeAccumulatedChars(true);
                    break;
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
