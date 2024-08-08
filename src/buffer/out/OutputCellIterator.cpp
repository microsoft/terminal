// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "OutputCellIterator.hpp"

#include <til/unicode.h>

#include "../../types/inc/convert.hpp"
#include "../../inc/conattrs.hpp"

static constexpr TextAttribute InvalidTextAttribute{ INVALID_COLOR, INVALID_COLOR, INVALID_COLOR };

// Routine Description:
// - This is a fill-mode iterator for one particular color. It will repeat forever if fillLimit is 0.
// Arguments:
// - attr - The color attribute to use for filling
// - fillLimit - How many times to allow this value to be viewed/filled. Infinite if 0.
OutputCellIterator::OutputCellIterator(const TextAttribute& attr, const size_t fillLimit) noexcept :
    _mode(Mode::Fill),
    _currentView(s_GenerateView(attr)),
    _run(),
    _attr(InvalidTextAttribute),
    _pos(0),
    _distance(0),
    _fillLimit(fillLimit)
{
}

// Routine Description:
// - This is an iterator over legacy colors only. The text is not modified.
// Arguments:
// - legacyAttrs - One legacy color item per cell
OutputCellIterator::OutputCellIterator(const std::span<const WORD> legacyAttrs) noexcept :
    _mode(Mode::LegacyAttr),
    _currentView(s_GenerateViewLegacyAttr(til::at(legacyAttrs, 0))),
    _run(legacyAttrs),
    _attr(InvalidTextAttribute),
    _distance(0),
    _pos(0),
    _fillLimit(0)
{
}

// Routine Description:
// - This is an iterator over existing OutputCells with full text and color data.
// Arguments:
// - cells - Multiple cells in a run
OutputCellIterator::OutputCellIterator(const std::span<const OutputCell> cells) :
    _mode(Mode::Cell),
    _currentView(s_GenerateView(til::at(cells, 0))),
    _run(cells),
    _attr(InvalidTextAttribute),
    _distance(0),
    _pos(0),
    _fillLimit(0)
{
}

// Routine Description:
// - Specifies whether this iterator is valid for dereferencing (still valid underlying data)
// Return Value:
// - True if the views on dereference are valid. False if it shouldn't be dereferenced.
OutputCellIterator::operator bool() const noexcept
{
    try
    {
        switch (_mode)
        {
        case Mode::Fill:
        {
            if (_fillLimit > 0)
            {
                return _pos < _fillLimit;
            }
            return true;
        }
        case Mode::Cell:
        {
            return _pos < std::get<std::span<const OutputCell>>(_run).size();
        }
        case Mode::LegacyAttr:
        {
            return _pos < std::get<std::span<const WORD>>(_run).size();
        }
        default:
            FAIL_FAST_HR(E_NOTIMPL);
        }
    }
    CATCH_FAIL_FAST();
}

size_t OutputCellIterator::Position() const noexcept
{
    return _pos;
}

// Routine Description:
// - Advances the iterator one position over the underlying data source.
// Return Value:
// - Reference to self after advancement.
OutputCellIterator& OutputCellIterator::operator++()
{
    // Keep track of total distance moved (cells filled)
    _distance++;

    switch (_mode)
    {
    case Mode::Fill:
    {
        if (!_TryMoveTrailing())
        {
            if (_currentView.DbcsAttr() == DbcsAttribute::Trailing)
            {
                _currentView = OutputCellView(_currentView.Chars(),
                                              DbcsAttribute::Leading,
                                              _currentView.TextAttr(),
                                              _currentView.TextAttrBehavior());
            }

            if (_fillLimit > 0)
            {
                // We walk forward by one because we fill with the same cell over and over no matter what
                _pos++;
            }
        }
        break;
    }
    case Mode::Cell:
    {
        // Walk forward by one because cells are assumed to be in the form they needed to be
        _pos++;
        if (operator bool())
        {
            _currentView = s_GenerateView(til::at(std::get<std::span<const OutputCell>>(_run), _pos));
        }
        break;
    }
    case Mode::LegacyAttr:
    {
        // Walk forward by one because color attributes apply cell by cell (no complex text information)
        _pos++;
        if (operator bool())
        {
            _currentView = s_GenerateViewLegacyAttr(til::at(std::get<std::span<const WORD>>(_run), _pos));
        }
        break;
    }
    default:
        FAIL_FAST_HR(E_NOTIMPL);
    }

    return (*this);
}

// Routine Description:
// - Advances the iterator one position over the underlying data source.
// Return Value:
// - Reference to self after advancement.
OutputCellIterator OutputCellIterator::operator++(int)
{
    auto temp = *this;
    operator++();
    return temp;
}

// Routine Description:
// - Reference the view to fully-formed output cell data representing the underlying data source.
// Return Value:
// - Reference to the view
const OutputCellView& OutputCellIterator::operator*() const noexcept
{
    return _currentView;
}

// Routine Description:
// - Get pointer to the view to fully-formed output cell data representing the underlying data source.
// Return Value:
// - Pointer to the view
const OutputCellView* OutputCellIterator::operator->() const noexcept
{
    return &_currentView;
}

// Routine Description:
// - Checks the current view. If it is a leading half, it updates the current
//   view to the trailing half of the same glyph.
// - This helps us to draw glyphs that are two columns wide by "doubling"
//   the view that is returned so it will consume two cells.
// Return Value:
// - True if we just turned a lead half into a trailing half (and caller doesn't
//   need to further update the view).
// - False if this wasn't applicable and the caller should update the view.
bool OutputCellIterator::_TryMoveTrailing() noexcept
{
    if (_currentView.DbcsAttr() == DbcsAttribute::Leading)
    {
        _currentView = OutputCellView(_currentView.Chars(),
                                      DbcsAttribute::Trailing,
                                      _currentView.TextAttr(),
                                      _currentView.TextAttrBehavior());
        return true;
    }
    else
    {
        return false;
    }
}

// Routine Description:
// - Static function to create a view.
// - It's pulled out statically so it can be used during construction with just the given
//   variables (so OutputCellView doesn't need an empty default constructor)
// - This will infer the width of the glyph and apply the appropriate attributes to the view.
// Arguments:
// - attr - View representing a single color
// Return Value:
// - Object representing the view into this cell
OutputCellView OutputCellIterator::s_GenerateView(const TextAttribute& attr) noexcept
{
    return OutputCellView({}, {}, attr, TextAttributeBehavior::StoredOnly);
}

// Routine Description:
// - Static function to create a view.
// - It's pulled out statically so it can be used during construction with just the given
//   variables (so OutputCellView doesn't need an empty default constructor)
// - This will infer the width of the glyph and apply the appropriate attributes to the view.
// Arguments:
// - legacyAttr - View representing a single legacy color
// Return Value:
// - Object representing the view into this cell
OutputCellView OutputCellIterator::s_GenerateViewLegacyAttr(const WORD& legacyAttr) noexcept
{
    auto cleanAttr = legacyAttr;
    WI_ClearAllFlags(cleanAttr, COMMON_LVB_SBCSDBCS); // don't use legacy lead/trailing byte flags for colors

    const TextAttribute attr(cleanAttr);
    return s_GenerateView(attr);
}

// Routine Description:
// - Static function to create a view.
// - It's pulled out statically so it can be used during construction with just the given
//   variables (so OutputCellView doesn't need an empty default constructor)
// Arguments:
// - cell - A reference to the cell for which we will make the read-only view
// Return Value:
// - Object representing the view into this cell
OutputCellView OutputCellIterator::s_GenerateView(const OutputCell& cell)
{
    return OutputCellView(cell.Chars(), cell.DbcsAttr(), cell.TextAttr(), cell.TextAttrBehavior());
}

// Routine Description:
// - Gets the distance between two iterators relative to the input data given in.
// Return Value:
// - The number of items of the input run consumed between these two iterators.
til::CoordType OutputCellIterator::GetInputDistance(OutputCellIterator other) const noexcept
{
    return gsl::narrow_cast<til::CoordType>(_pos - other._pos);
}

// Routine Description:
// - Gets the distance between two iterators relative to the number of cells inserted.
// Return Value:
// - The number of cells in the backing buffer filled between these two iterators.
til::CoordType OutputCellIterator::GetCellDistance(OutputCellIterator other) const noexcept
{
    return gsl::narrow_cast<til::CoordType>(_distance - other._distance);
}
