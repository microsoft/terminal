// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "textBufferCellIterator.hpp"

#include "CharRow.hpp"
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
TextBufferCellIterator::TextBufferCellIterator(const TextBuffer& buffer, COORD pos) :
    TextBufferCellIterator(buffer, pos, buffer.GetSize())
{
}

// Routine Description:
// - Creates a new read-only iterator to seek through cell data stored within a screen buffer
// Arguments:
// - buffer - Pointer to screen buffer to seek through
// - pos - Starting position to retrieve text data from (within screen buffer bounds)
// - limits - Viewport limits to restrict the iterator within the buffer bounds (smaller than the buffer itself)
TextBufferCellIterator::TextBufferCellIterator(const TextBuffer& buffer, COORD pos, const Viewport limits) :
    _buffer(buffer),
    _pos(pos),
    _pRow(s_GetRow(buffer, pos)),
    _bounds(limits),
    _exceeded(false),
    _view({}, {}, {}, TextAttributeBehavior::Stored),
    _attrIter(s_GetRow(buffer, pos)->GetAttrRow().cbegin())
{
    // Throw if the bounds rectangle is not limited to the inside of the given buffer.
    THROW_HR_IF(E_INVALIDARG, !buffer.GetSize().IsInBounds(limits));

    // Throw if the coordinate is not limited to the inside of the given buffer.
    THROW_HR_IF(E_INVALIDARG, !limits.IsInBounds(pos));

    _attrIter += pos.X;

    _GenerateView();
}

// Routine Description:
// - Creates a new read-only iterator to seek through cell data stored within a screen buffer
// Arguments:
// - buffer - Text buffer to seek through
// - pos - Starting position to retrieve text data from (within screen buffer bounds)
// - limits - Viewport limits to restrict the iterator within the buffer bounds (smaller than the buffer itself)
// - endPosInclusive - last position to iterate through (inclusive)
TextBufferCellIterator::TextBufferCellIterator(const TextBuffer& buffer, COORD pos, const Viewport limits, const COORD endPosInclusive) :
    TextBufferCellIterator(buffer, pos, limits)
{
    // Throw if the coordinate is not limited to the inside of the given buffer.
    THROW_HR_IF(E_INVALIDARG, !_bounds.IsInBounds(endPosInclusive));

    // Throw if pos is past endPos
    THROW_HR_IF(E_INVALIDARG, _bounds.CompareInBounds(pos, endPosInclusive) > 0);

    _endPosInclusive = endPosInclusive;
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
           _attrIter == it._attrIter &&
           _endPosInclusive == _endPosInclusive;
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
    ptrdiff_t move = movement;
    auto newPos = _pos;
    while (move > 0 && !_exceeded)
    {
        // If we have an endPos, check if we've exceeded it
        if (_endPosInclusive.has_value())
        {
            _exceeded = _bounds.CompareInBounds(newPos, *_endPosInclusive) > 0;
        }

        // If we already exceeded from endPos, we'll short-circuit and _not_ increment
        _exceeded |= !_bounds.IncrementInBounds(newPos);
        move--;
    }
    while (move < 0 && !_exceeded)
    {
        // If we have an endPos, check if we've exceeded it
        if (_endPosInclusive.has_value())
        {
            _exceeded = _bounds.CompareInBounds(newPos, *_endPosInclusive) < 0;
        }

        // If we already exceeded from endPos, we'll short-circuit and _not_ decrement
        _exceeded |= !_bounds.DecrementInBounds(newPos);
        move++;
    }
    _SetPos(newPos);
    return (*this);
}

// Routine Description:
// - Advances the iterator backward relative to the underlying text buffer by the specified movement
// Arguments:
// - movement - Magnitude and direction of movement.
// Return Value:
// - Reference to self after movement.
TextBufferCellIterator& TextBufferCellIterator::operator-=(const ptrdiff_t& movement)
{
    return this->operator+=(-movement);
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
    auto temp(*this);
    operator++();
    return temp;
}

// Routine Description:
// - Advances the iterator backward relative to the underlying text buffer by exactly 1
// Return Value:
// - Value with previous position prior to movement.
TextBufferCellIterator TextBufferCellIterator::operator--(int)
{
    auto temp(*this);
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
    auto temp(*this);
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
    auto temp(*this);
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
void TextBufferCellIterator::_SetPos(const COORD newPos)
{
    if (newPos.Y != _pos.Y)
    {
        _pRow = s_GetRow(_buffer, newPos);
        _attrIter = _pRow->GetAttrRow().cbegin();
        _pos.X = 0;
    }

    if (newPos.X != _pos.X)
    {
        const auto diff = gsl::narrow_cast<ptrdiff_t>(newPos.X) - gsl::narrow_cast<ptrdiff_t>(_pos.X);
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
const ROW* TextBufferCellIterator::s_GetRow(const TextBuffer& buffer, const COORD pos)
{
    return &buffer.GetRowByOffset(pos.Y);
}

// Routine Description:
// - Updates the internal view. Call after updating row, attribute, or positions.
void TextBufferCellIterator::_GenerateView()
{
    _view = OutputCellView(_pRow->GetCharRow().GlyphAt(_pos.X),
                           _pRow->GetCharRow().DbcsAttrAt(_pos.X),
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

COORD TextBufferCellIterator::Pos() const noexcept
{
    return _pos;
}
