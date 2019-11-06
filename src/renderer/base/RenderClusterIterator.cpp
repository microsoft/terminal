// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "../inc/RenderClusterIterator.hpp"

#include "../../buffer/out/textBufferCellIterator.hpp"

using namespace Microsoft::Console::Render;

RenderClusterIterator::RenderClusterIterator(TextBufferCellIterator& cellIter) :
    _cellIter(cellIter),
    _cluster(L"", 0),
    _attr((*cellIter).TextAttr())
{
    _GenerateCluster();
}

RenderClusterIterator::operator bool() const noexcept
{
    return !_exceeded;
}

// Routine Description:
// - Compares two iterators to see if they're pointing to the same position in the same buffer
// Arguments:
// - it - The other iterator to compare to this one.
// Return Value:
// - True if it's the same text buffer and same cell position. False otherwise.
bool RenderClusterIterator::operator==(const RenderClusterIterator& it) const
{
    return _attr == it._attr &&
           &_cellIter == &it._cellIter &&
           _exceeded == it._exceeded;
}

// Routine Description:
// - Compares two iterators to see if they're pointing to the different positions in the same buffer or different buffers entirely.
// Arguments:
// - it - The other iterator to compare to this one.
// Return Value:
// - True if it's the same text buffer and different cell position or if they're different buffers. False otherwise.
bool RenderClusterIterator::operator!=(const RenderClusterIterator& it) const
{
    return !(*this == it);
}

RenderClusterIterator& RenderClusterIterator::operator+=(const ptrdiff_t& movement)
{
    ptrdiff_t move = movement;
    auto newCellIter = _cellIter;
    while (move > 0 && !_exceeded)
    {
        ++newCellIter;
        _exceeded = !(newCellIter && (*newCellIter).TextAttr() == _attr);
        move--;
    }
    while (move < 0 && !_exceeded)
    {
        --newCellIter;
        _exceeded = !(newCellIter && (*newCellIter).TextAttr() == _attr);
        move++;
    }

    _cellIter += movement;
    _GenerateCluster();
    return (*this);
}

// Routine Description:
// - Advances the iterator backward relative to the underlying text buffer by the specified movement
// Arguments:
// - movement - Magnitude and direction of movement.
// Return Value:
// - Reference to self after movement.
RenderClusterIterator& RenderClusterIterator::operator-=(const ptrdiff_t& movement)
{
    return this->operator+=(-movement);
}

// Routine Description:
// - Advances the iterator forward relative to the underlying text buffer by exactly 1
// Return Value:
// - Reference to self after movement.
RenderClusterIterator& RenderClusterIterator::operator++()
{
    return this->operator+=(1);
}

// Routine Description:
// - Advances the iterator backward relative to the underlying text buffer by exactly 1
// Return Value:
// - Reference to self after movement.
RenderClusterIterator& RenderClusterIterator::operator--()
{
    return this->operator-=(1);
}

// Routine Description:
// - Advances the iterator forward relative to the underlying text buffer by exactly 1
// Return Value:
// - Value with previous position prior to movement.
RenderClusterIterator RenderClusterIterator::operator++(int)
{
    auto temp(*this);
    operator++();
    return temp;
}

// Routine Description:
// - Advances the iterator backward relative to the underlying text buffer by exactly 1
// Return Value:
// - Value with previous position prior to movement.
RenderClusterIterator RenderClusterIterator::operator--(int)
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
RenderClusterIterator RenderClusterIterator::operator+(const ptrdiff_t& movement)
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
RenderClusterIterator RenderClusterIterator::operator-(const ptrdiff_t& movement)
{
    auto temp(*this);
    temp -= movement;
    return temp;
}

void RenderClusterIterator::_GenerateCluster()
{
    _cluster = Cluster((*_cellIter).Chars(), (*_cellIter).Columns());
}

const Cluster& RenderClusterIterator::operator*() const noexcept
{
    return _cluster;
}

const Cluster* RenderClusterIterator::operator->() const noexcept
{
    return &_cluster;
}
