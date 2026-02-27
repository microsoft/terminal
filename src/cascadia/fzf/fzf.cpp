#include "pch.h"
#include "fzf.h"

#undef CharLower
#undef CharUpper

using namespace fzf::matcher;

constexpr int16_t ScoreMatch = 16;
constexpr int16_t ScoreGapStart = -3;
constexpr int16_t ScoreGapExtension = -1;
constexpr int16_t BoundaryBonus = ScoreMatch / 2;
constexpr int16_t NonWordBonus = ScoreMatch / 2;
constexpr int16_t CamelCaseBonus = BoundaryBonus + ScoreGapExtension;
constexpr int16_t BonusConsecutive = -(ScoreGapStart + ScoreGapExtension);
constexpr int16_t BonusFirstCharMultiplier = 2;
constexpr size_t npos = std::numeric_limits<size_t>::max();

enum class CharClass : uint8_t
{
    NonWord = 0,
    CharLower = 1,
    CharUpper = 2,
    Digit = 3,
};

static std::vector<UChar32> utf16ToUtf32(std::wstring_view text)
{
    const UChar* data = reinterpret_cast<const UChar*>(text.data());
    int32_t dataLen = static_cast<int32_t>(text.size());
    int32_t cpCount = u_countChar32(data, dataLen);

    std::vector<UChar32> out(cpCount);

    UErrorCode status = U_ZERO_ERROR;
    u_strToUTF32(out.data(), static_cast<int32_t>(out.size()), nullptr, data, dataLen, &status);
    THROW_HR_IF(E_UNEXPECTED, status > U_ZERO_ERROR);

    return out;
}

static void foldStringUtf32(std::vector<UChar32>& str)
{
    for (auto& cp : str)
    {
        cp = u_foldCase(cp, U_FOLD_CASE_DEFAULT);
    }
}

static size_t trySkip(const std::vector<UChar32>& input, const UChar32 searchChar, size_t startIndex)
{
    for (size_t i = startIndex; i < input.size(); ++i)
    {
        if (input[i] == searchChar)
        {
            return i;
        }
    }
    return npos;
}

// Unlike the equivalent in fzf, this one does more than Unicode.
static size_t asciiFuzzyIndex(const std::vector<UChar32>& input, const std::vector<UChar32>& pattern)
{
    size_t idx = 0;
    size_t firstIdx = 0;
    for (size_t pi = 0; pi < pattern.size(); ++pi)
    {
        idx = trySkip(input, pattern[pi], idx);
        if (idx == npos)
        {
            return npos;
        }

        if (pi == 0 && idx > 0)
        {
            firstIdx = idx - 1;
        }

        idx++;
    }
    return firstIdx;
}

static int16_t calculateBonus(CharClass prevClass, CharClass currentClass)
{
    if (prevClass == CharClass::NonWord && currentClass != CharClass::NonWord)
    {
        return BoundaryBonus;
    }
    if ((prevClass == CharClass::CharLower && currentClass == CharClass::CharUpper) ||
        (prevClass != CharClass::Digit && currentClass == CharClass::Digit))
    {
        return CamelCaseBonus;
    }
    if (currentClass == CharClass::NonWord)
    {
        return NonWordBonus;
    }
    return 0;
}

static constexpr auto s_charClassLut = []() {
    std::array<CharClass, U_CHAR_CATEGORY_COUNT> lut{};
    lut.fill(CharClass::NonWord);
    lut[U_UPPERCASE_LETTER] = CharClass::CharUpper;
    lut[U_LOWERCASE_LETTER] = CharClass::CharLower;
    lut[U_MODIFIER_LETTER] = CharClass::CharLower;
    lut[U_OTHER_LETTER] = CharClass::CharLower;
    lut[U_DECIMAL_DIGIT_NUMBER] = CharClass::Digit;
    return lut;
}();

static CharClass classOf(UChar32 ch)
{
    return s_charClassLut[u_charType(ch)];
}

static int32_t fzfFuzzyMatchV2(const std::vector<UChar32>& text, const std::vector<UChar32>& pattern, std::vector<size_t>* pos)
{
    if (pattern.size() == 0)
    {
        return 0;
    }

    auto foldedText = text;
    foldStringUtf32(foldedText);

    size_t firstIndexOf = asciiFuzzyIndex(foldedText, pattern);
    if (firstIndexOf == npos)
    {
        return 0;
    }

    auto initialScores = std::vector<int16_t>(text.size());
    auto consecutiveScores = std::vector<int16_t>(text.size());
    auto firstOccurrenceOfEachChar = std::vector<size_t>(pattern.size());
    auto bonusesSpan = std::vector<int16_t>(text.size());

    int16_t maxScore = 0;
    size_t maxScorePos = 0;
    size_t patternIndex = 0;
    size_t lastIndex = 0;
    UChar32 firstPatternChar = pattern[0];
    UChar32 currentPatternChar = pattern[0];
    int16_t previousInitialScore = 0;
    CharClass previousClass = CharClass::NonWord;
    bool inGap = false;

    std::span<const UChar32> lowerText(foldedText);
    auto lowerTextSlice = lowerText.subspan(firstIndexOf);
    auto initialScoresSlice = std::span(initialScores).subspan(firstIndexOf);
    auto consecutiveScoresSlice = std::span(consecutiveScores).subspan(firstIndexOf);
    auto bonusesSlice = std::span(bonusesSpan).subspan(firstIndexOf, text.size() - firstIndexOf);

    for (size_t i = 0; i < lowerTextSlice.size(); i++)
    {
        const auto currentChar = lowerTextSlice[i];
        const auto currentClass = classOf(text[i + firstIndexOf]);
        const auto bonus = calculateBonus(previousClass, currentClass);
        bonusesSlice[i] = bonus;
        previousClass = currentClass;

        //currentPatternChar was already folded in ParsePattern
        if (currentChar == currentPatternChar)
        {
            if (patternIndex < pattern.size())
            {
                firstOccurrenceOfEachChar[patternIndex] = firstIndexOf + i;
                patternIndex++;
                if (patternIndex < pattern.size())
                {
                    currentPatternChar = pattern[patternIndex];
                }
            }
            lastIndex = firstIndexOf + i;
        }
        if (currentChar == firstPatternChar)
        {
            int16_t score = ScoreMatch + bonus * BonusFirstCharMultiplier;
            initialScoresSlice[i] = score;
            consecutiveScoresSlice[i] = 1;
            if (pattern.size() == 1 && (score > maxScore))
            {
                maxScore = score;
                maxScorePos = firstIndexOf + i;
                if (bonus == BoundaryBonus)
                {
                    break;
                }
            }
            inGap = false;
        }
        else
        {
            initialScoresSlice[i] = std::max<int16_t>(previousInitialScore + (inGap ? ScoreGapExtension : ScoreGapStart), 0);
            consecutiveScoresSlice[i] = 0;
            inGap = true;
        }
        previousInitialScore = initialScoresSlice[i];
    }

    if (patternIndex != pattern.size())
    {
        return 0;
    }

    if (pattern.size() == 1)
    {
        if (pos)
        {
            pos->push_back(maxScorePos);
        }
        return maxScore;
    }

    const auto firstOccurrenceOfFirstChar = firstOccurrenceOfEachChar[0];
    const auto width = lastIndex - firstOccurrenceOfFirstChar + 1;
    const auto rows = pattern.size();
    auto consecutiveCharMatrixSize = width * pattern.size();

    std::vector<int16_t> scoreMatrix(width * rows);
    std::copy_n(initialScores.begin() + firstOccurrenceOfFirstChar, width, scoreMatrix.begin());
    std::span scoreSpan(scoreMatrix);

    std::vector<int16_t> consecutiveCharMatrix(width * rows);
    std::copy_n(consecutiveScores.begin() + firstOccurrenceOfFirstChar, width, consecutiveCharMatrix.begin());
    std::span consecutiveCharMatrixSpan(consecutiveCharMatrix);

    auto patternSliceStr = std::span(pattern).subspan(1);

    for (size_t off = 0; off < pattern.size() - 1; off++)
    {
        auto patternCharOffset = firstOccurrenceOfEachChar[off + 1];
        auto sliceLen = lastIndex - patternCharOffset + 1;
        currentPatternChar = patternSliceStr[off];
        patternIndex = off + 1;
        auto row = patternIndex * width;
        inGap = false;
        std::span<const UChar32> textSlice = lowerText.subspan(patternCharOffset, sliceLen);
        std::span bonusSlice(bonusesSpan.begin() + patternCharOffset, textSlice.size());
        std::span<int16_t> consecutiveCharMatrixSlice = consecutiveCharMatrixSpan.subspan(row + patternCharOffset - firstOccurrenceOfFirstChar, textSlice.size());
        std::span<int16_t> consecutiveCharMatrixDiagonalSlice = consecutiveCharMatrixSpan.subspan(row + patternCharOffset - firstOccurrenceOfFirstChar - 1 - width, textSlice.size());
        std::span<int16_t> scoreMatrixSlice = scoreSpan.subspan(row + patternCharOffset - firstOccurrenceOfFirstChar, textSlice.size());
        std::span<int16_t> scoreMatrixDiagonalSlice = scoreSpan.subspan(row + patternCharOffset - firstOccurrenceOfFirstChar - 1 - width, textSlice.size());
        std::span<int16_t> scoreMatrixLeftSlice = scoreSpan.subspan(row + patternCharOffset - firstOccurrenceOfFirstChar - 1, textSlice.size());

        if (!scoreMatrixLeftSlice.empty())
        {
            scoreMatrixLeftSlice[0] = 0;
        }

        for (size_t j = 0; j < textSlice.size(); j++)
        {
            const auto currentChar = textSlice[j];
            const auto column = patternCharOffset + j;
            const int16_t score = inGap ? scoreMatrixLeftSlice[j] + ScoreGapExtension : scoreMatrixLeftSlice[j] + ScoreGapStart;
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
            if (off + 2 == pattern.size() && cellScore > maxScore)
            {
                maxScore = cellScore;
                maxScorePos = column;
            }
            scoreMatrixSlice[j] = cellScore;
        }
    }

    size_t currentColIndex = maxScorePos;
    if (pos)
    {
        patternIndex = pattern.size() - 1;
        bool preferCurrentMatch = true;
        while (true)
        {
            const auto rowStartIndex = patternIndex * width;
            const auto colOffset = currentColIndex - firstOccurrenceOfFirstChar;
            const auto cellScore = scoreMatrix[rowStartIndex + colOffset];
            int32_t diagonalCellScore = 0;
            int32_t leftCellScore = 0;

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
    return maxScore;
}

Pattern fzf::matcher::ParsePattern(const std::wstring_view patternStr)
{
    Pattern patObj;
    size_t pos = 0;

    while (true)
    {
        const auto beg = patternStr.find_first_not_of(L' ', pos);
        if (beg == std::wstring_view::npos)
        {
            break; // No more non-space characters
        }

        const auto end = std::min(patternStr.size(), patternStr.find_first_of(L' ', beg));
        const auto word = patternStr.substr(beg, end - beg);
        auto codePoints = utf16ToUtf32(word);
        foldStringUtf32(codePoints);
        patObj.terms.push_back(std::move(codePoints));
        pos = end;
    }

    return patObj;
}

std::optional<MatchResult> fzf::matcher::Match(std::wstring_view text, const Pattern& pattern)
{
    if (pattern.terms.empty())
    {
        return MatchResult{};
    }

    const auto textCodePoints = utf16ToUtf32(text);

    int32_t totalScore = 0;
    std::vector<size_t> allUtf32Pos;

    for (const auto& term : pattern.terms)
    {
        std::vector<size_t> termPos;
        auto score = fzfFuzzyMatchV2(textCodePoints, term, &termPos);
        if (score <= 0)
        {
            return std::nullopt;
        }

        totalScore += score;
        allUtf32Pos.insert(allUtf32Pos.end(), termPos.begin(), termPos.end());
    }

    std::ranges::sort(allUtf32Pos);
    allUtf32Pos.erase(std::ranges::unique(allUtf32Pos).begin(), allUtf32Pos.end());

    std::vector<TextRun> runs;
    std::size_t nextCodePointPos = 0;
    size_t utf16Offset = 0;

    bool inRun = false;
    size_t runStart = 0;

    for (size_t cpIndex = 0; cpIndex < textCodePoints.size(); cpIndex++)
    {
        const auto cp = textCodePoints[cpIndex];
        const size_t cpWidth = U16_LENGTH(cp);

        const bool isMatch = (nextCodePointPos < allUtf32Pos.size() && allUtf32Pos[nextCodePointPos] == cpIndex);
        if (isMatch)
        {
            if (!inRun)
            {
                runStart = utf16Offset;
                inRun = true;
            }
            nextCodePointPos++;
        }
        else if (inRun)
        {
            runs.push_back({ runStart, utf16Offset - 1 });
            inRun = false;
        }

        utf16Offset += cpWidth;
    }

    if (inRun)
    {
        runs.push_back({ runStart, utf16Offset - 1 });
    }

    return MatchResult{ totalScore, std::move(runs) };
}
