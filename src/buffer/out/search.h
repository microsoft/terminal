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

enum class SearchFlag : unsigned int
{
    None = 0,

    CaseInsensitive = 1 << 0,
    RegularExpression = 1 << 1,
};

DEFINE_ENUM_FLAG_OPERATORS(SearchFlag);

class Search final
{
public:
    Search() = default;

    bool IsStale(const Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle, SearchFlag flags) const noexcept;
    void Reset(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle, SearchFlag flags, bool reverse);

    void MoveToPoint(til::point anchor) noexcept;
    void MovePastPoint(til::point anchor) noexcept;
    void FindNext(bool reverse) noexcept;

    const til::point_span* GetCurrent() const noexcept;
    bool SelectCurrent() const;

    const std::vector<til::point_span>& Results() const noexcept;
    std::vector<til::point_span>&& ExtractResults() noexcept;
    ptrdiff_t CurrentMatch() const noexcept;
    bool IsOk() const noexcept;

private:
    // _renderData is a pointer so that Search() is constexpr default constructable.
    Microsoft::Console::Render::IRenderData* _renderData = nullptr;
    std::wstring _needle;
    SearchFlag _flags{};
    uint64_t _lastMutationId = 0;

    bool _ok{ false };
    std::vector<til::point_span> _results;
    ptrdiff_t _index = 0;
    ptrdiff_t _step = 0;
};
