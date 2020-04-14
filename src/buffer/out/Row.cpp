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
ROW::ROW(const SHORT rowId, const short rowWidth, const TextAttribute fillAttribute, TextBuffer* const pParent) :
    _id{ rowId },
    _rowWidth{ gsl::narrow<size_t>(rowWidth) },
    _charRow{ gsl::narrow<size_t>(rowWidth), this },
    _attrRow{ gsl::narrow<UINT>(rowWidth), fillAttribute },
    _pParent{ pParent }
{
}

size_t ROW::size() const noexcept
{
    return _rowWidth;
}

const CharRow& ROW::GetCharRow() const noexcept
{
    return _charRow;
}

CharRow& ROW::GetCharRow() noexcept
{
    return _charRow;
}

const ATTR_ROW& ROW::GetAttrRow() const noexcept
{
    return _attrRow;
}

ATTR_ROW& ROW::GetAttrRow() noexcept
{
    return _attrRow;
}

SHORT ROW::GetId() const noexcept
{
    return _id;
}

void ROW::SetId(const SHORT id) noexcept
{
    _id = id;
}

// Routine Description:
// - Sets all properties of the ROW to default values
// Arguments:
// - Attr - The default attribute (color) to fill
// Return Value:
// - <none>
bool ROW::Reset(const TextAttribute Attr)
{
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
[[nodiscard]] HRESULT ROW::Resize(const size_t width)
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
void ROW::ClearColumn(const size_t column)
{
    THROW_HR_IF(E_INVALIDARG, column >= _charRow.size());
    _charRow.ClearCell(column);
}

// Routine Description:
// - gets the text of the row as it would be shown on the screen
// Return Value:
// - wstring containing text for the row
std::wstring ROW::GetText() const
{
    return _charRow.GetText();
}

RowCellIterator ROW::AsCellIter(const size_t startIndex) const
{
    return AsCellIter(startIndex, size() - startIndex);
}

RowCellIterator ROW::AsCellIter(const size_t startIndex, const size_t count) const
{
    return RowCellIterator(*this, startIndex, count);
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
OutputCellIterator ROW::WriteCells(OutputCellIterator it, const size_t index, const std::optional<bool> wrap, std::optional<size_t> limitRight)
{
    THROW_HR_IF(E_INVALIDARG, index >= _charRow.size());
    THROW_HR_IF(E_INVALIDARG, limitRight.value_or(0) >= _charRow.size());
    size_t currentIndex = index;

    // If we're given a right-side column limit, use it. Otherwise, the write limit is the final column index available in the char row.
    const auto finalColumnInRow = limitRight.value_or(_charRow.size() - 1);

    while (it && currentIndex <= finalColumnInRow)
    {
        // Fill the color if the behavior isn't set to keeping the current color.
        if (it->TextAttrBehavior() != TextAttributeBehavior::Current)
        {
            const TextAttributeRun attrRun{ 1, it->TextAttr() };
            LOG_IF_FAILED(_attrRow.InsertAttrRuns({ &attrRun, 1 },
                                                  currentIndex,
                                                  currentIndex,
                                                  _charRow.size()));
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
                _charRow.ClearCell(currentIndex);
            }
            // If we're trying to fill the last cell with a leading byte, pad it out instead by clearing it.
            // Don't increment iterator. We'll exit because we couldn't write a lead at the end of a line.
            else if (fillingLastColumn && it->DbcsAttr().IsLeading())
            {
                _charRow.ClearCell(currentIndex);
                _charRow.SetDoubleBytePadded(true);
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
                _charRow.SetWrapForced(wrap.value());
            }
        }
        else
        {
            ++it;
        }

        // Move to the next cell for the next time through the loop.
        ++currentIndex;
    }

    return it;
}
