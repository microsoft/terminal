#pragma once

#include <vector>
#include <icu.h>

namespace fuzzy
{
    struct MatchResult
    {
        int16_t Score = 0;
        std::vector<int32_t> Pos;
    };

    class Pattern
    {
    public:
        std::vector<std::vector<UChar32>> terms;
    };

    Pattern ParsePattern(const std::wstring_view patternStr);
    std::optional<MatchResult> Match(std::wstring_view text, const Pattern& pattern);
}
