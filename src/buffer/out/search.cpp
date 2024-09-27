// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "search.h"

#include "textBuffer.hpp"

using namespace Microsoft::Console::Types;

bool Search::IsStale(const Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle, SearchFlag flags) const noexcept
{
    UNREFERENCED_PARAMETER(renderData);
    UNREFERENCED_PARAMETER(needle);
    UNREFERENCED_PARAMETER(flags);
    return false;
}

void Search::Reset(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle, SearchFlag flags, bool reverse)
{
    UNREFERENCED_PARAMETER(renderData);
    UNREFERENCED_PARAMETER(needle);
    UNREFERENCED_PARAMETER(flags);
    UNREFERENCED_PARAMETER(reverse);
}

void Search::MoveToPoint(const til::point anchor) noexcept
{
    if (_results.empty())
    {
        return;
    }

    const auto count = gsl::narrow_cast<ptrdiff_t>(_results.size());
    ptrdiff_t index = 0;

    if (_step < 0)
    {
        for (index = count - 1; index >= 0 && til::at(_results, index).start > anchor; --index)
        {
        }
    }
    else
    {
        for (index = 0; index < count && til::at(_results, index).start < anchor; ++index)
        {
        }
    }

    _index = (index + count) % count;
}

void Search::MovePastPoint(const til::point anchor) noexcept
{
    if (_results.empty())
    {
        return;
    }

    const auto count = gsl::narrow_cast<ptrdiff_t>(_results.size());
    ptrdiff_t index = 0;

    if (_step < 0)
    {
        for (index = count - 1; index >= 0 && til::at(_results, index).start >= anchor; --index)
        {
        }
    }
    else
    {
        for (index = 0; index < count && til::at(_results, index).start <= anchor; ++index)
        {
        }
    }

    _index = (index + count) % count;
}

void Search::FindNext(bool reverse) noexcept
{
    _step = reverse ? -1 : 1;
    if (const auto count{ gsl::narrow_cast<ptrdiff_t>(_results.size()) })
    {
        _index = (_index + _step + count) % count;
    }
}

const til::point_span* Search::GetCurrent() const noexcept
{
    const auto index = gsl::narrow_cast<size_t>(_index);
    if (index < _results.size())
    {
        return &til::at(_results, index);
    }
    return nullptr;
}

// Routine Description:
// - Takes the found word and selects it in the screen buffer

bool Search::SelectCurrent() const
{
    return false;
}

const std::vector<til::point_span>& Search::Results() const noexcept
{
    return _results;
}

std::vector<til::point_span>&& Search::ExtractResults() noexcept
{
    return std::move(_results);
}

ptrdiff_t Search::CurrentMatch() const noexcept
{
    return _index;
}

bool Search::IsOk() const noexcept
{
    return _ok;
}
