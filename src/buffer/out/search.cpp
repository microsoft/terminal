// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "search.h"

#include "textBuffer.hpp"

using namespace Microsoft::Console::Types;

bool Search::IsStale(const Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle, SearchFlag flags) const noexcept
{
    return _renderData != &renderData ||
           _needle != needle ||
           _flags != flags ||
           _lastMutationId != renderData.GetTextBuffer().GetLastMutationId();
}

void Search::Reset(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle, SearchFlag flags, bool reverse)
{
    const auto& textBuffer = renderData.GetTextBuffer();

    _renderData = &renderData;
    _needle = needle;
    _flags = flags;
    _lastMutationId = textBuffer.GetLastMutationId();

    auto result = textBuffer.SearchText(needle, _flags);
    _ok = result.has_value();
    _results = std::move(result).value_or(std::vector<til::point_span>{});
    _index = reverse ? gsl::narrow_cast<ptrdiff_t>(_results.size()) - 1 : 0;
    _step = reverse ? -1 : 1;

    if (_renderData->IsSelectionActive())
    {
        MoveToPoint(_renderData->GetTextBuffer().ScreenToBufferPosition(_renderData->GetSelectionAnchor()));
    }
    else if (const auto span = _renderData->GetSearchHighlightFocused())
    {
        MoveToPoint(_step > 0 ? span->start : span->end);
    }
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
    if (const auto s = GetCurrent())
    {
        // Convert buffer selection offsets into the equivalent screen coordinates
        // required by SelectNewRegion, taking line renditions into account.
        const auto& textBuffer = _renderData->GetTextBuffer();
        const auto selStart = textBuffer.BufferToScreenPosition(s->start);
        const auto selEnd = textBuffer.BufferToScreenPosition(s->end);
        _renderData->SelectNewRegion(selStart, selEnd);
        return true;
    }

    _renderData->ClearSelection();
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
