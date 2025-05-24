#include "pch.h"
#include "fuzzy.h"

namespace fuzzy
{
    std::wstring_view trimSuffixSpaces(std::wstring_view input)
    {
        size_t end = input.size();
        while (end > 0 && input[end - 1] == L' ')
        {
            --end;
        }
        return input.substr(0, end);
    }

    std::wstring_view trimStart(const std::wstring_view str)
    {
        const auto off = str.find_first_not_of(L' ');
        return str.substr(std::min(off, str.size()));
    }

    int32_t utf32Length(std::wstring_view str)
    {
        return u_countChar32(reinterpret_cast<const UChar*>(str.data()), static_cast<int32_t>(str.size()));
    }

    static std::vector<UChar32> convertUtf16ToCodePoints(
        std::wstring_view text,
        std::vector<int32_t>* utf16OffsetsOut = nullptr)
    {
        const UChar* data = reinterpret_cast<const UChar*>(text.data());
        int32_t dataLen = static_cast<int32_t>(text.size());
        int32_t cpCount = utf32Length(text);

        std::vector<UChar32> out;
        out.reserve(cpCount);

        if (utf16OffsetsOut)
        {
            utf16OffsetsOut->clear();
            utf16OffsetsOut->reserve(cpCount);
        }

        int32_t src = 0;
        while (src < dataLen)
        {
            auto startUnit = src;
            if (utf16OffsetsOut)
            {
                utf16OffsetsOut->push_back(startUnit);
            }

            UChar32 cp;
            U16_NEXT(data, src, dataLen, cp);

            out.push_back(cp);
        }

        return out;
    }

    static bool equalish(UChar32 a, UChar32 b) noexcept
    {
        return a == b || (a == U'/' && b == U'\\') || (a == U'\\' && b == U'/');
    }

    static int separatorBonus(UChar32 ch) noexcept
    {
        switch (ch)
        {
        case U'/':
        case U'\\':
            return 5;
        case U'_':
        case U'-':
        case U'.':
        case U' ':
        case U'\'':
        case U'\"':
        case U':':
            return 4;
        default:
            return 0;
        }
    }

    static int charScore(UChar32 q, UChar32 qLow, std::optional<UChar32> prev, UChar32 t, UChar32 tLow, int seq) noexcept
    {
        if (!equalish(qLow, tLow))
        {
            return 0;
        }

        int s = 1;
        if (seq)
        {
            s += seq * 5;
        }
        if (q == t)
        {
            s += 1;
        }

        if (prev)
        {
            if (int sb = separatorBonus(*prev); sb)
            {
                s += sb;
            }
            else if (t != tLow && seq == 0)
            {
                s += 2;
            }
        }
        else
        {
            s += 8;
        }
        return s;
    }

    static std::optional<MatchResult> scoreFuzzy(const std::vector<UChar32>& haystack, const std::vector<UChar32>& pattern)
    {
        if (haystack.empty() || pattern.empty() || haystack.size() < pattern.size())
        {
            return std::nullopt;
        }

        std::vector<UChar32> targetLower;
        targetLower.reserve(haystack.size());
        std::vector<UChar32> queryLower;
        queryLower.reserve(pattern.size());
        std::ranges::transform(haystack, std::back_inserter(targetLower), [](UChar32 c) { return u_foldCase(c, U_FOLD_CASE_DEFAULT); });
        std::ranges::transform(pattern, std::back_inserter(queryLower), [](UChar32 c) { return u_foldCase(c, U_FOLD_CASE_DEFAULT); });

        const size_t rows = pattern.size();
        const size_t cols = haystack.size();
        const size_t area = rows * cols;

        std::vector<int> scores(area, 0);
        std::vector<int> matches(area, 0);

        for (size_t queryIndex = 0; queryIndex < rows; ++queryIndex)
        {
            const size_t queryIndexOffset = queryIndex * cols;
            const size_t queryIndexPreviousOffset = queryIndex ? (queryIndex - 1) * cols : 0;

            for (size_t targetIndex = 0; targetIndex < cols; ++targetIndex)
            {
                const size_t currentIndex = queryIndexOffset + targetIndex;
                const size_t diagIndex = (queryIndex && targetIndex) ? queryIndexPreviousOffset + targetIndex - 1 : 0;

                int leftScore = (targetIndex ? scores[currentIndex - 1] : 0);
                int diagScore = (queryIndex && targetIndex) ? scores[diagIndex] : 0;
                int matchesSequenceLen = (queryIndex && targetIndex) ? matches[diagIndex] : 0;

                int score = 0;
                if (diagScore || queryIndex == 0)
                {
                    std::optional<UChar32> previousTarget = targetIndex ? std::optional<UChar32>{ haystack[targetIndex - 1] } : std::nullopt;
                    score = charScore(pattern[queryIndex], queryLower[queryIndex], previousTarget, haystack[targetIndex], targetLower[targetIndex], matchesSequenceLen);
                }

                bool ok = score && diagScore + score >= leftScore;
                if (ok)
                {
                    matches[currentIndex] = matchesSequenceLen + 1;
                    scores[currentIndex] = diagScore + score;
                }
                else
                {
                    matches[currentIndex] = 0;
                    scores[currentIndex] = leftScore;
                }
            }
        }

        std::vector<int32_t> positions;
        if (!pattern.empty() && !haystack.empty())
        {
            size_t queryIndex = rows - 1, targetIndex = cols - 1;
            while (true)
            {
                size_t currentIndex = queryIndex * cols + targetIndex;
                if (matches[currentIndex] == 0)
                {
                    if (targetIndex == 0)
                    {
                        break;
                    }
                    --targetIndex;
                }
                else
                {
                    positions.push_back(static_cast<int32_t>(targetIndex));
                    if (queryIndex == 0 || targetIndex == 0)
                    {
                        break;
                    }
                    --queryIndex;
                    --targetIndex;
                }
            }
            std::ranges::reverse(positions);
        }

        if (positions.empty())
        {
            return std::nullopt;
        }
        return MatchResult{ static_cast<int16_t>(scores.back()), positions };
    }

    static std::vector<int32_t> mapCodepointsToUtf16(
        std::vector<int32_t> const& cpPos,
        std::vector<int32_t> const& cpMap,
        size_t dataLen)
    {
        std::vector<int32_t> utf16pos;
        utf16pos.reserve(cpPos.size() * 2);

        for (int32_t cpIndex : cpPos)
        {
            int32_t start = cpMap[cpIndex];
            int32_t end = cpIndex + int32_t{ 1 } < static_cast<int32_t>(cpMap.size()) ? cpMap[cpIndex + 1] : static_cast<int32_t>(dataLen);

            for (int32_t cu = end - 1; cu >= start; --cu)
            {
                utf16pos.push_back(cu);
            }
        }
        return utf16pos;
    }

    std::optional<MatchResult> Match(std::wstring_view text, const Pattern& pattern)
    {
        int32_t totalScore = 0;
        std::vector<int32_t> finalUtf16Pos;

        for (const auto& term : pattern.terms)
        {
            std::vector<int32_t> utf16map;
            auto textCps = convertUtf16ToCodePoints(text, &utf16map);

            auto res = scoreFuzzy(textCps, term);
            if (!res)
            {
                return std::nullopt;
            }

            auto termUtf16 = mapCodepointsToUtf16(res.value().Pos, utf16map, static_cast<int32_t>(text.size()));
            finalUtf16Pos.insert(finalUtf16Pos.end(),
                                 termUtf16.begin(),
                                 termUtf16.end());
            totalScore += res->Score;
        }

        return MatchResult{ static_cast<int16_t>(totalScore), std::move(finalUtf16Pos) };
    }

    Pattern ParsePattern(const std::wstring_view patternStr)
    {
        Pattern patObj;
        if (patternStr.empty())
        {
            return patObj;
        }

        auto trimmed = trimStart(patternStr);
        trimmed = trimSuffixSpaces(trimmed);

        size_t pos = 0;
        while (pos < trimmed.size())
        {
            size_t found = trimmed.find(L' ', pos);
            auto slice = (found == std::wstring_view::npos) ? trimmed.substr(pos) : trimmed.substr(pos, found - pos);

            patObj.terms.push_back(convertUtf16ToCodePoints(slice));

            if (found == std::wstring_view::npos)
            {
                break;
            }
            pos = found + 1;
        }

        return patObj;
    }
}
