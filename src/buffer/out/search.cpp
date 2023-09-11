// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "search.h"

#include "textBuffer.hpp"

using namespace Microsoft::Console::Types;

bool Search::ResetIfStale(Microsoft::Console::Render::IRenderData& renderData)
{
    return ResetIfStale(renderData,
                        _needle,
                        _step == -1, // this is the opposite of the initializer below
                        _caseInsensitive);
}

bool Search::ResetIfStale(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle, bool reverse, bool caseInsensitive)
{
    const auto& textBuffer = renderData.GetTextBuffer();
    const auto lastMutationId = textBuffer.GetLastMutationId();

    if (_needle == needle &&
        _reverse == reverse &&
        _caseInsensitive == caseInsensitive &&
        _lastMutationId == lastMutationId)
    {
        return false;
    }

    _renderData = &renderData;
    _needle = needle;
    _reverse = reverse;
    _caseInsensitive = caseInsensitive;
    _lastMutationId = lastMutationId;

    _results = textBuffer.SearchText(needle, caseInsensitive);
    _index = reverse ? gsl::narrow_cast<ptrdiff_t>(_results.size()) - 1 : 0;
    _step = reverse ? -1 : 1;

    return true;
}

void Search::MovePastCurrentSelection()
{
    if (_renderData->IsSelectionActive())
    {
        MovePastPoint(_renderData->GetTextBuffer().ScreenToBufferPosition(_renderData->GetSelectionAnchor()));
    }
}

void Search::MovePastPoint(const til::point anchor) noexcept
{
    if (_results.empty())
    {
        return;
    }

    const auto count = gsl::narrow_cast<ptrdiff_t>(_results.size());
    const auto highestIndex = count - 1;
    auto index = _reverse ? highestIndex : 0;

    if (_reverse)
    {
        for (; index >= 0 && til::at(_results, index).start >= anchor; --index)
        {
        }
    }
    else
    {
        for (; index <= highestIndex && til::at(_results, index).start <= anchor; ++index)
        {
        }
    }

    _index = (index + count) % count;
}

void Search::FindNext() noexcept
{
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

    return false;
}

const std::vector<til::point_span>& Search::Results() const noexcept
{
    return _results;
}

size_t Search::CurrentMatch() const noexcept
{
    return _index;
}
