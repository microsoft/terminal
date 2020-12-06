// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "RowCellIterator.hpp"
#include "Row.hpp"

#include "../../types/inc/convert.hpp"
#include "../../types/inc/Utf16Parser.hpp"

RowCellIterator::RowCellIterator(const ROW& row, const size_t start, const size_t length) :
    _row(row),
    _start(start),
    _length(length),
    _pos(start),
    _view(s_GenerateView(row, start))
{
}

RowCellIterator::operator bool() const noexcept
{
    // In lieu of using start and end, this custom iterator type simply becomes bool false
    // when we run out of items to iterate over.
    return _pos < (_start + _length);
}

bool RowCellIterator::operator==(const RowCellIterator& it) const noexcept
{
    return _row == it._row &&
           _start == it._start &&
           _length == it._length &&
           _pos == it._pos;
}
bool RowCellIterator::operator!=(const RowCellIterator& it) const noexcept
{
    return !(*this == it);
}

RowCellIterator& RowCellIterator::operator+=(const ptrdiff_t& movement) noexcept
{
    _pos += movement;

    return (*this);
}

RowCellIterator& RowCellIterator::operator++() noexcept
{
    return this->operator+=(1);
}

RowCellIterator RowCellIterator::operator++(int) noexcept
{
    auto temp(*this);
    operator++();
    return temp;
}

RowCellIterator RowCellIterator::operator+(const ptrdiff_t& movement) noexcept
{
    auto temp(*this);
    temp += movement;
    return temp;
}

const OutputCellView& RowCellIterator::operator*() const noexcept
{
    return _view;
}

const OutputCellView* RowCellIterator::operator->() const noexcept
{
    return &_view;
}

// Routine Description:
// - Member function to update the view to the current position in the buffer with
//   the data held on this object.
// Arguments:
// - <none>
// Return Value:
// - <none>
void RowCellIterator::_RefreshView()
{
    _view = s_GenerateView(_row, _pos);
}

// Routine Description:
// - Static function to create a view.
// - It's pulled out statically so it can be used during construction with just the given
//   variables (so OutputCellView doesn't need an empty default constructor)
// - This will infer the width of the glyph and apply the appropriate attributes to the view.
// Arguments:
// - view - View representing characters corresponding to a single glyph
// - attr - Color attributes to apply to the text
// Return Value:
// - Object representing the view into this cell
OutputCellView RowCellIterator::s_GenerateView(const ROW& row,
                                               const size_t pos)
{
    const auto& charRow = row.GetCharRow();

    const auto glyph = charRow.GlyphAt(pos);
    const auto dbcsAttr = charRow.DbcsAttrAt(pos);

    const auto textAttr = row.GetAttrRow().GetAttrByColumn(pos);
    const auto behavior = TextAttributeBehavior::Stored;

    return OutputCellView(glyph, dbcsAttr, textAttr, behavior);
}
