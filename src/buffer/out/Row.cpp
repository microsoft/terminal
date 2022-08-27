// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "Row.hpp"
#include "CharRow.hpp"
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
ROW::ROW(const til::CoordType rowId, const til::CoordType rowWidth, const TextAttribute fillAttribute, TextBuffer* const pParent) :
    _id{ rowId },
    _rowWidth{ rowWidth },
    _charRow{ rowWidth, this },
    _attrRow{ rowWidth, fillAttribute },
    _lineRendition{ LineRendition::SingleWidth },
    _wrapForced{ false },
    _doubleBytePadded{ false },
    _pParent{ pParent }
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
    _charRow.Reset();
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
[[nodiscard]] HRESULT ROW::Resize(const til::CoordType width)
{
    RETURN_IF_FAILED(_charRow.Resize(width));
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
void ROW::ClearColumn(const til::CoordType column)
{
    THROW_HR_IF(E_INVALIDARG, column >= _charRow.size());
    _charRow.ClearCell(column);
}

UnicodeStorage& ROW::GetUnicodeStorage() noexcept
{
    return _pParent->GetUnicodeStorage();
}

const UnicodeStorage& ROW::GetUnicodeStorage() const noexcept
{
    return _pParent->GetUnicodeStorage();
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
OutputCellIterator ROW::WriteCells(OutputCellIterator it, const til::CoordType index, const std::optional<bool> wrap, std::optional<til::CoordType> limitRight)
{
    THROW_HR_IF(E_INVALIDARG, index >= _charRow.size());
    THROW_HR_IF(E_INVALIDARG, limitRight.value_or(0) >= _charRow.size());

    // If we're given a right-side column limit, use it. Otherwise, the write limit is the final column index available in the char row.
    const auto finalColumnInRow = limitRight.value_or(_charRow.size() - 1);

    auto currentColor = it->TextAttr();
    uint16_t colorUses = 0;
    auto colorStarts = gsl::narrow_cast<uint16_t>(index);
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
                _attrRow.Replace(colorStarts, currentIndex, currentColor);
                currentColor = it->TextAttr();
                colorUses = 1;
                colorStarts = currentIndex;
            }
        }

        // Fill the text if the behavior isn't set to saying there's only a color stored in this iterator.
        if (it->TextAttrBehavior() != TextAttributeBehavior::StoredOnly)
        {
            const auto fillingLastColumn = currentIndex == finalColumnInRow;

            // TODO: MSFT: 19452170 - We need to ensure when writing any trailing byte that the one to the left
            // is a matching leading byte. Likewise, if we're writing a leading byte, we need to make sure we still have space in this loop
            // for the trailing byte coming up before writing it.

            // If we're trying to fill the first cell with a trailing byte, pad it out instead by clearing it.
            // Don't increment iterator. We'll advance the index and try again with this value on the next round through the loop.
            if (currentIndex == 0 && it->DbcsAttr().IsTrailing())
            {
                _charRow.ClearCell(currentIndex);
            }
            // If we're trying to fill the last cell with a leading byte, pad it out instead by clearing it.
            // Don't increment iterator. We'll exit because we couldn't write a lead at the end of a line.
            else if (fillingLastColumn && it->DbcsAttr().IsLeading())
            {
                _charRow.ClearCell(currentIndex);
                SetDoubleBytePadded(true);
            }
            // Otherwise, copy the data given and increment the iterator.
            else
            {
                _charRow.DbcsAttrAt(currentIndex) = it->DbcsAttr();
                _charRow.GlyphAt(currentIndex) = it->Chars();
                ++it;
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
