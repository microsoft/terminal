/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- textBufferCellIterator.hpp

Abstract:
- This module abstracts walking through text on the screen
- It is currently intended for read-only operations

Author(s):
- Michael Niksa (MiNiksa) 29-Jun-2018
--*/

#pragma once

#include "AttrRowIterator.hpp"
#include "CharRow.hpp"
#include "OutputCellView.hpp"
#include "../../types/inc/viewport.hpp"

class TextBuffer;

class TextBufferCellIterator
{
public:
    TextBufferCellIterator(const TextBuffer& buffer, COORD pos);
    TextBufferCellIterator(const TextBuffer& buffer, COORD pos, const Microsoft::Console::Types::Viewport limits);

    operator bool() const noexcept;

    bool operator==(const TextBufferCellIterator& it) const;
    bool operator!=(const TextBufferCellIterator& it) const;

    TextBufferCellIterator& operator+=(const ptrdiff_t& movement);
    TextBufferCellIterator& operator-=(const ptrdiff_t& movement);
    TextBufferCellIterator& operator++();
    TextBufferCellIterator& operator--();
    TextBufferCellIterator operator++(int);
    TextBufferCellIterator operator--(int);
    TextBufferCellIterator operator+(const ptrdiff_t& movement);
    TextBufferCellIterator operator-(const ptrdiff_t& movement);

    ptrdiff_t operator-(const TextBufferCellIterator& it);

    const OutputCellView& operator*() const noexcept;
    const OutputCellView* operator->() const noexcept;

protected:
    void _SetPos(const COORD newPos);
    void _GenerateView();
    static const ROW* s_GetRow(const TextBuffer& buffer, const COORD pos);

    OutputCellView _view;

    const ROW* _pRow;
    AttrRowIterator _attrIter;
    const TextBuffer& _buffer;
    const Microsoft::Console::Types::Viewport _bounds;
    bool _exceeded;
    COORD _pos;

#if UNIT_TESTING
    friend class TextBufferIteratorTests;
    friend class TextBufferTests;
    friend class ApiRoutinesTests;
#endif
};
