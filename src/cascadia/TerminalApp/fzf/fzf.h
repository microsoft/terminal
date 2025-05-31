#pragma once

#include <vector>
#include <icu.h>

namespace fzf::matcher
{
    struct TextRun
    {
        int32_t Start;
        int32_t End;
    };

    struct MatchResult
    {
        int32_t Score = 0;
        std::vector<TextRun> Runs;
    };

    struct Pattern
    {
        std::vector<std::vector<UChar32>> terms;
    };

    Pattern ParsePattern(std::wstring_view patternStr);
    std::optional<MatchResult> Match(std::wstring_view text, const Pattern& pattern);
}
