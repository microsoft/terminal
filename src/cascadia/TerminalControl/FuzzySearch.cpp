#include "pch.h"
#include "FuzzySearch.h"
#include "../../buffer/out/textBuffer.hpp"

#include "../../buffer/out/UTextAdapter.h"
#include "fzf/fzf.h"

FuzzySearch::FuzzySearch()
{
    _fzfSlab = fzf_make_default_slab();
}

std::vector<FuzzySearchResultRow> FuzzySearch::Search(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle) const
{
    struct RowResult
    {
        UText text;
        int score;
        til::CoordType startRowNumber;
        long long length;
    };

    auto searchResults = std::vector<FuzzySearchResultRow>();
    const auto& textBuffer = renderData.GetTextBuffer();

    auto searchTextNotBlank = std::ranges::any_of(needle, [](const wchar_t ch) {
        return !std::iswspace(ch);
    });

    if (!searchTextNotBlank)
    {
        return {};
    }

    const UChar* uPattern = reinterpret_cast<const UChar*>(needle.data());
    ufzf_pattern_t* fzfPattern = ufzf_parse_pattern(CaseSmart, false, uPattern, true);

    constexpr int32_t maxResults = 100;
    auto rowResults = std::vector<RowResult>();
    auto rowCount = textBuffer.GetLastNonSpaceCharacter().y + 1;
    //overkill?
    rowResults.reserve(rowCount);
    searchResults.reserve(maxResults);
    int minScore = 1;

    for (int rowNumber = 0; rowNumber < rowCount; rowNumber++)
    {
        const auto startRowNumber = rowNumber;
        //Warn: row number will be incremented by this function if there is a row wrap.  Is there a better way
        auto uRowText = Microsoft::Console::ICU::UTextForWrappableRow(textBuffer, rowNumber);
        auto length = utext_nativeLength(&uRowText);

        if (length > 0)
        {
            int icuRowScore = ufzf_get_score(&uRowText, fzfPattern, _fzfSlab);
            if (icuRowScore >= minScore)
            {
                auto rowResult = RowResult{};
                //I think this is small enough to copy
                rowResult.text = uRowText;
                rowResult.startRowNumber = startRowNumber;
                rowResult.score = icuRowScore;
                rowResult.length = length;
                rowResults.push_back(rowResult);
            }
        }
    }

    //sort so the highest scores and shortest lengths are first
    std::ranges::sort(rowResults, [](const auto& a, const auto& b) {
        if (a.score != b.score)
        {
            return a.score > b.score;
        }
        return a.length < b.length;
    });

    for (size_t rank = 0; rank < rowResults.size(); rank++)
    {
        auto rowResult = rowResults[rank];
        if (rank <= maxResults)
        {
            fzf_position_t* fzfPositions = ufzf_get_positions(&rowResult.text, fzfPattern, _fzfSlab);

            //This is likely the result of an inverse search that didn't have any positions
            //We want to return it in the results.  It just won't have any highlights
            if (!fzfPositions || fzfPositions->size == 0)
            {
                searchResults.emplace_back(FuzzySearchResultRow{ rowResult.startRowNumber, {} });
                fzf_free_positions(fzfPositions);
            }
            else
            {
                std::vector<int32_t> vec;
                vec.reserve(fzfPositions->size);

                for (size_t i = 0; i < fzfPositions->size; ++i)
                {
                    vec.push_back(static_cast<int32_t>(fzfPositions->data[i]));
                }

                searchResults.emplace_back(FuzzySearchResultRow{ rowResult.startRowNumber, vec });
                fzf_free_positions(fzfPositions);
            }
        }
        utext_close(&rowResult.text);
    }

    ufzf_free_pattern(fzfPattern);

    return searchResults;
}

FuzzySearch::~FuzzySearch()
{
    fzf_free_slab(_fzfSlab);
}
