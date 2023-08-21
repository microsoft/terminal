/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- search.h

Abstract:
- This module is used for searching through the screen for a substring

Author(s):
- Michael Niksa (MiNiksa) 20-Apr-2018

Revision History:
- From components of find.c by Jerry Shea (jerrysh) 1-May-1997
--*/

#pragma once

#include "TextAttribute.hpp"
#include "textBuffer.hpp"
#include "../renderer/inc/IRenderData.hpp"

class Search final
{
public:
    Search() = default;
    Search(Microsoft::Console::Render::IRenderData& renderData, std::wstring_view str, bool reverse, bool caseInsensitive);

    bool IsStale() const noexcept;
    bool SelectNext();

    void ColorAll(const TextAttribute& attr) const;

private:
    // _renderData is a pointer so that Search() is constexpr default constructable.
    Microsoft::Console::Render::IRenderData* _renderData = nullptr;
    std::vector<til::point_span> _results;
    ptrdiff_t _index = 0;
    ptrdiff_t _step = 0;
    uint64_t _mutationCount = 0;
};
