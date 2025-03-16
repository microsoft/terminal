#pragma once

#include <string>
#include <vector>

namespace fzf
{
    namespace matcher
    {
        struct FzfResult
        {
            int Start;
            int End;
            int Score;
        };

        class Pattern
        {
        public:
            std::vector<std::wstring> terms;
        };

        std::vector<int> GetPositions(std::wstring_view text, const Pattern& pattern);
        Pattern ParsePattern(const std::wstring_view patternStr);
        int GetScore(std::wstring_view text, const Pattern& pattern);
    }
}
