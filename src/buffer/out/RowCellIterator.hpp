/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- RowCellIterator.hpp

Abstract:
- Read-only view into cells already in the input buffer.
- This is done for performance reasons (avoid heap allocs and copies).

Author:
- Michael Niksa (MiNiksa) 12-Oct-2018

--*/

#pragma once

#include "OutputCellView.hpp"
class ROW;

class RowCellIterator final
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = OutputCellView;
    using difference_type = ptrdiff_t;
    using pointer = OutputCellView*;
    using reference = OutputCellView&;

    RowCellIterator(const ROW& row, const size_t start, const size_t length);
    ~RowCellIterator() = default;

    RowCellIterator& operator=(const RowCellIterator& it) = default;

    operator bool() const noexcept;

    bool operator==(const RowCellIterator& it) const noexcept;
    bool operator!=(const RowCellIterator& it) const noexcept;

    RowCellIterator& operator+=(const ptrdiff_t& movement) noexcept;
    RowCellIterator& operator++() noexcept;
    RowCellIterator operator++(int) noexcept;
    RowCellIterator operator+(const ptrdiff_t& movement) noexcept;

    const OutputCellView& operator*() const noexcept;
    const OutputCellView* operator->() const noexcept;

private:
    const ROW& _row;
    const size_t _start;
    const size_t _length;

    size_t _pos;
    OutputCellView _view;

    void _RefreshView();
    static OutputCellView s_GenerateView(const ROW& row, const size_t pos);
};
