#include "pch.h"
#include "fzf.h"
#include <algorithm>
#include <cwctype>

namespace fzf
{
    namespace matcher
    {
        constexpr int ScoreMatch = 16;
        constexpr int ScoreGapStart = -3;
        constexpr int ScoreGapExtension = -1;
        constexpr int BoundaryBonus = ScoreMatch / 2;
        constexpr int NonWordBonus = ScoreMatch / 2;
        constexpr int CamelCaseBonus = BoundaryBonus + ScoreGapExtension;
        constexpr int BonusConsecutive = -(ScoreGapStart + ScoreGapExtension);
        constexpr int BonusFirstCharMultiplier = 2;

        enum CharClass
        {
            NonWord = 0,
            CharLower = 1,
            CharUpper = 2,
            Digit = 3,
        };

        std::wstring_view TrimStart(const std::wstring_view str)
        {
            size_t start = str.find_first_not_of(L" ");
            return (start == std::wstring::npos) ? std::wstring_view() : std::wstring_view(str).substr(start);
        }

        std::wstring_view TrimSuffixSpaces(std::wstring_view input)
        {
            size_t end = input.size();
            while (end > 0 && input[end - 1] == L' ')
                --end;
            return input.substr(0, end);
        }

        int IndexOfChar(std::wstring_view input, const WCHAR value[], int startIndex)
        {
            for (size_t i = startIndex; i < input.size(); i++)
            {
                const WCHAR currentCharAsString[] = { input[i], L'\0' };
                auto isCurrentCharMatched = lstrcmpi(value, currentCharAsString) == 0;
                if (isCurrentCharMatched)
                    return static_cast<int>(i);
            }
            return -1;
        }

        int FuzzyIndexOf(std::wstring_view input, std::wstring_view pattern)
        {
            int idx = 0;
            int firstIdx = 0;
            for (size_t patternIndex = 0; patternIndex < pattern.size(); patternIndex++)
            {
                // GH#9941: search should be locale-aware as well
                // We use the same comparison method as upon sorting to guarantee consistent behavior
                const WCHAR searchCharAsString[] = { pattern[patternIndex], L'\0' };
                //const WCHAR currentCharAsString[] = { currentChar, L'\0' };
                idx = IndexOfChar(input, searchCharAsString, idx);
                if (idx < 0)
                    return -1;
                if (patternIndex == 0 && idx > 0)
                    firstIdx = idx - 1;
                idx++;
            }
            return firstIdx;
        }

        int CalculateBonus(CharClass prevClass, CharClass currentClass)
        {
            if (prevClass == NonWord && currentClass != NonWord)
                return BoundaryBonus;
            if ((prevClass == CharLower && currentClass == CharUpper) ||
                (prevClass != Digit && currentClass == Digit))
                return CamelCaseBonus;
            if (currentClass == NonWord)
                return NonWordBonus;
            return 0;
        }

        CharClass ClassOf(wchar_t ch)
        {
            if (std::iswlower(ch))
                return CharLower;
            if (std::iswupper(ch))
                return CharUpper;
            if (std::iswdigit(ch))
                return Digit;
            return NonWord;
        }

        FzfResult FzfFuzzyMatchV2(std::wstring_view text, std::wstring_view pattern, std::vector<int>* pos)
        {
            size_t patternSize = pattern.size();
            size_t textSize = text.size();

            if (patternSize == 0)
                return { 0, 0, 0 };

            int firstIndexOf = FuzzyIndexOf(text, pattern);
            if (firstIndexOf < 0)
                return { -1, -1, 0 };

            auto initialScores = std::vector<int>(textSize);
            auto consecutiveScores = std::vector<int>(textSize);
            auto firstOccurrenceOfEachChar = std::vector<int>(patternSize);
            int maxScore = 0;
            int maxScorePos = 0;
            size_t patternIndex = 0;
            int lastIndex = 0;
            wchar_t firstPatternChar = pattern[0];
            wchar_t currentPatternChar = pattern[0];
            int previousInitialScore = 0;
            CharClass previousClass = NonWord;
            bool inGap = false;

            auto textCopy = std::wstring{ text };
            std::ranges::transform(textCopy, textCopy.begin(), [](wchar_t c) {
                return std::towlower(c);
            });
            std::wstring_view lowerText(textCopy.data(), textCopy.size());
            std::wstring_view lowerTextSlice = lowerText.substr(firstIndexOf, textCopy.size() - firstIndexOf);
            auto initialScoresSlice = std::span<int>(initialScores).subspan(firstIndexOf);
            auto consecutiveScoresSlice = std::span<int>(consecutiveScores).subspan(firstIndexOf);
            auto bonusesSpan = std::vector<int>(textSize);
            auto bonusesSlice = std::span<int>(bonusesSpan).subspan(firstIndexOf, textSize - firstIndexOf);

            for (size_t i = 0; i < lowerTextSlice.size(); i++)
            {
                wchar_t currentChar = lowerTextSlice[i];
                CharClass currentClass = ClassOf(currentChar);
                int bonus = CalculateBonus(previousClass, currentClass);
                bonusesSlice[i] = bonus;
                previousClass = currentClass;

                // GH#9941: search should be locale-aware as well
                // We use the same comparison method as upon sorting to guarantee consistent behavior
                const WCHAR searchCharAsString[] = { currentPatternChar, L'\0' };
                const WCHAR currentCharAsString[] = { currentChar, L'\0' };
                auto isCurrentCharMatched = lstrcmpi(searchCharAsString, currentCharAsString) == 0;
                if (isCurrentCharMatched)
                {
                    if (patternIndex < pattern.size())
                    {
                        firstOccurrenceOfEachChar[patternIndex] = firstIndexOf + static_cast<int>(i);
                        patternIndex++;
                        if (patternIndex < patternSize)
                            currentPatternChar = pattern[patternIndex];
                    }
                    lastIndex = firstIndexOf + static_cast<int>(i);
                }
                if (currentChar == firstPatternChar)
                {
                    int score = ScoreMatch + bonus * BonusFirstCharMultiplier;
                    initialScoresSlice[i] = score;
                    consecutiveScoresSlice[i] = 1;
                    if (patternSize == 1 && (score > maxScore))
                    {
                        maxScore = score;
                        maxScorePos = firstIndexOf + static_cast<int>(i);
                        if (bonus == BoundaryBonus)
                            break;
                    }
                    inGap = false;
                }
                else
                {
                    initialScoresSlice[i] = inGap ? std::max(previousInitialScore + ScoreGapExtension, 0) : std::max(previousInitialScore + ScoreGapStart, 0);
                    consecutiveScoresSlice[i] = 0;
                    inGap = true;
                }
                previousInitialScore = initialScoresSlice[i];
            }

            if (patternIndex != pattern.size())
                return { -1, -1, 0 };

            if (pattern.size() == 1)
            {
                if (pos)
                    pos->push_back(maxScorePos);
                return { maxScorePos, maxScorePos + 1, maxScore };
            }

            int firstOccurrenceOfFirstChar = firstOccurrenceOfEachChar[0];
            int width = lastIndex - firstOccurrenceOfFirstChar + 1;
            auto scoreMatrix = std::vector<int>(width * patternSize);
            std::copy_n(initialScores.begin() + firstOccurrenceOfFirstChar,
                        width,
                        scoreMatrix.begin());
            auto scoreSpan = std::span<int>(scoreMatrix);
            auto consecutiveCharMatrixSize = width * patternSize;
            auto consecutiveCharMatrix = std::vector<int>(consecutiveCharMatrixSize);
            std::copy(consecutiveScores.begin() + firstOccurrenceOfFirstChar,
                      consecutiveScores.begin() + lastIndex,
                      consecutiveCharMatrix.begin());
            auto consecutiveCharMatrixSpan = std::span<int>(consecutiveCharMatrix);

            std::wstring_view patternSliceStr = pattern.substr(1);
            for (size_t off = 0; off < patternSize - 1; off++)
            {
                int patternCharOffset = firstOccurrenceOfEachChar[off + 1];
                currentPatternChar = patternSliceStr[off];
                patternIndex = off + 1;
                size_t row = patternIndex * width;
                inGap = false;
                std::wstring_view textSlice = lowerText.substr(patternCharOffset, lastIndex - patternCharOffset + 1);
                std::span<int> bonusSlice(bonusesSpan.begin() + patternCharOffset, textSlice.size());
                std::span<int> consecutiveCharMatrixSlice = consecutiveCharMatrixSpan.subspan(row + patternCharOffset - firstOccurrenceOfFirstChar, textSlice.size());
                std::span<int> consecutiveCharMatrixDiagonalSlice = consecutiveCharMatrixSpan.subspan(
                    +row + patternCharOffset - firstOccurrenceOfFirstChar - 1 - width, textSlice.size());
                std::span<int> scoreMatrixSlice = scoreSpan.subspan(row + patternCharOffset - firstOccurrenceOfFirstChar, textSlice.size());
                std::span<int> scoreMatrixDiagonalSlice = scoreSpan.subspan(row + patternCharOffset - firstOccurrenceOfFirstChar - 1 - width, textSlice.size());
                std::span<int> scoreMatrixLeftSlice = scoreSpan.subspan(row + patternCharOffset - firstOccurrenceOfFirstChar - 1, textSlice.size());
                if (!scoreMatrixLeftSlice.empty())
                    scoreMatrixLeftSlice[0] = 0;
                for (size_t j = 0; j < textSlice.size(); j++)
                {
                    wchar_t currentChar = textSlice[j];
                    int column = patternCharOffset + static_cast<int>(j);
                    int score = inGap ? scoreMatrixLeftSlice[j] + ScoreGapExtension : scoreMatrixLeftSlice[j] + ScoreGapStart;
                    int diagonalScore = 0;
                    int consecutive = 0;
                    if (currentChar == currentPatternChar)
                    {
                        diagonalScore = scoreMatrixDiagonalSlice[j] + ScoreMatch;
                        int bonus = bonusSlice[j];
                        consecutive = consecutiveCharMatrixDiagonalSlice[j] + 1;
                        if (bonus == BoundaryBonus)
                            consecutive = 1;
                        else if (consecutive > 1)
                            bonus = std::max({ bonus, BonusConsecutive, static_cast<int>(bonusesSpan[(column - consecutive) + 1]) });
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
                    int cellScore = std::max(0, std::max(diagonalScore, score));
                    if (off == patternSize - 2 && cellScore > maxScore)
                    {
                        maxScore = cellScore;
                        maxScorePos = column;
                    }
                    scoreMatrixSlice[j] = cellScore;
                }
            }

            int currentColIndex = maxScorePos;
            if (pos)
            {
                auto patternIndex = patternSize - 1;
                bool preferCurrentMatch = true;
                while (true)
                {
                    size_t rowStartIndex = patternIndex * width;
                    int colOffset = currentColIndex - firstOccurrenceOfFirstChar;
                    int cellScore = scoreMatrix[rowStartIndex + colOffset];
                    int diagonalCellScore = 0, leftCellScore = 0;

                    if (patternIndex > 0 && currentColIndex >= firstOccurrenceOfEachChar[patternIndex])
                        diagonalCellScore = scoreMatrix[rowStartIndex - width + colOffset - 1];
                    if (currentColIndex > firstOccurrenceOfEachChar[patternIndex])
                        leftCellScore = scoreMatrix[rowStartIndex + colOffset - 1];

                    if (cellScore > diagonalCellScore &&
                        (cellScore > leftCellScore || (cellScore == leftCellScore && preferCurrentMatch)))
                    {
                        pos->push_back(currentColIndex);
                        if (patternIndex == 0)
                            break;
                        patternIndex--;
                    }

                    currentColIndex--;
                    if (rowStartIndex + colOffset >= static_cast<size_t>(consecutiveCharMatrixSize))
                        break;

                    preferCurrentMatch = (consecutiveCharMatrix[rowStartIndex + colOffset] > 1) ||
                                         ((rowStartIndex + width + colOffset + 1 <
                                           static_cast<size_t>(consecutiveCharMatrixSize)) &&
                                          (consecutiveCharMatrix[rowStartIndex + width + colOffset + 1] > 0));
                }
            }
            return { currentColIndex, maxScorePos + 1, maxScore };
        }

        int BonusAt(std::wstring_view input, int idx)
        {
            if (idx == 0)
                return BoundaryBonus;
            return CalculateBonus(ClassOf(input[idx - 1]), ClassOf(input[idx]));
        }

        int LeadingWhiteSpace(std::wstring_view text)
        {
            int whiteSpaces = 0;
            for (wchar_t c : text)
            {
                if (c != L' ')
                    break;
                whiteSpaces++;
            }
            return whiteSpaces;
        }

        Pattern ParsePattern(const std::wstring_view patternStr)
        {
            Pattern patObj;
            if (patternStr.empty())
                return patObj;

            std::wstring_view trimmedPatternStr = TrimStart(patternStr);
            trimmedPatternStr = TrimSuffixSpaces(trimmedPatternStr);
            size_t pos = 0, found;
            while ((found = trimmedPatternStr.find(L' ', pos)) != std::wstring::npos)
            {
                std::wstring term = std::wstring{ trimmedPatternStr.substr(pos, found - pos) };
                std::ranges::transform(term, term.begin(), ::towlower);
                patObj.terms.push_back(term);
                pos = found + 1;
            }
            if (pos < trimmedPatternStr.size())
            {
                std::wstring term = std::wstring{ trimmedPatternStr.substr(pos) };
                std::ranges::transform(term, term.begin(), ::towlower);
                patObj.terms.push_back(term);
            }

            return patObj;
        }

        int GetScore(std::wstring_view text, const Pattern& pattern)
        {
            if (pattern.terms.empty())
                return 1;
            int totalScore = 0;
            for (const auto& term : pattern.terms)
            {
                FzfResult res = FzfFuzzyMatchV2(text, term, nullptr);
                if (res.Start >= 0)
                {
                    totalScore += res.Score;
                }
                else
                {
                    return 0;
                }
            }
            return totalScore;
        }

        std::vector<int> GetPositions(std::wstring_view text, const Pattern& pattern)
        {
            std::vector<int> result;
            for (const auto& termSet : pattern.terms)
            {
                FzfResult algResult = FzfFuzzyMatchV2(text, termSet, &result);
                if (algResult.Score == 0)
                {
                    return {};
                }
            }
            return result;
        }

    }

}
