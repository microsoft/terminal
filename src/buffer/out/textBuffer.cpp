// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "textBuffer.hpp"

#include <til/hash.h>

#include "UTextAdapter.h"
#include "../../types/inc/CodepointWidthDetector.hpp"
#include "../renderer/base/renderer.hpp"
#include "../types/inc/utils.hpp"
#include "search.h"

// BODGY: Misdiagnosis in MSVC 17.11: Referencing global constants in the member
// initializer list leads to this warning. Can probably be removed in the future.
#pragma warning(disable : 26493) // Don't use C-style casts (type.4).)

using namespace Microsoft::Console;
using namespace Microsoft::Console::Types;

using PointTree = interval_tree::IntervalTree<til::point, size_t>;

constexpr bool allWhitespace(const std::wstring_view& text) noexcept
{
    for (const auto ch : text)
    {
        if (ch != L' ')
        {
            return false;
        }
    }
    return true;
}

static std::atomic<uint64_t> s_lastMutationIdInitialValue;

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
                       Microsoft::Console::Render::Renderer* renderer) :
    _renderer{ renderer },
    _currentAttributes{ defaultAttributes },
    // This way every TextBuffer will start with a ""unique"" _lastMutationId
    // and so it'll compare unequal with the counter of other TextBuffers.
    _lastMutationId{ s_lastMutationIdInitialValue.fetch_add(0x100000000) },
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
    assert(row >= _commitWatermark);

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

// Constructs ROWs between [_commitWatermark,until).
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

// Destructs ROWs between [_buffer,_commitWatermark).
void TextBuffer::_destroy() const noexcept
{
    for (auto it = _buffer.get(); it < _commitWatermark; it += _bufferRowStride)
    {
        std::destroy_at(reinterpret_cast<ROW*>(it));
    }
}

// This function is "direct" because it trusts the caller to properly
// wrap the "offset" parameter modulo the _height of the buffer.
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

// See GetRowByOffset().
ROW& TextBuffer::_getRow(til::CoordType y) const
{
    // Rows are stored circularly, so the index you ask for is offset by the start position and mod the total of rows.
    auto offset = (_firstRow + y) % _height;

    // Support negative wrap around. This way an index of -1 will
    // wrap to _rowCount-1 and make implementing scrolling easier.
    if (offset < 0)
    {
        offset += _height;
    }

    // We add 1 to the row offset, because row "0" is the one returned by GetScratchpadRow().
    // See GetScratchpadRow() for more explanation.
#pragma warning(suppress : 26492) // Don't use const_cast to cast away const or volatile (type.3).
    return const_cast<TextBuffer*>(this)->_getRowByOffsetDirect(gsl::narrow_cast<size_t>(offset) + 1);
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
    return _getRow(index);
}

// Retrieves a row from the buffer by its offset from the first row of the text buffer
// (what corresponds to the top row of the screen buffer).
ROW& TextBuffer::GetMutableRowByOffset(const til::CoordType index)
{
    _lastMutationId++;
    return _getRow(index);
}

// Returns a row filled with whitespace and the current attributes, for you to freely use.
ROW& TextBuffer::GetScratchpadRow()
{
    return GetScratchpadRow(_currentAttributes);
}

// Returns a row filled with whitespace and the given attributes, for you to freely use.
ROW& TextBuffer::GetScratchpadRow(const TextAttribute& attributes)
{
    // The scratchpad row is mapped to the underlying index 0, whereas all regular rows are mapped to
    // index 1 and up. We do it this way instead of the other way around (scratchpad row at index _height),
    // because that would force us to MEM_COMMIT the entire buffer whenever this function is called.
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

// Given the character offset `position` in the `chars` string, this function returns the starting position of the next grapheme.
// For instance, given a `chars` of L"x\uD83D\uDE42y" and a `position` of 1 it'll return 3.
// GraphemePrev would do the exact inverse of this operation.
size_t TextBuffer::GraphemeNext(const std::wstring_view& chars, size_t position) noexcept
{
    auto& cwd = CodepointWidthDetector::Singleton();
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
    GraphemeState state{ .beg = chars.data() + position };
    cwd.GraphemeNext(state, chars);
    return position + state.len;
}

// It's the counterpart to GraphemeNext. See GraphemeNext.
size_t TextBuffer::GraphemePrev(const std::wstring_view& chars, size_t position) noexcept
{
    auto& cwd = CodepointWidthDetector::Singleton();
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
    GraphemeState state{ .beg = chars.data() + position };
    cwd.GraphemePrev(state, chars);
    return position - state.len;
}

// Ever wondered how much space a piece of text needs before inserting it? This function will tell you!
// It fundamentally behaves identical to the various ROW functions around `RowWriteState`.
//
// Set `columnLimit` to the amount of space that's available (e.g. `buffer_width - cursor_position.x`)
// and it'll return the amount of characters that fit into this space. The out parameter `columns`
// will contain the amount of columns this piece of text has actually used.
//
// Just like with `RowWriteState` one special case is when not all text fits into the given space:
// In that case, this function always returns exactly `columnLimit`. This distinction is important when "inserting"
// a wide glyph but there's only 1 column left. That 1 remaining column is supposed to be padded with whitespace.
size_t TextBuffer::FitTextIntoColumns(const std::wstring_view& chars, til::CoordType columnLimit, til::CoordType& columns) noexcept
{
    columnLimit = std::max(0, columnLimit);

    const auto beg = chars.begin();
    const auto end = chars.end();
    auto it = beg;
    const auto asciiEnd = beg + std::min(chars.size(), gsl::narrow_cast<size_t>(columnLimit));

    // ASCII fast-path: 1 char always corresponds to 1 column.
    for (; it != asciiEnd && *it < 0x80; ++it)
    {
    }

    auto dist = gsl::narrow_cast<size_t>(it - beg);
    auto col = gsl::narrow_cast<til::CoordType>(dist);

    if (it == asciiEnd) [[likely]]
    {
        columns = col;
        return dist;
    }

    // Unicode slow-path where we need to count text and columns separately.
    auto& cwd = CodepointWidthDetector::Singleton();
    const auto len = chars.size();

    // The non-ASCII character we have encountered may be a combining mark, like "a^" which is then displayed as "â".
    // In order to recognize both characters as a single grapheme, we need to back up by 1 ASCII character
    // and let GraphemeNext() find the next proper grapheme boundary.
    if (dist != 0)
    {
        dist--;
        col--;
    }

#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
    GraphemeState state{ .beg = chars.data() + dist };

    while (dist < len)
    {
        cwd.GraphemeNext(state, chars);
        col += state.width;

        if (col > columnLimit)
        {
            break;
        }

        dist += state.len;
    }

    // But if we simply ran out of text we just need to return the actual number of columns.
    columns = col;
    return dist;
}

// Pretend as if `position` is a regular cursor in the TextBuffer.
// This function will then pretend as if you pressed the left/right arrow
// keys `distance` amount of times (negative = left, positive = right).
til::point TextBuffer::NavigateCursor(til::point position, til::CoordType distance) const
{
    const til::CoordType maxX = _width - 1;
    const til::CoordType maxY = _height - 1;
    auto x = std::clamp(position.x, 0, maxX);
    auto y = std::clamp(position.y, 0, maxY);
    auto row = &GetRowByOffset(y);

    if (distance < 0)
    {
        do
        {
            if (x > 0)
            {
                x = row->NavigateToPrevious(x);
            }
            else if (y <= 0)
            {
                break;
            }
            else
            {
                --y;
                row = &GetRowByOffset(y);
                x = row->GetReadableColumnCount() - 1;
            }
        } while (++distance != 0);
    }
    else if (distance > 0)
    {
        auto rowWidth = row->GetReadableColumnCount();

        do
        {
            if (x < rowWidth)
            {
                x = row->NavigateToNext(x);
            }
            else if (y >= maxY)
            {
                break;
            }
            else
            {
                ++y;
                row = &GetRowByOffset(y);
                rowWidth = row->GetReadableColumnCount();
                x = 0;
            }
        } while (--distance != 0);
    }

    return { x, y };
}

// This function is intended for writing regular "lines" of text as it'll set the wrap flag on the given row.
// You can continue calling the function on the same row as long as state.columnEnd < state.columnLimit.
void TextBuffer::Replace(til::CoordType row, const TextAttribute& attributes, RowWriteState& state)
{
    auto& r = GetMutableRowByOffset(row);
    r.ReplaceText(state);
    r.ReplaceAttributes(state.columnBegin, state.columnEnd, attributes);
    ImageSlice::EraseCells(r, state.columnBegin, state.columnEnd);
    TriggerRedraw(Viewport::FromExclusive({ state.columnBeginDirty, row, state.columnEndDirty, row + 1 }));
}

void TextBuffer::Insert(til::CoordType row, const TextAttribute& attributes, RowWriteState& state)
{
    auto& r = GetMutableRowByOffset(row);
    auto& scratch = GetScratchpadRow();

    scratch.CopyFrom(r);

    r.ReplaceText(state);
    r.ReplaceAttributes(state.columnBegin, state.columnEnd, attributes);

    // Restore trailing text from our backup in scratch.
    RowWriteState restoreState{
        .text = scratch.GetText(state.columnBegin, state.columnLimit),
        .columnBegin = state.columnEnd,
        .columnLimit = state.columnLimit,
    };
    r.ReplaceText(restoreState);

    // Restore trailing attributes as well.
    if (const auto copyAmount = restoreState.columnEnd - restoreState.columnBegin; copyAmount > 0)
    {
        auto& rowAttr = r.Attributes();
        const auto& scratchAttr = scratch.Attributes();
        const auto restoreAttr = scratchAttr.slice(gsl::narrow<uint16_t>(state.columnBegin), gsl::narrow<uint16_t>(state.columnBegin + copyAmount));
        rowAttr.replace(gsl::narrow<uint16_t>(restoreState.columnBegin), gsl::narrow<uint16_t>(restoreState.columnEnd), restoreAttr);
        // If there is any image content, that needs to be copied too.
        ImageSlice::CopyCells(r, state.columnBegin, r, restoreState.columnBegin, restoreState.columnEnd);
    }
    // Image content at the insert position needs to be erased.
    ImageSlice::EraseCells(r, state.columnBegin, restoreState.columnBegin);

    TriggerRedraw(Viewport::FromExclusive({ state.columnBeginDirty, row, restoreState.columnEndDirty, row + 1 }));
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
            auto& r = GetMutableRowByOffset(y);
            r.CopyTextFrom(state);
            r.ReplaceAttributes(rect.left, rect.right, attributes);
            ImageSlice::EraseCells(r, rect.left, rect.right);
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
    auto& row = GetMutableRowByOffset(target.y);
    const auto newIt = row.WriteCells(givenIt, target.x, wrap, limitRight);

    // Take the cell distance written and notify that it needs to be repainted.
    const auto written = newIt.GetCellDistance(givenIt);
    const auto paint = Viewport::FromDimensions(target, { written, 1 });
    TriggerRedraw(paint);

    return newIt;
}

//Routine Description:
// - Increments the circular buffer by one. Circular buffer is represented by FirstRow variable.
//Arguments:
// - fillAttributes - the attributes with which the recycled row will be initialized.
//Return Value:
// - true if we successfully incremented the buffer.
void TextBuffer::IncrementCircularBuffer(const TextAttribute& fillAttributes)
{
    // Prune hyperlinks to delete obsolete references
    _PruneHyperlinks();

    // Second, clean out the old "first row" as it will become the "last row" of the buffer after the circle is performed.
    GetMutableRowByOffset(0).Reset(fillAttributes);
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
til::point TextBuffer::GetLastNonSpaceCharacter(const Viewport* viewOptional) const
{
    const auto viewport = viewOptional ? *viewOptional : GetSize();

    til::point coordEndOfText;
    // Search the given viewport by starting at the bottom.
    coordEndOfText.y = std::min(viewport.BottomInclusive(), _estimateOffsetOfLastCommittedRow());

    const auto& currRow = GetRowByOffset(coordEndOfText.y);
    // The X position of the end of the valid text is the Right draw boundary (which is one beyond the final valid character)
    coordEndOfText.x = currRow.MeasureRight() - 1;

    // If the X coordinate turns out to be -1, the row was empty, we need to search backwards for the real end of text.
    const auto viewportTop = viewport.Top();

    // while (this row is empty, and we're not at the top)
    while (coordEndOfText.x < 0 && coordEndOfText.y > viewportTop)
    {
        coordEndOfText.y--;
        const auto& backupRow = GetRowByOffset(coordEndOfText.y);
        // We need to back up to the previous row if this line is empty, AND there are more rows
        coordEndOfText.x = backupRow.MeasureRight() - 1;
    }

    // don't allow negative results
    coordEndOfText.y = std::max(coordEndOfText.y, 0);
    coordEndOfText.x = std::max(coordEndOfText.x, 0);

    return coordEndOfText;
}

const til::CoordType TextBuffer::GetFirstRowIndex() const noexcept
{
    return _firstRow;
}

const Viewport TextBuffer::GetSize() const noexcept
{
    return Viewport::FromDimensions({}, { _width, _height });
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
        CopyRow(y, y + delta, *this);
    }
}

void TextBuffer::CopyRow(const til::CoordType srcRowIndex, const til::CoordType dstRowIndex, TextBuffer& dstBuffer) const
{
    auto& dstRow = dstBuffer.GetMutableRowByOffset(dstRowIndex);
    const auto& srcRow = GetRowByOffset(srcRowIndex);
    dstRow.CopyFrom(srcRow);
    ImageSlice::CopyRow(srcRow, dstRow);
}

Cursor& TextBuffer::GetCursor() noexcept
{
    return _cursor;
}

const Cursor& TextBuffer::GetCursor() const noexcept
{
    return _cursor;
}

uint64_t TextBuffer::GetLastMutationId() const noexcept
{
    return _lastMutationId;
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
    GetMutableRowByOffset(y).SetWrapForced(wrap);
}

void TextBuffer::SetCurrentLineRendition(const LineRendition lineRendition, const TextAttribute& fillAttributes)
{
    const auto cursorPosition = GetCursor().GetPosition();
    const auto rowIndex = cursorPosition.y;
    auto& row = GetMutableRowByOffset(rowIndex);
    if (row.GetLineRendition() != lineRendition)
    {
        row.SetLineRendition(lineRendition);
        // If the line rendition has changed, the row can no longer be wrapped.
        row.SetWrapForced(false);
        // And all image content on the row is removed.
        row.SetImageSlice(nullptr);
        // And if it's no longer single width, the right half of the row should be erased.
        if (lineRendition != LineRendition::SingleWidth)
        {
            const auto fillOffset = GetLineWidth(rowIndex);
            FillRect({ fillOffset, rowIndex, til::CoordTypeMax, rowIndex + 1 }, L" ", fillAttributes);
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
        GetMutableRowByOffset(row).SetLineRendition(LineRendition::SingleWidth);
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

// Arguments:
// - newFirstRow: The current y-position of the viewport. We'll clear up until here.
// - rowsToKeep: the number of rows to keep in the buffer.
void TextBuffer::ClearScrollback(const til::CoordType newFirstRow, const til::CoordType rowsToKeep)
{
    // We're already at the top? don't clear anything. There's no scrollback.
    if (newFirstRow <= 0)
    {
        return;
    }
    // The new viewport should keep 0 rows? Then just reset everything.
    if (rowsToKeep <= 0)
    {
        _decommit();
        return;
    }

    ClearMarksInRange(til::point{ 0, 0 }, til::point{ _width, std::max(0, newFirstRow - 1) });

    // Our goal is to move the viewport to the absolute start of the underlying memory buffer so that we can
    // MEM_DECOMMIT the remaining memory. _firstRow is used to make the TextBuffer behave like a circular buffer.
    // The newFirstRow parameter is relative to the _firstRow. The trick to get the content to the absolute start
    // is to simply add _firstRow ourselves and then reset it to 0. This causes ScrollRows() to write into
    // the absolute start while reading from relative coordinates. This works because GetRowByOffset()
    // operates modulo the buffer height and so the possibly-too-large startAbsolute won't be an issue.
    const auto startAbsolute = _firstRow + newFirstRow;
    _firstRow = 0;
    ScrollRows(startAbsolute, rowsToKeep, -startAbsolute);

    const auto end = _estimateOffsetOfLastCommittedRow();
    for (auto y = rowsToKeep; y <= end; ++y)
    {
        GetMutableRowByOffset(y).Reset(_initialAttributes);
    }
}

// Routine Description:
// - This is the legacy screen resize with minimal changes
// Arguments:
// - newSize - new size of screen.
// Return Value:
// - Success if successful. Invalid parameter if screen buffer size is unexpected. No memory if allocation failed.
void TextBuffer::ResizeTraditional(til::size newSize)
{
    // Guard against resizing the text buffer to 0 columns/rows, which would break being able to insert text.
    newSize.width = std::max(newSize.width, 1);
    newSize.height = std::max(newSize.height, 1);

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
        CopyRow(srcRow, dstRow, newBuffer);
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

void TextBuffer::SetAsActiveBuffer(const bool isActiveBuffer) noexcept
{
    _isActiveBuffer = isActiveBuffer;
}

bool TextBuffer::IsActiveBuffer() const noexcept
{
    return _isActiveBuffer;
}

Microsoft::Console::Render::Renderer* TextBuffer::GetRenderer() noexcept
{
    return _renderer;
}

void TextBuffer::NotifyPaintFrame() noexcept
{
    if (_isActiveBuffer && _renderer)
    {
        _renderer->NotifyPaintFrame();
    }
}

void TextBuffer::TriggerRedraw(const Viewport& viewport)
{
    if (_isActiveBuffer && _renderer)
    {
        _renderer->TriggerRedraw(viewport);
    }
}

void TextBuffer::TriggerRedrawAll()
{
    if (_isActiveBuffer && _renderer)
    {
        _renderer->TriggerRedrawAll();
    }
}

void TextBuffer::TriggerScroll()
{
    if (_isActiveBuffer && _renderer)
    {
        _renderer->TriggerScroll();
    }
}

void TextBuffer::TriggerScroll(const til::point delta)
{
    if (_isActiveBuffer && _renderer)
    {
        _renderer->TriggerScroll(&delta);
    }
}

void TextBuffer::TriggerNewTextNotification(const std::wstring_view newText)
{
    if (_isActiveBuffer && _renderer)
    {
        _renderer->TriggerNewTextNotification(newText);
    }
}

void TextBuffer::TriggerSelection()
{
    if (_isActiveBuffer && _renderer)
    {
        _renderer->TriggerSelection();
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
    const auto realPos = ScreenToBufferPosition(pos);
    return GetRowByOffset(realPos.y).DelimiterClassAt(realPos.x, wordDelimiters);
}

til::point TextBuffer::GetWordStart2(til::point pos, const std::wstring_view wordDelimiters, bool includeWhitespace, std::optional<til::point> limitOptional) const
{
    const auto bufferSize{ GetSize() };
    const auto limit{ limitOptional.value_or(bufferSize.BottomInclusiveRightExclusive()) };

    if (pos < bufferSize.Origin())
    {
        // can't move further back, so return early at origin
        return bufferSize.Origin();
    }
    else if (pos >= limit)
    {
        // clamp to limit,
        // but still do movement
        pos = limit;
    }

    // Consider the delimiter classes represented as these chars:
    // - ControlChar:   "_"
    // - DelimiterChar: "D"
    // - RegularChar:   "C"
    // Expected results ("|" is the position):
    //   includeWhitespace: true     false
    //     CCC___|   -->   |CCC___  CCC|___
    //     DDD___|   -->   |DDD___  DDD|___
    //     ___CCC|   -->   ___|CCC  ___|CCC
    //     DDDCCC|   -->   DDD|CCC  DDD|CCC
    //     ___DDD|   -->   ___|DDD  ___|DDD
    //     CCCDDD|   -->   CCC|DDD  CCC|DDD
    // So the heuristic we use is:
    // 1. move to the beginning of the delimiter class run
    // 2. (includeWhitespace) if we were on a ControlChar, go back one more delimiter class run
    const auto initialDelimiter = bufferSize.IsInBounds(pos) ? _GetDelimiterClassAt(pos, wordDelimiters) : DelimiterClass::ControlChar;
    pos = _GetDelimiterClassRunStart(pos, wordDelimiters);
    if (!includeWhitespace || pos.x == bufferSize.Left())
    {
        // Special case:
        // we're at the left boundary (and end of a delimiter class run),
        // we already know we can't wrap, so return early
        return pos;
    }
    else if (initialDelimiter == DelimiterClass::ControlChar)
    {
        bufferSize.DecrementInExclusiveBounds(pos);
        pos = _GetDelimiterClassRunStart(pos, wordDelimiters);
    }
    return pos;
}

til::point TextBuffer::GetWordEnd2(til::point pos, const std::wstring_view wordDelimiters, bool includeWhitespace, std::optional<til::point> limitOptional) const
{
    const auto bufferSize{ GetSize() };
    const auto limit{ limitOptional.value_or(bufferSize.BottomInclusiveRightExclusive()) };

    if (pos >= limit)
    {
        // can't move further forward,
        // so return early at limit
        return limit;
    }
    else if (const auto origin{ bufferSize.Origin() }; pos < origin)
    {
        // clamp to origin,
        // but still do movement
        pos = origin;
    }

    // Consider the delimiter classes represented as these chars:
    // - ControlChar:   "_"
    // - DelimiterChar: "D"
    // - RegularChar:   "C"
    // Expected results ("|" is the position):
    //   includeWhitespace: true     false
    //     |CCC___   -->   CCC___|  CCC|___
    //     |DDD___   -->   DDD___|  DDD|___
    //     |___CCC   -->   ___|CCC  ___|CCC
    //     |DDDCCC   -->   DDD|CCC  DDD|CCC
    //     |___DDD   -->   ___|DDD  ___|DDD
    //     |CCCDDD   -->   CCC|DDD  CCC|DDD
    // So the heuristic we use is:
    // 1. move to the end of the delimiter class run
    // 2. (includeWhitespace) if the next delimiter class run is a ControlChar, go forward one more delimiter class run
    pos = _GetDelimiterClassRunEnd(pos, wordDelimiters);
    if (!includeWhitespace || pos.x == bufferSize.RightExclusive())
    {
        // Special case:
        // we're at the right boundary (and end of a delimiter class run),
        // we already know we can't wrap, so return early
        return pos;
    }

    if (const auto nextDelimClass = bufferSize.IsInBounds(pos) ? _GetDelimiterClassAt(pos, wordDelimiters) : DelimiterClass::ControlChar;
        nextDelimClass == DelimiterClass::ControlChar)
    {
        return _GetDelimiterClassRunEnd(pos, wordDelimiters);
    }
    return pos;
}

bool TextBuffer::IsWordBoundary(const til::point pos, const std::wstring_view wordDelimiters) const
{
    const auto bufferSize = GetSize();
    if (!bufferSize.IsInExclusiveBounds(pos))
    {
        // not in bounds
        return false;
    }

    // buffer boundaries are always word boundaries
    if (pos == bufferSize.Origin() || pos == bufferSize.BottomInclusiveRightExclusive())
    {
        return true;
    }

    // at beginning of the row, but we didn't wrap
    if (pos.x == bufferSize.Left())
    {
        const auto& row = GetRowByOffset(pos.y - 1);
        if (!row.WasWrapForced())
        {
            return true;
        }
    }

    // at end of the row, but we didn't wrap
    if (pos.x == bufferSize.RightExclusive())
    {
        const auto& row = GetRowByOffset(pos.y);
        if (!row.WasWrapForced())
        {
            return true;
        }
    }

    // we can treat text as contiguous,
    // use DecrementInBounds (not exclusive) here
    auto prevPos = pos;
    bufferSize.DecrementInBounds(prevPos);
    const auto prevDelimiterClass = _GetDelimiterClassAt(prevPos, wordDelimiters);

    // if we changed delimiter class
    // and the current delimiter class is not a control char,
    // we're at a word boundary
    const auto currentDelimiterClass = _GetDelimiterClassAt(pos, wordDelimiters);
    return prevDelimiterClass != currentDelimiterClass && currentDelimiterClass != DelimiterClass::ControlChar;
}

til::point TextBuffer::_GetDelimiterClassRunStart(til::point pos, const std::wstring_view wordDelimiters) const
{
    const auto bufferSize = GetSize();
    const auto initialDelimClass = bufferSize.IsInBounds(pos) ? _GetDelimiterClassAt(pos, wordDelimiters) : DelimiterClass::ControlChar;
    for (auto nextPos = pos; nextPos != bufferSize.Origin(); pos = nextPos)
    {
        bufferSize.DecrementInExclusiveBounds(nextPos);

        if (nextPos.x == bufferSize.RightExclusive())
        {
            // wrapped onto previous line,
            // check if it was forced to wrap
            const auto& row = GetRowByOffset(nextPos.y);
            if (!row.WasWrapForced())
            {
                return pos;
            }
        }
        else if (_GetDelimiterClassAt(nextPos, wordDelimiters) != initialDelimClass)
        {
            // if we changed delim class, we're done (don't apply move)
            return pos;
        }
    }
    return pos;
}

// Method Description:
// - Get the exclusive position for the end of the current delimiter class run
// Arguments:
// - pos - the buffer position being within the current delimiter class
// - wordDelimiters - what characters are we considering for the separation of words
til::point TextBuffer::_GetDelimiterClassRunEnd(til::point pos, const std::wstring_view wordDelimiters) const
{
    const auto bufferSize = GetSize();
    const auto initialDelimClass = bufferSize.IsInBounds(pos) ? _GetDelimiterClassAt(pos, wordDelimiters) : DelimiterClass::ControlChar;
    for (auto nextPos = pos; nextPos != bufferSize.BottomInclusiveRightExclusive(); pos = nextPos)
    {
        bufferSize.IncrementInExclusiveBounds(nextPos);

        if (nextPos.x == bufferSize.Left())
        {
            // wrapped onto next line,
            // check if it was forced to wrap or switched delimiter class
            const auto& row = GetRowByOffset(pos.y);
            if (!row.WasWrapForced() || _GetDelimiterClassAt(nextPos, wordDelimiters) != initialDelimClass)
            {
                return pos;
            }
        }
        else if (bufferSize.IsInBounds(nextPos) && _GetDelimiterClassAt(nextPos, wordDelimiters) != initialDelimClass)
        {
            // if we changed delim class,
            // apply the move and return
            return nextPos;
        }
    }
    return pos;
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

    // ignore left boundary. Continue until readable text found
    while (_GetDelimiterClassAt(result, wordDelimiters) != DelimiterClass::RegularChar)
    {
        if (result == bufferSize.Origin())
        {
            //looped around and hit origin (no word between origin and target)
            return result;
        }
        bufferSize.DecrementInBounds(result);
    }

    // make sure we expand to the left boundary or the beginning of the word
    while (_GetDelimiterClassAt(result, wordDelimiters) == DelimiterClass::RegularChar)
    {
        if (result == bufferSize.Origin())
        {
            // first char in buffer is a RegularChar
            // we can't move any further back
            return result;
        }
        bufferSize.DecrementInBounds(result);
    }

    // move off of delimiter
    bufferSize.IncrementInBounds(result);

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
    const bool isControlChar = initialDelimiter == DelimiterClass::ControlChar;

    // expand left until we hit the left boundary or a different delimiter class
    while (result != bufferSize.Origin() && _GetDelimiterClassAt(result, wordDelimiters) == initialDelimiter)
    {
        if (result.x == bufferSize.Left())
        {
            // Prevent wrapping to the previous line if the selection begins on whitespace
            if (isControlChar)
            {
                break;
            }

            if (result.y > 0)
            {
                // Prevent wrapping to the previous line if it was hard-wrapped (e.g. not forced by us to wrap)
                const auto& priorRow = GetRowByOffset(result.y - 1);
                if (!priorRow.WasWrapForced())
                {
                    break;
                }
            }
        }
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
        while (result != limit && result != bufferSize.BottomRightInclusive() && _GetDelimiterClassAt(result, wordDelimiters) == DelimiterClass::RegularChar)
        {
            // Iterate through readable text
            bufferSize.IncrementInBounds(result);
        }

        while (result != limit && result != bufferSize.BottomRightInclusive() && _GetDelimiterClassAt(result, wordDelimiters) != DelimiterClass::RegularChar)
        {
            // expand to the beginning of the NEXT word
            bufferSize.IncrementInBounds(result);
        }

        // Special case: we tried to move one past the end of the buffer
        // Manually increment onto the EndExclusive point.
        if (result == bufferSize.BottomRightInclusive())
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

    auto result = target;
    const auto initialDelimiter = _GetDelimiterClassAt(result, wordDelimiters);
    const bool isControlChar = initialDelimiter == DelimiterClass::ControlChar;

    // expand right until we hit the right boundary as a ControlChar or a different delimiter class
    while (result != bufferSize.BottomRightInclusive() && _GetDelimiterClassAt(result, wordDelimiters) == initialDelimiter)
    {
        if (result.x == bufferSize.RightInclusive())
        {
            // Prevent wrapping to the next line if the selection begins on whitespace
            if (isControlChar)
            {
                break;
            }

            // Prevent wrapping to the next line if this one was hard-wrapped (e.g. not forced by us to wrap)
            const auto& row = GetRowByOffset(result.y);
            if (!row.WasWrapForced())
            {
                break;
            }
        }

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
    if (resultPos > limit)
    {
        return limit;
    }

    // if we're on a trailing byte, move to the leading byte
    if (bufferSize.IsInBounds(resultPos) &&
        GetCellDataAt(resultPos)->DbcsAttr() == DbcsAttribute::Trailing)
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
    if (resultPos > limit)
    {
        return limit;
    }

    if (bufferSize.IsInBounds(resultPos) &&
        GetCellDataAt(resultPos)->DbcsAttr() == DbcsAttribute::Leading)
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

bool TextBuffer::MoveToNextGlyph2(til::point& pos, std::optional<til::point> limitOptional) const
{
    const auto bufferSize = GetSize();
    const auto limit{ limitOptional.value_or(bufferSize.BottomInclusiveRightExclusive()) };

    if (pos >= limit)
    {
        // Corner Case: we're on/past the limit
        // Clamp us to the limit
        pos = limit;
        return false;
    }

    // Try to move forward, but if we hit the buffer boundary, we fail to move.
    const bool success = bufferSize.IncrementInExclusiveBounds(pos);
    if (success &&
        bufferSize.IsInBounds(pos) &&
        GetCellDataAt(pos)->DbcsAttr() == DbcsAttribute::Trailing)
    {
        // Move again if we're on a wide glyph
        bufferSize.IncrementInExclusiveBounds(pos);
    }
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

bool TextBuffer::MoveToPreviousGlyph2(til::point& pos, std::optional<til::point> limitOptional) const
{
    const auto bufferSize = GetSize();
    const auto limit{ limitOptional.value_or(bufferSize.BottomInclusiveRightExclusive()) };

    if (pos >= limit)
    {
        // Corner Case: we're on/past the limit
        // Clamp us to the limit
        pos = limit;
        return false;
    }

    // Try to move backward, but if we hit the buffer boundary, we fail to move.
    const bool success = bufferSize.DecrementInExclusiveBounds(pos);
    if (success &&
        bufferSize.IsInBounds(pos) &&
        GetCellDataAt(pos)->DbcsAttr() == DbcsAttribute::Trailing)
    {
        // Move again if we're on a wide glyph
        bufferSize.DecrementInExclusiveBounds(pos);
    }
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

    // (0,0) is the top-left of the screen
    // the physically "higher" coordinate is closer to the top-left
    // the physically "lower" coordinate is closer to the bottom-right
    const auto [higherCoord, lowerCoord] = start <= end ?
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
            const auto bufferSize = GetSize();
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
// - end: the other end of the text region of interest (exclusive)
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
            higherCoord = ScreenToBufferLineInclusive(higherCoord, GetLineRendition(higherCoord.y));
            lowerCoord = ScreenToBufferLineInclusive(lowerCoord, GetLineRendition(lowerCoord.y));
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
    if (bufferSize.IsInBounds(targetPoint) && GetCellDataAt(targetPoint)->DbcsAttr() == DbcsAttribute::Trailing)
    {
        bufferSize.DecrementInExclusiveBounds(targetPoint);
        textRow.left = targetPoint.x;
    }

    // expand right side of rect
    targetPoint = { textRow.right, textRow.bottom };
    if (bufferSize.IsInBounds(targetPoint) && GetCellDataAt(targetPoint)->DbcsAttr() == DbcsAttribute::Trailing)
    {
        bufferSize.IncrementInExclusiveBounds(targetPoint);
        textRow.right = targetPoint.x;
    }
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
// - start - where to start getting text (should be at or prior to "end") (inclusive)
// - end - where to end getting text (exclusive)
// Return Value:
// - Just the text.
std::wstring TextBuffer::GetPlainText(const til::point start, const til::point end) const
{
    const auto req = CopyRequest::FromConfig(*this, start, end, true, false, false, false);
    return GetPlainText(req);
}

// Routine Description:
// - Given a copy request and a row, retrieves the row bounds [begin, end) and
//   a boolean indicating whether a line break should be added to this row.
// Arguments:
// - req - the copy request
// - iRow - the row index
// - row - the row
// Return Value:
// - The row bounds and a boolean for line break
std::tuple<til::CoordType, til::CoordType, bool> TextBuffer::_RowCopyHelper(const TextBuffer::CopyRequest& req, const til::CoordType iRow, const ROW& row) const
{
    til::CoordType rowBeg = 0;
    til::CoordType rowEnd = 0;
    if (req.blockSelection)
    {
        const auto lineRendition = row.GetLineRendition();
        const auto minX = req.bufferCoordinates ? req.minX : ScreenToBufferLineInclusive(til::point{ req.minX, iRow }, lineRendition).x;
        const auto maxX = req.bufferCoordinates ? req.maxX : ScreenToBufferLineInclusive(til::point{ req.maxX, iRow }, lineRendition).x;

        rowBeg = minX;
        rowEnd = maxX;
    }
    else
    {
        const auto lineRendition = row.GetLineRendition();
        const auto beg = req.bufferCoordinates ? req.beg : ScreenToBufferLineInclusive(req.beg, lineRendition);
        const auto end = req.bufferCoordinates ? req.end : ScreenToBufferLineInclusive(req.end, lineRendition);

        rowBeg = iRow != beg.y ? 0 : beg.x;
        rowEnd = iRow != end.y ? row.GetReadableColumnCount() : end.x;
    }

    // Our selection mechanism doesn't stick to glyph boundaries at the moment.
    // We need to adjust begin and end points manually to avoid partially
    // selected glyphs.
    rowBeg = row.AdjustToGlyphStart(rowBeg);
    rowEnd = row.AdjustToGlyphEnd(rowEnd);

    // When `formatWrappedRows` is set, apply formatting on all rows (wrapped
    // and non-wrapped), but when it's false, format non-wrapped rows only.
    const auto shouldFormatRow = req.formatWrappedRows || !row.WasWrapForced();

    // trim trailing whitespace
    if (shouldFormatRow && req.trimTrailingWhitespace)
    {
        rowEnd = std::min(rowEnd, row.GetLastNonSpaceColumn());
    }

    // line breaks
    const auto addLineBreak = shouldFormatRow && req.includeLineBreak;

    return { rowBeg, rowEnd, addLineBreak };
}

// Routine Description:
// - Retrieves the text data from the buffer and presents it in a clipboard-ready format.
// Arguments:
// - req - the copy request having the bounds of the selected region and other related configuration flags.
// Return Value:
// - The text data from the selected region of the text buffer. Empty if the copy request is invalid.
std::wstring TextBuffer::GetPlainText(const CopyRequest& req) const
{
    if (req.beg > req.end)
    {
        return {};
    }

    std::wstring selectedText;

    for (auto iRow = req.beg.y; iRow <= req.end.y; ++iRow)
    {
        const auto& row = GetRowByOffset(iRow);
        const auto& [rowBeg, rowEnd, addLineBreak] = _RowCopyHelper(req, iRow, row);

        // save selected text (exclusive end)
        selectedText += row.GetText(rowBeg, rowEnd);

        if (addLineBreak && iRow != req.end.y)
        {
            selectedText += L"\r\n";
        }
    }

    return selectedText;
}

// Retrieves the text data from the buffer *with* ANSI escape code control sequences and presents it in
//      a clipboard-ready format.
// Arguments:
// - req - the copy request having the bounds of the selected region and other related configuration flags.
// Return Value:
// - The text and control sequence data from the selected region of the text buffer. Empty if the copy request
//      is invalid.
std::wstring TextBuffer::GetWithControlSequences(const CopyRequest& req) const
{
    if (req.beg > req.end)
    {
        return {};
    }

    std::wstring selectedText;
    std::optional<TextAttribute> previousTextAttr;
    bool delayedLineBreak = false;

    const auto firstRow = req.beg.y;
    const auto lastRow = req.end.y;

    for (til::CoordType currentRow = firstRow; currentRow <= lastRow; currentRow++)
    {
        const auto& row = GetRowByOffset(currentRow);

        const auto [startX, endX, reqAddLineBreak] = _RowCopyHelper(req, currentRow, row);
        const bool isLastRow = currentRow == lastRow;
        const bool addLineBreak = reqAddLineBreak && !isLastRow;

        _SerializeRow(row, startX, endX, addLineBreak, isLastRow, selectedText, previousTextAttr, delayedLineBreak);
    }

    return selectedText;
}

// Routine Description:
// - Generates a CF_HTML compliant structure from the selected region of the buffer
// Arguments:
// - req - the copy request having the bounds of the selected region and other related configuration flags.
// - fontHeightPoints - the unscaled font height
// - fontFaceName - the name of the font used
// - backgroundColor - default background color for characters, also used in padding
// - isIntenseBold - true if being intense is treated as being bold
// - GetAttributeColors - function to get the colors of the text attributes as they're rendered
// Return Value:
// - string containing the generated HTML. Empty if the copy request is invalid.
std::string TextBuffer::GenHTML(const CopyRequest& req,
                                const int fontHeightPoints,
                                const std::wstring_view fontFaceName,
                                const COLORREF backgroundColor,
                                const bool isIntenseBold,
                                std::function<std::tuple<COLORREF, COLORREF, COLORREF>(const TextAttribute&)> GetAttributeColors) const noexcept
{
    // GH#5347 - Don't provide a title for the generated HTML, as many
    // web applications will paste the title first, followed by the HTML
    // content, which is unexpected.

    if (req.beg > req.end)
    {
        return {};
    }

    try
    {
        std::string htmlBuilder;

        // First we have to add some standard HTML boiler plate required for
        // CF_HTML as part of the HTML Clipboard format
        constexpr std::string_view htmlHeader = "<!DOCTYPE><HTML><HEAD></HEAD><BODY>";
        htmlBuilder += htmlHeader;

        htmlBuilder += "<!--StartFragment -->";

        // apply global style in div element
        {
            htmlBuilder += "<DIV STYLE=\"";
            htmlBuilder += "display:inline-block;";
            htmlBuilder += "white-space:pre;";
            fmt::format_to(std::back_inserter(htmlBuilder), FMT_COMPILE("background-color:{};"), Utils::ColorToHexString(backgroundColor));

            // even with different font, add monospace as fallback
            fmt::format_to(std::back_inserter(htmlBuilder), FMT_COMPILE("font-family:'{}',monospace;"), til::u16u8(fontFaceName));

            fmt::format_to(std::back_inserter(htmlBuilder), FMT_COMPILE("font-size:{}pt;"), fontHeightPoints);

            // note: MS Word doesn't support padding (in this way at least)
            // todo: customizable padding
            htmlBuilder += "padding:4px;";

            htmlBuilder += "\">";
        }

        for (auto iRow = req.beg.y; iRow <= req.end.y; ++iRow)
        {
            const auto& row = GetRowByOffset(iRow);
            const auto [rowBeg, rowEnd, addLineBreak] = _RowCopyHelper(req, iRow, row);
            const auto rowBegU16 = gsl::narrow_cast<uint16_t>(rowBeg);
            const auto rowEndU16 = gsl::narrow_cast<uint16_t>(rowEnd);
            const auto runs = row.Attributes().slice(rowBegU16, rowEndU16).runs();

            auto x = rowBegU16;
            for (const auto& [attr, length] : runs)
            {
                const auto nextX = gsl::narrow_cast<uint16_t>(x + length);
                const auto [fg, bg, ul] = GetAttributeColors(attr);
                const auto fgHex = Utils::ColorToHexString(fg);
                const auto bgHex = Utils::ColorToHexString(bg);
                const auto ulHex = Utils::ColorToHexString(ul);
                const auto ulStyle = attr.GetUnderlineStyle();
                const auto isUnderlined = ulStyle != UnderlineStyle::NoUnderline;
                const auto isCrossedOut = attr.IsCrossedOut();
                const auto isOverlined = attr.IsOverlined();

                htmlBuilder += "<SPAN STYLE=\"";
                fmt::format_to(std::back_inserter(htmlBuilder), FMT_COMPILE("color:{};"), fgHex);
                fmt::format_to(std::back_inserter(htmlBuilder), FMT_COMPILE("background-color:{};"), bgHex);

                if (isIntenseBold && attr.IsIntense())
                {
                    htmlBuilder += "font-weight:bold;";
                }

                if (attr.IsItalic())
                {
                    htmlBuilder += "font-style:italic;";
                }

                if (isCrossedOut || isOverlined)
                {
                    fmt::format_to(std::back_inserter(htmlBuilder),
                                   FMT_COMPILE("text-decoration:{} {} {};"),
                                   isCrossedOut ? "line-through" : "",
                                   isOverlined ? "overline" : "",
                                   fgHex);
                }

                if (isUnderlined)
                {
                    // Since underline, overline and strikethrough use the same css property,
                    // we cannot apply different colors to them at the same time. However, we
                    // can achieve the desired result by creating a nested <span> and applying
                    // underline style and color to it.
                    htmlBuilder += "\"><SPAN STYLE=\"";

                    switch (ulStyle)
                    {
                    case UnderlineStyle::NoUnderline:
                        break;
                    case UnderlineStyle::DoublyUnderlined:
                        fmt::format_to(std::back_inserter(htmlBuilder), FMT_COMPILE("text-decoration:underline double {};"), ulHex);
                        break;
                    case UnderlineStyle::CurlyUnderlined:
                        fmt::format_to(std::back_inserter(htmlBuilder), FMT_COMPILE("text-decoration:underline wavy {};"), ulHex);
                        break;
                    case UnderlineStyle::DottedUnderlined:
                        fmt::format_to(std::back_inserter(htmlBuilder), FMT_COMPILE("text-decoration:underline dotted {};"), ulHex);
                        break;
                    case UnderlineStyle::DashedUnderlined:
                        fmt::format_to(std::back_inserter(htmlBuilder), FMT_COMPILE("text-decoration:underline dashed {};"), ulHex);
                        break;
                    case UnderlineStyle::SinglyUnderlined:
                    default:
                        fmt::format_to(std::back_inserter(htmlBuilder), FMT_COMPILE("text-decoration:underline {};"), ulHex);
                        break;
                    }
                }

                htmlBuilder += "\">";

                // text
                std::string unescapedText;
                THROW_IF_FAILED(til::u16u8(row.GetText(x, nextX), unescapedText));
                for (const auto c : unescapedText)
                {
                    switch (c)
                    {
                    case '<':
                        htmlBuilder += "&lt;";
                        break;
                    case '>':
                        htmlBuilder += "&gt;";
                        break;
                    case '&':
                        htmlBuilder += "&amp;";
                        break;
                    default:
                        htmlBuilder += c;
                    }
                }

                if (isUnderlined)
                {
                    // close the nested span we created for underline
                    htmlBuilder += "</SPAN>";
                }

                htmlBuilder += "</SPAN>";

                // advance to next run of text
                x = nextX;
            }

            // never add line break to the last row.
            if (addLineBreak && iRow < req.end.y)
            {
                htmlBuilder += "<BR>";
            }
        }

        htmlBuilder += "</DIV>";

        htmlBuilder += "<!--EndFragment -->";

        constexpr std::string_view HtmlFooter = "</BODY></HTML>";
        htmlBuilder += HtmlFooter;

        // once filled with values, there will be exactly 157 bytes in the clipboard header
        constexpr size_t ClipboardHeaderSize = 157;

        // these values are byte offsets from start of clipboard
        const auto htmlStartPos = ClipboardHeaderSize;
        const auto htmlEndPos = ClipboardHeaderSize + gsl::narrow<size_t>(htmlBuilder.length());
        const auto fragStartPos = ClipboardHeaderSize + gsl::narrow<size_t>(htmlHeader.length());
        const auto fragEndPos = htmlEndPos - HtmlFooter.length();

        // header required by HTML 0.9 format
        std::string clipHeaderBuilder;
        clipHeaderBuilder += "Version:0.9\r\n";
        fmt::format_to(std::back_inserter(clipHeaderBuilder), FMT_COMPILE("StartHTML:{:0>10}\r\n"), htmlStartPos);
        fmt::format_to(std::back_inserter(clipHeaderBuilder), FMT_COMPILE("EndHTML:{:0>10}\r\n"), htmlEndPos);
        fmt::format_to(std::back_inserter(clipHeaderBuilder), FMT_COMPILE("StartFragment:{:0>10}\r\n"), fragStartPos);
        fmt::format_to(std::back_inserter(clipHeaderBuilder), FMT_COMPILE("EndFragment:{:0>10}\r\n"), fragEndPos);
        fmt::format_to(std::back_inserter(clipHeaderBuilder), FMT_COMPILE("StartSelection:{:0>10}\r\n"), fragStartPos);
        fmt::format_to(std::back_inserter(clipHeaderBuilder), FMT_COMPILE("EndSelection:{:0>10}\r\n"), fragEndPos);

        return clipHeaderBuilder + htmlBuilder;
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
        return {};
    }
}

// Routine Description:
// - Generates an RTF document from the selected region of the buffer
//   RTF 1.5 Spec: https://www.biblioscape.com/rtf15_spec.htm
//   RTF 1.9.1 Spec: https://msopenspecs.azureedge.net/files/Archive_References/[MSFT-RTF].pdf
// Arguments:
// - req - the copy request having the bounds of the selected region and other related configuration flags.
// - fontHeightPoints - the unscaled font height
// - fontFaceName - the name of the font used
// - backgroundColor - default background color for characters, also used in padding
// - isIntenseBold - true if being intense is treated as being bold
// - GetAttributeColors - function to get the colors of the text attributes as they're rendered
// Return Value:
// - string containing the generated RTF. Empty if the copy request is invalid.
std::string TextBuffer::GenRTF(const CopyRequest& req,
                               const int fontHeightPoints,
                               const std::wstring_view fontFaceName,
                               const COLORREF backgroundColor,
                               const bool isIntenseBold,
                               std::function<std::tuple<COLORREF, COLORREF, COLORREF>(const TextAttribute&)> GetAttributeColors) const noexcept
{
    if (req.beg > req.end)
    {
        return {};
    }

    try
    {
        std::string rtfBuilder;

        // start rtf
        rtfBuilder += "{";

        // Standard RTF header.
        // This is similar to the header generated by WordPad.
        // \ansi:
        //   Specifies that the ANSI char set is used in the current doc.
        // \ansicpg1252:
        //   Represents the ANSI code page which is used to perform
        //   the Unicode to ANSI conversion when writing RTF text.
        // \deff0:
        //   Specifies that the default font for the document is the one
        //   at index 0 in the font table.
        // \nouicompat:
        //   Some features are blocked by default to maintain compatibility
        //   with older programs (Eg. Word 97-2003). `nouicompat` disables this
        //   behavior, and unblocks these features. See: Spec 1.9.1, Pg. 51.
        rtfBuilder += "\\rtf1\\ansi\\ansicpg1252\\deff0\\nouicompat";

        // font table
        // Brace escape: add an extra brace (of same kind) after a brace to escape it within the format string.
        fmt::format_to(std::back_inserter(rtfBuilder), FMT_COMPILE("{{\\fonttbl{{\\f0\\fmodern\\fcharset0 {};}}}}"), til::u16u8(fontFaceName));

        // map to keep track of colors:
        // keys are colors represented by COLORREF
        // values are indices of the corresponding colors in the color table
        std::unordered_map<COLORREF, size_t> colorMap;

        // RTF color table
        std::string colorTableBuilder;
        colorTableBuilder += "{\\colortbl ;";

        const auto getColorTableIndex = [&](const COLORREF color) -> size_t {
            // Exclude the 0 index for the default color, and start with 1.

            const auto [it, inserted] = colorMap.emplace(color, colorMap.size() + 1);
            if (inserted)
            {
                const auto red = static_cast<int>(GetRValue(color));
                const auto green = static_cast<int>(GetGValue(color));
                const auto blue = static_cast<int>(GetBValue(color));
                fmt::format_to(std::back_inserter(colorTableBuilder), FMT_COMPILE("\\red{}\\green{}\\blue{};"), red, green, blue);
            }
            return it->second;
        };

        // content
        std::string contentBuilder;

        // \viewkindN: View mode of the document to be used. N=4 specifies that the document is in Normal view. (maybe unnecessary?)
        // \ucN: Number of unicode fallback characters after each codepoint. (global)
        contentBuilder += "\\viewkind4\\uc1";

        // paragraph styles
        // \pard: paragraph description
        // \slmultN: line-spacing multiple
        // \fN: font to be used for the paragraph, where N is the font index in the font table
        contentBuilder += "\\pard\\slmult1\\f0";

        // \fsN: specifies font size in half-points. E.g. \fs20 results in a font
        // size of 10 pts. That's why, font size is multiplied by 2 here.
        fmt::format_to(std::back_inserter(contentBuilder), FMT_COMPILE("\\fs{}"), 2 * fontHeightPoints);

        // Set the background color for the page. But the standard way (\cbN) to do
        // this isn't supported in Word. However, the following control words sequence
        // works in Word (and other RTF editors also) for applying the text background
        // color. See: Spec 1.9.1, Pg. 23.
        fmt::format_to(std::back_inserter(contentBuilder), FMT_COMPILE("\\chshdng0\\chcbpat{}"), getColorTableIndex(backgroundColor));

        for (auto iRow = req.beg.y; iRow <= req.end.y; ++iRow)
        {
            const auto& row = GetRowByOffset(iRow);
            const auto [rowBeg, rowEnd, addLineBreak] = _RowCopyHelper(req, iRow, row);
            const auto rowBegU16 = gsl::narrow_cast<uint16_t>(rowBeg);
            const auto rowEndU16 = gsl::narrow_cast<uint16_t>(rowEnd);
            const auto runs = row.Attributes().slice(rowBegU16, rowEndU16).runs();

            auto x = rowBegU16;
            for (auto& [attr, length] : runs)
            {
                const auto nextX = gsl::narrow_cast<uint16_t>(x + length);
                const auto [fg, bg, ul] = GetAttributeColors(attr);
                const auto fgIdx = getColorTableIndex(fg);
                const auto bgIdx = getColorTableIndex(bg);
                const auto ulIdx = getColorTableIndex(ul);
                const auto ulStyle = attr.GetUnderlineStyle();

                // start an RTF group that can be closed later to restore the
                // default attribute.
                contentBuilder += "{";

                fmt::format_to(std::back_inserter(contentBuilder), FMT_COMPILE("\\cf{}"), fgIdx);
                fmt::format_to(std::back_inserter(contentBuilder), FMT_COMPILE("\\chshdng0\\chcbpat{}"), bgIdx);

                if (isIntenseBold && attr.IsIntense())
                {
                    contentBuilder += "\\b";
                }

                if (attr.IsItalic())
                {
                    contentBuilder += "\\i";
                }

                if (attr.IsCrossedOut())
                {
                    contentBuilder += "\\strike";
                }

                switch (ulStyle)
                {
                case UnderlineStyle::NoUnderline:
                    break;
                case UnderlineStyle::DoublyUnderlined:
                    fmt::format_to(std::back_inserter(contentBuilder), FMT_COMPILE("\\uldb\\ulc{}"), ulIdx);
                    break;
                case UnderlineStyle::CurlyUnderlined:
                    fmt::format_to(std::back_inserter(contentBuilder), FMT_COMPILE("\\ulwave\\ulc{}"), ulIdx);
                    break;
                case UnderlineStyle::DottedUnderlined:
                    fmt::format_to(std::back_inserter(contentBuilder), FMT_COMPILE("\\uld\\ulc{}"), ulIdx);
                    break;
                case UnderlineStyle::DashedUnderlined:
                    fmt::format_to(std::back_inserter(contentBuilder), FMT_COMPILE("\\uldash\\ulc{}"), ulIdx);
                    break;
                case UnderlineStyle::SinglyUnderlined:
                default:
                    fmt::format_to(std::back_inserter(contentBuilder), FMT_COMPILE("\\ul\\ulc{}"), ulIdx);
                    break;
                }

                // RTF commands and the text data must be separated by a space.
                // Otherwise, if the text begins with a space then that space will
                // be interpreted as part of the last command, and will be lost.
                contentBuilder += " ";

                const auto unescapedText = row.GetText(x, nextX); // including character at nextX
                _AppendRTFText(contentBuilder, unescapedText);

                contentBuilder += "}"; // close RTF group

                // advance to next run of text
                x = nextX;
            }

            // never add line break to the last row.
            if (addLineBreak && iRow < req.end.y)
            {
                contentBuilder += "\\line";
            }
        }

        // add color table to the final RTF
        rtfBuilder += colorTableBuilder + "}";

        // add the text content to the final RTF
        rtfBuilder += contentBuilder + "}";

        return rtfBuilder;
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
        return {};
    }
}

void TextBuffer::_AppendRTFText(std::string& contentBuilder, const std::wstring_view& text)
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
                contentBuilder += "\\";
                [[fallthrough]];
            default:
                contentBuilder += gsl::narrow_cast<char>(codeUnit);
            }
        }
        else
        {
            // Windows uses unsigned wchar_t - RTF uses signed ones.
            // '?' is the fallback ascii character.
            fmt::format_to(std::back_inserter(contentBuilder), FMT_COMPILE("\\u{}?"), std::bit_cast<int16_t>(codeUnit));
        }
    }
}

void TextBuffer::SerializeToPath(const wchar_t* destination) const
{
    const wil::unique_handle file{ CreateFileW(destination, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) };
    THROW_LAST_ERROR_IF(!file);

    static constexpr size_t writeThreshold = 32 * 1024;
    std::wstring buffer;
    buffer.reserve(writeThreshold + writeThreshold / 2);
    buffer.push_back(L'\uFEFF');

    std::optional<TextAttribute> previousTextAttr;
    bool delayedLineBreak = false;

    const til::CoordType firstRow = 0;
    const til::CoordType lastRow = GetLastNonSpaceCharacter(nullptr).y;

    // This iterates through each row. The exit condition is at the end
    // of the for() loop so that we can properly handle file flushing.
    for (til::CoordType currentRow = firstRow;; currentRow++)
    {
        const auto& row = GetRowByOffset(currentRow);

        const auto isLastRow = currentRow == lastRow;
        const auto startX = 0;
        const auto endX = row.GetReadableColumnCount();
        const bool addLineBreak = !row.WasWrapForced() || isLastRow;

        _SerializeRow(row, startX, endX, addLineBreak, isLastRow, buffer, previousTextAttr, delayedLineBreak);

        if (buffer.size() >= writeThreshold || isLastRow)
        {
            const auto fileSize = gsl::narrow<DWORD>(buffer.size() * sizeof(wchar_t));
            DWORD bytesWritten = 0;
            THROW_IF_WIN32_BOOL_FALSE(WriteFile(file.get(), buffer.data(), fileSize, &bytesWritten, nullptr));
            THROW_WIN32_IF_MSG(ERROR_WRITE_FAULT, bytesWritten != fileSize, "failed to write");
            buffer.clear();
        }

        if (isLastRow)
        {
            break;
        }
    }
}

// Serializes one row of the text buffer including ANSI escape code control sequences.
// Arguments:
// - row - A reference to the row being serialized.
// - startX - The first column (inclusive) to include in the serialized content.
// - endX - The last column (exclusive) to include in the serialized content.
// - addLineBreak - Whether to add a line break at the end of the serialized row.
// - isLastRow - Whether this is the final row to be serialized.
// - buffer - A string to write the serialized row into.
// - previousTextAttr - Used for tracking state across multiple calls to `_SerializeRow` for sequential rows.
//      The value will be mutated by the call. The initial call should contain `nullopt`, and subsequent calls
//      should pass the value that was written by the previous call.
// - delayedLineBreak - Similarly used for tracking state across multiple calls, and similarly will be mutated
//      by the call. The initial call should pass `false` and subsequent calls should pass the value that was
//      written by the previous call.
void TextBuffer::_SerializeRow(const ROW& row, const til::CoordType startX, const til::CoordType endX, const bool addLineBreak, const bool isLastRow, std::wstring& buffer, std::optional<TextAttribute>& previousTextAttr, bool& delayedLineBreak) const
{
    if (const auto lr = row.GetLineRendition(); lr != LineRendition::SingleWidth)
    {
        static constexpr std::wstring_view mappings[] = {
            L"\x1b#6", // LineRendition::DoubleWidth
            L"\x1b#3", // LineRendition::DoubleHeightTop
            L"\x1b#4", // LineRendition::DoubleHeightBottom
        };
        const auto idx = std::clamp(static_cast<int>(lr) - 1, 0, 2);
        buffer.append(til::at(mappings, idx));
    }

    const auto startXU16 = gsl::narrow_cast<uint16_t>(startX);
    const auto endXU16 = gsl::narrow_cast<uint16_t>(endX);
    const auto runs = row.Attributes().slice(startXU16, endXU16).runs();

    const auto beg = runs.begin();
    const auto end = runs.end();
    auto it = beg;
    // Don't try to get `end - 1` if it's an empty iterator; in this case we're going to ignore the `last`
    // value anyway so just use `end`.
    const auto last = it == end ? end : end - 1;
    const auto lastCharX = row.MeasureRight();
    til::CoordType oldX = startX;

    for (; it != end; ++it)
    {
        const auto effectivePreviousTextAttr = previousTextAttr.value_or(TextAttribute{ CharacterAttributes::Unused1, TextColor{}, TextColor{}, 0, TextColor{} });
        const auto previousAttr = effectivePreviousTextAttr.GetCharacterAttributes();
        const auto previousHyperlinkId = effectivePreviousTextAttr.GetHyperlinkId();
        const auto previousFg = effectivePreviousTextAttr.GetForeground();
        const auto previousBg = effectivePreviousTextAttr.GetBackground();
        const auto previousUl = effectivePreviousTextAttr.GetUnderlineColor();

        const auto attr = it->value.GetCharacterAttributes();
        const auto hyperlinkId = it->value.GetHyperlinkId();
        const auto fg = it->value.GetForeground();
        const auto bg = it->value.GetBackground();
        const auto ul = it->value.GetUnderlineColor();

        if (previousAttr != attr)
        {
            auto attrDelta = attr ^ previousAttr;

            // There's no escape sequence that only turns off either bold/intense or dim/faint. SGR 22 turns off both.
            // This results in two issues in our generic "Mapping" code below. Assuming, both Intense and Faint were on...
            // * ...and either turned off, it would emit SGR 22 which turns both attributes off = Wrong.
            // * ...and both are now off, it would emit SGR 22 twice.
            //
            // This extra branch takes care of both issues. If both attributes turned off it'll emit a single \x1b[22m,
            // if faint turned off \x1b[22;1m (intense is still on), and \x1b[22;2m if intense turned off (vice versa).
            if (WI_AreAllFlagsSet(previousAttr, CharacterAttributes::Intense | CharacterAttributes::Faint) &&
                WI_IsAnyFlagSet(attrDelta, CharacterAttributes::Intense | CharacterAttributes::Faint))
            {
                wchar_t buf[8] = L"\x1b[22m";
                size_t len = 5;

                if (WI_IsAnyFlagSet(attr, CharacterAttributes::Intense | CharacterAttributes::Faint))
                {
                    buf[4] = L';';
                    buf[5] = WI_IsAnyFlagSet(attr, CharacterAttributes::Intense) ? L'1' : L'2';
                    buf[6] = L'm';
                    len = 7;
                }

                buffer.append(&buf[0], len);
                WI_ClearAllFlags(attrDelta, CharacterAttributes::Intense | CharacterAttributes::Faint);
            }

            {
                struct Mapping
                {
                    CharacterAttributes attr;
                    uint8_t change[2]; // [0] = off, [1] = on
                };
                static constexpr Mapping mappings[] = {
                    { CharacterAttributes::Intense, { 22, 1 } },
                    { CharacterAttributes::Italics, { 23, 3 } },
                    { CharacterAttributes::Blinking, { 25, 5 } },
                    { CharacterAttributes::Invisible, { 28, 8 } },
                    { CharacterAttributes::CrossedOut, { 29, 9 } },
                    { CharacterAttributes::Faint, { 22, 2 } },
                    { CharacterAttributes::TopGridline, { 55, 53 } },
                    { CharacterAttributes::ReverseVideo, { 27, 7 } },
                };
                for (const auto& mapping : mappings)
                {
                    if (WI_IsAnyFlagSet(attrDelta, mapping.attr))
                    {
                        const auto n = til::at(mapping.change, WI_IsAnyFlagSet(attr, mapping.attr));
                        fmt::format_to(std::back_inserter(buffer), FMT_COMPILE(L"\x1b[{}m"), n);
                    }
                }
            }

            if (WI_IsAnyFlagSet(attrDelta, CharacterAttributes::UnderlineStyle))
            {
                static constexpr std::wstring_view mappings[] = {
                    L"\x1b[24m", // UnderlineStyle::NoUnderline
                    L"\x1b[4m", // UnderlineStyle::SinglyUnderlined
                    L"\x1b[21m", // UnderlineStyle::DoublyUnderlined
                    L"\x1b[4:3m", // UnderlineStyle::CurlyUnderlined
                    L"\x1b[4:4m", // UnderlineStyle::DottedUnderlined
                    L"\x1b[4:5m", // UnderlineStyle::DashedUnderlined
                };

                auto idx = WI_EnumValue(it->value.GetUnderlineStyle());
                if (idx >= std::size(mappings))
                {
                    idx = 1; // UnderlineStyle::SinglyUnderlined
                }

                buffer.append(til::at(mappings, idx));
            }
        }

        if (previousFg != fg)
        {
            switch (fg.GetType())
            {
            case ColorType::IsDefault:
                buffer.append(L"\x1b[39m");
                break;
            case ColorType::IsIndex16:
            {
                uint8_t index = WI_IsFlagSet(fg.GetIndex(), 8) ? 90 : 30;
                index += fg.GetIndex() & 7;
                fmt::format_to(std::back_inserter(buffer), FMT_COMPILE(L"\x1b[{}m"), index);
                break;
            }
            case ColorType::IsIndex256:
                fmt::format_to(std::back_inserter(buffer), FMT_COMPILE(L"\x1b[38;5;{}m"), fg.GetIndex());
                break;
            case ColorType::IsRgb:
                fmt::format_to(std::back_inserter(buffer), FMT_COMPILE(L"\x1b[38;2;{};{};{}m"), fg.GetR(), fg.GetG(), fg.GetB());
                break;
            default:
                break;
            }
        }

        if (previousBg != bg)
        {
            switch (bg.GetType())
            {
            case ColorType::IsDefault:
                buffer.append(L"\x1b[49m");
                break;
            case ColorType::IsIndex16:
            {
                uint8_t index = WI_IsFlagSet(bg.GetIndex(), 8) ? 100 : 40;
                index += bg.GetIndex() & 7;
                fmt::format_to(std::back_inserter(buffer), FMT_COMPILE(L"\x1b[{}m"), index);
                break;
            }
            case ColorType::IsIndex256:
                fmt::format_to(std::back_inserter(buffer), FMT_COMPILE(L"\x1b[48;5;{}m"), bg.GetIndex());
                break;
            case ColorType::IsRgb:
                fmt::format_to(std::back_inserter(buffer), FMT_COMPILE(L"\x1b[48;2;{};{};{}m"), bg.GetR(), bg.GetG(), bg.GetB());
                break;
            default:
                break;
            }
        }

        if (previousUl != ul)
        {
            switch (fg.GetType())
            {
            case ColorType::IsDefault:
                buffer.append(L"\x1b[59m");
                break;
            case ColorType::IsIndex256:
                fmt::format_to(std::back_inserter(buffer), FMT_COMPILE(L"\x1b[58:5:{}m"), ul.GetIndex());
                break;
            case ColorType::IsRgb:
                fmt::format_to(std::back_inserter(buffer), FMT_COMPILE(L"\x1b[58:2::{}:{}:{}m"), ul.GetR(), ul.GetG(), ul.GetB());
                break;
            default:
                break;
            }
        }

        if (previousHyperlinkId != hyperlinkId)
        {
            if (hyperlinkId)
            {
                const auto uri = GetHyperlinkUriFromId(hyperlinkId);
                if (!uri.empty())
                {
                    buffer.append(L"\x1b]8;;");
                    buffer.append(uri);
                    buffer.append(L"\x1b\\");
                }
            }
            else
            {
                buffer.append(L"\x1b]8;;\x1b\\");
            }
        }

        previousTextAttr = it->value;

        // Initially, the buffer is initialized with the default attributes, but once it begins to scroll,
        // newly scrolled in rows are initialized with the current attributes. This means we need to set
        // the current attributes to those of the upcoming row before the row comes up. Or inversely:
        // We let the row come up, let it set its attributes and only then print the newline.
        if (delayedLineBreak)
        {
            buffer.append(L"\r\n");
            delayedLineBreak = false;
        }

        auto newX = oldX + it->length;

        // Since our text buffer doesn't store the original input text, the information over the amount of trailing
        // whitespaces was lost. If we don't do anything here then a row that just says "Hello" would be serialized
        // to "Hello                    ...". If the user restores the buffer dump with a different window size,
        // this would result in some fairly ugly reflow. This code attempts to at least trim trailing whitespaces.
        //
        // As mentioned above for `delayedLineBreak`, rows are initialized with their first attribute, BUT
        // only if the viewport has begun to scroll. Otherwise, they're initialized with the default attributes.
        // In other words, we can only skip \x1b[K = Erase in Line, if both the first/last attribute are the default attribute.
        static constexpr TextAttribute defaultAttr;
        const auto trimTrailingWhitespaces = it == last && lastCharX < newX;
        const auto clearToEndOfLine = trimTrailingWhitespaces && (beg->value != defaultAttr || last->value != defaultAttr);

        if (trimTrailingWhitespaces)
        {
            newX = lastCharX;
        }

        buffer.append(row.GetText(oldX, newX));

        if (clearToEndOfLine)
        {
            buffer.append(L"\x1b[K");
        }

        oldX = newX;
    }

    // Handle empty rows (with no runs). See above for more details about `delayedLineBreak`.
    if (delayedLineBreak)
    {
        buffer.append(L"\r\n");
        delayedLineBreak = false;
    }

    delayedLineBreak = !row.WasWrapForced() && addLineBreak;

    if (isLastRow)
    {
        if (previousTextAttr.has_value() && previousTextAttr->GetHyperlinkId())
        {
            buffer.append(L"\x1b]8;;\x1b\\");
        }
        buffer.append(L"\x1b[0m");
        if (addLineBreak)
        {
            buffer.append(L"\r\n");
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
void TextBuffer::Reflow(TextBuffer& oldBuffer, TextBuffer& newBuffer, const Viewport* lastCharacterViewport, PositionInformation* positionInfo)
{
    const auto& oldCursor = oldBuffer.GetCursor();
    auto& newCursor = newBuffer.GetCursor();

    til::point oldCursorPos = oldCursor.GetPosition();
    til::point newCursorPos;

    // BODGY: We use oldCursorPos in two critical places below:
    // * To compute an oldHeight that includes at a minimum the cursor row
    // * For REFLOW_JANK_CURSOR_WRAP (see comment below)
    // Both of these would break the reflow algorithm, but the latter of the two in particular
    // would cause the main copy loop below to deadlock. In other words, these two lines
    // protect this function against yet-unknown bugs in other parts of the code base.
    oldCursorPos.x = std::clamp(oldCursorPos.x, 0, oldBuffer._width - 1);
    oldCursorPos.y = std::clamp(oldCursorPos.y, 0, oldBuffer._height - 1);

    const auto lastRowWithText = oldBuffer.GetLastNonSpaceCharacter(lastCharacterViewport).y;

    auto mutableViewportTop = positionInfo ? positionInfo->mutableViewportTop : til::CoordTypeMax;
    auto visibleViewportTop = positionInfo ? positionInfo->visibleViewportTop : til::CoordTypeMax;

    til::CoordType oldY = 0;
    til::CoordType newY = 0;
    til::CoordType newX = 0;
    til::CoordType newWidth = newBuffer.GetSize().Width();
    til::CoordType newYLimit = til::CoordTypeMax;

    const auto oldHeight = std::max(lastRowWithText, oldCursorPos.y) + 1;
    const auto newHeight = newBuffer.GetSize().Height();
    const auto newWidthU16 = gsl::narrow_cast<uint16_t>(newWidth);

    // Copy oldBuffer into newBuffer until oldBuffer has been fully consumed.
    for (; oldY < oldHeight && newY < newYLimit; ++oldY)
    {
        const auto& oldRow = oldBuffer.GetRowByOffset(oldY);

        // A pair of double height rows should optimally wrap as a union (i.e. after wrapping there should be 4 lines).
        // But for this initial implementation I chose the alternative approach: Just truncate them.
        if (oldRow.GetLineRendition() != LineRendition::SingleWidth)
        {
            // Since rows with a non-standard line rendition should be truncated it's important
            // that we pretend as if the previous row ended in a newline, even if it didn't.
            // This is what this if does: It newlines.
            if (newX)
            {
                newX = 0;
                newY++;
            }

            auto& newRow = newBuffer.GetMutableRowByOffset(newY);

            // See the comment marked with "REFLOW_RESET".
            if (newY >= newHeight)
            {
                newRow.Reset(newBuffer._initialAttributes);
            }

            newRow.CopyFrom(oldRow);
            newRow.SetWrapForced(false);

            if (oldY == oldCursorPos.y)
            {
                newCursorPos = { newRow.AdjustToGlyphStart(oldCursorPos.x), newY };
            }
            if (oldY >= mutableViewportTop)
            {
                positionInfo->mutableViewportTop = newY;
                mutableViewportTop = til::CoordTypeMax;
            }
            if (oldY >= visibleViewportTop)
            {
                positionInfo->visibleViewportTop = newY;
                visibleViewportTop = til::CoordTypeMax;
            }

            newY++;
            continue;
        }

        // Rows don't store any information for what column the last written character is in.
        // We simply truncate all trailing whitespace in this implementation.
        auto oldRowLimit = oldRow.MeasureRight();
        if (oldY == oldCursorPos.y)
        {
            // REFLOW_JANK_CURSOR_WRAP:
            // Pretending as if there's always at least whitespace in front of the cursor has the benefit that
            // * the cursor retains its distance from any preceding text.
            // * when a client application starts writing on this new, empty line,
            //   enlarging the buffer unwraps the text onto the preceding line.
            oldRowLimit = std::max(oldRowLimit, oldCursorPos.x + 1);
        }

        // Immediately copy this mark over to our new row. The positions of the
        // marks themselves will be preserved, since they're just text
        // attributes. But the "bookmark" needs to get moved to the new row too.
        // * If a row wraps as it reflows, that's fine - we want to leave the
        //   mark on the row it started on.
        // * If the second row of a wrapped row had a mark, and it de-flows onto a
        //   single row, that's fine! The mark was on that logical row.
        if (oldRow.GetScrollbarData().has_value())
        {
            newBuffer.GetMutableRowByOffset(newY).SetScrollbarData(oldRow.GetScrollbarData());
        }

        til::CoordType oldX = 0;

        // Copy oldRow into newBuffer until oldRow has been fully consumed.
        // We use a do-while loop to ensure that line wrapping occurs and
        // that attributes are copied over even for seemingly empty rows.
        do
        {
            // This if condition handles line wrapping.
            // Only if we write past the last column we should wrap and as such this if
            // condition is in front of the text insertion code instead of behind it.
            // A SetWrapForced of false implies an explicit newline, which is the default.
            if (newX >= newWidth)
            {
                newBuffer.GetMutableRowByOffset(newY).SetWrapForced(true);
                newX = 0;
                newY++;
            }

            // REFLOW_RESET:
            // If we shrink the buffer vertically, for instance from 100 rows to 90 rows, we will write 10 rows in the
            // new buffer twice. We need to reset them before copying text, or otherwise we'll see the previous contents.
            // We don't need to be smart about this. Reset() is fast and shrinking doesn't occur often.
            if (newY >= newHeight && newX == 0)
            {
                // We need to ensure not to overwrite the row the cursor is on.
                if (newY >= newYLimit)
                {
                    break;
                }
                newBuffer.GetMutableRowByOffset(newY).Reset(newBuffer._initialAttributes);
            }

            auto& newRow = newBuffer.GetMutableRowByOffset(newY);

            RowCopyTextFromState state{
                .source = oldRow,
                .columnBegin = newX,
                .columnLimit = til::CoordTypeMax,
                .sourceColumnBegin = oldX,
                .sourceColumnLimit = oldRowLimit,
            };
            newRow.CopyTextFrom(state);

            // If we're at the start of the old row, copy its image content.
            if (oldX == 0)
            {
                ImageSlice::CopyRow(oldRow, newRow);
            }

            const auto& oldAttr = oldRow.Attributes();
            auto& newAttr = newRow.Attributes();
            const auto attributes = oldAttr.slice(gsl::narrow_cast<uint16_t>(oldX), oldAttr.size());
            newAttr.replace(gsl::narrow_cast<uint16_t>(newX), newAttr.size(), attributes);
            newAttr.resize_trailing_extent(newWidthU16);

            if (oldY == oldCursorPos.y && oldCursorPos.x >= oldX)
            {
                // In theory AdjustToGlyphStart ensures we don't put the cursor on a trailing wide glyph.
                // In practice I don't think that this can possibly happen. Better safe than sorry.
                newCursorPos = { newRow.AdjustToGlyphStart(oldCursorPos.x - oldX + newX), newY };
                // If there's so much text past the old cursor position that it doesn't fit into new buffer,
                // then the new cursor position will be "lost", because it's overwritten by unrelated text.
                // We have two choices how can handle this:
                // * If the new cursor is at an y < 0, just put the cursor at (0,0)
                // * Stop writing into the new buffer before we overwrite the new cursor position
                // This implements the second option. There's no fundamental reason why this is better.
                newYLimit = newY + newHeight;
            }
            if (oldY >= mutableViewportTop)
            {
                positionInfo->mutableViewportTop = newY;
                mutableViewportTop = til::CoordTypeMax;
            }
            if (oldY >= visibleViewportTop)
            {
                positionInfo->visibleViewportTop = newY;
                visibleViewportTop = til::CoordTypeMax;
            }

            oldX = state.sourceColumnEnd;
            newX = state.columnEnd;
        } while (oldX < oldRowLimit);

        // If the row had an explicit newline we also need to newline. :)
        if (!oldRow.WasWrapForced())
        {
            newX = 0;
            newY++;
        }
    }

    // The for loop right after this if condition will copy entire rows of attributes at a time.
    // This assumes of course that the "write cursor" (newX, newY) is at the start of a row.
    // If we didn't check for this, we may otherwise copy attributes from a later row into a previous one.
    if (newX != 0)
    {
        newX = 0;
        newY++;
    }

    // Finish copying buffer attributes to remaining rows below the last
    // printable character. This is to fix the `color 2f` scenario, where you
    // change the buffer colors then resize and everything below the last
    // printable char gets reset. See GH #12567
    const auto initializedRowsEnd = oldBuffer._estimateOffsetOfLastCommittedRow() + 1;
    for (; oldY < initializedRowsEnd && newY < newHeight; oldY++, newY++)
    {
        auto& oldRow = oldBuffer.GetRowByOffset(oldY);
        auto& newRow = newBuffer.GetMutableRowByOffset(newY);
        auto& newAttr = newRow.Attributes();
        newAttr = oldRow.Attributes();
        newAttr.resize_trailing_extent(newWidthU16);
    }

    // Since we didn't use IncrementCircularBuffer() we need to compute the proper
    // _firstRow offset now, in a way that replicates IncrementCircularBuffer().
    // We need to do the same for newCursorPos.y for basically the same reason.
    if (newY > newHeight)
    {
        newBuffer._firstRow = newY % newHeight;
        // _firstRow maps from API coordinates that always start at 0,0 in the top left corner of the
        // terminal's scrollback, to the underlying buffer Y coordinate via `(y + _firstRow) % height`.
        // Here, we need to un-map the `newCursorPos.y` from the underlying Y coordinate to the API coordinate
        // and so we do `(y - _firstRow) % height`, but we add `+ newHeight` to avoid getting negative results.
        newCursorPos.y = (newCursorPos.y - newBuffer._firstRow + newHeight) % newHeight;
    }

    newBuffer.CopyProperties(oldBuffer);
    newBuffer.CopyHyperlinkMaps(oldBuffer);

    assert(newCursorPos.x >= 0 && newCursorPos.x < newWidth);
    assert(newCursorPos.y >= 0 && newCursorPos.y < newHeight);
    newCursor.SetSize(oldCursor.GetSize());
    newCursor.SetPosition(newCursorPos);
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

// Searches through the entire (committed) text buffer for `needle` and returns the coordinates in absolute coordinates.
// The end coordinates of the returned ranges are considered inclusive.
std::optional<std::vector<til::point_span>> TextBuffer::SearchText(const std::wstring_view& needle, SearchFlag flags) const
{
    return SearchText(needle, flags, 0, til::CoordTypeMax);
}

// Searches through the given rows [rowBeg,rowEnd) for `needle` and returns the coordinates in absolute coordinates.
// While the end coordinates of the returned ranges are considered inclusive, the [rowBeg,rowEnd) range is half-open.
// Returns nullopt if the parameters were invalid (e.g. regex search was requested with an invalid regex)
std::optional<std::vector<til::point_span>> TextBuffer::SearchText(const std::wstring_view& needle, SearchFlag flags, til::CoordType rowBeg, til::CoordType rowEnd) const
{
    rowEnd = std::min(rowEnd, _estimateOffsetOfLastCommittedRow() + 1);

    std::vector<til::point_span> results;

    // All whitespace strings would match the not-yet-written parts of the TextBuffer which would be weird.
    if (allWhitespace(needle) || rowBeg >= rowEnd)
    {
        return results;
    }

    auto text = ICU::UTextFromTextBuffer(*this, rowBeg, rowEnd);

    uint32_t icuFlags{ 0 };
    WI_SetFlagIf(icuFlags, UREGEX_CASE_INSENSITIVE, WI_IsFlagSet(flags, SearchFlag::CaseInsensitive));

    if (WI_IsFlagSet(flags, SearchFlag::RegularExpression))
    {
        WI_SetFlag(icuFlags, UREGEX_MULTILINE);
    }
    else
    {
        WI_SetFlag(icuFlags, UREGEX_LITERAL);
    }

    UErrorCode status = U_ZERO_ERROR;
    const auto re = ICU::CreateRegex(needle, icuFlags, &status);
    if (status > U_ZERO_ERROR)
    {
        return std::nullopt;
    }

    uregex_setUText(re.get(), &text, &status);

    if (uregex_find(re.get(), -1, &status))
    {
        do
        {
            results.emplace_back(ICU::BufferRangeFromMatch(&text, re.get()));
        } while (uregex_findNext(re.get(), &status));
    }

    return results;
}

// Collect up all the rows that were marked, and the data marked on that row.
// This is what should be used for hot paths, like updating the scrollbar.
std::vector<ScrollMark> TextBuffer::GetMarkRows() const
{
    std::vector<ScrollMark> marks;
    const auto bottom = _estimateOffsetOfLastCommittedRow();
    for (auto y = 0; y <= bottom; y++)
    {
        const auto& row = GetRowByOffset(y);
        const auto& data{ row.GetScrollbarData() };
        if (data.has_value())
        {
            marks.emplace_back(y, *data);
        }
    }
    return marks;
}

// Get all the regions for all the shell integration marks in the buffer.
// Marks will be returned in top-down order.
//
// This possibly iterates over every run in the buffer, so don't do this on a
// hot path. Just do this once per user input, if at all possible.
//
// Use `limit` to control how many you get, _starting from the bottom_. (e.g.
// limit=1 will just give you the "most recent mark").
std::vector<MarkExtents> TextBuffer::GetMarkExtents(size_t limit) const
{
    if (limit == 0u)
    {
        return {};
    }

    std::vector<MarkExtents> marks{};
    const auto bottom = _estimateOffsetOfLastCommittedRow();
    auto lastPromptY = bottom;
    for (auto promptY = bottom; promptY >= 0; promptY--)
    {
        const auto& currRow = GetRowByOffset(promptY);
        auto& rowPromptData = currRow.GetScrollbarData();
        if (!rowPromptData.has_value())
        {
            // This row didn't start a prompt, don't even look here.
            continue;
        }

        // Future thought! In #11000 & #14792, we considered the possibility of
        // scrolling to only an error mark, or something like that. Perhaps in
        // the future, add a customizable filter that's a set of types of mark
        // to include?
        //
        // For now, skip any "Default" marks, since those came from the UI. We
        // just want the ones that correspond to shell integration.

        if (rowPromptData->category == MarkCategory::Default)
        {
            continue;
        }

        // This row did start a prompt! Find the prompt that starts here.
        // Presumably, no rows below us will have prompts, so pass in the last
        // row with text as the bottom
        marks.push_back(_scrollMarkExtentForRow(promptY, lastPromptY));

        // operator>=(T, optional<U>) will return true if the optional is
        // nullopt, unfortunately.
        if (marks.size() >= limit)
        {
            break;
        }

        lastPromptY = promptY;
    }

    std::reverse(marks.begin(), marks.end());
    return marks;
}

// Remove all marks between `start` & `end`, inclusive.
void TextBuffer::ClearMarksInRange(
    const til::point start,
    const til::point end)
{
    auto top = std::clamp(std::min(start.y, end.y), 0, _height - 1);
    auto bottom = std::clamp(std::max(start.y, end.y), 0, _estimateOffsetOfLastCommittedRow());

    for (auto y = top; y <= bottom; y++)
    {
        auto& row = GetMutableRowByOffset(y);
        auto& runs = row.Attributes().runs();
        row.SetScrollbarData(std::nullopt);
        for (auto& [attr, length] : runs)
        {
            attr.SetMarkAttributes(MarkKind::None);
        }
    }
}
void TextBuffer::ClearAllMarks()
{
    ClearMarksInRange({ 0, 0 }, { _width - 1, _height - 1 });
}

// Collect up the extent of the prompt and possibly command and output for the
// mark that starts on this row.
MarkExtents TextBuffer::_scrollMarkExtentForRow(const til::CoordType rowOffset,
                                                const til::CoordType bottomInclusive) const
{
    const auto& startRow = GetRowByOffset(rowOffset);
    const auto& rowPromptData = startRow.GetScrollbarData();
    assert(rowPromptData.has_value());

    MarkExtents mark{
        .data = *rowPromptData,
    };

    bool startedPrompt = false;
    bool startedCommand = false;
    bool startedOutput = false;
    MarkKind lastMarkKind = MarkKind::Output;

    const auto endThisMark = [&](auto x, auto y) {
        if (startedOutput)
        {
            mark.outputEnd = til::point{ x, y };
        }
        if (!startedOutput && startedCommand)
        {
            mark.commandEnd = til::point{ x, y };
        }
        if (!startedCommand)
        {
            mark.end = til::point{ x, y };
        }
    };
    auto x = 0;
    auto y = rowOffset;
    til::point lastMarkedText{ x, y };
    for (; y <= bottomInclusive; y++)
    {
        // Now we need to iterate over text attributes. We need to find a
        // segment of Prompt attributes, we'll skip those. Then there should be
        // Command attributes. Collect up all of those, till we get to the next
        // Output attribute.

        const auto& row = GetRowByOffset(y);
        const auto runs = row.Attributes().runs();
        x = 0;
        for (const auto& [attr, length] : runs)
        {
            const auto nextX = gsl::narrow_cast<uint16_t>(x + length);
            const auto markKind{ attr.GetMarkAttributes() };

            if (markKind != MarkKind::None)
            {
                lastMarkedText = { nextX, y };

                if (markKind == MarkKind::Prompt)
                {
                    if (startedCommand || startedOutput)
                    {
                        // we got a _new_ prompt. bail out.
                        break;
                    }
                    if (!startedPrompt)
                    {
                        // We entered the first prompt here
                        startedPrompt = true;
                        mark.start = til::point{ x, y };
                    }
                    endThisMark(lastMarkedText.x, lastMarkedText.y);
                }
                else if (markKind == MarkKind::Command && startedPrompt)
                {
                    startedCommand = true;
                    endThisMark(lastMarkedText.x, lastMarkedText.y);
                }
                else if ((markKind == MarkKind::Output) && startedPrompt)
                {
                    startedOutput = true;
                    if (!mark.commandEnd.has_value())
                    {
                        // immediately just end the command at the start here, so we can treat this whole run as output
                        mark.commandEnd = mark.end;
                        startedCommand = true;
                    }

                    endThisMark(lastMarkedText.x, lastMarkedText.y);
                }
                // Otherwise, we've changed from any state -> any state, and it doesn't really matter.
                lastMarkKind = markKind;
            }
            // advance to next run of text
            x = nextX;
        }
        // we went over all the runs in this row, but we're not done yet. Keep iterating on the next row.
    }

    // Okay, we're at the bottom of the buffer? Yea, just return what we found.
    if (!startedCommand)
    {
        // If we never got to a Command or Output run, then we never set .end.
        // Set it here to the last run we saw.
        endThisMark(lastMarkedText.x, lastMarkedText.y);
    }
    return mark;
}

std::wstring TextBuffer::_commandForRow(const til::CoordType rowOffset,
                                        const til::CoordType bottomInclusive,
                                        const bool clipAtCursor) const
{
    std::wstring commandBuilder;
    MarkKind lastMarkKind = MarkKind::Prompt;
    const auto cursorPosition = GetCursor().GetPosition();
    for (auto y = rowOffset; y <= bottomInclusive; y++)
    {
        const bool onCursorRow = clipAtCursor && y == cursorPosition.y;
        // Now we need to iterate over text attributes. We need to find a
        // segment of Prompt attributes, we'll skip those. Then there should be
        // Command attributes. Collect up all of those, till we get to the next
        // Output attribute.
        const auto& row = GetRowByOffset(y);
        const auto runs = row.Attributes().runs();
        auto x = 0;
        for (const auto& [attr, length] : runs)
        {
            auto nextX = gsl::narrow_cast<uint16_t>(x + length);
            if (onCursorRow)
            {
                nextX = std::min(nextX, gsl::narrow_cast<uint16_t>(cursorPosition.x));
            }
            const auto markKind{ attr.GetMarkAttributes() };
            if (markKind != lastMarkKind)
            {
                if (lastMarkKind == MarkKind::Command)
                {
                    // We've changed away from being in a command. We're done.
                    // Return what we've gotten so far.
                    return commandBuilder;
                }
                // Otherwise, we've changed from any state -> any state, and it doesn't really matter.
                lastMarkKind = markKind;
            }

            if (markKind == MarkKind::Command)
            {
                commandBuilder += row.GetText(x, nextX);
            }
            // advance to next run of text
            x = nextX;
            if (onCursorRow && x == cursorPosition.x)
            {
                return commandBuilder;
            }
        }
        // we went over all the runs in this row, but we're not done yet. Keep iterating on the next row.
    }
    // Okay, we're at the bottom of the buffer? Yea, just return what we found.
    return commandBuilder;
}

std::wstring TextBuffer::CurrentCommand() const
{
    auto promptY = GetCursor().GetPosition().y;
    for (; promptY >= 0; promptY--)
    {
        const auto& currRow = GetRowByOffset(promptY);
        auto& rowPromptData = currRow.GetScrollbarData();
        if (!rowPromptData.has_value())
        {
            // This row didn't start a prompt, don't even look here.
            continue;
        }

        // This row did start a prompt! Find the prompt that starts here.
        // Presumably, no rows below us will have prompts, so pass in the last
        // row with text as the bottom
        return _commandForRow(promptY, _estimateOffsetOfLastCommittedRow(), true);
    }
    return L"";
}

std::vector<std::wstring> TextBuffer::Commands() const
{
    std::vector<std::wstring> commands{};
    const auto bottom = _estimateOffsetOfLastCommittedRow();
    auto lastPromptY = bottom;
    for (auto promptY = bottom; promptY >= 0; promptY--)
    {
        const auto& currRow = GetRowByOffset(promptY);
        auto& rowPromptData = currRow.GetScrollbarData();
        if (!rowPromptData.has_value())
        {
            // This row didn't start a prompt, don't even look here.
            continue;
        }

        // This row did start a prompt! Find the prompt that starts here.
        // Presumably, no rows below us will have prompts, so pass in the last
        // row with text as the bottom
        auto foundCommand = _commandForRow(promptY, lastPromptY);
        if (!foundCommand.empty())
        {
            commands.emplace_back(std::move(foundCommand));
        }
        lastPromptY = promptY;
    }
    std::reverse(commands.begin(), commands.end());
    return commands;
}

void TextBuffer::StartPrompt()
{
    const auto currentRowOffset = GetCursor().GetPosition().y;
    auto& currentRow = GetMutableRowByOffset(currentRowOffset);
    currentRow.StartPrompt();

    _currentAttributes.SetMarkAttributes(MarkKind::Prompt);
}

bool TextBuffer::_createPromptMarkIfNeeded()
{
    // We might get here out-of-order, without seeing a StartPrompt (FTCS A)
    // first. Since StartPrompt actually sets up the prompt mark on the ROW, we
    // need to do a bit of extra work here to start a new mark (if the last one
    // wasn't in an appropriate state).

    const auto mostRecentMarks = GetMarkExtents(1u);
    if (!mostRecentMarks.empty())
    {
        const auto& mostRecentMark = til::at(mostRecentMarks, 0);
        if (!mostRecentMark.HasOutput())
        {
            // The most recent command mark _didn't_ have output yet. Great!
            // we'll leave it alone, and just start treating text as Command or Output.
            return false;
        }

        // The most recent command mark had output. That suggests that either:
        // * shell integration wasn't enabled (but the user would still
        //   like lines with enters to be marked as prompts)
        // * or we're in the middle of a command that's ongoing.

        // If it does have a command, then we're still in the output of
        // that command.
        //   --> the current attrs should already be set to Output.
        if (mostRecentMark.HasCommand())
        {
            return false;
        }
        // If the mark doesn't have any command - then we know we're
        // playing silly games with just marking whole lines as prompts,
        // then immediately going to output.
        //   --> Below, we'll add a new mark to this row.
    }

    // There were no marks at all!
    //   --> add a new mark to this row, set all the attrs in this row
    //   to be Prompt, and set the current attrs to Output.

    auto& row = GetMutableRowByOffset(GetCursor().GetPosition().y);
    row.StartPrompt();
    return true;
}

bool TextBuffer::StartCommand()
{
    const auto createdMark = _createPromptMarkIfNeeded();
    _currentAttributes.SetMarkAttributes(MarkKind::Command);
    return createdMark;
}
bool TextBuffer::StartOutput()
{
    const auto createdMark = _createPromptMarkIfNeeded();
    _currentAttributes.SetMarkAttributes(MarkKind::Output);
    return createdMark;
}

// Find the row above the cursor where this most recent prompt started, and set
// the exit code on that row's scroll mark.
void TextBuffer::EndCurrentCommand(std::optional<unsigned int> error)
{
    _currentAttributes.SetMarkAttributes(MarkKind::None);

    for (auto y = GetCursor().GetPosition().y; y >= 0; y--)
    {
        auto& currRow = GetMutableRowByOffset(y);
        auto& rowPromptData = currRow.GetScrollbarData();
        if (rowPromptData.has_value())
        {
            currRow.EndOutput(error);
            return;
        }
    }
}

void TextBuffer::SetScrollbarData(ScrollbarData mark, til::CoordType y)
{
    auto& row = GetMutableRowByOffset(y);
    row.SetScrollbarData(mark);
}
void TextBuffer::ManuallyMarkRowAsPrompt(til::CoordType y)
{
    auto& row = GetMutableRowByOffset(y);
    for (auto& [attr, len] : row.Attributes().runs())
    {
        attr.SetMarkAttributes(MarkKind::Prompt);
    }
}
