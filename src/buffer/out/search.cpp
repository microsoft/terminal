// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "search.h"

#include "textBuffer.hpp"
#include "UTextAdapter.h"

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

    return true;
}

void Search::QuickSelectRegex(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle, bool caseInsensitive)
{
    _renderData = &renderData;
    _needle = needle;
    _caseInsensitive = caseInsensitive;

    _results = _regexSearch(renderData, needle, caseInsensitive);
    _index = 0;
    _step = 1;
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

void Search::HighlightResults() const
{
    std::vector<til::inclusive_rect> toSelect;
    const auto& textBuffer = _renderData->GetTextBuffer();

    for (const auto& r : _results)
    {
        const auto rbStart = textBuffer.BufferToScreenPosition(r.start);
        const auto rbEnd = textBuffer.BufferToScreenPosition(r.end);

        til::inclusive_rect re;
        re.top = rbStart.y;
        re.bottom = rbEnd.y;
        re.left = rbStart.x;
        re.right = rbEnd.x;

        toSelect.emplace_back(re);
    }

    _renderData->SelectSearchRegions(std::move(toSelect));
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

std::vector<til::point_span> Search::_regexSearch(const Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle, bool caseInsensitive)
{
    std::vector<til::point_span> results;
    UErrorCode status = U_ZERO_ERROR;
    uint32_t flags = 0;
    WI_SetFlagIf(flags, UREGEX_CASE_INSENSITIVE, caseInsensitive);
    const auto& textBuffer = renderData.GetTextBuffer();

    const auto rowCount = textBuffer.GetLastNonSpaceCharacter().y + 1;

    const auto regex = Microsoft::Console::ICU::CreateRegex(needle, flags, &status);
    if (U_FAILURE(status))
    {
        return results;
    }

    const auto viewPortWidth = textBuffer.GetSize().Width();

    for (int32_t i = 0; i < rowCount; i++)
    {
        const auto startRow = i;
        auto uText = Microsoft::Console::ICU::UTextForWrappableRow(textBuffer, i);

        uregex_setUText(regex.get(), &uText, &status);
        if (U_FAILURE(status))
        {
            return results;
        }

        if (uregex_find(regex.get(), -1, &status))
        {
            do
            {
                const int32_t icuStart = uregex_start(regex.get(), 0, &status);
                int32_t icuEnd = uregex_end(regex.get(), 0, &status);
                icuEnd--;

                //Start of line is 0,0 and should be skipped (^)
                if (icuEnd >= 0)
                {
                    const auto matchLength = utext_nativeLength(&uText);
                    auto adjustedMatchStart = icuStart - 1 == matchLength ? icuStart - 1 : icuStart;
                    auto adjustedMatchEnd = std::min(static_cast<int32_t>(matchLength), icuEnd);

                    const size_t matchStartLine = (adjustedMatchStart / viewPortWidth) + startRow;
                    const size_t matchEndLine = (adjustedMatchEnd / viewPortWidth) + startRow;

                    if (matchStartLine > startRow)
                    {
                        adjustedMatchStart %= (matchStartLine - startRow) * viewPortWidth;
                    }

                    if (matchEndLine > startRow)
                    {
                        adjustedMatchEnd %= (matchEndLine - startRow) * viewPortWidth;
                    }

                    auto ps = til::point_span{};
                    ps.start = til::point{ adjustedMatchStart, static_cast<int32_t>(matchStartLine) };
                    ps.end = til::point{ adjustedMatchEnd, static_cast<int32_t>(matchEndLine) };
                    results.emplace_back(ps);
                }
            } while (uregex_findNext(regex.get(), &status));
        }
    }
    return results;
}
