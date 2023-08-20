// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "Row.hpp"

#include <isa_availability.h>
#include <til/unicode.h>

#include "textBuffer.hpp"
#include "../../types/inc/GlyphWidth.hpp"

extern "C" int __isa_available;

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
    _init();
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

uint16_t ROW::GetLineWidth() const noexcept
{
    const auto scale = _lineRendition != LineRendition::SingleWidth ? 1 : 0;
    return _columnCount >> scale;
}

// Routine Description:
// - Sets all properties of the ROW to default values
// Arguments:
// - Attr - The default attribute (color) to fill
// Return Value:
// - <none>
void ROW::Reset(const TextAttribute& attr) noexcept
{
    _charsHeap.reset();
    _chars = { _charsBuffer, _columnCount };
    // Constructing and then moving objects into place isn't free.
    // Modifying the existing object is _much_ faster.
    *_attr.runs().unsafe_shrink_to_size(1) = til::rle_pair{ attr, _columnCount };
    _lineRendition = LineRendition::SingleWidth;
    _wrapForced = false;
    _doubleBytePadded = false;
    _init();
}

void ROW::_init() noexcept
{
#pragma warning(push)
#pragma warning(disable : 26462) // The value pointed to by '...' is assigned only once, mark it as a pointer to const (con.4).
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26490) // Don't use reinterpret_cast (type.1).

    // Fills _charsBuffer with whitespace and correspondingly _charOffsets
    // with successive numbers from 0 to _columnCount+1.
#if defined(TIL_SSE_INTRINSICS)
    alignas(__m256i) static constexpr uint16_t whitespaceData[]{ 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20 };
    alignas(__m256i) static constexpr uint16_t offsetsData[]{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    alignas(__m256i) static constexpr uint16_t increment16Data[]{ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16 };
    alignas(__m128i) static constexpr uint16_t increment8Data[]{ 8, 8, 8, 8, 8, 8, 8, 8 };

    // The AVX loop operates on 32 bytes at a minimum. Since _charsBuffer/_charOffsets uses 2 byte large
    // wchar_t/uint16_t respectively, this translates to 16-element writes, which equals a _columnCount of 15,
    // because it doesn't include the past-the-end char-offset as described in the _charOffsets member comment.
    if (__isa_available >= __ISA_AVAILABLE_AVX2 && _columnCount >= 15)
    {
        auto chars = _charsBuffer;
        auto charOffsets = _charOffsets.data();

        // The backing buffer for both chars and charOffsets is guaranteed to be 16-byte aligned,
        // but AVX operations are 32-byte large. As such, when we write out the last chunk, we
        // have to align it to the ends of the 2 buffers. This results in a potential overlap of
        // 16 bytes between the last write in the main loop below and the final write afterwards.
        //
        // An example:
        // If you have a terminal between 16 and 23 columns the buffer has a size of 48 bytes.
        // The main loop below will iterate once, as it writes out bytes 0-31 and then exits.
        // The final write afterwards cannot write bytes 32-63 because that would write
        // out of bounds. Instead it writes bytes 16-47, overwriting 16 overlapping bytes.
        // This is better than branching and switching to SSE2, because both things are slow.
        //
        // Since we want to exit the main loop with at least 1 write left to do as the final write,
        // we need to subtract 1 alignment from the buffer length (= 16 bytes). Since _columnCount is
        // in wchar_t's we subtract -8. The same applies to the ~7 here vs ~15. If you squint slightly
        // you'll see how this is effectively the inverse of what CalculateCharsBufferStride does.
        const auto tailColumnOffset = gsl::narrow_cast<uint16_t>((_columnCount - 8u) & ~7);
        const auto charsEndLoop = chars + tailColumnOffset;
        const auto charOffsetsEndLoop = charOffsets + tailColumnOffset;

        const auto whitespace = _mm256_load_si256(reinterpret_cast<const __m256i*>(&whitespaceData[0]));
        auto offsetsLoop = _mm256_load_si256(reinterpret_cast<const __m256i*>(&offsetsData[0]));
        const auto offsets = _mm256_add_epi16(offsetsLoop, _mm256_set1_epi16(tailColumnOffset));

        if (chars < charsEndLoop)
        {
            const auto increment = _mm256_load_si256(reinterpret_cast<const __m256i*>(&increment16Data[0]));

            do
            {
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(chars), whitespace);
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(charOffsets), offsetsLoop);
                offsetsLoop = _mm256_add_epi16(offsetsLoop, increment);
                chars += 16;
                charOffsets += 16;
            } while (chars < charsEndLoop);
        }

        _mm256_storeu_si256(reinterpret_cast<__m256i*>(charsEndLoop), whitespace);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(charOffsetsEndLoop), offsets);
    }
    else
    {
        auto chars = _charsBuffer;
        auto charOffsets = _charOffsets.data();
        const auto charsEnd = chars + _columnCount;

        const auto whitespace = _mm_load_si128(reinterpret_cast<const __m128i*>(&whitespaceData[0]));
        const auto increment = _mm_load_si128(reinterpret_cast<const __m128i*>(&increment8Data[0]));
        auto offsets = _mm_load_si128(reinterpret_cast<const __m128i*>(&offsetsData[0]));

        do
        {
            _mm_storeu_si128(reinterpret_cast<__m128i*>(chars), whitespace);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(charOffsets), offsets);
            offsets = _mm_add_epi16(offsets, increment);
            chars += 8;
            charOffsets += 8;
            // If _columnCount is something like 120, the actual backing buffer for charOffsets is 121 items large.
            // --> The while loop uses <= to emit at least 1 more write.
        } while (chars <= charsEnd);
    }
#elif defined(TIL_ARM_NEON_INTRINSICS)
    alignas(uint16x8_t) static constexpr uint16_t offsetsData[]{ 0, 1, 2, 3, 4, 5, 6, 7 };

    auto chars = _charsBuffer;
    auto charOffsets = _charOffsets.data();
    const auto charsEnd = chars + _columnCount;

    const auto whitespace = vdupq_n_u16(L' ');
    const auto increment = vdupq_n_u16(8);
    auto offsets = vld1q_u16(&offsetsData[0]);

    do
    {
        vst1q_u16(chars, whitespace);
        vst1q_u16(charOffsets, offsets);
        offsets = vaddq_u16(offsets, increment);
        chars += 8;
        charOffsets += 8;
        // If _columnCount is something like 120, the actual backing buffer for charOffsets is 121 items large.
        // --> The while loop uses <= to emit at least 1 more write.
    } while (chars <= charsEnd);
#else
#error "Vectorizing this function improves overall performance by up to 40%. Don't remove this warning, just add the vectorized code."
    std::fill_n(_charsBuffer, _columnCount, UNICODE_SPACE);
    std::iota(_charOffsets.begin(), _charOffsets.end(), uint16_t{ 0 });
#endif

#pragma warning(push)
}

void ROW::TransferAttributes(const til::small_rle<TextAttribute, uint16_t, 1>& attr, til::CoordType newWidth)
{
    _attr = attr;
    _attr.resize_trailing_extent(gsl::narrow<uint16_t>(newWidth));
}

void ROW::CopyFrom(const ROW& source)
{
    RowCopyTextFromState state{ .source = source };
    CopyTextFrom(state);
    TransferAttributes(source.Attributes(), _columnCount);
    _lineRendition = source._lineRendition;
    _wrapForced = source._wrapForced;
}

// Returns the previous possible cursor position, preceding the given column.
// Returns 0 if column is less than or equal to 0.
til::CoordType ROW::NavigateToPrevious(til::CoordType column) const noexcept
{
    return _adjustBackward(_clampedColumn(column - 1));
}

// Returns the next possible cursor position, following the given column.
// Returns the row width if column is beyond the width of the row.
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

void ROW::SetAttrToEnd(const til::CoordType columnBegin, const TextAttribute attr)
{
    _attr.replace(_clampedColumnInclusive(columnBegin), _attr.size(), attr);
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
    leadingSpaces = colBeg - colBegDirty;
    chBeg = chBegDirty + leadingSpaces;
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

void ROW::ReplaceText(RowWriteState& state)
try
{
    WriteHelper h{ *this, state.columnBegin, state.columnLimit, state.text };
    if (!h.IsValid())
    {
        state.columnEnd = h.colBeg;
        state.columnBeginDirty = h.colBeg;
        state.columnEndDirty = h.colBeg;
        return;
    }
    h.ReplaceText();
    h.Finish();

    state.text = state.text.substr(h.charsConsumed);
    // Here's why we set `state.columnEnd` to `colLimit` if there's remaining text:
    // Callers should be able to use `state.columnEnd` as the next cursor position, as well as the parameter for a
    // follow-up call to ReplaceAttributes(). But if we fail to insert a wide glyph into the last column of a row,
    // that last cell (which now contains padding whitespace) should get the same attributes as the rest of the
    // string so that the row looks consistent. This requires us to return `colLimit` instead of `colLimit - 1`.
    // Additionally, this has the benefit that callers can detect line wrapping by checking `columnEnd >= columnLimit`.
    state.columnEnd = state.text.empty() ? h.colEnd : h.colLimit;
    state.columnBeginDirty = h.colBegDirty;
    state.columnEndDirty = h.colEndDirty;
}
catch (...)
{
    Reset(TextAttribute{});
    throw;
}

[[msvc::forceinline]] void ROW::WriteHelper::ReplaceText() noexcept
{
    // This function starts with a fast-pass for ASCII. ASCII is still predominant in technical areas.
    //
    // We can infer the "end" from the amount of columns we're given (colLimit - colBeg),
    // because ASCII is always 1 column wide per character.
    auto it = chars.begin();
    const auto end = it + std::min<size_t>(chars.size(), colLimit - colBeg);
    size_t ch = chBeg;

    while (it != end)
    {
        if (*it >= 0x80) [[unlikely]]
        {
            _replaceTextUnicode(ch, it);
            return;
        }

        til::at(row._charOffsets, colEnd) = gsl::narrow_cast<uint16_t>(ch);
        ++colEnd;
        ++ch;
        ++it;
    }

    colEndDirty = colEnd;
    charsConsumed = ch - chBeg;
}

[[msvc::forceinline]] void ROW::WriteHelper::_replaceTextUnicode(size_t ch, std::wstring_view::const_iterator it) noexcept
{
    const auto end = chars.end();

    while (it != end)
    {
        unsigned int width = 1;
        auto ptr = &*it;
        const auto wch = *ptr;
        size_t advance = 1;

        ++it;

        // Even in our slow-path we can avoid calling IsGlyphFullWidth if the current character is ASCII.
        // It also allows us to skip the surrogate pair decoding at the same time.
        if (wch >= 0x80)
        {
            if (til::is_surrogate(wch))
            {
                if (it != end && til::is_leading_surrogate(wch) && til::is_trailing_surrogate(*it))
                {
                    advance = 2;
                    ++it;
                }
                else
                {
                    ptr = &UNICODE_REPLACEMENT;
                }
            }

            width = IsGlyphFullWidth({ ptr, advance }) + 1u;
        }

        const auto colEndNew = gsl::narrow_cast<uint16_t>(colEnd + width);
        if (colEndNew > colLimit)
        {
            colEndDirty = colLimit;
            charsConsumed = ch - chBeg;
            return;
        }

        // Fill our char-offset buffer with 1 entry containing the mapping from the
        // current column (colEnd) to the start of the glyph in the string (ch)...
        til::at(row._charOffsets, colEnd++) = gsl::narrow_cast<uint16_t>(ch);
        // ...followed by 0-N entries containing an indication that the
        // columns are just a wide-glyph extension of the preceding one.
        while (colEnd < colEndNew)
        {
            til::at(row._charOffsets, colEnd++) = gsl::narrow_cast<uint16_t>(ch | CharOffsetsTrailer);
        }

        ch += advance;
    }

    colEndDirty = colEnd;
    charsConsumed = ch - chBeg;
}

void ROW::CopyTextFrom(RowCopyTextFromState& state)
try
{
    auto& source = state.source;
    const auto sourceColBeg = source._clampedColumnInclusive(state.sourceColumnBegin);
    const auto sourceColLimit = source._clampedColumnInclusive(state.sourceColumnLimit);
    std::span<const uint16_t> charOffsets;
    std::wstring_view chars;

    if (sourceColBeg < sourceColLimit)
    {
        charOffsets = source._charOffsets.subspan(sourceColBeg, static_cast<size_t>(sourceColLimit) - sourceColBeg + 1);
        const auto charsOffset = charOffsets.front() & CharOffsetsMask;
        // We _are_ using span. But C++ decided that string_view and span aren't convertible.
        // _chars is a std::span for performance and because it refers to raw, shared memory.
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
        chars = { source._chars.data() + charsOffset, source._chars.size() - charsOffset };
    }

    WriteHelper h{ *this, state.columnBegin, state.columnLimit, chars };

    if (!h.IsValid() ||
        // If we were to copy text from ourselves, we'd overwrite
        // our _charOffsets and break Finish() which reads from it.
        this == &state.source ||
        // Any valid charOffsets array is at least 2 elements long (the 1st element is the start offset and the 2nd
        // element is the length of the first glyph) and begins/ends with a non-trailer offset. We don't really
        // need to test for the end offset, since `WriteHelper::WriteWithOffsets` already takes care of that.
        charOffsets.size() < 2 || WI_IsFlagSet(charOffsets.front(), CharOffsetsTrailer))
    {
        state.columnEnd = h.colBeg;
        state.columnBeginDirty = h.colBeg;
        state.columnEndDirty = h.colBeg;
        state.sourceColumnEnd = source._columnCount;
        return;
    }

    h.CopyTextFrom(charOffsets);
    h.Finish();

    // state.columnEnd is computed identical to ROW::ReplaceText. Check it out for more information.
    state.columnEnd = h.charsConsumed == chars.size() ? h.colEnd : h.colLimit;
    state.columnBeginDirty = h.colBegDirty;
    state.columnEndDirty = h.colEndDirty;
    state.sourceColumnEnd = sourceColBeg + h.colEnd - h.colBeg;
}
catch (...)
{
    Reset(TextAttribute{});
    throw;
}

[[msvc::forceinline]] void ROW::WriteHelper::CopyTextFrom(const std::span<const uint16_t>& charOffsets) noexcept
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
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
    const auto dst = row._charOffsets.data() + colEnd;

    _copyOffsets(dst, charOffsets.data(), colEndInput, inToOutOffset);

    colEnd += colEndInput;
    colEndDirty = gsl::narrow_cast<uint16_t>(colBeg + colEndDirtyInput);
    charsConsumed = endOffset - baseOffset;
}

#pragma warning(push)
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
[[msvc::forceinline]] void ROW::WriteHelper::_copyOffsets(uint16_t* __restrict dst, const uint16_t* __restrict src, uint16_t size, uint16_t offset) noexcept
{
    __assume(src != nullptr);
    __assume(dst != nullptr);

    // All tested compilers (including MSVC) will neatly unroll and vectorize
    // this loop, which is why it's written in this particular way.
    for (const auto end = src + size; src != end; ++src, ++dst)
    {
        const uint16_t ch = *src;
        const uint16_t off = ch & CharOffsetsMask;
        const uint16_t trailer = ch & CharOffsetsTrailer;
        const uint16_t newOff = off + offset;
        *dst = newOff | trailer;
    }
}
#pragma warning(pop)

[[msvc::forceinline]] void ROW::WriteHelper::Finish()
{
    colEndDirty = row._adjustForward(colEndDirty);

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

til::CoordType ROW::LineRenditionColumns() const noexcept
{
    const auto scale = _lineRendition != LineRendition::SingleWidth ? 1 : 0;
    return _columnCount >> scale;
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

std::wstring_view ROW::GetText(til::CoordType columnBegin, til::CoordType columnEnd) const noexcept
{
    const til::CoordType columns = _columnCount;
    const auto colBeg = std::max(0, std::min(columns, columnBegin));
    const auto colEnd = std::max(colBeg, std::min(columns, columnEnd));
    const size_t chBeg = _uncheckedCharOffset(gsl::narrow_cast<size_t>(colBeg));
    const size_t chEnd = _uncheckedCharOffset(gsl::narrow_cast<size_t>(colEnd));
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
    return { _chars.data() + chBeg, chEnd - chBeg };
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
