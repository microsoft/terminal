// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "search.h"

#include "textBuffer.hpp"

using namespace Microsoft::Console::Types;

bool Search::ResetIfStale(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle, bool reverse, bool caseInsensitive)
{
    const auto& textBuffer = renderData.GetTextBuffer();
    const auto lastMutationId = textBuffer.GetLastMutationId();

    if (_needle == needle &&
        _caseInsensitive == caseInsensitive &&
        _lastMutationId == lastMutationId)
    {
        _step = reverse ? -1 : 1;
        return false;
    }

    _renderData = &renderData;
    _needle = needle;
    _caseInsensitive = caseInsensitive;
    _lastMutationId = lastMutationId;

    _results = textBuffer.SearchText(needle, caseInsensitive);
    _index = reverse ? gsl::narrow_cast<ptrdiff_t>(_results.size()) - 1 : 0;
    _step = reverse ? -1 : 1;

    _setResultInData();
    _setCurrentInData();
    return true;
}

void Search::MoveToCurrentSelection()
{
    if (_renderData->IsSelectionActive())
    {
        MoveToPoint(_renderData->GetTextBuffer().ScreenToBufferPosition(_renderData->GetSelectionAnchor()));
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
    _setCurrentInData();
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
    _setCurrentInData();
}

void Search::FindNext() noexcept
{
    if (const auto count{ gsl::narrow_cast<ptrdiff_t>(_results.size()) })
    {
        _index = (_index + _step + count) % count;
    }

    _setCurrentInData();
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

ptrdiff_t Search::CurrentMatch() const noexcept
{
    return _index;
}

void Search::_setResultInData() const
{
    const auto& buffer = _renderData->GetTextBuffer();
    std::vector<til::inclusive_rect> resultRects;
    for (const auto& result : _results)
    {
        auto bufferRects = buffer.GetTextRects(result.start, result.end, false, true);
        resultRects.insert(resultRects.end(), bufferRects.begin(), bufferRects.end());
    }

    _renderData->SetSearchHighlights(std::move(resultRects));
}

void Search::_setCurrentInData() const noexcept
try
{
    if (const auto current = GetCurrent())
    {
        const auto& buffer = _renderData->GetTextBuffer();
        auto bufferRects = buffer.GetTextRects(current->start, current->end, false, true);
        _renderData->SetSearchHighlightFocused(std::move(bufferRects));
    }
    else
    {
        _renderData->SetSearchHighlightFocused({});
    }
}
CATCH_LOG()