// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "OutputCellRect.hpp"

// Routine Description:
// - Constructs an in-memory region for holding a copy of output buffer cell data.
// - NOTE: This creatively skips the constructors for every cell. You must fill
//   every single cell in this rectangle before iterating/reading for it to be valid.
// - NOTE: This is designed for perf-sensitive paths ONLY. Take care.
// Arguments:
// - rows - Rows in the rectangle (height)
// - cols - Columns in the rectangle (width)
OutputCellRect::OutputCellRect(const til::size size) :
    _size(size)
{
    _storage.resize(size.area<size_t>());
}

// Routine Description:
// - Gets a read/write span over a single row inside the rectangle.
// Arguments:
// - row - The Y position or row index in the buffer.
// Return Value:
// - Read/write span of OutputCells
gsl::span<OutputCell> OutputCellRect::GetRow(const til::CoordType row)
{
    return gsl::span<OutputCell>(_FindRowOffset(row), _size.width);
}

// Routine Description:
// - Gets a read-only iterator view over a single row of the rectangle.
// Arguments:
// - row - The Y position or row index in the buffer.
// Return Value:
// - Read-only iterator of OutputCells
OutputCellIterator OutputCellRect::GetRowIter(const til::CoordType row) const
{
    const gsl::span<const OutputCell> view(_FindRowOffset(row), _size.width);

    return OutputCellIterator(view);
}

// Routine Description:
// - Internal helper to find the pointer to the specific row offset in the giant
//   contiguous block of memory allocated for this rectangle.
// Arguments:
// - row - The Y position or row index in the buffer.
// Return Value:
// - Pointer to the location in the rectangle that represents the start of the requested row.
OutputCell* OutputCellRect::_FindRowOffset(const til::CoordType row)
{
    return &_storage.at(row * _size.width);
}

// Routine Description:
// - Internal helper to find the pointer to the specific row offset in the giant
//   contiguous block of memory allocated for this rectangle.
// Arguments:
// - row - The Y position or row index in the buffer.
// Return Value:
// - Pointer to the location in the rectangle that represents the start of the requested row.
const OutputCell* OutputCellRect::_FindRowOffset(const til::CoordType row) const
{
    return &_storage.at(row * _size.width);
}

// Routine Description:
// - Gets the height of the rectangle
// Return Value:
// - Height
til::CoordType OutputCellRect::Height() const noexcept
{
    return _size.height;
}

// Routine Description:
// - Gets the width of the rectangle
// Return Value:
// - Width
til::CoordType OutputCellRect::Width() const noexcept
{
    return _size.width;
}
