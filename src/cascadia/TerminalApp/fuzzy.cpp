#include "pch.h"
#include "fuzzy.h"

namespace fuzzy
{


    int32_t utf32Length(std::wstring_view str)
    {
        return u_countChar32(reinterpret_cast<const UChar*>(str.data()), static_cast<int32_t>(str.size()));
    }

    static std::vector<UChar32> ConvertUtf16ToCodePoints(
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

    static int sep_bonus(UChar32 ch) noexcept
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

    static int char_score(UChar32 q, UChar32 qLow, std::optional<UChar32> prev, UChar32 t, UChar32 tLow, int seq) noexcept
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
            if (int sb = sep_bonus(*prev); sb)
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

    static std::optional<MatchResult> fuzzy_search(const std::vector<UChar32>& text, const std::vector<UChar32>& pattern)
    {
        if (text.empty() || pattern.empty() || text.size() < pattern.size())
        {
            return std::nullopt;
        }

        std::vector<UChar32> textLow;
        textLow.reserve(text.size());
        std::vector<UChar32> pattLow;
        pattLow.reserve(pattern.size());
        std::ranges::transform(text, std::back_inserter(textLow), [](UChar32 c) { return u_foldCase(c, U_FOLD_CASE_DEFAULT); });
        std::ranges::transform(pattern, std::back_inserter(pattLow), [](UChar32 c) { return u_foldCase(c, U_FOLD_CASE_DEFAULT); });

        const size_t rows = pattern.size();
        const size_t cols = text.size();
        const size_t area = rows * cols;

        std::vector<int> score(area, 0);
        std::vector<int> run(area, 0);

        for (size_t qi = 0; qi < rows; ++qi)
        {
            const size_t ro = qi * cols;
            const size_t po = qi ? (qi - 1) * cols : 0;

            for (size_t ti = 0; ti < cols; ++ti)
            {
                const size_t idx = ro + ti;
                const size_t didx = (qi && ti) ? po + ti - 1 : 0;

                int left = (ti ? score[idx - 1] : 0);
                int diag = (qi && ti) ? score[didx] : 0;
                int seq = (qi && ti) ? run[didx] : 0;

                int s = 0;
                if (diag || qi == 0)
                {
                    std::optional<UChar32> prev = ti ? std::optional<UChar32>{ text[ti - 1] } : std::nullopt;
                    s = char_score(pattern[qi], pattLow[qi], prev, text[ti], textLow[ti], seq);
                }

                bool ok = s && diag + s >= left;
                if (ok)
                {
                    run[idx] = seq + 1;
                    score[idx] = diag + s;
                }
                else
                {
                    run[idx] = 0;
                    score[idx] = left;
                }
            }
        }

        std::vector<int32_t> cpPos;
        if (!pattern.empty() && !text.empty())
        {
            size_t qi = rows - 1, ti = cols - 1;
            while (true)
            {
                size_t idx = qi * cols + ti;
                if (run[idx] == 0)
                {
                    if (ti == 0)
                    {
                        break;
                    }
                    --ti;
                }
                else
                {
                    cpPos.push_back(static_cast<int32_t>(ti));
                    if (qi == 0 || ti == 0)
                    {
                        break;
                    }
                    --qi;
                    --ti;
                }
            }
            std::ranges::reverse(cpPos);
        }

        if (cpPos.empty())
        {
            return std::nullopt;
        }
        return MatchResult{ static_cast<int16_t>(score.back()), cpPos };
    }

    static std::vector<int32_t> MapCodepointsToUtf16(
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
            auto textCps = ConvertUtf16ToCodePoints(text, &utf16map);

            auto res = fuzzy_search(textCps, term);
            if (!res)
            {
                return std::nullopt;
            }

            auto termUtf16 = MapCodepointsToUtf16(res.value().Pos, utf16map, static_cast<int32_t>(text.size()));
            finalUtf16Pos.insert(finalUtf16Pos.end(),
                                 termUtf16.begin(),
                                 termUtf16.end());
            totalScore += res->Score;
        }

        return MatchResult{ static_cast<int16_t>(totalScore), std::move(finalUtf16Pos) };
    }

    std::wstring_view TrimSuffixSpaces(std::wstring_view input)
    {
        size_t end = input.size();
        while (end > 0 && input[end - 1] == L' ')
        {
            --end;
        }
        return input.substr(0, end);
    }

    std::wstring_view TrimStart(const std::wstring_view str)
    {
        const auto off = str.find_first_not_of(L' ');
        return str.substr(std::min(off, str.size()));
    }

    Pattern ParsePattern(const std::wstring_view patternStr)
    {
        Pattern patObj;
        if (patternStr.empty())
        {
            return patObj;
        }

        auto trimmed = TrimStart(patternStr);
        trimmed = TrimSuffixSpaces(trimmed);

        size_t pos = 0;
        while (pos < trimmed.size())
        {
            size_t found = trimmed.find(L' ', pos);
            auto slice = (found == std::wstring_view::npos) ? trimmed.substr(pos) : trimmed.substr(pos, found - pos);

            patObj.terms.push_back(ConvertUtf16ToCodePoints(slice));

            if (found == std::wstring_view::npos)
            {
                break;
            }
            pos = found + 1;
        }

        return patObj;
    }
}
