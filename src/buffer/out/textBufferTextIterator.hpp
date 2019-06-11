/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- textBufferTextIterator.hpp

Abstract:
- This module abstracts walking through text on the screen
- It is currently intended for read-only operations

Author(s):
- Michael Niksa (MiNiksa) 01-May-2018
--*/

#pragma once

#include "textBufferCellIterator.hpp"

class SCREEN_INFORMATION;

class TextBufferTextIterator final : public TextBufferCellIterator
{
public:
    TextBufferTextIterator(const TextBufferCellIterator& cellIter);

    const std::wstring_view operator*() const;
    const std::wstring_view* operator->() const;

protected:
#if UNIT_TESTING
    friend class TextBufferIteratorTests;
#endif
};
