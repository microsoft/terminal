#pragma once
#include "fzf/fzf.h"
#include "../renderer/inc/IRenderData.hpp"

namespace Microsoft::Console::Render
{
    class IRenderData;
}

struct FuzzySearchResultRow
{
    til::CoordType startRowNumber;
    std::vector<int32_t> positions;
};

class FuzzySearch
{
public:
    FuzzySearch();
    std::vector<FuzzySearchResultRow> Search(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle) const;
    ~FuzzySearch();
private:
    fzf_slab_t* _fzfSlab;
};
