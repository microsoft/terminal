// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "textBuffer.hpp"

#include <til/hash.h>
#include <til/unicode.h>

#include "../renderer/base/renderer.hpp"
#include "../types/inc/utils.hpp"
#include "../types/inc/convert.hpp"
#include "../../types/inc/GlyphWidth.hpp"

using namespace Microsoft::Console;
using namespace Microsoft::Console::Types;

using PointTree = interval_tree::IntervalTree<til::point, size_t>;

// Routine Description:
// - Creates a new instance of TextBuffer
// Arguments:
// - screenBufferSize - The X by Y dimensions of the new screen buffer
// - defaultAttributes - The attributes with which the buffer will be initialized
// - cursorSize - The height of the cursor within this buffer
// - isActiveBuffer - Whether this is the currently active buffer
// - renderer - The renderer to use for triggering a redraw
// Return Value:
// - constructed object
// Note: may throw exception
TextBuffer::TextBuffer(til::size screenBufferSize,
                       const TextAttribute defaultAttributes,
                       const UINT cursorSize,
                       const bool isActiveBuffer,
                       Microsoft::Console::Render::Renderer& renderer) :
    _renderer{ renderer },
    _currentAttributes{ defaultAttributes },
    _cursor{ cursorSize, *this },
    _isActiveBuffer{ isActiveBuffer }
{
    // Guard against resizing the text buffer to 0 columns/rows, which would break being able to insert text.
    screenBufferSize.width = std::max(screenBufferSize.width, 1);
    screenBufferSize.height = std::max(screenBufferSize.height, 1);
    _reserve(screenBufferSize, defaultAttributes);
}

TextBuffer::~TextBuffer()
{
    if (_buffer)
    {
        _destroy();
    }
}

// I put these functions in a block at the start of the class, because they're the most
// fundamental aspect of TextBuffer: It implements the basic gap buffer text storage.
// It's also fairly tricky code.
#pragma region buffer management
#pragma warning(push)
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26490) // Don't use reinterpret_cast (type.1).

// MEM_RESERVEs memory sufficient to store height-many ROW structs,
// as well as their ROW::_chars and ROW::_charOffsets buffers.
//
// We use explicit virtual memory allocations to not taint the general purpose allocator
// with our huge allocation, as well as to be able to reduce the private working set of
// the application by only committing what we actually need. This reduces conhost's
// memory usage from ~7MB down to just ~2MB at startup in the general case.
void TextBuffer::_reserve(til::size screenBufferSize, const TextAttribute& defaultAttributes)
{
    const auto w = gsl::narrow<uint16_t>(screenBufferSize.width);
    const auto h = gsl::narrow<uint16_t>(screenBufferSize.height);

    constexpr auto rowSize = ROW::CalculateRowSize();
    const auto charsBufferSize = ROW::CalculateCharsBufferSize(w);
    const auto charOffsetsBufferSize = ROW::CalculateCharOffsetsBufferSize(w);
    const auto rowStride = rowSize + charsBufferSize + charOffsetsBufferSize;
    assert(rowStride % alignof(ROW) == 0);

    // 65535*65535 cells would result in a allocSize of 8GiB.
    // --> Use uint64_t so that we can safely do our calculations even on x86.
    // We allocate 1 additional row, which will be used for GetScratchpadRow().
    const auto rowCount = ::base::strict_cast<uint64_t>(h) + 1;
    const auto allocSize = gsl::narrow<size_t>(rowCount * rowStride);

    // NOTE: Modifications to this block of code might have to be mirrored over to ResizeTraditional().
    // It constructs a temporary TextBuffer and then extracts the members below, overwriting itself.
    _buffer = wil::unique_virtualalloc_ptr<std::byte>{
        static_cast<std::byte*>(THROW_LAST_ERROR_IF_NULL(VirtualAlloc(nullptr, allocSize, MEM_RESERVE, PAGE_READWRITE)))
    };
    _bufferEnd = _buffer.get() + allocSize;
    _commitWatermark = _buffer.get();
    _initialAttributes = defaultAttributes;
    _bufferRowStride = rowStride;
    _bufferOffsetChars = rowSize;
    _bufferOffsetCharOffsets = rowSize + charsBufferSize;
    _width = w;
    _height = h;
}

// MEM_COMMITs the memory and constructs all ROWs up to and including the given row pointer.
// It's expected that the caller verifies the parameter. It goes hand in hand with _getRowByOffsetDirect().
//
// Declaring this function as noinline allows _getRowByOffsetDirect() to be inlined,
// which improves overall TextBuffer performance by ~6%. And all it cost is this annotation.
// The compiler doesn't understand the likelihood of our branches. (PGO does, but that's imperfect.)
__declspec(noinline) void TextBuffer::_commit(const std::byte* row)
{
    const auto rowEnd = row + _bufferRowStride;
    const auto remaining = gsl::narrow_cast<uintptr_t>(_bufferEnd - _commitWatermark);
    const auto minimum = gsl::narrow_cast<uintptr_t>(rowEnd - _commitWatermark);
    const auto ideal = minimum + _bufferRowStride * _commitReadAheadRowCount;
    const auto size = std::min(remaining, ideal);

    THROW_LAST_ERROR_IF_NULL(VirtualAlloc(_commitWatermark, size, MEM_COMMIT, PAGE_READWRITE));

    _construct(_commitWatermark + size);
}

// Destructs and MEM_DECOMMITs all previously constructed ROWs.
// You can use this (or rather the Reset() method) to fully clear the TextBuffer.
void TextBuffer::_decommit() noexcept
{
    _destroy();
    VirtualFree(_buffer.get(), 0, MEM_DECOMMIT);
    _commitWatermark = _buffer.get();
}

// Constructs ROWs up to (excluding) the ROW pointed to by `until`.
void TextBuffer::_construct(const std::byte* until) noexcept
{
    for (; _commitWatermark < until; _commitWatermark += _bufferRowStride)
    {
        const auto row = reinterpret_cast<ROW*>(_commitWatermark);
        const auto chars = reinterpret_cast<wchar_t*>(_commitWatermark + _bufferOffsetChars);
        const auto indices = reinterpret_cast<uint16_t*>(_commitWatermark + _bufferOffsetCharOffsets);
        std::construct_at(row, chars, indices, _width, _initialAttributes);
    }
}

// Destroys all previously constructed ROWs.
// Be careful! This doesn't reset any of the members, in particular the _commitWatermark.
void TextBuffer::_destroy() const noexcept
{
    for (auto it = _buffer.get(); it < _commitWatermark; it += _bufferRowStride)
    {
        std::destroy_at(reinterpret_cast<ROW*>(it));
    }
}

// This function is "direct" because it trusts the caller to properly wrap the "offset"
// parameter modulo the _height of the buffer, etc. But keep in mind that a offset=0
// is the GetScratchpadRow() and not the GetRowByOffset(0). That one is offset=1.
ROW& TextBuffer::_getRowByOffsetDirect(size_t offset)
{
    const auto row = _buffer.get() + _bufferRowStride * offset;
    THROW_HR_IF(E_UNEXPECTED, row < _buffer.get() || row >= _bufferEnd);

    if (row >= _commitWatermark)
    {
        _commit(row);
    }

    return *reinterpret_cast<ROW*>(row);
}

// Returns the "user-visible" index of the last committed row, which can be used
// to short-circuit some algorithms that try to scan the entire buffer.
// Returns 0 if no rows are committed in.
til::CoordType TextBuffer::_estimateOffsetOfLastCommittedRow() const noexcept
{
    const auto lastRowOffset = (_commitWatermark - _buffer.get()) / _bufferRowStride;
    // This subtracts 2 from the offset to account for the:
    // * scratchpad row at offset 0, whereas regular rows start at offset 1.
    // * fact that _commitWatermark points _past_ the last committed row,
    //   but we want to return an index pointing at the last row.
    return std::max(0, gsl::narrow_cast<til::CoordType>(lastRowOffset - 2));
}

// Retrieves a row from the buffer by its offset from the first row of the text buffer
// (what corresponds to the top row of the screen buffer).
const ROW& TextBuffer::GetRowByOffset(const til::CoordType index) const
{
    // The const_cast is safe because "const" never had any meaning in C++ in the first place.
#pragma warning(suppress : 26492) // Don't use const_cast to cast away const or volatile (type.3).
    return const_cast<TextBuffer*>(this)->GetRowByOffset(index);
}

// Retrieves a row from the buffer by its offset from the first row of the text buffer
// (what corresponds to the top row of the screen buffer).
ROW& TextBuffer::GetRowByOffset(const til::CoordType index)
{
    // Rows are stored circularly, so the index you ask for is offset by the start position and mod the total of rows.
    auto offset = (_firstRow + index) % _height;

    // Support negative wrap around. This way an index of -1 will
    // wrap to _rowCount-1 and make implementing scrolling easier.
    if (offset < 0)
    {
        offset += _height;
    }

    // We add 1 to the row offset, because row "0" is the one returned by GetScratchpadRow().
    return _getRowByOffsetDirect(gsl::narrow_cast<size_t>(offset) + 1);
}

// Returns a row filled with whitespace and the current attributes, for you to freely use.
ROW& TextBuffer::GetScratchpadRow()
{
    return GetScratchpadRow(_currentAttributes);
}

// Returns a row filled with whitespace and the given attributes, for you to freely use.
ROW& TextBuffer::GetScratchpadRow(const TextAttribute& attributes)
{
    auto& r = _getRowByOffsetDirect(0);
    r.Reset(attributes);
    return r;
}

#pragma warning(pop)
#pragma endregion

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
til::CoordType TextBuffer::TotalRowCount() const noexcept
{
    return _height;
}

// Method Description:
// - Gets the number of glyphs in the buffer between two points.
// - IMPORTANT: Make sure that start is before end, or this will never return!
// Arguments:
// - start - The starting point of the range to get the glyph count for.
// - end - The ending point of the range to get the glyph count for.
// Return Value:
// - The number of glyphs in the buffer between the two points.
size_t TextBuffer::GetCellDistance(const til::point from, const til::point to) const
{
    auto startCell = GetCellDataAt(from);
    const auto endCell = GetCellDataAt(to);
    auto delta = 0;
    while (startCell != endCell)
    {
        ++startCell;
        ++delta;
    }
    return delta;
}

// Routine Description:
// - Retrieves read-only text iterator at the given buffer location
// Arguments:
// - at - X,Y position in buffer for iterator start position
// Return Value:
// - Read-only iterator of text data only.
TextBufferTextIterator TextBuffer::GetTextDataAt(const til::point at) const
{
    return TextBufferTextIterator(GetCellDataAt(at));
}

// Routine Description:
// - Retrieves read-only cell iterator at the given buffer location
// Arguments:
// - at - X,Y position in buffer for iterator start position
// Return Value:
// - Read-only iterator of cell data.
TextBufferCellIterator TextBuffer::GetCellDataAt(const til::point at) const
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
TextBufferTextIterator TextBuffer::GetTextLineDataAt(const til::point at) const
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
TextBufferCellIterator TextBuffer::GetCellLineDataAt(const til::point at) const
{
    til::inclusive_rect limit;
    limit.top = at.y;
    limit.bottom = at.y;
    limit.left = 0;
    limit.right = GetSize().RightInclusive();

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
TextBufferTextIterator TextBuffer::GetTextDataAt(const til::point at, const Viewport limit) const
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
TextBufferCellIterator TextBuffer::GetCellDataAt(const til::point at, const Viewport limit) const
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
    const auto coordPrevPosition = _GetPreviousFromCursor();
    auto& prevRow = GetRowByOffset(coordPrevPosition.y);
    DbcsAttribute prevDbcsAttr = DbcsAttribute::Single;
    try
    {
        prevDbcsAttr = prevRow.DbcsAttrAt(coordPrevPosition.x);
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
        return false;
    }

    auto fValidSequence = true; // Valid until proven otherwise
    auto fCorrectableByErase = false; // Can't be corrected until proven otherwise

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
    if (prevDbcsAttr == DbcsAttribute::Single && dbcsAttribute == DbcsAttribute::Trailing)
    {
        // N, T failing case (uncorrectable)
        fValidSequence = false;
    }
    else if (prevDbcsAttr == DbcsAttribute::Leading)
    {
        if (dbcsAttribute == DbcsAttribute::Single || dbcsAttribute == DbcsAttribute::Leading)
        {
            // L, N and L, L failing cases (correctable)
            fValidSequence = false;
            fCorrectableByErase = true;
        }
    }
    else if (prevDbcsAttr == DbcsAttribute::Trailing && dbcsAttribute == DbcsAttribute::Trailing)
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
            prevRow.ClearCell(coordPrevPosition.x);
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
void TextBuffer::_PrepareForDoubleByteSequence(const DbcsAttribute dbcsAttribute)
{
    // Now compensate if we don't have enough space for the upcoming double byte sequence
    // We only need to compensate for leading bytes
    if (dbcsAttribute == DbcsAttribute::Leading)
    {
        const auto cursorPosition = GetCursor().GetPosition();
        const auto lineWidth = GetLineWidth(cursorPosition.y);

        // If we're about to lead on the last column in the row, we need to add a padding space
        if (cursorPosition.x == lineWidth - 1)
        {
            // set that we're wrapping for double byte reasons
            auto& row = GetRowByOffset(cursorPosition.y);
            row.SetDoubleBytePadded(true);

            // then move the cursor forward and onto the next row
            IncrementCursor();
        }
    }
}

// Given the character offset `position` in the `chars` string, this function returns the starting position of the next grapheme.
// For instance, given a `chars` of L"x\uD83D\uDE42y" and a `position` of 1 it'll return 3.
// GraphemePrev would do the exact inverse of this operation.
// In the future, these functions are expected to also deliver information about how many columns a grapheme occupies.
// (I know that mere UTF-16 code point iteration doesn't handle graphemes, but that's what we're working towards.)
size_t TextBuffer::GraphemeNext(const std::wstring_view& chars, size_t position) noexcept
{
    return til::utf16_iterate_next(chars, position);
}

// It's the counterpart to GraphemeNext. See GraphemeNext.
size_t TextBuffer::GraphemePrev(const std::wstring_view& chars, size_t position) noexcept
{
    return til::utf16_iterate_prev(chars, position);
}

// This function is intended for writing regular "lines" of text as it'll set the wrap flag on the given row.
// You can continue calling the function on the same row as long as state.columnEnd < state.columnLimit.
void TextBuffer::Write(til::CoordType row, const TextAttribute& attributes, RowWriteState& state)
{
    auto& r = GetRowByOffset(row);
    r.ReplaceText(state);
    r.ReplaceAttributes(state.columnBegin, state.columnEnd, attributes);
    TriggerRedraw(Viewport::FromExclusive({ state.columnBeginDirty, row, state.columnEndDirty, row + 1 }));
}

// Fills an area of the buffer with a given fill character(s) and attributes.
void TextBuffer::FillRect(const til::rect& rect, const std::wstring_view& fill, const TextAttribute& attributes)
{
    if (!rect || fill.empty())
    {
        return;
    }

    auto& scratchpad = GetScratchpadRow(attributes);

    // The scratchpad row gets reset to whitespace by default, so there's no need to
    // initialize it again. Filling with whitespace is the most common operation by far.
    if (fill != L" ")
    {
        RowWriteState state{
            .columnLimit = rect.right,
            .columnEnd = rect.left,
        };

        // Fill the scratchpad row with consecutive copies of "fill" up to the amount we need.
        //
        // We don't just create a single string with N copies of "fill" and write that at once,
        // because that might join neighboring combining marks unintentionally.
        //
        // Building the buffer this way is very wasteful and slow, but it's still 3x
        // faster than what we had before and no one complained about that either.
        // It's seldom used code and probably not worth optimizing for.
        while (state.columnEnd < rect.right)
        {
            state.columnBegin = state.columnEnd;
            state.text = fill;
            scratchpad.ReplaceText(state);
        }
    }

    // Fill the given rows with copies of the scratchpad row. That's a little
    // slower when filling just a single row, but will be much faster for >1 rows.
    {
        RowCopyTextFromState state{
            .source = scratchpad,
            .columnBegin = rect.left,
            .columnLimit = rect.right,
            .sourceColumnBegin = rect.left,
        };

        for (auto y = rect.top; y < rect.bottom; ++y)
        {
            auto& r = GetRowByOffset(y);
            r.CopyTextFrom(state);
            r.ReplaceAttributes(rect.left, rect.right, attributes);
            TriggerRedraw(Viewport::FromExclusive({ state.columnBeginDirty, y, state.columnEndDirty, y + 1 }));
        }
    }
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
                                     const til::point target,
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
        lineTarget.x = 0;
        ++lineTarget.y;
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
                                         const til::point target,
                                         const std::optional<bool> wrap,
                                         std::optional<til::CoordType> limitRight)
{
    // If we're not in bounds, exit early.
    if (!GetSize().IsInBounds(target))
    {
        return givenIt;
    }

    //  Get the row and write the cells
    auto& row = GetRowByOffset(target.y);
    const auto newIt = row.WriteCells(givenIt, target.x, wrap, limitRight);

    // Take the cell distance written and notify that it needs to be repainted.
    const auto written = newIt.GetCellDistance(givenIt);
    const auto paint = Viewport::FromDimensions(target, { written, 1 });
    TriggerRedraw(paint);

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
void TextBuffer::InsertCharacter(const std::wstring_view chars,
                                 const DbcsAttribute dbcsAttribute,
                                 const TextAttribute attr)
{
    // Ensure consistent buffer state for double byte characters based on the character type we're about to insert
    _PrepareForDoubleByteSequence(dbcsAttribute);

    // Get the current cursor position
    const auto iRow = GetCursor().GetPosition().y; // row stored as logical position, not array position
    const auto iCol = GetCursor().GetPosition().x; // column logical and array positions are equal.

    // Get the row associated with the given logical position
    auto& Row = GetRowByOffset(iRow);

    // Store character and double byte data
    switch (dbcsAttribute)
    {
    case DbcsAttribute::Leading:
        Row.ReplaceCharacters(iCol, 2, chars);
        break;
    case DbcsAttribute::Trailing:
        Row.ReplaceCharacters(iCol - 1, 2, chars);
        break;
    default:
        Row.ReplaceCharacters(iCol, 1, chars);
        break;
    }

    // Store color data
    Row.SetAttrToEnd(iCol, attr);
    IncrementCursor();
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
void TextBuffer::InsertCharacter(const wchar_t wch, const DbcsAttribute dbcsAttribute, const TextAttribute attr)
{
    InsertCharacter({ &wch, 1 }, dbcsAttribute, attr);
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
    const auto uiCurrentRowOffset = GetCursor().GetPosition().y;

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
void TextBuffer::IncrementCursor()
{
    // Cursor position is stored as logical array indices (starts at 0) for the window
    // Buffer Size is specified as the "length" of the array. It would say 80 for valid values of 0-79.
    // So subtract 1 from buffer size in each direction to find the index of the final column in the buffer
    const auto iFinalColumnIndex = GetLineWidth(GetCursor().GetPosition().y) - 1;

    // Move the cursor one position to the right
    GetCursor().IncrementXPosition(1);

    // If we've passed the final valid column...
    if (GetCursor().GetPosition().x > iFinalColumnIndex)
    {
        // Then mark that we've been forced to wrap
        _SetWrapOnCurrentRow();

        // Then move the cursor to a new line
        NewlineCursor();
    }
}

//Routine Description:
// - Increments the cursor one line down in the buffer and to the beginning of the line
//Arguments:
// - <none>
//Return Value:
// - true if we successfully moved the cursor.
void TextBuffer::NewlineCursor()
{
    const auto iFinalRowIndex = GetSize().BottomInclusive();

    // Reset the cursor position to 0 and move down one line
    GetCursor().SetXPosition(0);
    GetCursor().IncrementYPosition(1);

    // If we've passed the final valid row...
    if (GetCursor().GetPosition().y > iFinalRowIndex)
    {
        // Stay on the final logical/offset row of the buffer.
        GetCursor().SetYPosition(iFinalRowIndex);

        // Instead increment the circular buffer to move us into the "oldest" row of the backing buffer
        IncrementCircularBuffer();
    }
}

//Routine Description:
// - Increments the circular buffer by one. Circular buffer is represented by FirstRow variable.
//Arguments:
// - fillAttributes - the attributes with which the recycled row will be initialized.
//Return Value:
// - true if we successfully incremented the buffer.
void TextBuffer::IncrementCircularBuffer(const TextAttribute& fillAttributes)
{
    // FirstRow is at any given point in time the array index in the circular buffer that corresponds
    // to the logical position 0 in the window (cursor coordinates and all other coordinates).
    if (_isActiveBuffer)
    {
        _renderer.TriggerFlush(true);
    }

    // Prune hyperlinks to delete obsolete references
    _PruneHyperlinks();

    // Second, clean out the old "first row" as it will become the "last row" of the buffer after the circle is performed.
    GetRowByOffset(0).Reset(fillAttributes);
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
til::point TextBuffer::GetLastNonSpaceCharacter(std::optional<const Microsoft::Console::Types::Viewport> viewOptional) const
{
    const auto viewport = viewOptional.has_value() ? viewOptional.value() : GetSize();

    til::point coordEndOfText;
    // Search the given viewport by starting at the bottom.
    coordEndOfText.y = std::min(viewport.BottomInclusive(), _estimateOffsetOfLastCommittedRow());

    const auto& currRow = GetRowByOffset(coordEndOfText.y);
    // The X position of the end of the valid text is the Right draw boundary (which is one beyond the final valid character)
    coordEndOfText.x = currRow.MeasureRight() - 1;

    // If the X coordinate turns out to be -1, the row was empty, we need to search backwards for the real end of text.
    const auto viewportTop = viewport.Top();
    auto fDoBackUp = (coordEndOfText.x < 0 && coordEndOfText.y > viewportTop); // this row is empty, and we're not at the top
    while (fDoBackUp)
    {
        coordEndOfText.y--;
        const auto& backupRow = GetRowByOffset(coordEndOfText.y);
        // We need to back up to the previous row if this line is empty, AND there are more rows

        coordEndOfText.x = backupRow.MeasureRight() - 1;
        fDoBackUp = (coordEndOfText.x < 0 && coordEndOfText.y > viewportTop);
    }

    // don't allow negative results
    coordEndOfText.y = std::max(coordEndOfText.y, 0);
    coordEndOfText.x = std::max(coordEndOfText.x, 0);

    return coordEndOfText;
}

// Routine Description:
// - Retrieves the position of the previous character relative to the current cursor position
// Arguments:
// - <none>
// Return Value:
// - Coordinate position in screen coordinates of the character just before the cursor.
// - NOTE: Will return 0,0 if already in the top left corner
til::point TextBuffer::_GetPreviousFromCursor() const
{
    auto coordPosition = GetCursor().GetPosition();

    // If we're not at the left edge, simply move the cursor to the left by one
    if (coordPosition.x > 0)
    {
        coordPosition.x--;
    }
    else
    {
        // Otherwise, only if we're not on the top row (e.g. we don't move anywhere in the top left corner. there is no previous)
        if (coordPosition.y > 0)
        {
            // move the cursor up one line
            coordPosition.y--;

            // and to the right edge
            coordPosition.x = GetLineWidth(coordPosition.y) - 1;
        }
    }

    return coordPosition;
}

const til::CoordType TextBuffer::GetFirstRowIndex() const noexcept
{
    return _firstRow;
}

const Viewport TextBuffer::GetSize() const noexcept
{
    return Viewport::FromDimensions({ _width, _height });
}

void TextBuffer::_SetFirstRowIndex(const til::CoordType FirstRowIndex) noexcept
{
    _firstRow = FirstRowIndex;
}

void TextBuffer::ScrollRows(const til::CoordType firstRow, til::CoordType size, const til::CoordType delta)
{
    if (delta == 0)
    {
        return;
    }

    // Since the for() loop uses !=, we must ensure that size is positive.
    // A negative size doesn't make any sense anyways.
    size = std::max(0, size);

    til::CoordType y = 0;
    til::CoordType end = 0;
    til::CoordType step = 0;

    if (delta < 0)
    {
        // The layout is like this:
        // delta is -2, size is 3, firstRow is 5
        // We want 3 rows from 5 (5, 6, and 7) to move up 2 spots.
        // --- (storage) ----
        // | 0 begin
        // | 1
        // | 2
        // | 3 A. firstRow + delta (because delta is negative)
        // | 4
        // | 5 B. firstRow
        // | 6
        // | 7
        // | 8 C. firstRow + size
        // | 9
        // | 10
        // | 11
        // - end
        // We want B to slide up to A (the negative delta) and everything from [B,C) to slide up with it.
        y = firstRow;
        end = firstRow + size;
        step = 1;
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
        // | 5 A. firstRow
        // | 6
        // | 7
        // | 8 B. firstRow + size
        // | 9
        // | 10 C. firstRow + size + delta
        // | 11
        // - end
        // We want B-1 to slide down to C-1 (the positive delta) and everything from [A, B) to slide down with it.
        y = firstRow + size - 1;
        end = firstRow - 1;
        step = -1;
    }

    for (; y != end; y += step)
    {
        GetRowByOffset(y + delta).CopyFrom(GetRowByOffset(y));
    }
}

Cursor& TextBuffer::GetCursor() noexcept
{
    return _cursor;
}

const Cursor& TextBuffer::GetCursor() const noexcept
{
    return _cursor;
}

const TextAttribute& TextBuffer::GetCurrentAttributes() const noexcept
{
    return _currentAttributes;
}

void TextBuffer::SetCurrentAttributes(const TextAttribute& currentAttributes) noexcept
{
    _currentAttributes = currentAttributes;
}

void TextBuffer::SetWrapForced(const til::CoordType y, bool wrap)
{
    GetRowByOffset(y).SetWrapForced(wrap);
}

void TextBuffer::SetCurrentLineRendition(const LineRendition lineRendition, const TextAttribute& fillAttributes)
{
    const auto cursorPosition = GetCursor().GetPosition();
    const auto rowIndex = cursorPosition.y;
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
            const auto fillOffset = GetLineWidth(rowIndex);
            const auto fillLength = gsl::narrow<size_t>(GetSize().Width() - fillOffset);
            const OutputCellIterator fillData{ fillChar, fillAttributes, fillLength };
            row.WriteCells(fillData, fillOffset, false);
            // We also need to make sure the cursor is clamped within the new width.
            GetCursor().SetPosition(ClampPositionWithinLine(cursorPosition));
        }
        TriggerRedraw(Viewport::FromDimensions({ 0, rowIndex }, { GetSize().Width(), 1 }));
    }
}

void TextBuffer::ResetLineRenditionRange(const til::CoordType startRow, const til::CoordType endRow)
{
    for (auto row = startRow; row < endRow; row++)
    {
        GetRowByOffset(row).SetLineRendition(LineRendition::SingleWidth);
    }
}

LineRendition TextBuffer::GetLineRendition(const til::CoordType row) const
{
    return GetRowByOffset(row).GetLineRendition();
}

bool TextBuffer::IsDoubleWidthLine(const til::CoordType row) const
{
    return GetLineRendition(row) != LineRendition::SingleWidth;
}

til::CoordType TextBuffer::GetLineWidth(const til::CoordType row) const
{
    // Use shift right to quickly divide the width by 2 for double width lines.
    const auto scale = IsDoubleWidthLine(row) ? 1 : 0;
    return GetSize().Width() >> scale;
}

til::point TextBuffer::ClampPositionWithinLine(const til::point position) const
{
    const auto rightmostColumn = GetLineWidth(position.y) - 1;
    return { std::min(position.x, rightmostColumn), position.y };
}

til::point TextBuffer::ScreenToBufferPosition(const til::point position) const
{
    // Use shift right to quickly divide the X pos by 2 for double width lines.
    const auto scale = IsDoubleWidthLine(position.y) ? 1 : 0;
    return { position.x >> scale, position.y };
}

til::point TextBuffer::BufferToScreenPosition(const til::point position) const
{
    // Use shift left to quickly multiply the X pos by 2 for double width lines.
    const auto scale = IsDoubleWidthLine(position.y) ? 1 : 0;
    return { position.x << scale, position.y };
}

// Routine Description:
// - Resets the text contents of this buffer with the default character
//   and the default current color attributes
void TextBuffer::Reset() noexcept
{
    _decommit();
    _initialAttributes = _currentAttributes;
}

// Routine Description:
// - This is the legacy screen resize with minimal changes
// Arguments:
// - newSize - new size of screen.
// Return Value:
// - Success if successful. Invalid parameter if screen buffer size is unexpected. No memory if allocation failed.
[[nodiscard]] NTSTATUS TextBuffer::ResizeTraditional(til::size newSize) noexcept
{
    // Guard against resizing the text buffer to 0 columns/rows, which would break being able to insert text.
    newSize.width = std::max(newSize.width, 1);
    newSize.height = std::max(newSize.height, 1);

    try
    {
        TextBuffer newBuffer{ newSize, _currentAttributes, 0, false, _renderer };
        const auto cursorRow = GetCursor().GetPosition().y;
        const auto copyableRows = std::min<til::CoordType>(_height, newSize.height);
        til::CoordType srcRow = 0;
        til::CoordType dstRow = 0;

        if (cursorRow >= newSize.height)
        {
            srcRow = cursorRow - newSize.height + 1;
        }

        for (; dstRow < copyableRows; ++dstRow, ++srcRow)
        {
            newBuffer.GetRowByOffset(dstRow).CopyFrom(GetRowByOffset(srcRow));
        }

        // NOTE: Keep this in sync with _reserve().
        _buffer = std::move(newBuffer._buffer);
        _bufferEnd = newBuffer._bufferEnd;
        _commitWatermark = newBuffer._commitWatermark;
        _initialAttributes = newBuffer._initialAttributes;
        _bufferRowStride = newBuffer._bufferRowStride;
        _bufferOffsetChars = newBuffer._bufferOffsetChars;
        _bufferOffsetCharOffsets = newBuffer._bufferOffsetCharOffsets;
        _width = newBuffer._width;
        _height = newBuffer._height;

        _SetFirstRowIndex(0);
    }
    CATCH_RETURN();

    return S_OK;
}

void TextBuffer::SetAsActiveBuffer(const bool isActiveBuffer) noexcept
{
    _isActiveBuffer = isActiveBuffer;
}

bool TextBuffer::IsActiveBuffer() const noexcept
{
    return _isActiveBuffer;
}

Microsoft::Console::Render::Renderer& TextBuffer::GetRenderer() noexcept
{
    return _renderer;
}

void TextBuffer::TriggerRedraw(const Viewport& viewport)
{
    if (_isActiveBuffer)
    {
        _renderer.TriggerRedraw(viewport);
    }
}

void TextBuffer::TriggerRedrawCursor(const til::point position)
{
    if (_isActiveBuffer)
    {
        _renderer.TriggerRedrawCursor(&position);
    }
}

void TextBuffer::TriggerRedrawAll()
{
    if (_isActiveBuffer)
    {
        _renderer.TriggerRedrawAll();
    }
}

void TextBuffer::TriggerScroll()
{
    if (_isActiveBuffer)
    {
        _renderer.TriggerScroll();
    }
}

void TextBuffer::TriggerScroll(const til::point delta)
{
    if (_isActiveBuffer)
    {
        _renderer.TriggerScroll(&delta);
    }
}

void TextBuffer::TriggerNewTextNotification(const std::wstring_view newText)
{
    if (_isActiveBuffer)
    {
        _renderer.TriggerNewTextNotification(newText);
    }
}

// Method Description:
// - get delimiter class for buffer cell position
// - used for double click selection and uia word navigation
// Arguments:
// - pos: the buffer cell under observation
// - wordDelimiters: the delimiters defined as a part of the DelimiterClass::DelimiterChar
// Return Value:
// - the delimiter class for the given char
DelimiterClass TextBuffer::_GetDelimiterClassAt(const til::point pos, const std::wstring_view wordDelimiters) const
{
    return GetRowByOffset(pos.y).DelimiterClassAt(pos.x, wordDelimiters);
}

// Method Description:
// - Get the til::point for the beginning of the word you are on
// Arguments:
// - target - a til::point on the word you are currently on
// - wordDelimiters - what characters are we considering for the separation of words
// - accessibilityMode - when enabled, we continue expanding left until we are at the beginning of a readable word.
//                        Otherwise, expand left until a character of a new delimiter class is found
//                        (or a row boundary is encountered)
// - limitOptional - (optional) the last possible position in the buffer that can be explored. This can be used to improve performance.
// Return Value:
// - The til::point for the first character on the "word" (inclusive)
til::point TextBuffer::GetWordStart(const til::point target, const std::wstring_view wordDelimiters, bool accessibilityMode, std::optional<til::point> limitOptional) const
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
    auto copy{ target };
    const auto bufferSize{ GetSize() };
    const auto limit{ limitOptional.value_or(bufferSize.EndExclusive()) };
    if (target == bufferSize.Origin())
    {
        // can't expand left
        return target;
    }
    else if (target == bufferSize.EndExclusive())
    {
        // GH#7664: Treat EndExclusive as EndInclusive so
        // that it actually points to a space in the buffer
        copy = bufferSize.BottomRightInclusive();
    }
    else if (bufferSize.CompareInBounds(target, limit, true) >= 0)
    {
        // if at/past the limit --> clamp to limit
        copy = limitOptional.value_or(bufferSize.BottomRightInclusive());
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
// - Helper method for GetWordStart(). Get the til::point for the beginning of the word (accessibility definition) you are on
// Arguments:
// - target - a til::point on the word you are currently on
// - wordDelimiters - what characters are we considering for the separation of words
// Return Value:
// - The til::point for the first character on the current/previous READABLE "word" (inclusive)
til::point TextBuffer::_GetWordStartForAccessibility(const til::point target, const std::wstring_view wordDelimiters) const
{
    auto result = target;
    const auto bufferSize = GetSize();
    auto stayAtOrigin = false;

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
// - Helper method for GetWordStart(). Get the til::point for the beginning of the word (selection definition) you are on
// Arguments:
// - target - a til::point on the word you are currently on
// - wordDelimiters - what characters are we considering for the separation of words
// Return Value:
// - The til::point for the first character on the current word or delimiter run (stopped by the left margin)
til::point TextBuffer::_GetWordStartForSelection(const til::point target, const std::wstring_view wordDelimiters) const
{
    auto result = target;
    const auto bufferSize = GetSize();

    const auto initialDelimiter = _GetDelimiterClassAt(result, wordDelimiters);

    // expand left until we hit the left boundary or a different delimiter class
    while (result.x > bufferSize.Left() && (_GetDelimiterClassAt(result, wordDelimiters) == initialDelimiter))
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
// - Get the til::point for the beginning of the NEXT word
// Arguments:
// - target - a til::point on the word you are currently on
// - wordDelimiters - what characters are we considering for the separation of words
// - accessibilityMode - when enabled, we continue expanding right until we are at the beginning of the next READABLE word
//                        Otherwise, expand right until a character of a new delimiter class is found
//                        (or a row boundary is encountered)
// - limitOptional - (optional) the last possible position in the buffer that can be explored. This can be used to improve performance.
// Return Value:
// - The til::point for the last character on the "word" (inclusive)
til::point TextBuffer::GetWordEnd(const til::point target, const std::wstring_view wordDelimiters, bool accessibilityMode, std::optional<til::point> limitOptional) const
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

    // Already at/past the limit. Can't move forward.
    const auto bufferSize{ GetSize() };
    const auto limit{ limitOptional.value_or(bufferSize.EndExclusive()) };
    if (bufferSize.CompareInBounds(target, limit, true) >= 0)
    {
        return target;
    }

    if (accessibilityMode)
    {
        return _GetWordEndForAccessibility(target, wordDelimiters, limit);
    }
    else
    {
        return _GetWordEndForSelection(target, wordDelimiters);
    }
}

// Method Description:
// - Helper method for GetWordEnd(). Get the til::point for the beginning of the next READABLE word
// Arguments:
// - target - a til::point on the word you are currently on
// - wordDelimiters - what characters are we considering for the separation of words
// - limit - the last "valid" position in the text buffer (to improve performance)
// Return Value:
// - The til::point for the first character of the next readable "word". If no next word, return one past the end of the buffer
til::point TextBuffer::_GetWordEndForAccessibility(const til::point target, const std::wstring_view wordDelimiters, const til::point limit) const
{
    const auto bufferSize{ GetSize() };
    auto result{ target };

    if (bufferSize.CompareInBounds(target, limit, true) >= 0)
    {
        // if we're already on/past the last RegularChar,
        // clamp result to that position
        result = limit;

        // make the result exclusive
        bufferSize.IncrementInBounds(result, true);
    }
    else
    {
        auto iter{ GetCellDataAt(result, bufferSize) };
        while (iter && iter.Pos() != limit && _GetDelimiterClassAt(iter.Pos(), wordDelimiters) == DelimiterClass::RegularChar)
        {
            // Iterate through readable text
            ++iter;
        }

        while (iter && iter.Pos() != limit && _GetDelimiterClassAt(iter.Pos(), wordDelimiters) != DelimiterClass::RegularChar)
        {
            // expand to the beginning of the NEXT word
            ++iter;
        }

        result = iter.Pos();

        // Special case: we tried to move one past the end of the buffer,
        // but iter prevented that (because that pos doesn't exist).
        // Manually increment onto the EndExclusive point.
        if (!iter)
        {
            bufferSize.IncrementInBounds(result, true);
        }
    }

    return result;
}

// Method Description:
// - Helper method for GetWordEnd(). Get the til::point for the beginning of the NEXT word
// Arguments:
// - target - a til::point on the word you are currently on
// - wordDelimiters - what characters are we considering for the separation of words
// Return Value:
// - The til::point for the last character of the current word or delimiter run (stopped by right margin)
til::point TextBuffer::_GetWordEndForSelection(const til::point target, const std::wstring_view wordDelimiters) const
{
    const auto bufferSize = GetSize();

    // can't expand right
    if (target.x == bufferSize.RightInclusive())
    {
        return target;
    }

    auto result = target;
    const auto initialDelimiter = _GetDelimiterClassAt(result, wordDelimiters);

    // expand right until we hit the right boundary or a different delimiter class
    while (result.x < bufferSize.RightInclusive() && (_GetDelimiterClassAt(result, wordDelimiters) == initialDelimiter))
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
    const auto hyperlinks = GetRowByOffset(0).GetHyperlinks();

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
        for (til::CoordType i = 1; i < total; ++i)
        {
            const auto nextRowRefs = GetRowByOffset(i).GetHyperlinks();
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
// - pos - a til::point on the word you are currently on
// - wordDelimiters - what characters are we considering for the separation of words
// - limitOptional - (optional) the last possible position in the buffer that can be explored. This can be used to improve performance.
// Return Value:
// - true, if successfully updated pos. False, if we are unable to move (usually due to a buffer boundary)
// - pos - The til::point for the first character on the "word" (inclusive)
bool TextBuffer::MoveToNextWord(til::point& pos, const std::wstring_view wordDelimiters, std::optional<til::point> limitOptional) const
{
    // move to the beginning of the next word
    // NOTE: _GetWordEnd...() returns the exclusive position of the "end of the word"
    //       This is also the inclusive start of the next word.
    const auto bufferSize{ GetSize() };
    const auto limit{ limitOptional.value_or(bufferSize.EndExclusive()) };
    const auto copy{ _GetWordEndForAccessibility(pos, wordDelimiters, limit) };

    if (bufferSize.CompareInBounds(copy, limit, true) >= 0)
    {
        return false;
    }

    pos = copy;
    return true;
}

// Method Description:
// - Update pos to be the position of the first character of the previous word. This is used for accessibility
// Arguments:
// - pos - a til::point on the word you are currently on
// - wordDelimiters - what characters are we considering for the separation of words
// Return Value:
// - true, if successfully updated pos. False, if we are unable to move (usually due to a buffer boundary)
// - pos - The til::point for the first character on the "word" (inclusive)
bool TextBuffer::MoveToPreviousWord(til::point& pos, std::wstring_view wordDelimiters) const
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
// - pos - a til::point on the word you are currently on
// - limitOptional - (optional) the last possible position in the buffer that can be explored. This can be used to improve performance.
// Return Value:
// - pos - The til::point for the first cell of the current glyph (inclusive)
til::point TextBuffer::GetGlyphStart(const til::point pos, std::optional<til::point> limitOptional) const
{
    auto resultPos = pos;
    const auto bufferSize = GetSize();
    const auto limit{ limitOptional.value_or(bufferSize.EndExclusive()) };

    // Clamp pos to limit
    if (bufferSize.CompareInBounds(resultPos, limit, true) > 0)
    {
        resultPos = limit;
    }

    // limit is exclusive, so we need to move back to be within valid bounds
    if (resultPos != limit && GetCellDataAt(resultPos)->DbcsAttr() == DbcsAttribute::Trailing)
    {
        bufferSize.DecrementInBounds(resultPos, true);
    }

    return resultPos;
}

// Method Description:
// - Update pos to be the end of the current glyph/character.
// Arguments:
// - pos - a til::point on the word you are currently on
// - accessibilityMode - this is being used for accessibility; make the end exclusive.
// Return Value:
// - pos - The til::point for the last cell of the current glyph (exclusive)
til::point TextBuffer::GetGlyphEnd(const til::point pos, bool accessibilityMode, std::optional<til::point> limitOptional) const
{
    auto resultPos = pos;
    const auto bufferSize = GetSize();
    const auto limit{ limitOptional.value_or(bufferSize.EndExclusive()) };

    // Clamp pos to limit
    if (bufferSize.CompareInBounds(resultPos, limit, true) > 0)
    {
        resultPos = limit;
    }

    if (resultPos != limit && GetCellDataAt(resultPos)->DbcsAttr() == DbcsAttribute::Leading)
    {
        bufferSize.IncrementInBounds(resultPos, true);
    }

    // increment one more time to become exclusive
    if (accessibilityMode)
    {
        bufferSize.IncrementInBounds(resultPos, true);
    }
    return resultPos;
}

// Method Description:
// - Update pos to be the beginning of the next glyph/character. This is used for accessibility
// Arguments:
// - pos - a til::point on the word you are currently on
// - allowExclusiveEnd - allow result to be the exclusive limit (one past limit)
// - limit - boundaries for the iterator to operate within
// Return Value:
// - true, if successfully updated pos. False, if we are unable to move (usually due to a buffer boundary)
// - pos - The til::point for the first cell of the current glyph (inclusive)
bool TextBuffer::MoveToNextGlyph(til::point& pos, bool allowExclusiveEnd, std::optional<til::point> limitOptional) const
{
    const auto bufferSize = GetSize();
    const auto limit{ limitOptional.value_or(bufferSize.EndExclusive()) };

    const auto distanceToLimit{ bufferSize.CompareInBounds(pos, limit, true) };
    if (distanceToLimit >= 0)
    {
        // Corner Case: we're on/past the limit
        // Clamp us to the limit
        pos = limit;
        return false;
    }
    else if (!allowExclusiveEnd && distanceToLimit == -1)
    {
        // Corner Case: we're just before the limit
        // and we are not allowed onto the exclusive end.
        // Fail to move.
        return false;
    }

    // Try to move forward, but if we hit the buffer boundary, we fail to move.
    auto iter{ GetCellDataAt(pos, bufferSize) };
    const bool success{ ++iter };

    // Move again if we're on a wide glyph
    if (success && iter->DbcsAttr() == DbcsAttribute::Trailing)
    {
        ++iter;
    }

    pos = iter.Pos();
    return success;
}

// Method Description:
// - Update pos to be the beginning of the previous glyph/character. This is used for accessibility
// Arguments:
// - pos - a til::point on the word you are currently on
// Return Value:
// - true, if successfully updated pos. False, if we are unable to move (usually due to a buffer boundary)
// - pos - The til::point for the first cell of the previous glyph (inclusive)
bool TextBuffer::MoveToPreviousGlyph(til::point& pos, std::optional<til::point> limitOptional) const
{
    auto resultPos = pos;
    const auto bufferSize = GetSize();
    const auto limit{ limitOptional.value_or(bufferSize.EndExclusive()) };

    if (bufferSize.CompareInBounds(pos, limit, true) > 0)
    {
        // we're past the end
        // clamp us to the limit
        pos = limit;
        return true;
    }

    // try to move. If we can't, we're done.
    const auto success = bufferSize.DecrementInBounds(resultPos, true);
    if (resultPos != bufferSize.EndExclusive() && GetCellDataAt(resultPos)->DbcsAttr() == DbcsAttribute::Leading)
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
// - One or more rects corresponding to the selection area
const std::vector<til::inclusive_rect> TextBuffer::GetTextRects(til::point start, til::point end, bool blockSelection, bool bufferCoordinates) const
{
    std::vector<til::inclusive_rect> textRects;

    const auto bufferSize = GetSize();

    // (0,0) is the top-left of the screen
    // the physically "higher" coordinate is closer to the top-left
    // the physically "lower" coordinate is closer to the bottom-right
    const auto [higherCoord, lowerCoord] = bufferSize.CompareInBounds(start, end) <= 0 ?
                                               std::make_tuple(start, end) :
                                               std::make_tuple(end, start);

    const auto textRectSize = 1 + lowerCoord.y - higherCoord.y;
    textRects.reserve(textRectSize);
    for (auto row = higherCoord.y; row <= lowerCoord.y; row++)
    {
        til::inclusive_rect textRow;

        textRow.top = row;
        textRow.bottom = row;

        if (blockSelection || higherCoord.y == lowerCoord.y)
        {
            // set the left and right margin to the left-/right-most respectively
            textRow.left = std::min(higherCoord.x, lowerCoord.x);
            textRow.right = std::max(higherCoord.x, lowerCoord.x);
        }
        else
        {
            textRow.left = (row == higherCoord.y) ? higherCoord.x : bufferSize.Left();
            textRow.right = (row == lowerCoord.y) ? lowerCoord.x : bufferSize.RightInclusive();
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
// - Computes the span(s) for the given selection
// - If not a blockSelection, returns a single span (start - end)
// - Else if a blockSelection, returns spans corresponding to each line in the block selection
// Arguments:
// - start: beginning of the text region of interest (inclusive)
// - end: the other end of the text region of interest (inclusive)
// - blockSelection: when enabled, get spans for each line covered by the block
// - bufferCoordinates: when enabled, treat the coordinates as relative to
//                      the buffer rather than the screen.
// Return Value:
// - one or more sets of start-end coordinates, representing spans of text in the buffer
std::vector<til::point_span> TextBuffer::GetTextSpans(til::point start, til::point end, bool blockSelection, bool bufferCoordinates) const
{
    std::vector<til::point_span> textSpans;
    if (blockSelection)
    {
        // If blockSelection, this is effectively the same operation as GetTextRects, but
        // expressed in til::point coordinates.
        const auto rects = GetTextRects(start, end, /*blockSelection*/ true, bufferCoordinates);
        textSpans.reserve(rects.size());

        for (auto rect : rects)
        {
            const til::point first = { rect.left, rect.top };
            const til::point second = { rect.right, rect.bottom };
            textSpans.emplace_back(first, second);
        }
    }
    else
    {
        const auto bufferSize = GetSize();

        // (0,0) is the top-left of the screen
        // the physically "higher" coordinate is closer to the top-left
        // the physically "lower" coordinate is closer to the bottom-right
        auto [higherCoord, lowerCoord] = start <= end ?
                                             std::make_tuple(start, end) :
                                             std::make_tuple(end, start);

        textSpans.reserve(1);

        // If we were passed screen coordinates, convert the given range into
        // equivalent buffer offsets, taking line rendition into account.
        if (!bufferCoordinates)
        {
            higherCoord = ScreenToBufferLine(higherCoord, GetLineRendition(higherCoord.y));
            lowerCoord = ScreenToBufferLine(lowerCoord, GetLineRendition(lowerCoord.y));
        }

        til::inclusive_rect asRect = { higherCoord.x, higherCoord.y, lowerCoord.x, lowerCoord.y };
        _ExpandTextRow(asRect);
        higherCoord.x = asRect.left;
        higherCoord.y = asRect.top;
        lowerCoord.x = asRect.right;
        lowerCoord.y = asRect.bottom;

        textSpans.emplace_back(higherCoord, lowerCoord);
    }

    return textSpans;
}

// Method Description:
// - Expand the selection row according to include wide glyphs fully
// - this is particularly useful for box selections (ALT + selection)
// Arguments:
// - selectionRow: the selection row to be expanded
// Return Value:
// - modifies selectionRow's Left and Right values to expand properly
void TextBuffer::_ExpandTextRow(til::inclusive_rect& textRow) const
{
    const auto bufferSize = GetSize();

    // expand left side of rect
    til::point targetPoint{ textRow.left, textRow.top };
    if (GetCellDataAt(targetPoint)->DbcsAttr() == DbcsAttribute::Trailing)
    {
        if (targetPoint.x == bufferSize.Left())
        {
            bufferSize.IncrementInBounds(targetPoint);
        }
        else
        {
            bufferSize.DecrementInBounds(targetPoint);
        }
        textRow.left = targetPoint.x;
    }

    // expand right side of rect
    targetPoint = { textRow.right, textRow.bottom };
    if (GetCellDataAt(targetPoint)->DbcsAttr() == DbcsAttribute::Leading)
    {
        if (targetPoint.x == bufferSize.RightInclusive())
        {
            bufferSize.DecrementInBounds(targetPoint);
        }
        else
        {
            bufferSize.IncrementInBounds(targetPoint);
        }
        textRow.right = targetPoint.x;
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
                                                   const std::vector<til::inclusive_rect>& selectionRects,
                                                   std::function<std::pair<COLORREF, COLORREF>(const TextAttribute&)> GetAttributeColors,
                                                   const bool formatWrappedRows) const
{
    TextAndColor data;
    const auto copyTextColor = GetAttributeColors != nullptr;

    // preallocate our vectors to reduce reallocs
    const auto rows = selectionRects.size();
    data.text.reserve(rows);
    if (copyTextColor)
    {
        data.FgAttr.reserve(rows);
        data.BkAttr.reserve(rows);
    }

    // for each row in the selection
    for (size_t i = 0; i < rows; i++)
    {
        const auto iRow = selectionRects.at(i).top;

        const auto highlight = Viewport::FromInclusive(selectionRects.at(i));

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

            if (cell.DbcsAttr() != DbcsAttribute::Trailing)
            {
                const auto chars = cell.Chars();
                selectionText.append(chars);

                if (copyTextColor)
                {
                    const auto cellData = cell.TextAttr();
                    const auto [CellFgAttr, CellBkAttr] = GetAttributeColors(cellData);
                    for (size_t j = 0; j < chars.size(); ++j)
                    {
                        selectionFgAttr.push_back(CellFgAttr);
                        selectionBkAttr.push_back(CellBkAttr);
                    }
                }
            }

            ++it;
        }

        // We apply formatting to rows if the row was NOT wrapped or formatting of wrapped rows is allowed
        const auto shouldFormatRow = formatWrappedRows || !GetRowByOffset(iRow).WasWrapForced();

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
                    // can't see CR/LF so just use black FG & BK
                    const auto Blackness = RGB(0x00, 0x00, 0x00);
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

size_t TextBuffer::SpanLength(const til::point coordStart, const til::point coordEnd) const
{
    const auto bufferSize = GetSize();
    // The coords are inclusive, so to get the (inclusive) length we add 1.
    const auto length = bufferSize.CompareInBounds(coordEnd, coordStart) + 1;
    return gsl::narrow<size_t>(length);
}

// Routine Description:
// - Retrieves the plain text data between the specified coordinates.
// Arguments:
// - trimTrailingWhitespace - remove the trailing whitespace at the end of the result.
// - start - where to start getting text (should be at or prior to "end")
// - end - where to end getting text
// Return Value:
// - Just the text.
std::wstring TextBuffer::GetPlainText(const til::point& start, const til::point& end) const
{
    std::wstring text;
    auto spanLength = SpanLength(start, end);
    text.reserve(spanLength);

    auto it = GetCellDataAt(start);

    for (; it && spanLength > 0; ++it, --spanLength)
    {
        const auto& cell = *it;
        if (cell.DbcsAttr() != DbcsAttribute::Trailing)
        {
            const auto chars = cell.Chars();
            text.append(chars);
        }
    }

    return text;
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
        auto hasWrittenAnyText = false;
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

                auto colorChanged = false;
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
        const auto htmlStartPos = ClipboardHeaderSize;
        const auto htmlEndPos = ClipboardHeaderSize + gsl::narrow<size_t>(htmlBuilder.tellp());
        const auto fragStartPos = ClipboardHeaderSize + gsl::narrow<size_t>(htmlHeader.length());
        const auto fragEndPos = htmlEndPos - HtmlFooter.length();

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
        auto nextColorIndex = 1; // leave 0 for the default color and start from 1.

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
                        const auto text = std::wstring_view{ rows.text.at(row) }.substr(startOffset, col - startOffset + includeCurrent);
                        _AppendRTFText(contentBuilder, text);

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

                auto colorChanged = false;
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

                    auto bkColorIndex = 0;
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

                    auto fgColorIndex = 0;
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

void TextBuffer::_AppendRTFText(std::ostringstream& contentBuilder, const std::wstring_view& text)
{
    for (const auto codeUnit : text)
    {
        if (codeUnit <= 127)
        {
            switch (codeUnit)
            {
            case L'\\':
            case L'{':
            case L'}':
                contentBuilder << "\\" << gsl::narrow<char>(codeUnit);
                break;
            default:
                contentBuilder << gsl::narrow<char>(codeUnit);
            }
        }
        else
        {
            // Windows uses unsigned wchar_t - RTF uses signed ones.
            contentBuilder << "\\u" << std::to_string(til::bit_cast<int16_t>(codeUnit)) << "?";
        }
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
try
{
    const auto& oldCursor = oldBuffer.GetCursor();
    auto& newCursor = newBuffer.GetCursor();

    // We need to save the old cursor position so that we can
    // place the new cursor back on the equivalent character in
    // the new buffer.
    const auto cOldCursorPos = oldCursor.GetPosition();
    const auto cOldLastChar = oldBuffer.GetLastNonSpaceCharacter(lastCharacterViewport);

    const auto cOldRowsTotal = cOldLastChar.y + 1;

    til::point cNewCursorPos;
    auto fFoundCursorPos = false;
    auto foundOldMutable = false;
    auto foundOldVisible = false;
    // Loop through all the rows of the old buffer and reprint them into the new buffer
    til::CoordType iOldRow = 0;
    for (; iOldRow < cOldRowsTotal; iOldRow++)
    {
        // Fetch the row and its "right" which is the last printable character.
        const auto& row = oldBuffer.GetRowByOffset(iOldRow);
        const auto cOldColsTotal = oldBuffer.GetLineWidth(iOldRow);
        auto iRight = row.MeasureRight();

        // If we're starting a new row, try and preserve the line rendition
        // from the row in the original buffer.
        const auto newBufferPos = newCursor.GetPosition();
        if (newBufferPos.x == 0)
        {
            auto& newRow = newBuffer.GetRowByOffset(newBufferPos.y);
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
        til::CoordType iOldCol = 0;
        const auto copyRight = iRight;
        for (; iOldCol < copyRight; iOldCol++)
        {
            if (iOldCol == cOldCursorPos.x && iOldRow == cOldCursorPos.y)
            {
                cNewCursorPos = newCursor.GetPosition();
                fFoundCursorPos = true;
            }

            // TODO: MSFT: 19446208 - this should just use an iterator and the inserter...
            const auto glyph = row.GlyphAt(iOldCol);
            const auto dbcsAttr = row.DbcsAttrAt(iOldCol);
            const auto textAttr = row.GetAttrByColumn(iOldCol);

            newBuffer.InsertCharacter(glyph, dbcsAttr, textAttr);
        }

        // GH#32: Copy the attributes from the rest of the row into this new buffer.
        // From where we are in the old buffer, to the end of the row, copy the
        // remaining attributes.
        // - if the old buffer is smaller than the new buffer, then just copy
        //   what we have, as it was. We already copied all _text_ with colors,
        //   but it's possible for someone to just put some color into the
        //   buffer to the right of that without any text (as just spaces). The
        //   buffer looks weird to the user when we resize and it starts losing
        //   those colors, so we need to copy them over too... as long as there
        //   is space. The last attr in the row will be extended to the end of
        //   the row in the new buffer.
        // - if the old buffer is WIDER, than we might have wrapped onto a new
        //   line. Use the cursor's position's Y so that we know where the new
        //   row is, and start writing at the cursor position. Again, the attr
        //   in the last column of the old row will be extended to the end of the
        //   row that the text was flowed onto.
        //   - if the text in the old buffer didn't actually fill the whole
        //     line in the new buffer, then we didn't wrap. That's fine. just
        //     copy attributes from the old row till the end of the new row, and
        //     move on.
        const auto newRowY = newCursor.GetPosition().y;
        auto& newRow = newBuffer.GetRowByOffset(newRowY);
        auto newAttrColumn = newCursor.GetPosition().x;
        const auto newWidth = newBuffer.GetLineWidth(newRowY);
        // Stop when we get to the end of the buffer width, or the new position
        // for inserting an attr would be past the right of the new buffer.
        for (auto copyAttrCol = iOldCol;
             copyAttrCol < cOldColsTotal && newAttrColumn < newWidth;
             copyAttrCol++, newAttrColumn++)
        {
            // TODO: MSFT: 19446208 - this should just use an iterator and the inserter...
            const auto textAttr = row.GetAttrByColumn(copyAttrCol);
            newRow.SetAttrToEnd(newAttrColumn, textAttr);
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
                    positionInfo.value().get().mutableViewportTop = newCursor.GetPosition().y;
                    foundOldMutable = true;
                }
            }

            if (!foundOldVisible)
            {
                if (iOldRow >= positionInfo.value().get().visibleViewportTop)
                {
                    positionInfo.value().get().visibleViewportTop = newCursor.GetPosition().y;
                    foundOldVisible = true;
                }
            }
        }

        // If we didn't have a full row to copy, insert a new
        // line into the new buffer.
        // Only do so if we were not forced to wrap. If we did
        // force a word wrap, then the existing line break was
        // only because we ran out of space.
        if (iRight < cOldColsTotal && !row.WasWrapForced())
        {
            if (!fFoundCursorPos && (iRight == cOldCursorPos.x && iOldRow == cOldCursorPos.y))
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
                newBuffer.NewlineCursor();
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
                const auto coordNewCursor = newCursor.GetPosition();
                if (coordNewCursor.x == 0 && coordNewCursor.y > 0)
                {
                    if (newBuffer.GetRowByOffset(coordNewCursor.y - 1).WasWrapForced())
                    {
                        newBuffer.NewlineCursor();
                    }
                }
            }
        }
    }

    // Finish copying buffer attributes to remaining rows below the last
    // printable character. This is to fix the `color 2f` scenario, where you
    // change the buffer colors then resize and everything below the last
    // printable char gets reset. See GH #12567
    auto newRowY = newCursor.GetPosition().y + 1;
    const auto newHeight = newBuffer.GetSize().Height();
    const auto oldHeight = oldBuffer._estimateOffsetOfLastCommittedRow() + 1;
    for (;
         iOldRow < oldHeight && newRowY < newHeight;
         iOldRow++)
    {
        const auto& row = oldBuffer.GetRowByOffset(iOldRow);

        // Optimization: Since all these rows are below the last printable char,
        // we can reasonably assume that they are filled with just spaces.
        // That's convenient, we can just copy the attr row from the old buffer
        // into the new one, and resize the row to match. We'll rely on the
        // behavior of ATTR_ROW::Resize to trim down when narrower, or extend
        // the last attr when wider.
        auto& newRow = newBuffer.GetRowByOffset(newRowY);
        const auto newWidth = newBuffer.GetLineWidth(newRowY);
        newRow.TransferAttributes(row.Attributes(), newWidth);

        newRowY++;
    }

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
        auto iNewlines = cOldCursorPos.y - cOldLastChar.y;
        const auto iIncrements = cOldCursorPos.x - cOldLastChar.x;
        const auto cNewLastChar = newBuffer.GetLastNonSpaceCharacter();

        // If the last row of the new buffer wrapped, there's going to be one less newline needed,
        //   because the cursor is already on the next line
        if (newBuffer.GetRowByOffset(cNewLastChar.y).WasWrapForced())
        {
            iNewlines = std::max(iNewlines - 1, 0);
        }
        else
        {
            // if this buffer didn't wrap, but the old one DID, then the d(columns) of the
            //   old buffer will be one more than in this buffer, so new need one LESS.
            if (oldBuffer.GetRowByOffset(cOldLastChar.y).WasWrapForced())
            {
                iNewlines = std::max(iNewlines - 1, 0);
            }
        }

        for (auto r = 0; r < iNewlines; r++)
        {
            newBuffer.NewlineCursor();
        }
        for (auto c = 0; c < iIncrements - 1; c++)
        {
            newBuffer.IncrementCursor();
        }
    }

    // Save old cursor size before we delete it
    const auto ulSize = oldCursor.GetSize();

    // Set size back to real size as it will be taking over the rendering duties.
    newCursor.SetSize(ulSize);

    newBuffer._marks = oldBuffer._marks;
    newBuffer._trimMarksOutsideBuffer();

    return S_OK;
}
CATCH_RETURN()

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
        newId += L"%" + std::to_wstring(til::hash(uri));
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
PointTree TextBuffer::GetPatterns(const til::CoordType firstRow, const til::CoordType lastRow) const
{
    PointTree::interval_vector intervals;

    std::wstring concatAll;
    const auto rowSize = GetRowByOffset(0).size();
    concatAll.reserve(gsl::narrow_cast<size_t>(rowSize) * gsl::narrow_cast<size_t>(lastRow - firstRow + 1));

    // to deal with text that spans multiple lines, we will first concatenate
    // all the text into one string and find the patterns in that string
    for (til::CoordType i = firstRow; i <= lastRow; ++i)
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

        til::CoordType lenUpToThis = 0;
        for (auto i = words_begin; i != words_end; ++i)
        {
            // record the locations -
            // when we find a match, the prefix is text that is between this
            // match and the previous match, so we use the size of the prefix
            // along with the size of the match to determine the locations
            til::CoordType prefixSize = 0;
            for (const auto str = i->prefix().str(); const auto& glyph : til::utf16_iterator{ str })
            {
                prefixSize += IsGlyphFullWidth(glyph) ? 2 : 1;
            }
            const auto start = lenUpToThis + prefixSize;
            til::CoordType matchSize = 0;
            for (const auto str = i->str(); const auto& glyph : til::utf16_iterator{ str })
            {
                matchSize += IsGlyphFullWidth(glyph) ? 2 : 1;
            }
            const auto end = start + matchSize;
            lenUpToThis = end;

            const til::point startCoord{ start % rowSize, start / rowSize };
            const til::point endCoord{ end % rowSize, end / rowSize };

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

const std::vector<ScrollMark>& TextBuffer::GetMarks() const noexcept
{
    return _marks;
}

// Remove all marks between `start` & `end`, inclusive.
void TextBuffer::ClearMarksInRange(
    const til::point start,
    const til::point end)
{
    auto inRange = [&start, &end](const ScrollMark& m) {
        return (m.start >= start && m.start <= end) ||
               (m.end >= start && m.end <= end);
    };

    _marks.erase(std::remove_if(_marks.begin(),
                                _marks.end(),
                                inRange),
                 _marks.end());
}
void TextBuffer::ClearAllMarks() noexcept
{
    _marks.clear();
}

// Adjust all the marks in the y-direction by `delta`. Positive values move the
// marks down (the positive y direction). Negative values move up. This will
// trim marks that are no longer have a start in the bounds of the buffer
void TextBuffer::ScrollMarks(const int delta)
{
    for (auto& mark : _marks)
    {
        mark.start.y += delta;

        // If the mark had sub-regions, then move those pointers too
        if (mark.commandEnd.has_value())
        {
            (*mark.commandEnd).y += delta;
        }
        if (mark.outputEnd.has_value())
        {
            (*mark.outputEnd).y += delta;
        }
    }
    _trimMarksOutsideBuffer();
}

// Method Description:
// - Add a mark to our list of marks, and treat it as the active "prompt". For
//   the sake of shell integration, we need to know which mark represents the
//   current prompt/command/output. Internally, we'll always treat the _last_
//   mark in the list as the current prompt.
// Arguments:
// - m: the mark to add.
void TextBuffer::StartPromptMark(const ScrollMark& m)
{
    _marks.push_back(m);
}
// Method Description:
// - Add a mark to our list of marks. Don't treat this as the active prompt.
//   This should be used for marks created by the UI or from other user input.
//   By inserting at the start of the list, we can separate out marks that were
//   generated by client programs vs ones created by the user.
// Arguments:
// - m: the mark to add.
void TextBuffer::AddMark(const ScrollMark& m)
{
    _marks.insert(_marks.begin(), m);
}

void TextBuffer::_trimMarksOutsideBuffer()
{
    const auto height = GetSize().Height();
    _marks.erase(std::remove_if(_marks.begin(),
                                _marks.end(),
                                [height](const auto& m) {
                                    return (m.start.y < 0) ||
                                           (m.start.y >= height);
                                }),
                 _marks.end());
}

void TextBuffer::SetCurrentPromptEnd(const til::point pos) noexcept
{
    if (_marks.empty())
    {
        return;
    }
    auto& curr{ _marks.back() };
    curr.end = pos;
}
void TextBuffer::SetCurrentCommandEnd(const til::point pos) noexcept
{
    if (_marks.empty())
    {
        return;
    }
    auto& curr{ _marks.back() };
    curr.commandEnd = pos;
}
void TextBuffer::SetCurrentOutputEnd(const til::point pos, ::MarkCategory category) noexcept
{
    if (_marks.empty())
    {
        return;
    }
    auto& curr{ _marks.back() };
    curr.outputEnd = pos;
    curr.category = category;
}
