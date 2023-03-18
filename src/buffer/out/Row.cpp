// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "Row.hpp"

#include <til/unicode.h>

#include "textBuffer.hpp"
#include "../../types/inc/GlyphWidth.hpp"

// The STL is missing a std::iota_n analogue for std::iota, so I made my own.
template<typename OutIt, typename Diff, typename T>
constexpr OutIt iota_n(OutIt dest, Diff count, T val)
{
    for (; count; --count, ++dest, ++val)
    {
        *dest = val;
    }
    return dest;
}

// ROW::ReplaceCharacters needs to calculate `val + count` after
// calling iota_n() and this function achieves both things at once.
template<typename OutIt, typename Diff, typename T>
constexpr OutIt iota_n_mut(OutIt dest, Diff count, T& val)
{
    for (; count; --count, ++dest, ++val)
    {
        *dest = val;
    }
    return dest;
}

// Same as std::fill, but purpose-built for very small `last - first`
// where a trivial loop outperforms vectorization.
template<typename FwdIt, typename T>
constexpr FwdIt fill_small(FwdIt first, FwdIt last, const T val)
{
    for (; first != last; ++first)
    {
        *first = val;
    }
    return first;
}

// Same as std::fill_n, but purpose-built for very small `count`
// where a trivial loop outperforms vectorization.
template<typename OutIt, typename Diff, typename T>
constexpr OutIt fill_n_small(OutIt dest, Diff count, const T val)
{
    for (; count; --count, ++dest)
    {
        *dest = val;
    }
    return dest;
}

// Same as std::copy_n, but purpose-built for very short `count`
// where a trivial loop outperforms vectorization.
template<typename InIt, typename Diff, typename OutIt>
constexpr OutIt copy_n_small(InIt first, Diff count, OutIt dest)
{
    for (; count; --count, ++dest, ++first)
    {
        *dest = *first;
    }
    return dest;
}

RowTextIterator::RowTextIterator(std::span<const wchar_t> chars, std::span<const uint16_t> charOffsets, uint16_t offset) noexcept :
    _chars{ chars },
    _charOffsets{ charOffsets },
    _beg{ offset },
    _end{ offset }
{
    operator++();
}

bool RowTextIterator::operator==(const RowTextIterator& other) const noexcept
{
    return _beg == other._beg;
}

RowTextIterator& RowTextIterator::operator++() noexcept
{
    _beg = _end;
    while (++_end < _charOffsets.size() && _uncheckedIsTrailer(_end))
    {
    }
    return *this;
}

const RowTextIterator& RowTextIterator::operator*() const noexcept
{
    return *this;
}

std::wstring_view RowTextIterator::Text() const noexcept
{
    const auto beg = _uncheckedCharOffset(_beg);
    const auto end = _uncheckedCharOffset(_end);
    return { _chars.begin() + beg, _chars.begin() + gsl::narrow_cast<size_t>(end - beg) };
}

til::CoordType RowTextIterator::Cols() const noexcept
{
    return _end - _beg;
}

DbcsAttribute RowTextIterator::DbcsAttr() const noexcept
{
    return Cols() == 2 ? DbcsAttribute::Leading : DbcsAttribute::Single;
}

// Safety: col must be [0, _columnCount].
uint16_t RowTextIterator::_uncheckedCharOffset(size_t col) const noexcept
{
    assert(col <= _charOffsets.size());
    return til::at(_charOffsets, col) & CharOffsetsMask;
}

// Safety: col must be [0, _columnCount].
bool RowTextIterator::_uncheckedIsTrailer(size_t col) const noexcept
{
    assert(col <= _charOffsets.size());
    return WI_IsFlagSet(til::at(_charOffsets, col), CharOffsetsTrailer);
}

// Routine Description:
// - constructor
// Arguments:
// - rowWidth - the width of the row, cell elements
// - fillAttribute - the default text attribute
// Return Value:
// - constructed object
ROW::ROW(wchar_t* charsBuffer, uint16_t* charOffsetsBuffer, uint16_t rowWidth, const TextAttribute& fillAttribute) :
    _charsBuffer{ charsBuffer },
    _chars{ charsBuffer, rowWidth },
    _charOffsets{ charOffsetsBuffer, ::base::strict_cast<size_t>(rowWidth) + 1u },
    _attr{ rowWidth, fillAttribute },
    _columnCount{ rowWidth }
{
    if (_chars.data())
    {
        _init();
    }
}

void swap(ROW& lhs, ROW& rhs) noexcept
{
    std::swap(lhs._charsBuffer, rhs._charsBuffer);
    std::swap(lhs._charsHeap, rhs._charsHeap);
    std::swap(lhs._chars, rhs._chars);
    std::swap(lhs._charOffsets, rhs._charOffsets);
    std::swap(lhs._attr, rhs._attr);
    std::swap(lhs._columnCount, rhs._columnCount);
    std::swap(lhs._lineRendition, rhs._lineRendition);
    std::swap(lhs._wrapForced, rhs._wrapForced);
    std::swap(lhs._doubleBytePadded, rhs._doubleBytePadded);
}

void ROW::SetWrapForced(const bool wrap) noexcept
{
    _wrapForced = wrap;
}

bool ROW::WasWrapForced() const noexcept
{
    return _wrapForced;
}

void ROW::SetDoubleBytePadded(const bool doubleBytePadded) noexcept
{
    _doubleBytePadded = doubleBytePadded;
}

bool ROW::WasDoubleBytePadded() const noexcept
{
    return _doubleBytePadded;
}

void ROW::SetLineRendition(const LineRendition lineRendition) noexcept
{
    _lineRendition = lineRendition;
}

LineRendition ROW::GetLineRendition() const noexcept
{
    return _lineRendition;
}

RowTextIterator ROW::Begin() const noexcept
{
    return { _chars, _charOffsets, 0 };
}

RowTextIterator ROW::End() const noexcept
{
    return { _chars, _charOffsets, _columnCount };
}

// Routine Description:
// - Sets all properties of the ROW to default values
// Arguments:
// - Attr - The default attribute (color) to fill
// Return Value:
// - <none>
void ROW::Reset(const TextAttribute& attr)
{
    _charsHeap.reset();
    _chars = { _charsBuffer, _columnCount };
    _attr = { _columnCount, attr };
    _lineRendition = LineRendition::SingleWidth;
    _wrapForced = false;
    _doubleBytePadded = false;
    _init();
}

void ROW::_init() noexcept
{
    std::fill_n(_chars.begin(), _columnCount, UNICODE_SPACE);
    std::iota(_charOffsets.begin(), _charOffsets.end(), uint16_t{ 0 });
}

// Routine Description:
// - resizes ROW to new width
// Arguments:
// - charsBuffer - a new backing buffer to use for _charsBuffer
// - charOffsetsBuffer - a new backing buffer to use for _charOffsets
// - rowWidth - the new width, in cells
// - fillAttribute - the attribute to use for any newly added, trailing cells
void ROW::Resize(wchar_t* charsBuffer, uint16_t* charOffsetsBuffer, uint16_t rowWidth, const TextAttribute& fillAttribute)
{
    // A default-constructed ROW has no cols/chars to copy.
    // It can be detected by the lack of a _charsBuffer (among others).
    //
    // Otherwise, this block figures out how much we can copy into the new `rowWidth`.
    uint16_t colsToCopy = 0;
    uint16_t charsToCopy = 0;
    if (_charsBuffer)
    {
        colsToCopy = std::min(rowWidth, _columnCount);
        // Safety: colsToCopy is [0, _columnCount].
        charsToCopy = _uncheckedCharOffset(colsToCopy);
        // Safety: colsToCopy is [0, _columnCount] due to colsToCopy != 0.
        for (; colsToCopy != 0 && _uncheckedIsTrailer(colsToCopy); --colsToCopy)
        {
        }
    }

    // If we grow the row width, we have to append a bunch of whitespace.
    // `trailingWhitespace` stores that amount.
    // Safety: The preceding block left colsToCopy in the range [0, rowWidth].
    const uint16_t trailingWhitespace = rowWidth - colsToCopy;

    // Allocate memory for the new `_chars` array.
    // Use the provided charsBuffer if possible, otherwise allocate a `_charsHeap`.
    std::unique_ptr<wchar_t[]> charsHeap;
    std::span chars{ charsBuffer, rowWidth };
    const std::span charOffsets{ charOffsetsBuffer, ::base::strict_cast<size_t>(rowWidth) + 1u };
    if (const uint16_t charsCapacity = charsToCopy + trailingWhitespace; charsCapacity > rowWidth)
    {
        charsHeap = std::make_unique_for_overwrite<wchar_t[]>(charsCapacity);
        chars = { charsHeap.get(), charsCapacity };
    }

    // Copy chars and charOffsets over.
    {
        const auto it = std::copy_n(_chars.begin(), charsToCopy, chars.begin());
        std::fill_n(it, trailingWhitespace, L' ');
    }
    {
        const auto it = std::copy_n(_charOffsets.begin(), colsToCopy, charOffsets.begin());
        // The _charOffsets array is 1 wider than newWidth indicates.
        // This is because the extra column contains the past-the-end index into _chars.
        iota_n(it, trailingWhitespace + 1u, charsToCopy);
    }

    _charsBuffer = charsBuffer;
    _charsHeap = std::move(charsHeap);
    _chars = chars;
    _charOffsets = charOffsets;
    _columnCount = rowWidth;

    // .resize_trailing_extent() doesn't work if the vector is empty,
    // since there's no trailing item that could be extended.
    if (_attr.empty())
    {
        _attr = { rowWidth, fillAttribute };
    }
    else
    {
        _attr.resize_trailing_extent(rowWidth);
    }
}

void ROW::TransferAttributes(const til::small_rle<TextAttribute, uint16_t, 1>& attr, til::CoordType newWidth)
{
    _attr = attr;
    _attr.resize_trailing_extent(gsl::narrow<uint16_t>(newWidth));
}

til::CoordType ROW::NavigateToPrevious(til::CoordType column) const noexcept
{
    return _adjustBackward(_clampedColumn(column - 1));
}

til::CoordType ROW::NavigateToNext(til::CoordType column) const noexcept
{
    return _adjustForward(_clampedColumn(column + 1));
}

uint16_t ROW::_adjustBackward(uint16_t column) const noexcept
{
    // Safety: This is a little bit more dangerous. The first column is supposed
    // to never be a trailer and so this loop should exit if column == 0.
    for (; _uncheckedIsTrailer(column); --column)
    {
    }
    return column;
}

uint16_t ROW::_adjustForward(uint16_t column) const noexcept
{
    // Safety: This is a little bit more dangerous. The last column is supposed
    // to never be a trailer and so this loop should exit if column == _columnCount.
    for (; _uncheckedIsTrailer(column); ++column)
    {
    }
    return column;
}

// Routine Description:
// - clears char data in column in row
// Arguments:
// - column - 0-indexed column index
// Return Value:
// - <none>
void ROW::ClearCell(const til::CoordType column)
{
    static constexpr std::wstring_view space{ L" " };
    ReplaceCharacters(column, 1, space);
}

// Routine Description:
// - writes cell data to the row
// Arguments:
// - it - custom console iterator to use for seeking input data. bool() false when it becomes invalid while seeking.
// - index - column in row to start writing at
// - wrap - change the wrap flag if we hit the end of the row while writing and there's still more data in the iterator.
// - limitRight - right inclusive column ID for the last write in this row. (optional, will just write to the end of row if nullopt)
// Return Value:
// - iterator to first cell that was not written to this row.
OutputCellIterator ROW::WriteCells(OutputCellIterator it, const til::CoordType columnBegin, const std::optional<bool> wrap, std::optional<til::CoordType> limitRight)
{
    THROW_HR_IF(E_INVALIDARG, columnBegin >= size());
    THROW_HR_IF(E_INVALIDARG, limitRight.value_or(0) >= size());

    // If we're given a right-side column limit, use it. Otherwise, the write limit is the final column index available in the char row.
    const auto finalColumnInRow = limitRight.value_or(size() - 1);

    auto currentColor = it->TextAttr();
    uint16_t colorUses = 0;
    auto colorStarts = gsl::narrow_cast<uint16_t>(columnBegin);
    auto currentIndex = colorStarts;

    while (it && currentIndex <= finalColumnInRow)
    {
        // Fill the color if the behavior isn't set to keeping the current color.
        if (it->TextAttrBehavior() != TextAttributeBehavior::Current)
        {
            // If the color of this cell is the same as the run we're currently on,
            // just increment the counter.
            if (currentColor == it->TextAttr())
            {
                ++colorUses;
            }
            else
            {
                // Otherwise, commit this color into the run and save off the new one.
                // Now commit the new color runs into the attr row.
                _attr.replace(colorStarts, currentIndex, currentColor);
                currentColor = it->TextAttr();
                colorUses = 1;
                colorStarts = currentIndex;
            }
        }

        // Fill the text if the behavior isn't set to saying there's only a color stored in this iterator.
        if (it->TextAttrBehavior() != TextAttributeBehavior::StoredOnly)
        {
            const auto fillingFirstColumn = currentIndex == 0;
            const auto fillingLastColumn = currentIndex == finalColumnInRow;
            const auto attr = it->DbcsAttr();
            const auto& chars = it->Chars();

            switch (attr)
            {
            case DbcsAttribute::Leading:
                if (fillingLastColumn)
                {
                    // The wide char doesn't fit. Pad with whitespace.
                    // Don't increment the iterator. Instead we'll return from this function and the
                    // caller can call WriteCells() again on the next row with the same iterator position.
                    ClearCell(currentIndex);
                    SetDoubleBytePadded(true);
                }
                else
                {
                    ReplaceCharacters(currentIndex, 2, chars);
                    ++it;
                }
                break;
            case DbcsAttribute::Trailing:
                if (fillingFirstColumn)
                {
                    // The wide char doesn't fit. Pad with whitespace.
                    // Ignore the character. There's no correct alternative way to handle this situation.
                    ClearCell(currentIndex);
                }
                else if (it.Position() == 0)
                {
                    // A common way to back up and restore the buffer is via `ReadConsoleOutputW` and
                    // `WriteConsoleOutputW` respectively. But the area might bisect/intersect/clip wide characters and
                    // only backup either their leading or trailing half. In general, in the rest of conhost, we're
                    // throwing away the trailing half of all `CHAR_INFO`s (during text rendering, as well as during
                    // `ReadConsoleOutputW`), so to make this code behave the same and prevent surprises, we need to
                    // make sure to only look at the trailer if it's the first `CHAR_INFO` the user is trying to write.
                    ReplaceCharacters(currentIndex - 1, 2, chars);
                }
                ++it;
                break;
            default:
                ReplaceCharacters(currentIndex, 1, chars);
                ++it;
                break;
            }

            // If we're asked to (un)set the wrap status and we just filled the last column with some text...
            // NOTE:
            //  - wrap = std::nullopt    --> don't change the wrap value
            //  - wrap = true            --> we're filling cells as a steam, consider this a wrap
            //  - wrap = false           --> we're filling cells as a block, unwrap
            if (wrap.has_value() && fillingLastColumn)
            {
                // set wrap status on the row to parameter's value.
                SetWrapForced(*wrap);
            }
        }
        else
        {
            ++it;
        }

        // Move to the next cell for the next time through the loop.
        ++currentIndex;
    }

    // Now commit the final color into the attr row
    if (colorUses)
    {
        _attr.replace(colorStarts, currentIndex, currentColor);
    }

    return it;
}

bool ROW::SetAttrToEnd(const til::CoordType columnBegin, const TextAttribute attr)
{
    _attr.replace(_clampedColumnInclusive(columnBegin), _attr.size(), attr);
    return true;
}

void ROW::ReplaceAttributes(const til::CoordType beginIndex, const til::CoordType endIndex, const TextAttribute& newAttr)
{
    _attr.replace(_clampedColumnInclusive(beginIndex), _clampedColumnInclusive(endIndex), newAttr);
}

[[msvc::forceinline]] ROW::WriteHelper::WriteHelper(ROW& row, til::CoordType columnBegin, til::CoordType columnLimit, const std::wstring_view& chars) noexcept :
    row{ row },
    chars{ chars }
{
    colBeg = row._clampedColumnInclusive(columnBegin);
    colLimit = row._clampedColumnInclusive(columnLimit);
    chBegDirty = row._uncheckedCharOffset(colBeg);
    colBegDirty = row._adjustBackward(colBeg);
    chBeg = chBegDirty + colBeg - colBegDirty;
    colEnd = colBeg;
    colEndDirty = 0;
    charsConsumed = 0;
}

[[msvc::forceinline]] bool ROW::WriteHelper::IsValid() const noexcept
{
    return colBeg < colLimit && !chars.empty();
}

void ROW::ReplaceCharacters(til::CoordType columnBegin, til::CoordType width, const std::wstring_view& chars)
try
{
    WriteHelper h{ *this, columnBegin, _columnCount, chars };
    if (!h.IsValid())
    {
        return;
    }
    h.ReplaceCharacters(width);
    h.Finish();
}
catch (...)
{
    // Due to this function writing _charOffsets first, then calling _resizeChars (which may throw) and only then finally
    // filling in _chars, we might end up in a situation were _charOffsets contains offsets outside of the _chars array.
    // --> Restore this row to a known "okay"-state.
    Reset(TextAttribute{});
    throw;
}

[[msvc::forceinline]] void ROW::WriteHelper::ReplaceCharacters(til::CoordType width) noexcept
{
    const auto colEndNew = gsl::narrow_cast<uint16_t>(colEnd + width);
    if (colEndNew > colLimit)
    {
        colEndDirty = colLimit;
    }
    else
    {
        til::at(row._charOffsets, colEnd++) = chBeg;
        for (; colEnd < colEndNew; ++colEnd)
        {
            til::at(row._charOffsets, colEnd) = gsl::narrow_cast<uint16_t>(chBeg | CharOffsetsTrailer);
        }

        colEndDirty = colEnd;
        charsConsumed = chars.size();
    }
}

til::CoordType ROW::ReplaceText(til::CoordType columnBegin, til::CoordType columnLimit, std::wstring_view& chars)
try
{
    WriteHelper h{ *this, columnBegin, columnLimit, chars };
    if (!h.IsValid())
    {
        return h.colBeg;
    }
    h.ReplaceText();
    h.Finish();

    chars = chars.substr(h.charsConsumed);
    return h.colEndDirty;
}
catch (...)
{
    Reset(TextAttribute{});
    throw;
}

[[msvc::forceinline]] void ROW::WriteHelper::ReplaceText() noexcept
{
    size_t ch = chBeg;

    for (const auto& s : til::utf16_iterator{ chars })
    {
        const auto wide = til::at(s, 0) < 0x80 ? false : IsGlyphFullWidth(s);
        const auto colEndNew = gsl::narrow_cast<uint16_t>(colEnd + 1u + wide);
        if (colEndNew > colLimit)
        {
            colEndDirty = colLimit;
            break;
        }

        til::at(row._charOffsets, colEnd++) = gsl::narrow_cast<uint16_t>(ch);
        if (wide)
        {
            til::at(row._charOffsets, colEnd++) = gsl::narrow_cast<uint16_t>(ch | CharOffsetsTrailer);
        }

        colEndDirty = colEnd;
        ch += s.size();
    }

    charsConsumed = ch - chBeg;
}

til::CoordType ROW::CopyRangeFrom(til::CoordType columnBegin, til::CoordType columnLimit, const ROW& other, til::CoordType& otherBegin, til::CoordType otherLimit)
try
{
    const auto otherColBeg = other._clampedColumnInclusive(otherBegin);
    const auto otherColLimit = other._clampedColumnInclusive(otherLimit);
    std::span<uint16_t> charOffsets;
    std::wstring_view chars;

    if (otherColBeg < otherColLimit)
    {
        charOffsets = other._charOffsets.subspan(otherColBeg, static_cast<size_t>(otherColLimit) - otherColBeg + 1);
        const auto charsOffset = charOffsets.front() & CharOffsetsMask;
        // _chars is a std::span (for performance and because it refers to raw, shared memory), whereas
        // chars is a std::wstring_view, because that's what we use everywhere else for sharing strings.
        // We can't trivially convert the two (C++ Committee be like: Having 2 span types is completely
        // reasonable and normal. Also, they're not convertible. Because fu.) so we do pointer stuff.
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
        chars = { other._chars.data() + charsOffset, other._chars.size() - charsOffset };
    }

    WriteHelper h{ *this, columnBegin, columnLimit, chars };
    if (!h.IsValid())
    {
        return h.colBeg;
    }
    // Any valid charOffsets array is at least 2 elements long (the 1st element is the start offset and the 2nd
    // element is the length of the first glyph) and begins/ends with a non-trailer offset. We don't really
    // need to test for the end offset, since `WriteHelper::WriteWithOffsets` already takes care of that.
    if (charOffsets.size() < 2 || WI_IsFlagSet(charOffsets.front(), CharOffsetsTrailer))
    {
        assert(false);
        otherBegin = other.size();
        return h.colBeg;
    }
    h.CopyRangeFrom(charOffsets);
    h.Finish();

    otherBegin += h.colEnd - h.colBeg;
    return h.colEndDirty;
}
catch (...)
{
    Reset(TextAttribute{});
    throw;
}

[[msvc::forceinline]] void ROW::WriteHelper::CopyRangeFrom(const std::span<const uint16_t>& charOffsets) noexcept
{
    // Since our `charOffsets` input is already in columns (just like the `ROW::_charOffsets`),
    // we can directly look up the end char-offset, but...
    const auto colEndDirtyInput = std::min(gsl::narrow_cast<uint16_t>(colLimit - colBeg), gsl::narrow<uint16_t>(charOffsets.size() - 1));

    // ...since the colLimit might intersect with a wide glyph in `charOffset`, we need to adjust our input-colEnd.
    auto colEndInput = colEndDirtyInput;
    for (; WI_IsFlagSet(til::at(charOffsets, colEndInput), CharOffsetsTrailer); --colEndInput)
    {
    }

    const auto baseOffset = til::at(charOffsets, 0);
    const auto endOffset = til::at(charOffsets, colEndInput);
    const auto inToOutOffset = gsl::narrow_cast<uint16_t>(chBeg - baseOffset);

    // Now with the `colEndInput` figured out, we can easily copy the `charOffsets` into the `_charOffsets`.
    // It's possible to use SIMD for this loop for extra perf gains. Something like this for SSE2 (~8x faster):
    //   const auto in = _mm_loadu_si128(...);
    //   const auto off = _mm_and_epi32(in, _mm_set1_epi16(CharOffsetsMask));
    //   const auto trailer  = _mm_and_epi32(in, _mm_set1_epi16(CharOffsetsTrailer));
    //   const auto out = _mm_or_epi32(_mm_add_epi16(off, _mm_set1_epi16(inToOutOffset)), trailer);
    //   _mm_store_si128(..., out);
    for (uint16_t i = 0; i < colEndInput; ++i, ++colEnd)
    {
        const auto ch = til::at(charOffsets, i);
        const auto off = ch & CharOffsetsMask;
        const auto trailer = ch & CharOffsetsTrailer;
        til::at(row._charOffsets, colEnd) = gsl::narrow_cast<uint16_t>((off + inToOutOffset) | trailer);
    }

    colEndDirty = gsl::narrow_cast<uint16_t>(colBeg + colEndDirtyInput);
    charsConsumed = endOffset - baseOffset;
}

[[msvc::forceinline]] void ROW::WriteHelper::Finish()
{
    colEndDirty = row._adjustForward(colEndDirty);

    const uint16_t leadingSpaces = chBeg - chBegDirty;
    const uint16_t trailingSpaces = colEndDirty - colEnd;
    const auto chEndDirtyOld = row._uncheckedCharOffset(colEndDirty);
    const auto chEndDirty = chBegDirty + charsConsumed + leadingSpaces + trailingSpaces;

    if (chEndDirty != chEndDirtyOld)
    {
        row._resizeChars(colEndDirty, chBegDirty, chEndDirty, chEndDirtyOld);
    }

    {
        // std::copy_n compiles to memmove. We can do better. It also gets rid of an extra branch,
        // because std::copy_n avoids calling memmove if the count is 0. It's never 0 for us.
        const auto itBeg = row._chars.begin() + chBeg;
        memcpy(&*itBeg, chars.data(), charsConsumed * sizeof(wchar_t));

        if (leadingSpaces)
        {
            fill_n_small(row._chars.begin() + chBegDirty, leadingSpaces, L' ');
            iota_n(row._charOffsets.begin() + colBegDirty, leadingSpaces, chBegDirty);
        }
        if (trailingSpaces)
        {
            fill_n_small(itBeg + charsConsumed, trailingSpaces, L' ');
            iota_n(row._charOffsets.begin() + colEnd, trailingSpaces, gsl::narrow_cast<uint16_t>(chBeg + charsConsumed));
        }
    }

    // This updates `_doubleBytePadded` whenever we write the last column in the row. `_doubleBytePadded` tells our text
    // reflow algorithm whether it should ignore the last column. This is important when writing wide characters into
    // the terminal: If the last wide character in a row only fits partially, we should render whitespace, but
    // during text reflow pretend as if no whitespace exists. After all, the user didn't write any whitespace there.
    //
    // The way this is written, it'll set `_doubleBytePadded` to `true` no matter whether a wide character didn't fit,
    // or if the last 2 columns contain a wide character and a narrow character got written into the left half of it.
    // In both cases `trailingSpaces` is 1 and fills the last column and `_doubleBytePadded` will be `true`.
    if (colEndDirty == row._columnCount)
    {
        row.SetDoubleBytePadded(colEnd < row._columnCount);
    }
}

// This function represents the slow path of ReplaceCharacters(),
// as it reallocates the backing buffer and shifts the char offsets.
// The parameters are difficult to explain, but their names are identical to
// local variables in ReplaceCharacters() which I've attempted to document there.
void ROW::_resizeChars(uint16_t colEndDirty, uint16_t chBegDirty, size_t chEndDirty, uint16_t chEndDirtyOld)
{
    const auto diff = chEndDirty - chEndDirtyOld;
    const auto currentLength = _charSize();
    const auto newLength = currentLength + diff;

    if (newLength <= _chars.size())
    {
        std::copy_n(_chars.begin() + chEndDirtyOld, currentLength - chEndDirtyOld, _chars.begin() + chEndDirty);
    }
    else
    {
        const auto minCapacity = std::min<size_t>(UINT16_MAX, _chars.size() + (_chars.size() >> 1));
        const auto newCapacity = gsl::narrow<uint16_t>(std::max(newLength, minCapacity));

        auto charsHeap = std::make_unique_for_overwrite<wchar_t[]>(newCapacity);
        const std::span chars{ charsHeap.get(), newCapacity };

        std::copy_n(_chars.begin(), chBegDirty, chars.begin());
        std::copy_n(_chars.begin() + chEndDirtyOld, currentLength - chEndDirtyOld, chars.begin() + chEndDirty);

        _charsHeap = std::move(charsHeap);
        _chars = chars;
    }

    auto it = _charOffsets.begin() + colEndDirty;
    const auto end = _charOffsets.end();
    for (; it != end; ++it)
    {
        *it = gsl::narrow_cast<uint16_t>(*it + diff);
    }
}

til::small_rle<TextAttribute, uint16_t, 1>& ROW::Attributes() noexcept
{
    return _attr;
}

const til::small_rle<TextAttribute, uint16_t, 1>& ROW::Attributes() const noexcept
{
    return _attr;
}

TextAttribute ROW::GetAttrByColumn(const til::CoordType column) const
{
    return _attr.at(_clampedUint16(column));
}

std::vector<uint16_t> ROW::GetHyperlinks() const
{
    std::vector<uint16_t> ids;
    for (const auto& run : _attr.runs())
    {
        if (run.value.IsHyperlink())
        {
            ids.emplace_back(run.value.GetHyperlinkId());
        }
    }
    return ids;
}

uint16_t ROW::size() const noexcept
{
    return _columnCount;
}

til::CoordType ROW::MeasureLeft() const noexcept
{
    const auto text = GetText();
    const auto beg = text.begin();
    const auto end = text.end();
    auto it = beg;

    for (; it != end; ++it)
    {
        if (*it != L' ')
        {
            break;
        }
    }

    return gsl::narrow_cast<til::CoordType>(it - beg);
}

til::CoordType ROW::MeasureRight() const noexcept
{
    const auto text = GetText();
    const auto beg = text.begin();
    const auto end = text.end();
    auto it = end;

    for (; it != beg; --it)
    {
        // it[-1] is safe as `it` is always greater than `beg` (loop invariant).
        if (til::at(it, -1) != L' ')
        {
            break;
        }
    }

    // We're supposed to return the measurement in cells and not characters
    // and therefore simply calculating `it - beg` would be wrong.
    //
    // An example: The row is 10 cells wide and `it` points to the second character.
    // `it - beg` would return 1, but it's possible it's actually 1 wide glyph and 8 whitespace.
    return gsl::narrow_cast<til::CoordType>(_columnCount - (end - it));
}

bool ROW::ContainsText() const noexcept
{
    const auto text = GetText();
    const auto beg = text.begin();
    const auto end = text.end();
    auto it = beg;

    for (; it != end; ++it)
    {
        if (*it != L' ')
        {
            return true;
        }
    }

    return false;
}

std::wstring_view ROW::GlyphAt(til::CoordType column) const noexcept
{
    auto col = _clampedColumn(column);

    // Safety: col is [0, _columnCount).
    const auto beg = _uncheckedCharOffset(col);
    // Safety: col cannot be incremented past _columnCount, because the last
    // _charOffset at index _columnCount will never get the CharOffsetsTrailer flag.
    while (_uncheckedIsTrailer(++col))
    {
    }
    // Safety: col is now (0, _columnCount].
    const auto end = _uncheckedCharOffset(col);

    return { _chars.begin() + beg, _chars.begin() + end };
}

DbcsAttribute ROW::DbcsAttrAt(til::CoordType column) const noexcept
{
    const auto col = _clampedColumn(column);

    auto attr = DbcsAttribute::Single;
    // Safety: col is [0, _columnCount).
    if (_uncheckedIsTrailer(col))
    {
        attr = DbcsAttribute::Trailing;
    }
    // Safety: col+1 is [1, _columnCount].
    else if (_uncheckedIsTrailer(::base::strict_cast<size_t>(col) + 1u))
    {
        attr = DbcsAttribute::Leading;
    }

    return { attr };
}

std::wstring_view ROW::GetText() const noexcept
{
    return { _chars.data(), _charSize() };
}

DelimiterClass ROW::DelimiterClassAt(til::CoordType column, const std::wstring_view& wordDelimiters) const noexcept
{
    const auto col = _clampedColumn(column);
    // Safety: col is [0, _columnCount).
    const auto glyph = _uncheckedChar(_uncheckedCharOffset(col));

    if (glyph <= L' ')
    {
        return DelimiterClass::ControlChar;
    }
    else if (wordDelimiters.find(glyph) != std::wstring_view::npos)
    {
        return DelimiterClass::DelimiterChar;
    }
    else
    {
        return DelimiterClass::RegularChar;
    }
}

template<typename T>
constexpr uint16_t ROW::_clampedUint16(T v) noexcept
{
    return static_cast<uint16_t>(std::max(T{ 0 }, std::min(T{ 65535 }, v)));
}

template<typename T>
constexpr uint16_t ROW::_clampedColumn(T v) const noexcept
{
    return static_cast<uint16_t>(std::max(T{ 0 }, std::min<T>(_columnCount - 1u, v)));
}

template<typename T>
constexpr uint16_t ROW::_clampedColumnInclusive(T v) const noexcept
{
    return static_cast<uint16_t>(std::max(T{ 0 }, std::min<T>(_columnCount, v)));
}

// Safety: off must be [0, _charSize()].
wchar_t ROW::_uncheckedChar(size_t off) const noexcept
{
    return til::at(_chars, off);
}

uint16_t ROW::_charSize() const noexcept
{
    // Safety: _charOffsets is an array of `_columnCount + 1` entries.
    return til::at(_charOffsets, _columnCount);
}

// Safety: col must be [0, _columnCount].
uint16_t ROW::_uncheckedCharOffset(size_t col) const noexcept
{
    assert(col < _charOffsets.size());
    return til::at(_charOffsets, col) & CharOffsetsMask;
}

// Safety: col must be [0, _columnCount].
bool ROW::_uncheckedIsTrailer(size_t col) const noexcept
{
    assert(col < _charOffsets.size());
    return WI_IsFlagSet(til::at(_charOffsets, col), CharOffsetsTrailer);
}
