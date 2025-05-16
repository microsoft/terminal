// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "..\TerminalApp\fzf\fzf.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace TerminalAppUnitTests
{
    typedef enum
    {
        ScoreMatch = 16,
        ScoreGapStart = -3,
        ScoreGapExtension = -1,
        BonusBoundary = ScoreMatch / 2,
        BonusNonWord = ScoreMatch / 2,
        BonusCamel123 = BonusBoundary + ScoreGapExtension,
        BonusConsecutive = -(ScoreGapStart + ScoreGapExtension),
        BonusFirstCharMultiplier = 2,
    } score_t;

    class FzfTests
    {
        BEGIN_TEST_CLASS(FzfTests)
        END_TEST_CLASS()

        TEST_METHOD(AllPatternCharsDoNotMatch);
        TEST_METHOD(ConsecutiveChars);
        TEST_METHOD(ConsecutiveChars_FirstCharBonus);
        TEST_METHOD(NonWordBonusBoundary_ConsecutiveChars);
        TEST_METHOD(MatchOnNonWordChars_CaseInSensitive);
        TEST_METHOD(MatchOnNonWordCharsWithGap);
        TEST_METHOD(BonusBoundaryAndFirstCharMultiplier);
        TEST_METHOD(MatchesAreCaseInSensitive);
        TEST_METHOD(MultipleTerms);
        TEST_METHOD(MultipleTerms_AllCharsMatch);
        TEST_METHOD(MultipleTerms_NotAllTermsMatch);
        TEST_METHOD(MatchesAreCaseInSensitive_BonusBoundary);
        TEST_METHOD(TraceBackWillPickTheFirstMatchIfBothHaveTheSameScore);
        TEST_METHOD(TraceBackWillPickTheMatchWithTheHighestScore);
        TEST_METHOD(TraceBackWillPickTheMatchWithTheHighestScore_Gaps);
        TEST_METHOD(TraceBackWillPickEarlierCharsWhenNoBonus);
        TEST_METHOD(MatchWithGapCanAHaveHigherScoreThanConsecutiveWhenGapMatchHasBoundaryBonus);
        TEST_METHOD(ConsecutiveMatchWillScoreHigherThanMatchWithGapWhenBothHaveFirstCharBonus);
        TEST_METHOD(ConsecutiveMatchWillScoreHigherThanMatchWithGapWhenBothDontHaveBonus);
        TEST_METHOD(MatchWithGapCanHaveHigherScoreThanConsecutiveWhenGapHasFirstCharBonus);
        TEST_METHOD(MatchWithGapThatMatchesOnTheFirstCharWillNoLongerScoreHigherThanConsecutiveCharsWhenTheGapIs3_NoConsecutiveChar_4CharPattern);
        TEST_METHOD(MatchWithGapThatMatchesOnTheFirstCharWillNoLongerHigherScoreThanConsecutiveCharsWhenTheGapIs11_2CharPattern);
        TEST_METHOD(MatchWithGapThatMatchesOnTheFirstCharWillNoLongerHigherScoreThanConsecutiveCharsWhenTheGapIs11_3CharPattern_1ConsecutiveChar);
        TEST_METHOD(MatchWithGapThatMatchesOnTheFirstCharWillNoLongerHigherScoreThanConsecutiveCharsWhenTheGapIs5_NoConsecutiveChars_3CharPattern);
        TEST_METHOD(Russian_CaseMisMatch);
        TEST_METHOD(Russian_CaseMatch);
        TEST_METHOD(English_CaseMatch);
        TEST_METHOD(English_CaseMisMatch);
        TEST_METHOD(SurrogatePair);
        TEST_METHOD(French_CaseMatch);
        TEST_METHOD(French_CaseMisMatch);
        TEST_METHOD(German_CaseMatch);
        TEST_METHOD(German_CaseMisMatch_FoldResultsInMultipleCodePoints);
        TEST_METHOD(Greek_CaseMisMatch);
        TEST_METHOD(Greek_CaseMatch);
        TEST_METHOD(SurrogatePair_ToUtf16Pos_ConsecutiveChars);
        TEST_METHOD(SurrogatePair_ToUtf16Pos_PreferConsecutiveChars);
        TEST_METHOD(SurrogatePair_ToUtf16Pos_GapAndBoundary);
    };

    void AssertScoreAndPositions(std::wstring_view patternText, std::wstring_view text, int expectedScore, std::vector<int16_t> expectedPositions)
    {
        auto pattern = fzf::matcher::ParsePattern(patternText);
        auto match = fzf::matcher::Match(text, pattern);
        if (expectedScore == 0 && expectedPositions.size() == 0)
        {
            VERIFY_ARE_EQUAL(std::nullopt, match);
            return;
        }
        auto score = match->Score;
        auto positions = match->Pos;
        VERIFY_ARE_EQUAL(expectedScore, score);
        VERIFY_ARE_EQUAL(expectedPositions.size(), positions.size());
        for (auto i = 0; i < expectedPositions.size(); i++)
        {
            VERIFY_ARE_EQUAL(expectedPositions[i], positions[i]);
        }
    }

    void FzfTests::AllPatternCharsDoNotMatch()
    {
        AssertScoreAndPositions(
            L"fbb",
            L"foo bar",
            0,
            {});
    }

    void FzfTests::ConsecutiveChars()
    {
        AssertScoreAndPositions(
            L"oba",
            L"foobar",
            ScoreMatch * 3 + BonusConsecutive * 2,
            { 4, 3, 2 });
    }

    void FzfTests::ConsecutiveChars_FirstCharBonus()
    {
        AssertScoreAndPositions(
            L"foo",
            L"foobar",
            ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 2,
            { 2, 1, 0 });
    }

    void FzfTests::NonWordBonusBoundary_ConsecutiveChars()
    {
        AssertScoreAndPositions(
            L"zshc",
            L"/man1/zshcompctl.1",
            ScoreMatch * 4 + BonusBoundary * BonusFirstCharMultiplier + BonusFirstCharMultiplier * BonusConsecutive * 3,
            { 9, 8, 7, 6 });
    }

    void FzfTests::Russian_CaseMisMatch()
    {
        AssertScoreAndPositions(
            L"новая",
            L"Новая вкладка",
            ScoreMatch * 5 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 4,
            { 4, 3, 2, 1, 0 });
    }

    void FzfTests::Russian_CaseMatch()
    {
        AssertScoreAndPositions(
            L"Новая",
            L"Новая вкладка",
            ScoreMatch * 5 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 4,
            { 4, 3, 2, 1, 0 });
    }

    void FzfTests::German_CaseMatch()
    {
        AssertScoreAndPositions(
            L"fuß",
            L"Fußball",
            ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 2,
            { 2, 1, 0 });
    }

    void FzfTests::German_CaseMisMatch_FoldResultsInMultipleCodePoints()
    {
        //This doesn't currently pass, I think ucase_toFullFolding would give the number of code points that resulted from the fold.
        //I wasn't sure how to reference that
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Ignore", L"true")
        END_TEST_METHOD_PROPERTIES()

        AssertScoreAndPositions(
            L"fuss",
            L"Fußball",
            //I think ScoreMatch * 4 is correct in this case since it matches 4 codepoints pattern??? fuss
            ScoreMatch * 4 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 3,
            //Only 3 positions in the text were matched
            { 2, 1, 0 });
    }

    void FzfTests::French_CaseMatch()
    {
        AssertScoreAndPositions(
            L"Éco",
            L"École",
            ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 2,
            { 2, 1, 0 });
    }

    void FzfTests::French_CaseMisMatch()
    {
        AssertScoreAndPositions(
            L"Éco",
            L"école",
            ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 2,
            { 2, 1, 0 });
    }

    void FzfTests::Greek_CaseMatch()
    {
        AssertScoreAndPositions(
            L"λόγος",
            L"λόγος",
            ScoreMatch * 5 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 4,
            { 4, 3, 2, 1, 0 });
    }

    void FzfTests::Greek_CaseMisMatch()
    {
        //I think this tests validates folding (σ, ς)
        AssertScoreAndPositions(
            L"λόγοσ",
            L"λόγος",
            ScoreMatch * 5 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 4,
            { 4, 3, 2, 1, 0 });
    }

    void FzfTests::English_CaseMatch()
    {
        AssertScoreAndPositions(
            L"Newer",
            L"Newer tab",
            ScoreMatch * 5 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 4,
            { 4, 3, 2, 1, 0 });
    }

    void FzfTests::English_CaseMisMatch()
    {
        AssertScoreAndPositions(
            L"newer",
            L"Newer tab",
            ScoreMatch * 5 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 4,
            { 4, 3, 2, 1, 0 });
    }

    void FzfTests::SurrogatePair()
    {
        AssertScoreAndPositions(
            L"N😀ewer",
            L"N😀ewer tab",
            ScoreMatch * 6 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 5,
            { 6, 5, 4, 3, 2, 1, 0 });
    }

    void FzfTests::SurrogatePair_ToUtf16Pos_ConsecutiveChars()
    {
        AssertScoreAndPositions(
            L"N𠀋N😀𝄞e𐐷",
            L"N𠀋N😀𝄞e𐐷 tab",
            ScoreMatch * 7 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 6,
            { 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 });
    }

    void FzfTests::SurrogatePair_ToUtf16Pos_PreferConsecutiveChars()
    {
        AssertScoreAndPositions(
            L"𠀋😀",
            L"N𠀋😀wer 😀b𐐷 ",
            ScoreMatch * 2 + BonusConsecutive * 2,
            { 4, 3, 2, 1 });
    }

    void FzfTests::SurrogatePair_ToUtf16Pos_GapAndBoundary()
    {
        AssertScoreAndPositions(
            L"𠀋😀",
            L"N𠀋wer 😀b𐐷 ",
            ScoreMatch * 2 + ScoreGapStart + ScoreGapExtension * 3 + BonusBoundary,
            { 8, 7, 2, 1 });
    }

    void FzfTests::MatchOnNonWordChars_CaseInSensitive()
    {
        AssertScoreAndPositions(
            L"foo-b",
            L"xFoo-Bar Baz",
            ScoreMatch * 5 + BonusConsecutive * 4 + BonusBoundary,
            { 5, 4, 3, 2, 1 });
    }

    void FzfTests::MatchOnNonWordCharsWithGap()
    {
        AssertScoreAndPositions(
            L"12356",
            L"abc123 456",
            ScoreMatch * 5 + BonusCamel123 * BonusFirstCharMultiplier + BonusCamel123 * 2 + BonusConsecutive + ScoreGapStart + ScoreGapExtension,
            { 9, 8, 5, 4, 3 });
    }

    void FzfTests::BonusBoundaryAndFirstCharMultiplier()
    {
        AssertScoreAndPositions(
            L"fbb",
            L"foo bar baz",
            ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusBoundary * 2 + 2 * ScoreGapStart + 4 * ScoreGapExtension,
            { 8, 4, 0 });
    }

    void FzfTests::MatchesAreCaseInSensitive()
    {
        AssertScoreAndPositions(
            L"FBB",
            L"foo bar baz",
            ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusBoundary * 2 + 2 * ScoreGapStart + 4 * ScoreGapExtension,
            { 8, 4, 0 });
    }

    void FzfTests::MultipleTerms()
    {
        auto term1Score = ScoreMatch * 2 + BonusBoundary * BonusFirstCharMultiplier + (BonusFirstCharMultiplier * BonusConsecutive);
        auto term2Score = ScoreMatch * 4 + BonusBoundary * BonusFirstCharMultiplier + (BonusFirstCharMultiplier * BonusConsecutive) * 3;

        AssertScoreAndPositions(
            L"sp anta",
            L"Split Pane, split: horizontal, profile: SSH: Antares",
            term1Score + term2Score,
            { 1, 0, 48, 47, 46, 45 });
    }

    void FzfTests::MultipleTerms_AllCharsMatch()
    {
        auto term1Score = ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + (BonusFirstCharMultiplier * BonusConsecutive * 2);
        auto term2Score = term1Score;

        AssertScoreAndPositions(
            L"foo bar",
            L"foo bar",
            term1Score + term2Score,
            { 2, 1, 0, 6, 5, 4 });
    }

    void FzfTests::MultipleTerms_NotAllTermsMatch()
    {
        AssertScoreAndPositions(
            L"sp anta zz",
            L"Split Pane, split: horizontal, profile: SSH: Antares",
            0,
            {});
    }

    void FzfTests::MatchesAreCaseInSensitive_BonusBoundary()
    {
        AssertScoreAndPositions(
            L"fbb",
            L"Foo Bar Baz",
            ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusBoundary * 2 + 2 * ScoreGapStart + 4 * ScoreGapExtension,
            { 8, 4, 0 });
    }

    void FzfTests::TraceBackWillPickTheFirstMatchIfBothHaveTheSameScore()
    {
        AssertScoreAndPositions(
            L"bar",
            L"Foo Bar Bar",
            ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier * 2,
            { 6, 5, 4 });
    }

    void FzfTests::TraceBackWillPickTheMatchWithTheHighestScore()
    {
        AssertScoreAndPositions(
            L"bar",
            L"Foo aBar Bar",
            ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier * 2,
            { 11, 10, 9 });
    }

    void FzfTests::TraceBackWillPickTheMatchWithTheHighestScore_Gaps()
    {
        AssertScoreAndPositions(
            L"bar",
            L"Boo Author Raz Bar",
            ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 2,
            { 17, 16, 15 });
    }

    void FzfTests::TraceBackWillPickEarlierCharsWhenNoBonus()
    {
        AssertScoreAndPositions(
            L"clts",
            L"close all tabs after this",
            ScoreMatch * 4 + BonusBoundary * BonusFirstCharMultiplier + BonusFirstCharMultiplier * BonusConsecutive + ScoreGapStart + ScoreGapExtension * 7 + BonusBoundary + ScoreGapStart + ScoreGapExtension,
            { 13, 10, 1, 0 });
    }

    void FzfTests::ConsecutiveMatchWillScoreHigherThanMatchWithGapWhenBothDontHaveBonus()
    {
        auto consecutiveScore = ScoreMatch * 3 + BonusConsecutive * 2;
        auto gapScore = (ScoreMatch * 3) + ScoreGapStart + ScoreGapStart;

        AssertScoreAndPositions(
            L"oob",
            L"aoobar",
            consecutiveScore,
            { 3, 2, 1 });

        AssertScoreAndPositions(
            L"oob",
            L"aoaoabound",
            gapScore,
            { 5, 3, 1 });

        VERIFY_IS_GREATER_THAN(consecutiveScore, gapScore);
    }

    void FzfTests::ConsecutiveMatchWillScoreHigherThanMatchWithGapWhenBothHaveFirstCharBonus()
    {
        auto consecutiveScore = ScoreMatch * 3 + BonusFirstCharMultiplier * BonusBoundary + BonusFirstCharMultiplier * BonusConsecutive * 2;
        auto gapScore = (ScoreMatch * 3) + (BonusBoundary * BonusFirstCharMultiplier) + ScoreGapStart + ScoreGapStart;

        AssertScoreAndPositions(
            L"oob",
            L"oobar",
            consecutiveScore,
            { 2, 1, 0 });

        AssertScoreAndPositions(
            L"oob",
            L"oaoabound",
            gapScore,
            { 4, 2, 0 });

        VERIFY_IS_GREATER_THAN(consecutiveScore, gapScore);
    }

    void FzfTests::MatchWithGapCanAHaveHigherScoreThanConsecutiveWhenGapMatchHasBoundaryBonus()
    {
        auto consecutiveScore = ScoreMatch * 3 + BonusConsecutive * 2;
        auto gapScore = (ScoreMatch * 3) + (BonusBoundary * BonusFirstCharMultiplier) + (BonusBoundary * 2) + ScoreGapStart + (ScoreGapExtension * 2) + ScoreGapStart + ScoreGapExtension;

        AssertScoreAndPositions(
            L"oob",
            L"foobar",
            consecutiveScore,
            { 3, 2, 1 });

        AssertScoreAndPositions(
            L"oob",
            L"out-of-bound",
            gapScore,
            { 7, 4, 0 });

        VERIFY_IS_GREATER_THAN(gapScore, consecutiveScore);
    }

    void FzfTests::MatchWithGapCanHaveHigherScoreThanConsecutiveWhenGapHasFirstCharBonus()
    {
        auto consecutiveScore = ScoreMatch * 2 + BonusConsecutive;
        auto gapScore = ScoreMatch * 2 + BonusBoundary * BonusFirstCharMultiplier + ScoreGapStart;

        AssertScoreAndPositions(
            L"ob",
            L"aobar",
            consecutiveScore,
            { 2, 1 });

        AssertScoreAndPositions(
            L"ob",
            L"oabar",
            gapScore,
            { 2, 0 });

        VERIFY_IS_GREATER_THAN(gapScore, consecutiveScore);
    }

    void FzfTests::MatchWithGapThatMatchesOnTheFirstCharWillNoLongerHigherScoreThanConsecutiveCharsWhenTheGapIs11_2CharPattern()
    {
        auto consecutiveScore = ScoreMatch * 2 + BonusConsecutive;
        auto gapScore = ScoreMatch * 2 + BonusBoundary * BonusFirstCharMultiplier + ScoreGapStart + ScoreGapExtension * 10;

        AssertScoreAndPositions(
            L"ob",
            L"aobar",
            consecutiveScore,
            { 2, 1 });

        AssertScoreAndPositions(
            L"ob",
            L"oaaaaaaaaaaabar",
            gapScore,
            { 12, 0 });

        VERIFY_IS_GREATER_THAN(consecutiveScore, gapScore);
    }

    void FzfTests::MatchWithGapThatMatchesOnTheFirstCharWillNoLongerHigherScoreThanConsecutiveCharsWhenTheGapIs11_3CharPattern_1ConsecutiveChar()
    {
        auto consecutiveScore = ScoreMatch * 3 + BonusConsecutive * 2;
        auto gapScore = ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive + ScoreGapStart + ScoreGapExtension * 10;

        AssertScoreAndPositions(
            L"oba",
            L"aobar",
            consecutiveScore,
            { 3, 2, 1 });

        AssertScoreAndPositions(
            L"oba",
            L"oaaaaaaaaaaabar",
            gapScore,
            { 13, 12, 0 });

        VERIFY_IS_GREATER_THAN(consecutiveScore, gapScore);
    }

    void FzfTests::MatchWithGapThatMatchesOnTheFirstCharWillNoLongerHigherScoreThanConsecutiveCharsWhenTheGapIs5_NoConsecutiveChars_3CharPattern()
    {
        auto allConsecutiveScore = ScoreMatch * 3 + BonusConsecutive * 2;
        auto allBoundaryWithGapScore = ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + ScoreGapStart + ScoreGapExtension + ScoreGapExtension + ScoreGapStart + ScoreGapExtension;

        AssertScoreAndPositions(
            L"oba",
            L"aobar",
            allConsecutiveScore,
            { 3, 2, 1 });

        AssertScoreAndPositions(
            L"oba",
            L"oaaabzzar",
            allBoundaryWithGapScore,
            { 7, 4, 0 });

        VERIFY_IS_GREATER_THAN(allConsecutiveScore, allBoundaryWithGapScore);
    }

    void FzfTests::MatchWithGapThatMatchesOnTheFirstCharWillNoLongerScoreHigherThanConsecutiveCharsWhenTheGapIs3_NoConsecutiveChar_4CharPattern()
    {
        auto consecutiveScore = ScoreMatch * 4 + BonusConsecutive * 3;
        auto gapScore = ScoreMatch * 4 + BonusBoundary * BonusFirstCharMultiplier + ScoreGapStart * 3;

        AssertScoreAndPositions(
            L"obar",
            L"aobar",
            consecutiveScore,
            { 4, 3, 2, 1 });

        AssertScoreAndPositions(
            L"obar",
            L"oabzazr",
            gapScore,
            { 6, 4, 2, 0 });

        VERIFY_IS_GREATER_THAN(consecutiveScore, gapScore);
    }
}
