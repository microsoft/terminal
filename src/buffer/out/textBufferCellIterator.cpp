// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "textBufferCellIterator.hpp"

#include "textBuffer.hpp"
#include "../types/inc/convert.hpp"
#include "../types/inc/viewport.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Types;

// Routine Description:
// - Creates a new read-only iterator to seek through cell data stored within a screen buffer
// Arguments:
// - buffer - Text buffer to seek through
// - pos - Starting position to retrieve text data from (within screen buffer bounds)
TextBufferCellIterator::TextBufferCellIterator(const TextBuffer& buffer, til::point pos) :
    TextBufferCellIterator(buffer, pos, buffer.GetSize())
{
}

// Routine Description:
// - Creates a new read-only iterator to seek through cell data stored within a screen buffer
// Arguments:
// - buffer - Pointer to screen buffer to seek through
// - pos - Starting position to retrieve text data from (within screen buffer bounds)
// - limits - Viewport limits to restrict the iterator within the buffer bounds (smaller than the buffer itself)
TextBufferCellIterator::TextBufferCellIterator(const TextBuffer& buffer, til::point pos, const Viewport limits) :
    _buffer(buffer),
    _pos(pos),
    _pRow(s_GetRow(buffer, pos)),
    _bounds(limits),
    _exceeded(false),
    _view({}, {}, {}, TextAttributeBehavior::Stored),
    _attrIter(s_GetRow(buffer, pos)->AttrBegin())
{
    // Throw if the bounds rectangle is not limited to the inside of the given buffer.
    THROW_HR_IF(E_INVALIDARG, !buffer.GetSize().IsInBounds(limits));

    // Throw if the coordinate is not limited to the inside of the given buffer.
    THROW_HR_IF(E_INVALIDARG, !limits.IsInBounds(pos));

    _attrIter += pos.x;

    _GenerateView();
}

// Routine Description:
// - Tells if the iterator is still valid (hasn't exceeded boundaries of underlying text buffer)
// Return Value:
// - True if this iterator can still be dereferenced for data. False if we've passed the end and are out of data.
TextBufferCellIterator::operator bool() const noexcept
{
    return !_exceeded && _bounds.IsInBounds(_pos);
}

// Routine Description:
// - Compares two iterators to see if they're pointing to the same position in the same buffer
// Arguments:
// - it - The other iterator to compare to this one.
// Return Value:
// - True if it's the same text buffer and same cell position. False otherwise.
bool TextBufferCellIterator::operator==(const TextBufferCellIterator& it) const noexcept
{
    return _pos == it._pos &&
           &_buffer == &it._buffer &&
           _exceeded == it._exceeded &&
           _bounds == it._bounds &&
           _pRow == it._pRow &&
           _attrIter == it._attrIter;
}

// Routine Description:
// - Compares two iterators to see if they're pointing to the different positions in the same buffer or different buffers entirely.
// Arguments:
// - it - The other iterator to compare to this one.
// Return Value:
// - True if it's the same text buffer and different cell position or if they're different buffers. False otherwise.
bool TextBufferCellIterator::operator!=(const TextBufferCellIterator& it) const noexcept
{
    return !(*this == it);
}

// Routine Description:
// - Advances the iterator forward relative to the underlying text buffer by the specified movement
// Arguments:
// - movement - Magnitude and direction of movement.
// Return Value:
// - Reference to self after movement.
TextBufferCellIterator& TextBufferCellIterator::operator+=(const ptrdiff_t& movement)
{
    // Note that this method is called intensively when the terminal is under heavy load.
    // We need to aggressively optimize it, comparing to the -= operator.
    auto move = movement;
    if (move < 0)
    {
        // Early branching to leave the rare case to -= operator.
        // This helps reducing the instruction count within this method, which is good for instruction cache.
        return *this -= -move;
    }

    // The remaining code in this function is functionally equivalent to:
    //   auto newPos = _pos;
    //   while (move > 0 && !_exceeded)
    //   {
    //       _exceeded = !_bounds.IncrementInBounds(newPos);
    //       move--;
    //   }
    //   _SetPos(newPos);
    //
    // _SetPos() necessitates calling _GenerateView() and thus the construction
    // of a new OutputCellView(). This has a high performance impact (ICache spill?).
    // The code below inlines _bounds.IncrementInBounds as well as SetPos.
    // In the hot path (_pos.y doesn't change) we modify the _view directly.

    // Hoist these integers which will be used frequently later.
    const auto boundsRightInclusive = _bounds.RightInclusive();
    const auto boundsLeft = _bounds.Left();
    const auto boundsBottomInclusive = _bounds.BottomInclusive();
    const auto boundsTop = _bounds.Top();
    const auto oldX = _pos.x;
    const auto oldY = _pos.y;

    // Under MSVC writing the individual members of a til::point generates worse assembly
    // compared to having them be local variables. This causes a performance impact.
    auto newX = oldX;
    auto newY = oldY;

    while (move > 0)
    {
        if (newX == boundsRightInclusive)
        {
            newX = boundsLeft;
            newY++;
            if (newY > boundsBottomInclusive)
            {
                newY = boundsTop;
                _exceeded = true;
                break;
            }
        }
        else
        {
            newX++;
            _exceeded = false;
        }
        move--;
    }

    if (_exceeded)
    {
        // Early return because nothing needs to be done here.
        return *this;
    }

    if (newY == oldY)
    {
        // hot path
        const auto diff = gsl::narrow_cast<ptrdiff_t>(newX) - gsl::narrow_cast<ptrdiff_t>(oldX);
        _attrIter += diff;
        _view.UpdateTextAttribute(*_attrIter);

        _view.UpdateText(_pRow->GlyphAt(newX));
        _view.UpdateDbcsAttribute(_pRow->DbcsAttrAt(newX));
        _pos.x = newX;
    }
    else
    {
        // cold path (_GenerateView is slow)
        _pRow = s_GetRow(_buffer, { newX, newY });
        _attrIter = _pRow->AttrBegin() + newX;
        _pos.x = newX;
        _pos.y = newY;
        _GenerateView();
    }

    return *this;
}

// Routine Description:
// - Advances the iterator backward relative to the underlying text buffer by the specified movement
// Arguments:
// - movement - Magnitude and direction of movement.
// Return Value:
// - Reference to self after movement.
TextBufferCellIterator& TextBufferCellIterator::operator-=(const ptrdiff_t& movement)
{
    auto move = movement;
    if (move < 0)
    {
        return (*this) += (-move);
    }

    auto newPos = _pos;
    while (move > 0 && !_exceeded)
    {
        _exceeded = !_bounds.DecrementInBounds(newPos);
        move--;
    }
    _SetPos(newPos);

    _GenerateView();
    return (*this);
}

// Routine Description:
// - Advances the iterator forward relative to the underlying text buffer by exactly 1
// Return Value:
// - Reference to self after movement.
TextBufferCellIterator& TextBufferCellIterator::operator++()
{
    return this->operator+=(1);
}

// Routine Description:
// - Advances the iterator backward relative to the underlying text buffer by exactly 1
// Return Value:
// - Reference to self after movement.
TextBufferCellIterator& TextBufferCellIterator::operator--()
{
    return this->operator-=(1);
}

// Routine Description:
// - Advances the iterator forward relative to the underlying text buffer by exactly 1
// Return Value:
// - Value with previous position prior to movement.
TextBufferCellIterator TextBufferCellIterator::operator++(int)
{
    auto temp = *this;
    operator++();
    return temp;
}

// Routine Description:
// - Advances the iterator backward relative to the underlying text buffer by exactly 1
// Return Value:
// - Value with previous position prior to movement.
TextBufferCellIterator TextBufferCellIterator::operator--(int)
{
    auto temp = *this;
    operator--();
    return temp;
}

// Routine Description:
// - Advances the iterator forward relative to the underlying text buffer by the specified movement
// Arguments:
// - movement - Magnitude and direction of movement.
// Return Value:
// - Value with previous position prior to movement.
TextBufferCellIterator TextBufferCellIterator::operator+(const ptrdiff_t& movement)
{
    auto temp = *this;
    temp += movement;
    return temp;
}

// Routine Description:
// - Advances the iterator negative relative to the underlying text buffer by the specified movement
// Arguments:
// - movement - Magnitude and direction of movement.
// Return Value:
// - Value with previous position prior to movement.
TextBufferCellIterator TextBufferCellIterator::operator-(const ptrdiff_t& movement)
{
    auto temp = *this;
    temp -= movement;
    return temp;
}

// Routine Description:
// - Provides the difference in position between two iterators.
// Arguments:
// - it - The other iterator to compare to this one.
ptrdiff_t TextBufferCellIterator::operator-(const TextBufferCellIterator& it)
{
    THROW_HR_IF(E_NOT_VALID_STATE, &_buffer != &it._buffer); // It's not valid to compare this for iterators pointing at different buffers.
    return _bounds.CompareInBounds(_pos, it._pos);
}

// Routine Description:
// - Sets the coordinate position that this iterator will inspect within the text buffer on dereference.
// Arguments:
// - newPos - The new coordinate position.
void TextBufferCellIterator::_SetPos(const til::point newPos)
{
    if (newPos.y != _pos.y)
    {
        _pRow = s_GetRow(_buffer, newPos);
        _attrIter = _pRow->AttrBegin();
        _pos.x = 0;
    }

    if (newPos.x != _pos.x)
    {
        const auto diff = gsl::narrow_cast<ptrdiff_t>(newPos.x) - gsl::narrow_cast<ptrdiff_t>(_pos.x);
        _attrIter += diff;
    }

    _pos = newPos;

    _GenerateView();
}

// Routine Description:
// - Shortcut for pulling the row out of the text buffer embedded in the screen information.
//   We'll hold and cache this to improve performance over looking it up every time.
// Arguments:
// - buffer - Screen information pointer to pull text buffer data from
// - pos - Position inside screen buffer bounds to retrieve row
// Return Value:
// - Pointer to the underlying CharRow structure
const ROW* TextBufferCellIterator::s_GetRow(const TextBuffer& buffer, const til::point pos)
{
    return &buffer.GetRowByOffset(pos.y);
}

// Routine Description:
// - Updates the internal view. Call after updating row, attribute, or positions.
void TextBufferCellIterator::_GenerateView() noexcept
{
    _view = OutputCellView(_pRow->GlyphAt(_pos.x),
                           _pRow->DbcsAttrAt(_pos.x),
                           *_attrIter,
                           TextAttributeBehavior::Stored);
}

// Routine Description:
// - Provides full fidelity view of the cell data in the underlying buffer.
// Arguments:
// - <none> - Uses current position
// Return Value:
// - OutputCellView representation that provides a read-only view into the underlying text buffer data.
const OutputCellView& TextBufferCellIterator::operator*() const noexcept
{
    return _view;
}

// Routine Description:
// - Provides full fidelity view of the cell data in the underlying buffer.
// Arguments:
// - <none> - Uses current position
// Return Value:
// - OutputCellView representation that provides a read-only view into the underlying text buffer data.
const OutputCellView* TextBufferCellIterator::operator->() const noexcept
{
    return &_view;
}

til::point TextBufferCellIterator::Pos() const noexcept
{
    return _pos;
}
