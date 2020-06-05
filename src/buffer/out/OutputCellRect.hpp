/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- OutputCellRect.hpp

Abstract:
- Designed to hold a rectangular area of OutputCells where the column/row count is known ahead of time.
- This is done for performance reasons (one big heap allocation block with appropriate views instead of tiny allocations.)
- NOTE: For cases where the internal buffer will not change during your call, use Iterators and Views to completely
        avoid any copy or allocate at all. Only use this when a copy of your content or the buffer is needed.

Author:
- Michael Niksa (MiNiksa) 12-Oct-2018

Revision History:
- Based on work from OutputCell.hpp/cpp by Austin Diviness (AustDi)

--*/

#pragma once

#include "DbcsAttribute.hpp"
#include "TextAttribute.hpp"
#include "OutputCell.hpp"
#include "OutputCellIterator.hpp"

class OutputCellRect final
{
public:
    OutputCellRect() noexcept;
    OutputCellRect(const size_t rows, const size_t cols);

    gsl::span<OutputCell> GetRow(const size_t row);
    OutputCellIterator GetRowIter(const size_t row) const;

    size_t Height() const noexcept;
    size_t Width() const noexcept;

private:
    std::vector<OutputCell> _storage;

    OutputCell* _FindRowOffset(const size_t row);
    const OutputCell* _FindRowOffset(const size_t row) const;

    size_t _cols;
    size_t _rows;
};
