#pragma once

#include <string>
#include <vector>
#include <icu.h>

namespace fzf
{
    namespace matcher
    {
        struct FzfResult
        {
            int32_t Start;
            int32_t End;
            int16_t Score;
        };

        class Pattern
        {
        public:
            std::vector<std::vector<UChar32>> terms;
        };

        std::vector<int32_t> GetPositions(std::wstring_view text, const Pattern& pattern);
        Pattern ParsePattern(const std::wstring_view patternStr);
        int16_t GetScore(std::wstring_view text, const Pattern& pattern);
    }
}
