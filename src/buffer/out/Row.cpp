// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "Row.hpp"
#include "textBuffer.hpp"
#include "../types/inc/convert.hpp"

// Routine Description:
// - constructor
// Arguments:
// - rowId - the row index in the text buffer
// - rowWidth - the width of the row, cell elements
// - fillAttribute - the default text attribute
// - pParent - the text buffer that this row belongs to
// Return Value:
// - constructed object
ROW::ROW(const SHORT /*rowId*/, const unsigned short rowWidth, const TextAttribute fillAttribute, TextBuffer* const /*pParent*/) :
    _attrRow{ rowWidth, fillAttribute },
    _lineRendition{ LineRendition::SingleWidth },
    _wrapForced{ false },
    _doubleBytePadded{ false },
    _rowWidth(rowWidth),
    _cwid(_rowWidth, 1),
    _data(_rowWidth, UNICODE_SPACE)
{
}

// Routine Description:
// - Sets all properties of the ROW to default values
// Arguments:
// - Attr - The default attribute (color) to fill
// Return Value:
// - <none>
bool ROW::Reset(const TextAttribute Attr)
{
    _lineRendition = LineRendition::SingleWidth;
    _wrapForced = false;
    _doubleBytePadded = false;
#if BACKING_BUFFER_IS_STRINGLIKE
    _cwid.replace(0, _rowWidth, { 1, _rowWidth }); // replace entire RLE with one run
#else
    _cwid.resize(_rowWidth);
    _cwid.fill(1);
#endif
    _data.replace(0, _rowWidth, _rowWidth, UNICODE_SPACE);
    try
    {
        _attrRow.Reset(Attr);
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
        return false;
    }
    return true;
}

// Routine Description:
// - resizes ROW to new width
// Arguments:
// - width - the new width, in cells
// Return Value:
// - S_OK if successful, otherwise relevant error
[[nodiscard]] HRESULT ROW::Resize(const unsigned short width)
{
    _data.resize(width, L' ');
#if BACKING_BUFFER_IS_STRINGLIKE
    auto oldEnd{ _cwid.size() };
    _cwid.resize_trailing_extent(width);
    if (width > oldEnd)
    {
        _cwid.replace(oldEnd, width, { 1, gsl::narrow_cast<uint16_t>(width - oldEnd) });
    }
#else
    _cwid.resize(width);
    _cwid.fill(1);
#endif
    try
    {
        _attrRow.Resize(width);
    }
    CATCH_RETURN();

    _rowWidth = width;

    return S_OK;
}

// Routine Description:
// - clears char data in column in row
// Arguments:
// - column - 0-indexed column index
// Return Value:
// - <none>
void ROW::ClearColumn(const size_t column)
{
    THROW_HR_IF(E_INVALIDARG, column >= _rowWidth);
    WriteGlyphAtMeasured(column, 1, L" ");
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
OutputCellIterator ROW::WriteCells(OutputCellIterator it, const size_t index, const std::optional<bool> wrap, std::optional<size_t> limitRight)
{
    THROW_HR_IF(E_INVALIDARG, index >= _rowWidth);
    THROW_HR_IF(E_INVALIDARG, limitRight.value_or(0) >= _rowWidth);

    // If we're given a right-side column limit, use it. Otherwise, the write limit is the final column index available in the char row.
    const auto finalColumnInRow = limitRight.value_or(_rowWidth - 1);

    auto currentColor = it->TextAttr();
    uint16_t colorUses = 0;
    uint16_t colorStarts = gsl::narrow_cast<uint16_t>(index);
    uint16_t currentIndex = colorStarts;

    auto [ibegin, ilen, ioff, icols] = _indicesForCol(currentIndex);
    auto ihintcol = currentIndex - ioff;
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
                _attrRow.Replace(colorStarts, currentIndex, currentColor);
                currentColor = it->TextAttr();
                colorUses = 1;
                colorStarts = currentIndex;
            }
        }

        // Fill the text if the behavior isn't set to saying there's only a color stored in this iterator.
        if (it->TextAttrBehavior() != TextAttributeBehavior::StoredOnly)
        {
            const bool fillingLastColumn = currentIndex == finalColumnInRow;

            // TODO: MSFT: 19452170 - We need to ensure when writing any trailing byte that the one to the left
            // is a matching leading byte. Likewise, if we're writing a leading byte, we need to make sure we still have space in this loop
            // for the trailing byte coming up before writing it.

            // If we're trying to fill the first cell with a trailing byte, pad it out instead by clearing it.
            // Don't increment iterator. We'll advance the index and try again with this value on the next round through the loop.
            if (currentIndex == 0 && it->DbcsAttr().IsTrailing())
            {
                ClearColumn(currentIndex);
                it.AddCellDistanceFault(1); // we couldn't fit a cell here but we skipped a column :|
            }
            // If we're trying to fill the last cell with a leading byte, pad it out instead by clearing it.
            // Don't increment iterator. We'll exit because we couldn't write a lead at the end of a line.
            else if (fillingLastColumn && it->DbcsAttr().IsLeading())
            {
                ClearColumn(currentIndex);
                it.AddCellDistanceFault(1); // we couldn't fit a cell here but we skipped a column :|
                SetDoubleBytePadded(true);
            }
            // Otherwise, copy the data given and increment the iterator.
            else
            {
                if (!it->DbcsAttr().IsTrailing())
                {
                    uint16_t d = it->DbcsAttr().IsSingle() ? 1 : 2;
                    std::tie(ibegin, ihintcol) = WriteGlyphAtMeasured(currentIndex, d, it->Chars(), ibegin, ihintcol);
                    currentIndex += d - 1;
                    while (d > 0)
                    { // TODO(DH) FFS
                        ++it;
                        --d;
                    }
                }
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
        _attrRow.Replace(colorStarts, currentIndex, currentColor);
    }

    return it;
}
