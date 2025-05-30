#pragma once

#include <vector>
#include <icu.h>

namespace fzf::matcher
{
    struct FzfResult
    {
        int32_t Start;
        int32_t End;
        int32_t Score;
    };

    struct MatchResult
    {
        int32_t Score = 0;
        std::vector<int32_t> Pos;
    };

    struct Pattern
    {
        std::vector<std::vector<UChar32>> terms;
    };

    Pattern ParsePattern(std::wstring_view patternStr);
    std::optional<MatchResult> Match(std::wstring_view text, const Pattern& pattern);
}
