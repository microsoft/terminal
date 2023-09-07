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

#include "textBuffer.hpp"
#include "../renderer/inc/IRenderData.hpp"

class Search final
{
public:
    Search() = default;

    bool ResetIfStale(Microsoft::Console::Render::IRenderData& renderData);
    bool ResetIfStale(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle, bool reverse, bool caseInsensitive);

    void MovePastCurrentSelection();
    void MovePastPoint(til::point anchor) noexcept;
    void FindNext() noexcept;

    const til::point_span* GetCurrent() const noexcept;
    bool SelectCurrent() const;

    const std::vector<til::point_span>& Results() const noexcept;
    size_t CurrentMatch() const noexcept;
    bool CurrentDirection() const noexcept;

private:
    // _renderData is a pointer so that Search() is constexpr default constructable.
    Microsoft::Console::Render::IRenderData* _renderData = nullptr;
    std::wstring _needle;
    bool _reverse = false;
    bool _caseInsensitive = false;
    uint64_t _lastMutationId = 0;

    std::vector<til::point_span> _results;
    ptrdiff_t _index = 0;
    ptrdiff_t _step = 0;
};
