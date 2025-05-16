#include "pch.h"
#include "fzf.h"
#include <algorithm>

namespace fzf
{
    namespace matcher
    {
        constexpr int16_t ScoreMatch = 16;
        constexpr int16_t ScoreGapStart = -3;
        constexpr int16_t ScoreGapExtension = -1;
        constexpr int16_t BoundaryBonus = ScoreMatch / 2;
        constexpr int16_t NonWordBonus = ScoreMatch / 2;
        constexpr int16_t CamelCaseBonus = BoundaryBonus + ScoreGapExtension;
        constexpr int16_t BonusConsecutive = -(ScoreGapStart + ScoreGapExtension);
        constexpr int16_t BonusFirstCharMultiplier = 2;

        enum CharClass : uint8_t
        {
            NonWord = 0,
            CharLower = 1,
            CharUpper = 2,
            Digit = 3,
        };

        std::wstring_view TrimStart(const std::wstring_view str)
        {
            const auto off = str.find_first_not_of(L' ');
            return str.substr(std::min(off, str.size()));
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

        UChar32 FoldCase(UChar32 c) noexcept
        {
            return u_foldCase(c, U_FOLD_CASE_DEFAULT);
        }

        int32_t IndexOfChar(const std::vector<UChar32>& input, const UChar32 searchChar, int32_t startIndex)
        {
            for (int32_t i = startIndex; i < static_cast<int32_t>(input.size()); ++i)
            {
                if (input[i] == searchChar)
                {
                    return i;
                }
            }
            return -1;
        }

        int32_t FuzzyIndexOf(const std::vector<UChar32>& input, const std::vector<UChar32>& pattern)
        {
            int32_t idx = 0;
            int32_t firstIdx = 0;
            for (int32_t pi = 0; pi < static_cast<int32_t>(pattern.size()); ++pi)
            {
                idx = IndexOfChar(input, pattern[pi], idx);
                if (idx < 0)
                {
                    return -1;
                }

                if (pi == 0 && idx > 0)
                {
                    firstIdx = idx - 1;
                }

                idx++;
            }
            return firstIdx;
        }

        int16_t CalculateBonus(CharClass prevClass, CharClass currentClass)
        {
            if (prevClass == NonWord && currentClass != NonWord)
            {
                return BoundaryBonus;
            }
            if ((prevClass == CharLower && currentClass == CharUpper) ||
                (prevClass != Digit && currentClass == Digit))
            {
                return CamelCaseBonus;
            }
            if (currentClass == NonWord)
            {
                return NonWordBonus;
            }
            return 0;
        }

        static constexpr auto s_charClassLut = []() {
            std::array<CharClass, U_CHAR_CATEGORY_COUNT> lut{};
            lut.fill(CharClass::NonWord);
            lut[U_UPPERCASE_LETTER] = CharUpper;
            lut[U_LOWERCASE_LETTER] = CharLower;
            lut[U_MODIFIER_LETTER] = CharLower;
            lut[U_OTHER_LETTER] = CharLower;
            lut[U_DECIMAL_DIGIT_NUMBER] = Digit;
            return lut;
        }();

        CharClass ClassOf(UChar32 ch)
        {
            return s_charClassLut[u_charType(ch)];
        }

        FzfResult FzfFuzzyMatchV2(const std::vector<UChar32>& text, const std::vector<UChar32>& pattern, std::vector<int32_t>* pos)
        {
            int32_t patternSize = static_cast<int32_t>(pattern.size());
            int32_t textSize = static_cast<int32_t>(text.size());

            if (patternSize == 0)
            {
                return { 0, 0, 0 };
            }

            std::vector<UChar32> foldedText;
            foldedText.reserve(text.size());
            for (auto cp : text)
            {
                auto foldedCp = u_foldCase(cp, U_FOLD_CASE_DEFAULT);
                foldedText.push_back(foldedCp);
            }

            int32_t firstIndexOf = FuzzyIndexOf(foldedText, pattern);
            if (firstIndexOf < 0)
            {
                return { -1, -1, 0 };
            }

            auto initialScores = std::vector<int16_t>(textSize);
            auto consecutiveScores = std::vector<int16_t>(textSize);
            auto firstOccurrenceOfEachChar = std::vector<int32_t>(patternSize);
            auto bonusesSpan = std::vector<int16_t>(textSize);

            int16_t maxScore = 0;
            int32_t maxScorePos = 0;
            int32_t patternIndex = 0;
            int32_t lastIndex = 0;
            UChar32 firstPatternChar = pattern[0];
            UChar32 currentPatternChar = pattern[0];
            int16_t previousInitialScore = 0;
            CharClass previousClass = NonWord;
            bool inGap = false;

            std::span<const UChar32> lowerText(foldedText);
            auto lowerTextSlice = lowerText.subspan(firstIndexOf);
            auto initialScoresSlice = std::span(initialScores).subspan(firstIndexOf);
            auto consecutiveScoresSlice = std::span(consecutiveScores).subspan(firstIndexOf);
            auto bonusesSlice = std::span(bonusesSpan).subspan(firstIndexOf, textSize - firstIndexOf);

            for (int32_t i = 0; i < static_cast<int32_t>(lowerTextSlice.size()); i++)
            {
                UChar32 currentChar = lowerTextSlice[i];
                CharClass currentClass = ClassOf(currentChar);
                int16_t bonus = CalculateBonus(previousClass, currentClass);
                bonusesSlice[i] = bonus;
                previousClass = currentClass;

                //currentPatternChar was already folded in ParsePattern
                if (currentChar == currentPatternChar)
                {
                    if (patternIndex < static_cast<int32_t>(pattern.size()))
                    {
                        firstOccurrenceOfEachChar[patternIndex] = firstIndexOf + static_cast<int32_t>(i);
                        patternIndex++;
                        if (patternIndex < patternSize)
                        {
                            currentPatternChar = pattern[patternIndex];
                        }
                    }
                    lastIndex = firstIndexOf + static_cast<int32_t>(i);
                }
                if (currentChar == firstPatternChar)
                {
                    int16_t score = ScoreMatch + bonus * BonusFirstCharMultiplier;
                    initialScoresSlice[i] = score;
                    consecutiveScoresSlice[i] = 1;
                    if (patternSize == 1 && (score > maxScore))
                    {
                        maxScore = score;
                        maxScorePos = firstIndexOf + static_cast<int32_t>(i);
                        if (bonus == BoundaryBonus)
                        {
                            break;
                        }
                    }
                    inGap = false;
                }
                else
                {
                    initialScoresSlice[i] = inGap ? std::max<int16_t>(previousInitialScore + ScoreGapExtension, 0) : std::max<int16_t>(previousInitialScore + ScoreGapStart, 0);
                    consecutiveScoresSlice[i] = 0;
                    inGap = true;
                }
                previousInitialScore = initialScoresSlice[i];
            }

            if (patternIndex != static_cast<int32_t>(pattern.size()))
            {
                return { -1, -1, 0 };
            }

            if (pattern.size() == 1)
            {
                if (pos)
                {
                    pos->push_back(maxScorePos);
                }
                int32_t end = maxScorePos + 1;
                return { maxScorePos, end, maxScore };
            }

            int32_t firstOccurrenceOfFirstChar = firstOccurrenceOfEachChar[0];
            int32_t width = lastIndex - firstOccurrenceOfFirstChar + 1;
            int32_t rows = static_cast<int32_t>(pattern.size());
            auto consecutiveCharMatrixSize = width * patternSize;

            std::vector<int16_t> scoreMatrix(width * rows);
            std::copy_n(
                initialScores.begin() + firstOccurrenceOfFirstChar,
                width,
                scoreMatrix.begin());
            auto scoreSpan = std::span<int16_t>(scoreMatrix);

            std::vector<int16_t> consecutiveCharMatrix(width * rows);
            std::copy_n(
                consecutiveScores.begin() + firstOccurrenceOfFirstChar,
                width,
                consecutiveCharMatrix.begin());
            auto consecutiveCharMatrixSpan = std::span(consecutiveCharMatrix);

            auto patternSliceStr = std::span(pattern).subspan(1);

            for (int32_t off = 0; off < patternSize - 1; off++)
            {
                auto patternCharOffset = firstOccurrenceOfEachChar[off + 1];
                auto sliceLen = lastIndex - patternCharOffset + 1;
                currentPatternChar = patternSliceStr[off];
                patternIndex = off + 1;
                int32_t row = patternIndex * width;
                inGap = false;
                auto tmp = lowerText.subspan(patternCharOffset, sliceLen);
                std::span<const UChar32> textSlice = lowerText.subspan(patternCharOffset, sliceLen);
                std::span<int16_t> bonusSlice(bonusesSpan.begin() + patternCharOffset, textSlice.size());
                std::span<int16_t> consecutiveCharMatrixSlice = consecutiveCharMatrixSpan.subspan(row + patternCharOffset - firstOccurrenceOfFirstChar, textSlice.size());
                std::span<int16_t> consecutiveCharMatrixDiagonalSlice = consecutiveCharMatrixSpan.subspan(
                    +row + patternCharOffset - firstOccurrenceOfFirstChar - 1 - width, textSlice.size());
                std::span<int16_t> scoreMatrixSlice = scoreSpan.subspan(row + patternCharOffset - firstOccurrenceOfFirstChar, textSlice.size());
                std::span<int16_t> scoreMatrixDiagonalSlice = scoreSpan.subspan(row + patternCharOffset - firstOccurrenceOfFirstChar - 1 - width, textSlice.size());
                std::span<int16_t> scoreMatrixLeftSlice = scoreSpan.subspan(row + patternCharOffset - firstOccurrenceOfFirstChar - 1, textSlice.size());
                if (!scoreMatrixLeftSlice.empty())
                {
                    scoreMatrixLeftSlice[0] = 0;
                }
                for (int32_t j = 0; j < static_cast<int32_t>(textSlice.size()); j++)
                {
                    UChar32 currentChar = textSlice[j];
                    int32_t column = patternCharOffset + static_cast<int32_t>(j);
                    int16_t score = inGap ? scoreMatrixLeftSlice[j] + ScoreGapExtension : scoreMatrixLeftSlice[j] + ScoreGapStart;
                    int16_t diagonalScore = 0;
                    int16_t consecutive = 0;
                    if (currentChar == currentPatternChar)
                    {
                        diagonalScore = scoreMatrixDiagonalSlice[j] + ScoreMatch;
                        int16_t bonus = bonusSlice[j];
                        consecutive = consecutiveCharMatrixDiagonalSlice[j] + 1;
                        if (bonus == BoundaryBonus)
                        {
                            consecutive = 1;
                        }
                        else if (consecutive > 1)
                        {
                            bonus = std::max({ bonus, BonusConsecutive, (bonusesSpan[column - consecutive + 1]) });
                        }
                        if (diagonalScore + bonus < score)
                        {
                            diagonalScore += bonusSlice[j];
                            consecutive = 0;
                        }
                        else
                        {
                            diagonalScore += bonus;
                        }
                    }
                    consecutiveCharMatrixSlice[j] = consecutive;
                    inGap = (diagonalScore < score);
                    int16_t cellScore = std::max(int16_t{ 0 }, std::max(diagonalScore, score));
                    if (off == patternSize - 2 && cellScore > maxScore)
                    {
                        maxScore = cellScore;
                        maxScorePos = column;
                    }
                    scoreMatrixSlice[j] = cellScore;
                }
            }

            int32_t currentColIndex = maxScorePos;
            if (pos)
            {
                patternIndex = patternSize - 1;
                bool preferCurrentMatch = true;
                while (true)
                {
                    int32_t rowStartIndex = patternIndex * width;
                    int colOffset = currentColIndex - firstOccurrenceOfFirstChar;
                    int cellScore = scoreMatrix[rowStartIndex + colOffset];
                    int diagonalCellScore = 0, leftCellScore = 0;

                    if (patternIndex > 0 && currentColIndex >= firstOccurrenceOfEachChar[patternIndex])
                    {
                        diagonalCellScore = scoreMatrix[rowStartIndex - width + colOffset - 1];
                    }
                    if (currentColIndex > firstOccurrenceOfEachChar[patternIndex])
                    {
                        leftCellScore = scoreMatrix[rowStartIndex + colOffset - 1];
                    }

                    if (cellScore > diagonalCellScore &&
                        (cellScore > leftCellScore || (cellScore == leftCellScore && preferCurrentMatch)))
                    {
                        pos->push_back(currentColIndex);
                        if (patternIndex == 0)
                        {
                            break;
                        }
                        patternIndex--;
                    }

                    currentColIndex--;
                    if (rowStartIndex + colOffset >= consecutiveCharMatrixSize)
                    {
                        break;
                    }

                    preferCurrentMatch = (consecutiveCharMatrix[rowStartIndex + colOffset] > 1) ||
                                         ((rowStartIndex + width + colOffset + 1 <
                                           consecutiveCharMatrixSize) &&
                                          (consecutiveCharMatrix[rowStartIndex + width + colOffset + 1] > 0));
                }
            }
            int32_t end = maxScorePos + 1;
            return { currentColIndex, end, maxScore };
        }

        int BonusAt(std::wstring_view input, int idx)
        {
            if (idx == 0)
            {
                return BoundaryBonus;
            }
            return CalculateBonus(ClassOf(input[idx - 1]), ClassOf(input[idx]));
        }

        int32_t utf32Length(std::wstring_view str)
        {
            return u_countChar32(reinterpret_cast<const UChar*>(str.data()), static_cast<int32_t>(str.size()));
        }

        static std::vector<UChar32> ConvertUtf16ToCodePoints(
            std::wstring_view text,
            bool fold,
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

                if (fold)
                {
                    cp = u_foldCase(cp, U_FOLD_CASE_DEFAULT);
                }

                out.push_back(cp);
            }

            return out;
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

                patObj.terms.push_back(ConvertUtf16ToCodePoints(slice, true));

                if (found == std::wstring_view::npos)
                {
                    break;
                }
                pos = found + 1;
            }

            return patObj;
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
            if (pattern.terms.empty())
            {
                return MatchResult{};
                ;
            }

            int16_t totalScore = 0;
            std::vector<int32_t> pos;
            for (const auto& term : pattern.terms)
            {
                std::vector<int32_t> utf16map;
                std::vector<int32_t> codePointPos;
                auto textCodePoints = ConvertUtf16ToCodePoints(text, false, &utf16map);
                FzfResult res = FzfFuzzyMatchV2(textCodePoints, term, &codePointPos);
                if (res.Score <= 0)
                {
                    return std::nullopt;
                }

                auto termUtf16Pos = MapCodepointsToUtf16(codePointPos, utf16map, text.size());
                for (auto t : termUtf16Pos)
                {
                    pos.push_back(t);
                }
                totalScore += res.Score;
            }
            return MatchResult{ totalScore, pos };
        }
    }

}
