// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "../inc/RenderClusterIterator.hpp"

#include "../../buffer/out/textBufferCellIterator.hpp"

using namespace Microsoft::Console::Render;

// Routine Description:
// - Creates a new read-only iterator to seek through cluster data stored in cells
// Arguments:
// - cellIter - Text buffer cells to seek through
RenderClusterIterator::RenderClusterIterator(TextBufferCellIterator& cellIter) :
    _cellIter(cellIter),
    _cluster(L"", 0),
    _attr((*cellIter).TextAttr()),
    _distance(0),
    _exceeded(false)
{
    _GenerateCluster();
}

// Routine Description:
// - Tells if the iterator is still valid. The iterator will be invalidated when it meets a text cell that
//     has different text attribute from the cell where the iteration starts, which pratically separate each
//     runs of the text.
// - See also: Microsoft::Console::Render::Renderer::_PaintBufferOutputHelper
// Return Value:
// - True if this iterator is still inside the bounds of current run. False if we've passed the run boundary.
RenderClusterIterator::operator bool() const noexcept
{
    return !_exceeded;
}

// Routine Description:
// - Compares two iterators to see if they're pointing to the same cluster in the same text buffer
// Arguments:
// - it - The other iterator to compare to this one.
// Return Value:
// - True if it's the same text buffer and same cluster position. False otherwise.
bool RenderClusterIterator::operator==(const RenderClusterIterator& it) const
{
    return _attr == it._attr &&
           &_cellIter == &it._cellIter &&
           &_distance == &it._distance &&
           _exceeded == it._exceeded;
}

// Routine Description:
// - Compares two iterators to see if they're pointing to the different cluster in the same buffer or different buffer entirely.
// Arguments:
// - it - The other iterator to compare to this one.
// Return Value:
// - True if it's the same text buffer and different cluster or if they're different buffers. False otherwise.
bool RenderClusterIterator::operator!=(const RenderClusterIterator& it) const
{
    return !(*this == it);
}

// Routine Description:
// - Advances the iterator forward relative to the underlying text buffer by the specified movement
// Arguments:
// - movement - Magnitude and direction of movement.
// Return Value:
// - Reference to self after movement.
RenderClusterIterator& RenderClusterIterator::operator+=(const ptrdiff_t& movement)
{
    ptrdiff_t move = movement;
    auto newCellIter = _cellIter;
    size_t cols = 0;
    while (move > 0 && !_exceeded)
    {
        bool skip = (*newCellIter).DbcsAttr().IsTrailing();
        if (!skip)
        {
            cols += (*newCellIter).Columns();
        }
        ++newCellIter;
        _exceeded = !(newCellIter && (*newCellIter).TextAttr() == _attr);
        move--;
    }
    while (move < 0 && !_exceeded)
    {
        bool skip = (*newCellIter).DbcsAttr().IsTrailing();
        if (!skip)
        {
            cols -= (*newCellIter).Columns();
        }
        --newCellIter;
        _exceeded = !(newCellIter && (*newCellIter).TextAttr() == _attr);
        move++;
    }

    _cellIter += movement;
    _GenerateCluster();
    _distance += cols;

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


// Routine Description:
// - Updates the internal cluster. Call after updating underlying cell position.
void RenderClusterIterator::_GenerateCluster()
{
    _cluster = Cluster((*_cellIter).Chars(), (*_cellIter).Columns());
}

// Routine Description:
// - Provides cluster data of the corresponding text buffer cell.
// Arguments:
// - <none> - Uses current position
// Return Value:
// - Cluster data of current text buffer cell.
const Cluster& RenderClusterIterator::operator*() const noexcept
{
    return _cluster;
}

// Routine Description:
// - Provides cluster data of the corresponding text buffer cell.
// Arguments:
// - <none> - Uses current position
// Return Value:
// - Cluster data of current text buffer cell.
const Cluster* RenderClusterIterator::operator->() const noexcept
{
    return &_cluster;
}

// Routine Description:
// - Gets the distance between two iterators relative to the number of columns needed for rendering.
// Return Value:
// - The number of columns consumed for rendering between these two iterators.
ptrdiff_t RenderClusterIterator::GetClusterDistance(RenderClusterIterator other) const noexcept
{
    return _distance - other._distance;
}
