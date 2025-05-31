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
        TEST_METHOD(BonusForCamelCaseMatch);
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

    void AssertScoreAndRuns(std::wstring_view patternText, std::wstring_view text, int expectedScore, const std::vector<fzf::matcher::TextRun>& expectedRuns)
    {
        const auto pattern = fzf::matcher::ParsePattern(patternText);
        const auto match = fzf::matcher::Match(text, pattern);

        if (expectedScore == 0 && expectedRuns.empty())
        {
            VERIFY_ARE_EQUAL(std::nullopt, match);
            return;
        }

        VERIFY_IS_TRUE(match.has_value());
        VERIFY_ARE_EQUAL(expectedScore, match->Score);

        const auto& runs = match->Runs;
        VERIFY_ARE_EQUAL(expectedRuns.size(), runs.size());

        for (size_t i = 0; i < expectedRuns.size(); ++i)
        {
            VERIFY_ARE_EQUAL(expectedRuns[i].Start, runs[i].Start);
            VERIFY_ARE_EQUAL(expectedRuns[i].End, runs[i].End);
        }
    }

    void FzfTests::AllPatternCharsDoNotMatch()
    {
        AssertScoreAndRuns(
            L"fbb",
            L"foo bar",
            0,
            {});
    }

    void FzfTests::ConsecutiveChars()
    {
        AssertScoreAndRuns(
            L"oba",
            L"foobar",
            ScoreMatch * 3 + BonusConsecutive * 2,
            { { 2, 4 } });
    }

    void FzfTests::ConsecutiveChars_FirstCharBonus()
    {
        AssertScoreAndRuns(
            L"foo",
            L"foobar",
            ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 2,
            { { 0, 2 } });
    }

    void FzfTests::NonWordBonusBoundary_ConsecutiveChars()
    {
        AssertScoreAndRuns(
            L"zshc",
            L"/man1/zshcompctl.1",
            ScoreMatch * 4 + BonusBoundary * BonusFirstCharMultiplier + BonusFirstCharMultiplier * BonusConsecutive * 3,
            { { 6, 9 } });
    }

    void FzfTests::Russian_CaseMisMatch()
    {
        AssertScoreAndRuns(
            L"новая",
            L"Новая вкладка",
            ScoreMatch * 5 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 4,
            { { 0, 4 } });
    }

    void FzfTests::Russian_CaseMatch()
    {
        AssertScoreAndRuns(
            L"Новая",
            L"Новая вкладка",
            ScoreMatch * 5 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 4,
            { { 0, 4 } });
    }

    void FzfTests::German_CaseMatch()
    {
        AssertScoreAndRuns(
            L"fuß",
            L"Fußball",
            ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 2,
            { { 0, 2 } });
    }

    void FzfTests::German_CaseMisMatch_FoldResultsInMultipleCodePoints()
    {
        //This doesn't currently pass, I think ucase_toFullFolding would give the number of code points that resulted from the fold.
        //I wasn't sure how to reference that
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Ignore", L"true")
        END_TEST_METHOD_PROPERTIES()

        AssertScoreAndRuns(
            L"fuss",
            L"Fußball",
            //I think ScoreMatch * 4 is correct in this case since it matches 4 codepoints pattern??? fuss
            ScoreMatch * 4 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 3,
            //Only 3 positions in the text were matched
            { { 0, 2 } });
    }

    void FzfTests::French_CaseMatch()
    {
        AssertScoreAndRuns(
            L"Éco",
            L"École",
            ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 2,
            { { 0, 2 } });
    }

    void FzfTests::French_CaseMisMatch()
    {
        AssertScoreAndRuns(
            L"Éco",
            L"école",
            ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 2,
            { { 0, 2 } });
    }

    void FzfTests::Greek_CaseMatch()
    {
        AssertScoreAndRuns(
            L"λόγος",
            L"λόγος",
            ScoreMatch * 5 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 4,
            { { 0, 4 } });
    }

    void FzfTests::Greek_CaseMisMatch()
    {
        //I think this tests validates folding (σ, ς)
        AssertScoreAndRuns(
            L"λόγοσ",
            L"λόγος",
            ScoreMatch * 5 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 4,
            { { 0, 4 } });
    }

    void FzfTests::English_CaseMatch()
    {
        AssertScoreAndRuns(
            L"Newer",
            L"Newer tab",
            ScoreMatch * 5 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 4,
            { { 0, 4 } });
    }

    void FzfTests::English_CaseMisMatch()
    {
        AssertScoreAndRuns(
            L"newer",
            L"Newer tab",
            ScoreMatch * 5 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 4,
            { { 0, 4 } });
    }

    void FzfTests::SurrogatePair()
    {
        AssertScoreAndRuns(
            L"N😀ewer",
            L"N😀ewer tab",
            ScoreMatch * 6 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 5,
            { { 0, 6 } });
    }

    void FzfTests::SurrogatePair_ToUtf16Pos_ConsecutiveChars()
    {
        AssertScoreAndRuns(
            L"N𠀋N😀𝄞e𐐷",
            L"N𠀋N😀𝄞e𐐷 tab",
            ScoreMatch * 7 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 6,
            { { 0, 10 } });
    }

    void FzfTests::SurrogatePair_ToUtf16Pos_PreferConsecutiveChars()
    {
        AssertScoreAndRuns(
            L"𠀋😀",
            L"N𠀋😀wer 😀b𐐷 ",
            ScoreMatch * 2 + BonusConsecutive * 2,
            { { 1, 4 } });
    }

    void FzfTests::SurrogatePair_ToUtf16Pos_GapAndBoundary()
    {
        AssertScoreAndRuns(
            L"𠀋😀",
            L"N𠀋wer 😀b𐐷 ",
            ScoreMatch * 2 + ScoreGapStart + ScoreGapExtension * 3 + BonusBoundary,
            { { 1, 2 }, { 7, 8 } });
    }

    void FzfTests::MatchOnNonWordChars_CaseInSensitive()
    {
        AssertScoreAndRuns(
            L"foo-b",
            L"xFoo-Bar Baz",
            (ScoreMatch + BonusCamel123 * BonusFirstCharMultiplier) +
                (ScoreMatch + BonusCamel123) +
                (ScoreMatch + BonusCamel123) +
                (ScoreMatch + BonusBoundary) +
                (ScoreMatch + BonusNonWord),
            { { 1, 5 } });
    }

    void FzfTests::MatchOnNonWordCharsWithGap()
    {
        AssertScoreAndRuns(
            L"12356",
            L"abc123 456",
            (ScoreMatch + BonusCamel123 * BonusFirstCharMultiplier) +
                (ScoreMatch + BonusCamel123) +
                (ScoreMatch + BonusCamel123) +
                ScoreGapStart +
                ScoreGapExtension +
                ScoreMatch +
                ScoreMatch + BonusConsecutive,
            { { 3, 5 }, { 8, 9 } });
    }

    void FzfTests::BonusForCamelCaseMatch()
    {
        AssertScoreAndRuns(
            L"def56",
            L"abcDEF 456",
            (ScoreMatch + BonusCamel123 * BonusFirstCharMultiplier) +
                (ScoreMatch + BonusCamel123) +
                (ScoreMatch + BonusCamel123) +
                ScoreGapStart +
                ScoreGapExtension +
                ScoreMatch +
                (ScoreMatch + BonusConsecutive),
            { { 3, 5 }, { 8, 9 } });
    }

    void FzfTests::BonusBoundaryAndFirstCharMultiplier()
    {
        AssertScoreAndRuns(
            L"fbb",
            L"foo bar baz",
            ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusBoundary * 2 + 2 * ScoreGapStart + 4 * ScoreGapExtension,
            { { 0, 0 }, { 4, 4 }, { 8, 8 } });
    }

    void FzfTests::MatchesAreCaseInSensitive()
    {
        AssertScoreAndRuns(
            L"FBB",
            L"foo bar baz",
            ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusBoundary * 2 + 2 * ScoreGapStart + 4 * ScoreGapExtension,
            { { 0, 0 }, { 4, 4 }, { 8, 8 } });
    }

    void FzfTests::MultipleTerms()
    {
        auto term1Score = ScoreMatch * 2 + BonusBoundary * BonusFirstCharMultiplier + (BonusFirstCharMultiplier * BonusConsecutive);
        auto term2Score = ScoreMatch * 4 + BonusBoundary * BonusFirstCharMultiplier + (BonusFirstCharMultiplier * BonusConsecutive) * 3;

        AssertScoreAndRuns(
            L"sp anta",
            L"Split Pane, split: horizontal, profile: SSH: Antares",
            term1Score + term2Score,
            { { 0, 1 }, { 45, 48 } });
    }

    void FzfTests::MultipleTerms_AllCharsMatch()
    {
        auto term1Score = ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + (BonusFirstCharMultiplier * BonusConsecutive * 2);
        auto term2Score = term1Score;

        AssertScoreAndRuns(
            L"foo bar",
            L"foo bar",
            term1Score + term2Score,
            { { 0, 2 }, { 4, 6 } });
    }

    void FzfTests::MultipleTerms_NotAllTermsMatch()
    {
        AssertScoreAndRuns(
            L"sp anta zz",
            L"Split Pane, split: horizontal, profile: SSH: Antares",
            0,
            {});
    }

    void FzfTests::MatchesAreCaseInSensitive_BonusBoundary()
    {
        AssertScoreAndRuns(
            L"fbb",
            L"Foo Bar Baz",
            ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusBoundary * 2 + 2 * ScoreGapStart + 4 * ScoreGapExtension,
            { { 0, 0 }, { 4, 4 }, { 8, 8 } });
    }

    void FzfTests::TraceBackWillPickTheFirstMatchIfBothHaveTheSameScore()
    {
        AssertScoreAndRuns(
            L"bar",
            L"Foo Bar Bar",
            (ScoreMatch + BonusBoundary * BonusFirstCharMultiplier) +
                (ScoreMatch + BonusBoundary) +
                (ScoreMatch + BonusBoundary),
            //ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier * 2,
            { { 4, 6 } });
    }

    void FzfTests::TraceBackWillPickTheMatchWithTheHighestScore()
    {
        AssertScoreAndRuns(
            L"bar",
            L"Foo aBar Bar",
            ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier * 2,
            { { 9, 11 } });
    }

    void FzfTests::TraceBackWillPickTheMatchWithTheHighestScore_Gaps()
    {
        AssertScoreAndRuns(
            L"bar",
            L"Boo Author Raz Bar",
            ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive * BonusFirstCharMultiplier * 2,
            { { 15, 17 } });
    }

    void FzfTests::TraceBackWillPickEarlierCharsWhenNoBonus()
    {
        AssertScoreAndRuns(
            L"clts",
            L"close all tabs after this",
            ScoreMatch * 4 + BonusBoundary * BonusFirstCharMultiplier + BonusFirstCharMultiplier * BonusConsecutive + ScoreGapStart + ScoreGapExtension * 7 + BonusBoundary + ScoreGapStart + ScoreGapExtension,
            { { 0, 1 }, { 10, 10 }, { 13, 13 } });
    }

    void FzfTests::ConsecutiveMatchWillScoreHigherThanMatchWithGapWhenBothDontHaveBonus()
    {
        auto consecutiveScore = ScoreMatch * 3 + BonusConsecutive * 2;
        auto gapScore = (ScoreMatch * 3) + ScoreGapStart + ScoreGapStart;

        AssertScoreAndRuns(
            L"oob",
            L"aoobar",
            consecutiveScore,
            { { 1, 3 } });

        AssertScoreAndRuns(
            L"oob",
            L"aoaoabound",
            gapScore,
            { { 1, 1 }, { 3, 3 }, { 5, 5 } });

        VERIFY_IS_GREATER_THAN(consecutiveScore, gapScore);
    }

    void FzfTests::ConsecutiveMatchWillScoreHigherThanMatchWithGapWhenBothHaveFirstCharBonus()
    {
        auto consecutiveScore = ScoreMatch * 3 + BonusFirstCharMultiplier * BonusBoundary + BonusFirstCharMultiplier * BonusConsecutive * 2;
        auto gapScore = (ScoreMatch * 3) + (BonusBoundary * BonusFirstCharMultiplier) + ScoreGapStart + ScoreGapStart;

        AssertScoreAndRuns(
            L"oob",
            L"oobar",
            consecutiveScore,
            { { 0, 2 } });

        AssertScoreAndRuns(
            L"oob",
            L"oaoabound",
            gapScore,
            { { 0, 0 }, { 2, 2 }, { 4, 4 } });

        VERIFY_IS_GREATER_THAN(consecutiveScore, gapScore);
    }

    void FzfTests::MatchWithGapCanAHaveHigherScoreThanConsecutiveWhenGapMatchHasBoundaryBonus()
    {
        auto consecutiveScore = ScoreMatch * 3 + BonusConsecutive * 2;
        auto gapScore = (ScoreMatch * 3) + (BonusBoundary * BonusFirstCharMultiplier) + (BonusBoundary * 2) + ScoreGapStart + (ScoreGapExtension * 2) + ScoreGapStart + ScoreGapExtension;

        AssertScoreAndRuns(
            L"oob",
            L"foobar",
            consecutiveScore,
            { { 1, 3 } });

        AssertScoreAndRuns(
            L"oob",
            L"out-of-bound",
            gapScore,
            { { 0, 0 }, { 4, 4 }, { 7, 7 } });

        VERIFY_IS_GREATER_THAN(gapScore, consecutiveScore);
    }

    void FzfTests::MatchWithGapCanHaveHigherScoreThanConsecutiveWhenGapHasFirstCharBonus()
    {
        auto consecutiveScore = ScoreMatch * 2 + BonusConsecutive;
        auto gapScore = ScoreMatch * 2 + BonusBoundary * BonusFirstCharMultiplier + ScoreGapStart;

        AssertScoreAndRuns(
            L"ob",
            L"aobar",
            consecutiveScore,
            { { 1, 2 } });

        AssertScoreAndRuns(
            L"ob",
            L"oabar",
            gapScore,
            { { 0, 0 }, { 2, 2 } });

        VERIFY_IS_GREATER_THAN(gapScore, consecutiveScore);
    }

    void FzfTests::MatchWithGapThatMatchesOnTheFirstCharWillNoLongerHigherScoreThanConsecutiveCharsWhenTheGapIs11_2CharPattern()
    {
        auto consecutiveScore = ScoreMatch * 2 + BonusConsecutive;
        auto gapScore = ScoreMatch * 2 + BonusBoundary * BonusFirstCharMultiplier + ScoreGapStart + ScoreGapExtension * 10;

        AssertScoreAndRuns(
            L"ob",
            L"aobar",
            consecutiveScore,
            { { 1, 2 } });

        AssertScoreAndRuns(
            L"ob",
            L"oaaaaaaaaaaabar",
            gapScore,
            { { 0, 0 }, { 12, 12 } });

        VERIFY_IS_GREATER_THAN(consecutiveScore, gapScore);
    }

    void FzfTests::MatchWithGapThatMatchesOnTheFirstCharWillNoLongerHigherScoreThanConsecutiveCharsWhenTheGapIs11_3CharPattern_1ConsecutiveChar()
    {
        auto consecutiveScore = ScoreMatch * 3 + BonusConsecutive * 2;
        auto gapScore = ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusConsecutive + ScoreGapStart + ScoreGapExtension * 10;

        AssertScoreAndRuns(
            L"oba",
            L"aobar",
            consecutiveScore,
            { { 1, 3 } });

        AssertScoreAndRuns(
            L"oba",
            L"oaaaaaaaaaaabar",
            gapScore,
            { { 0, 0 }, { 12, 13 } });

        VERIFY_IS_GREATER_THAN(consecutiveScore, gapScore);
    }

    void FzfTests::MatchWithGapThatMatchesOnTheFirstCharWillNoLongerHigherScoreThanConsecutiveCharsWhenTheGapIs5_NoConsecutiveChars_3CharPattern()
    {
        auto allConsecutiveScore = ScoreMatch * 3 + BonusConsecutive * 2;
        auto allBoundaryWithGapScore = ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + ScoreGapStart + ScoreGapExtension + ScoreGapExtension + ScoreGapStart + ScoreGapExtension;

        AssertScoreAndRuns(
            L"oba",
            L"aobar",
            allConsecutiveScore,
            { { 1, 3 } });

        AssertScoreAndRuns(
            L"oba",
            L"oaaabzzar",
            allBoundaryWithGapScore,
            { { 0, 0 }, { 4, 4 }, { 7, 7 } });

        VERIFY_IS_GREATER_THAN(allConsecutiveScore, allBoundaryWithGapScore);
    }

    void FzfTests::MatchWithGapThatMatchesOnTheFirstCharWillNoLongerScoreHigherThanConsecutiveCharsWhenTheGapIs3_NoConsecutiveChar_4CharPattern()
    {
        auto consecutiveScore = ScoreMatch * 4 + BonusConsecutive * 3;
        auto gapScore = ScoreMatch * 4 + BonusBoundary * BonusFirstCharMultiplier + ScoreGapStart * 3;

        AssertScoreAndRuns(
            L"obar",
            L"aobar",
            consecutiveScore,
            { { 1, 4 } });

        AssertScoreAndRuns(
            L"obar",
            L"oabzazr",
            gapScore,
            { { 0, 0 }, { 2, 2 }, { 4, 4 }, { 6, 6 } });

        VERIFY_IS_GREATER_THAN(consecutiveScore, gapScore);
    }
}
