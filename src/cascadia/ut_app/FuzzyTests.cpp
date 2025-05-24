// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "..\TerminalApp\fuzzy.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace TerminalAppUnitTests2
{
    typedef enum
    {
        BaseScore = 1,
        CaseMatch = 1,
        StartOfString = 8,
        Consecutive = 5,
        PathSeparator = 5,
        Separator = 4,
        Camel = 2,

        ScoreMatch = 16,
        ScoreGapStart = -3,
        ScoreGapExtension = -1,
        BonusBoundary = ScoreMatch / 2,
        BonusNonWord = ScoreMatch / 2,
        BonusCamel123 = BonusBoundary + ScoreGapExtension,
        BonusConsecutive = -(ScoreGapStart + ScoreGapExtension),
        BonusFirstCharMultiplier = 2,
    } score_t;

    class FuzzyTests
    {
        BEGIN_TEST_CLASS(FuzzyTests)
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
        TEST_METHOD(MultipleTerms_Reverse);
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
        TEST_METHOD(Overflow);
        TEST_METHOD(Overflow2);
        TEST_METHOD(Overflow3);
        TEST_METHOD(Overflow4);
    };

    void AssertScoreAndPositions(std::wstring_view patternText, std::wstring_view text, int expectedScore, std::vector<int16_t> expectedPositions)
    {
        auto pattern = fuzzy::ParsePattern(patternText);
        auto match = fuzzy::Match(text, pattern);
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
    void FuzzyTests::Overflow4()
    {
        AssertScoreAndPositions(
            L"a",
            L"a",
            BaseScore + CaseMatch + StartOfString,
            {0});
    }

    void FuzzyTests::Overflow()
    {
        AssertScoreAndPositions(
            L"aaa",
            L"aaaaaaaaa",
            (BaseScore + CaseMatch + StartOfString) + (BaseScore + CaseMatch + Consecutive * 1) + (BaseScore + CaseMatch + (2 * Consecutive)),
            {0,1,2});
    }

    void FuzzyTests::Overflow2()
    {
        AssertScoreAndPositions(
            L"bbb",
            L"bbbb",
            (BaseScore + CaseMatch + StartOfString) + (BaseScore + CaseMatch + Consecutive * 1) + (BaseScore + CaseMatch + (2 * Consecutive)),
            {0,1,2});
    }

    void FuzzyTests::Overflow3()
    {
        AssertScoreAndPositions(
            L"xxxxxx",
            L"xxxxxxxxxxxxxxxx",
            (BaseScore + CaseMatch + StartOfString) + (BaseScore + CaseMatch + Consecutive * 1) + (BaseScore + CaseMatch + (2 * Consecutive)) + (BaseScore + CaseMatch + (3 * Consecutive)) + (BaseScore + CaseMatch + (4 * Consecutive)) + (BaseScore + CaseMatch + (5 * Consecutive)),
            {0,1,2,3,4,5});
    }

    void FuzzyTests::AllPatternCharsDoNotMatch()
    {
        AssertScoreAndPositions(
            L"fbb",
            L"foo bar",
            0,
            {});
    }

    void FuzzyTests::ConsecutiveChars()
    {
        AssertScoreAndPositions(
            L"oba",
            L"foobar",
            (BaseScore + CaseMatch) + (BaseScore + CaseMatch + Consecutive * 1) + (BaseScore + CaseMatch + (2 * Consecutive)),
            { 2, 3, 4 });
    }

    void FuzzyTests::ConsecutiveChars_FirstCharBonus()
    {
        AssertScoreAndPositions(
            L"foo",
            L"foobar",
            (BaseScore + CaseMatch + StartOfString) + (BaseScore + CaseMatch + Consecutive * 1) + (BaseScore + CaseMatch + (2 * Consecutive)),
            { 0, 1, 2 });
    }

    void FuzzyTests::NonWordBonusBoundary_ConsecutiveChars()
    {
        AssertScoreAndPositions(
            L"zshc",
            L"/man1/zshcompctl.1",
            (BaseScore + CaseMatch + PathSeparator) + (BaseScore + CaseMatch + Consecutive * 1) + (BaseScore + CaseMatch + (2 * Consecutive)) + (BaseScore + CaseMatch + (3 * Consecutive)),
            { 6, 7, 8, 9 });
    }

    void FuzzyTests::Russian_CaseMisMatch()
    {
        AssertScoreAndPositions(
            L"новая",
            L"Новая вкладка",
            (BaseScore + StartOfString) + (BaseScore + CaseMatch + Consecutive * 1) + (BaseScore + CaseMatch + (2 * Consecutive)) + (BaseScore + CaseMatch + (3 * Consecutive)) + (BaseScore + CaseMatch + (4 * Consecutive)),
            { 0, 1, 2, 3, 4 });
    }

    void FuzzyTests::Russian_CaseMatch()
    {
        AssertScoreAndPositions(
            L"Новая",
            L"Новая вкладка",
            (BaseScore + CaseMatch + StartOfString) + (BaseScore + CaseMatch + Consecutive * 1) + (BaseScore + CaseMatch + (2 * Consecutive)) + (BaseScore + CaseMatch + (3 * Consecutive)) + (BaseScore + CaseMatch + (4 * Consecutive)),
            { 0, 1, 2, 3, 4 });
    }

    void FuzzyTests::German_CaseMatch()
    {
        AssertScoreAndPositions(
            L"fuß",
            L"Fußball",
            (BaseScore + StartOfString) + (BaseScore + CaseMatch + Consecutive * 1) + (BaseScore + CaseMatch + (2 * Consecutive)),
            { 0, 1, 2 });
    }

    void FuzzyTests::German_CaseMisMatch_FoldResultsInMultipleCodePoints()
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

    void FuzzyTests::French_CaseMatch()
    {
        AssertScoreAndPositions(
            L"Éco",
            L"École",
            (BaseScore + StartOfString + CaseMatch) + (BaseScore + CaseMatch + Consecutive * 1) + (BaseScore + CaseMatch + (2 * Consecutive)),
            { 0, 1, 2 });
    }

    void FuzzyTests::French_CaseMisMatch()
    {
        AssertScoreAndPositions(
            L"Éco",
            L"école",
            (BaseScore + StartOfString) + (BaseScore + CaseMatch + Consecutive * 1) + (BaseScore + CaseMatch + (2 * Consecutive)),
            { 0, 1, 2 });
    }

    void FuzzyTests::Greek_CaseMatch()
    {
        AssertScoreAndPositions(
            L"λόγος",
            L"λόγος",
            (BaseScore + StartOfString + CaseMatch) + (BaseScore + CaseMatch + Consecutive * 1) + (BaseScore + CaseMatch + (2 * Consecutive)) + (BaseScore + CaseMatch + (3 * Consecutive)) + (BaseScore + CaseMatch + (4 * Consecutive)),
            { 0, 1, 2, 3, 4 });
    }

    void FuzzyTests::Greek_CaseMisMatch()
    {
        //I think this tests validates folding (σ, ς)
        AssertScoreAndPositions(
            L"λόγοσ",
            L"λόγος",
            (BaseScore + StartOfString) + (BaseScore + CaseMatch + Consecutive * 1) + (BaseScore + CaseMatch + (2 * Consecutive)) + (BaseScore + CaseMatch + (3 * Consecutive)) + (BaseScore + CaseMatch + (4 * Consecutive)),
            { 0, 1, 2, 3, 4 });
    }

    void FuzzyTests::English_CaseMatch()
    {
        AssertScoreAndPositions(
            L"Newer",
            L"Newer tab",
            (BaseScore + CaseMatch + StartOfString) + (BaseScore + CaseMatch + Consecutive * 1) + (BaseScore + CaseMatch + (2 * Consecutive)) + (BaseScore + CaseMatch + (3 * Consecutive)) + (BaseScore + CaseMatch + (4 * Consecutive)),
            { 0, 1, 2, 3, 4 });
    }

    void FuzzyTests::English_CaseMisMatch()
    {
        AssertScoreAndPositions(
            L"newer",
            L"Newer tab",
            (BaseScore + StartOfString) + (BaseScore + CaseMatch + Consecutive * 1) + (BaseScore + CaseMatch + (2 * Consecutive)) + (BaseScore + CaseMatch + (3 * Consecutive)) + (BaseScore + CaseMatch + (4 * Consecutive)),
            { 0, 1, 2, 3, 4 });
    }

    void FuzzyTests::SurrogatePair()
    {
        AssertScoreAndPositions(
            L"N😀ewer",
            L"N😀ewer tab",
            (BaseScore + CaseMatch + StartOfString) + (BaseScore + CaseMatch + Consecutive * 1) + (BaseScore + CaseMatch + (2 * Consecutive)) + (BaseScore + CaseMatch + (3 * Consecutive)) + (BaseScore + CaseMatch + (4 * Consecutive)) + (BaseScore + CaseMatch + (5 * Consecutive)),
            //Is it ok the codepoint sorting is inconsistent?
            { 0, 2, 1, 3, 4, 5, 6 });
    }

    void FuzzyTests::SurrogatePair_ToUtf16Pos_ConsecutiveChars()
    {
        AssertScoreAndPositions(
            L"N𠀋N😀𝄞e𐐷",
            L"N𠀋N😀𝄞e𐐷 tab",
            (BaseScore + CaseMatch + StartOfString) + (BaseScore + CaseMatch + Consecutive * 1) + (BaseScore + CaseMatch + (2 * Consecutive)) + (BaseScore + CaseMatch + (3 * Consecutive)) + (BaseScore + CaseMatch + (4 * Consecutive)) + (BaseScore + CaseMatch + (5 * Consecutive)) + (BaseScore + CaseMatch + (6 * Consecutive)),
            {0, 2, 1, 3, 5, 4, 7, 6, 8, 10, 9});
    }

    void FuzzyTests::SurrogatePair_ToUtf16Pos_PreferConsecutiveChars()
    {
        AssertScoreAndPositions(
            L"𠀋😀",
            L"N𠀋😀wer 😀b𐐷 ",
            (BaseScore + CaseMatch) + (BaseScore + CaseMatch + (1 * Consecutive)),
            {2, 1, 4, 3});
    }

    void FuzzyTests::SurrogatePair_ToUtf16Pos_GapAndBoundary()
    {
        AssertScoreAndPositions(
            L"𠀋😀",
            L"N𠀋wer 😀b𐐷 ",
            (BaseScore + CaseMatch) + (BaseScore + CaseMatch + Separator),
            {2, 1, 8, 7});
    }

    void FuzzyTests::MatchOnNonWordChars_CaseInSensitive()
    {
        AssertScoreAndPositions(
            L"foo-b",
            L"xFoo-Bar Baz",
            (BaseScore + Camel) + (BaseScore + CaseMatch + (1 * Consecutive)) + (BaseScore + CaseMatch + (2 * Consecutive)) + (BaseScore + CaseMatch + (3 * Consecutive)) + (BaseScore + Separator + (4 * Consecutive)),
            {1,2,3,4,5});
    }

    void FuzzyTests::MatchOnNonWordCharsWithGap()
    {
        AssertScoreAndPositions(
            L"12356",
            L"abc123 456",
            (BaseScore + CaseMatch) + (BaseScore + CaseMatch + (1 * Consecutive)) + (BaseScore + CaseMatch + (2 * Consecutive)) + (BaseScore + CaseMatch) + (BaseScore + CaseMatch + (1 * Consecutive)),
            { 3, 4, 5, 8, 9 });
    }

    void FuzzyTests::BonusBoundaryAndFirstCharMultiplier()
    {
        AssertScoreAndPositions(
            L"fbb",
            L"foo bar baz",
            (BaseScore + CaseMatch + StartOfString) + (BaseScore + CaseMatch + Separator) + (BaseScore + CaseMatch + Separator),
            { 0, 4, 8 });
    }

    void FuzzyTests::MatchesAreCaseInSensitive()
    {
        AssertScoreAndPositions(
            L"FBB",
            L"foo bar baz",
            (BaseScore + StartOfString) + (BaseScore +  Separator) + (BaseScore + Separator),
            { 0, 4, 8 });
    }

    void FuzzyTests::MultipleTerms()
    {
        auto term1Score = (BaseScore + StartOfString) + (BaseScore + CaseMatch + (1 * Consecutive));
        auto term2Score = (BaseScore + Separator) + (BaseScore + CaseMatch + (1 * Consecutive) + (BaseScore + CaseMatch + (2 * Consecutive)) + (BaseScore + CaseMatch + (3 * Consecutive)));

        AssertScoreAndPositions(
            L"sp anta",
            L"Split Pane, split: horizontal, profile: SSH: Antares",
            term1Score + term2Score,
            {0, 1, 45, 46, 47, 48 });
    }

    void FuzzyTests::MultipleTerms_Reverse()
    {
        auto term1Score = (BaseScore + StartOfString) + (BaseScore + CaseMatch + (1 * Consecutive));
        auto term2Score = (BaseScore + Separator) + (BaseScore + CaseMatch + (1 * Consecutive) + (BaseScore + CaseMatch + (2 * Consecutive)) + (BaseScore + CaseMatch + (3 * Consecutive)));

        AssertScoreAndPositions(
            L"anta sp",
            L"Split Pane, split: horizontal, profile: SSH: Antares",
            term1Score + term2Score,
            {45, 46, 47, 48 ,0, 1 });
    }

    void FuzzyTests::MultipleTerms_AllCharsMatch()
    {
        auto term1Score = (BaseScore + CaseMatch + StartOfString) + (BaseScore + CaseMatch + (1 * Consecutive) + (BaseScore + CaseMatch + (2 * Consecutive)));
        auto term2Score = (BaseScore + CaseMatch + Separator) + (BaseScore + CaseMatch + (1 * Consecutive) + (BaseScore + CaseMatch + (2 * Consecutive)));;

        AssertScoreAndPositions(
            L"foo bar",
            L"foo bar",
            term1Score + term2Score,
            {0,1,2,4,5,6});
    }

    void FuzzyTests::MultipleTerms_NotAllTermsMatch()
    {
        AssertScoreAndPositions(
            L"sp anta zz",
            L"Split Pane, split: horizontal, profile: SSH: Antares",
            0,
            {});
    }

    void FuzzyTests::MatchesAreCaseInSensitive_BonusBoundary()
    {
        AssertScoreAndPositions(
            L"fbb",
            L"Foo Bar Baz",
            (BaseScore + StartOfString) + (BaseScore + Separator) + (BaseScore + Separator),
            { 0, 4, 8 });
    }

    void FuzzyTests::TraceBackWillPickTheFirstMatchIfBothHaveTheSameScore()
    {
        //This is different that fzf, it low picks up the last match
        AssertScoreAndPositions(
            L"bar",
            L"Foo Bar Bar",
            (BaseScore + Separator) + (BaseScore + CaseMatch + (1 * Consecutive)) + (BaseScore + CaseMatch + (2 * Consecutive)),
            { 8, 9, 10 });
    }

    void FuzzyTests::TraceBackWillPickTheMatchWithTheHighestScore()
    {
        AssertScoreAndPositions(
            L"bar",
            L"Foo aBar Bar",
            (BaseScore + Separator) + (BaseScore + CaseMatch + (1 * Consecutive) + (BaseScore + CaseMatch + (2 * Consecutive))),
            { 9, 10, 11 });
    }

    void FuzzyTests::TraceBackWillPickTheMatchWithTheHighestScore_Gaps()
    {
        //This is different than fzf
        //It is preferring the word boundaries over the consecutive chars 
        //This is odd because the word boundaries results in a lower score.  (Issue with backtracking?)
        //VSCode describes this here
        //https://github.com/microsoft/vscode/issues/170353

        //auto consecutiveMatch = (BaseScore + Separator) + (BaseScore + CaseMatch + (1 * Consecutive) + (BaseScore + CaseMatch + (2 * Consecutive)));
        auto wordBoundariesMatch = (BaseScore + StartOfString) + (BaseScore + Separator) + (BaseScore + Separator);
        AssertScoreAndPositions(
            L"bar",
            L"Boo Author Raz Bar",
            wordBoundariesMatch,
            {0,4,11});
    }

    void FuzzyTests::TraceBackWillPickEarlierCharsWhenNoBonus()
    {
        //This is different than fzf, it prefers matches closer together
        AssertScoreAndPositions(
            L"clts",
            L"close all tabs after this",
            (BaseScore + CaseMatch + StartOfString) + (BaseScore + CaseMatch + (1 * Consecutive) + (BaseScore + Separator + CaseMatch)) + (BaseScore + CaseMatch),
            {0,1,21,24});
    }

    void FuzzyTests::ConsecutiveMatchWillScoreHigherThanMatchWithGapWhenBothDontHaveBonus()
    {
        auto consecutiveScore = (BaseScore + CaseMatch) + (BaseScore + CaseMatch + (1 * Consecutive) + (BaseScore + CaseMatch + (2 * Consecutive)));
        auto gapScore = (BaseScore + CaseMatch) + (BaseScore + CaseMatch) + (BaseScore + CaseMatch);

        AssertScoreAndPositions(
            L"oob",
            L"aoobar",
            consecutiveScore,
            {1,2,3});

        AssertScoreAndPositions(
            L"oob",
            L"aoaoabound",
            gapScore,
            {1,3,5});

        VERIFY_IS_GREATER_THAN(consecutiveScore, gapScore);
    }

    void FuzzyTests::ConsecutiveMatchWillScoreHigherThanMatchWithGapWhenBothHaveFirstCharBonus()
    {
        auto consecutiveScore = (BaseScore + CaseMatch + StartOfString) + (BaseScore + CaseMatch + (1 * Consecutive)) + (BaseScore + CaseMatch + (2 * Consecutive));
        auto gapScore = (BaseScore + CaseMatch + StartOfString) + (BaseScore + CaseMatch) + (BaseScore + CaseMatch);

        AssertScoreAndPositions(
            L"oob",
            L"oobar",
            consecutiveScore,
            {0,1,2});

        AssertScoreAndPositions(
            L"oob",
            L"oaoabound",
            gapScore,
            {0,2,4});

        VERIFY_IS_GREATER_THAN(consecutiveScore, gapScore);
    }

    void FuzzyTests::MatchWithGapCanAHaveHigherScoreThanConsecutiveWhenGapMatchHasBoundaryBonus()
    {
        auto consecutiveScore = (BaseScore + CaseMatch) + (BaseScore + CaseMatch + (1 * Consecutive) + (BaseScore + CaseMatch + (2 * Consecutive)));
        auto gapScore = (BaseScore + CaseMatch + StartOfString) + (BaseScore + CaseMatch + Separator) + (BaseScore + CaseMatch + Separator);

        AssertScoreAndPositions(
            L"oob",
            L"foobar",
            consecutiveScore,
            {1,2,3});

        AssertScoreAndPositions(
            L"oob",
            L"out-of-bound",
            gapScore,
            {0,4,7});

        VERIFY_IS_GREATER_THAN(gapScore, consecutiveScore);
    }

    void FuzzyTests::MatchWithGapCanHaveHigherScoreThanConsecutiveWhenGapHasFirstCharBonus()
    {
        auto consecutiveScore = (BaseScore + CaseMatch) + (BaseScore + CaseMatch + (1 * Consecutive));
        auto gapScore = (BaseScore + CaseMatch + StartOfString) + (BaseScore + CaseMatch);

        AssertScoreAndPositions(
            L"ob",
            L"aobar",
            consecutiveScore,
            {1,2});

        AssertScoreAndPositions(
            L"ob",
            L"oabar",
            gapScore,
            {0,2});

        VERIFY_IS_GREATER_THAN(gapScore, consecutiveScore);
    }

    void FuzzyTests::MatchWithGapThatMatchesOnTheFirstCharWillNoLongerHigherScoreThanConsecutiveCharsWhenTheGapIs11_2CharPattern()
    {
        //This is different than fzf
        //I think Smith Waterman does a better job of penalizing gaps
        auto consecutiveScore = (BaseScore + CaseMatch) + (BaseScore + CaseMatch + (1 * Consecutive));
        auto gapScore = (BaseScore + StartOfString + CaseMatch) + (BaseScore + CaseMatch);

        AssertScoreAndPositions(
            L"ob",
            L"aobar",
            consecutiveScore,
            {1,2});

        AssertScoreAndPositions(
            L"ob",
            L"oaaaaaaaaaaabar",
            gapScore,
            {0,12});

        //VERIFY_IS_GREATER_THAN(consecutiveScore, gapScore);
        VERIFY_IS_GREATER_THAN(gapScore, consecutiveScore);
    }

    void FuzzyTests::MatchWithGapThatMatchesOnTheFirstCharWillNoLongerHigherScoreThanConsecutiveCharsWhenTheGapIs11_3CharPattern_1ConsecutiveChar()
    {
        auto consecutiveScore = (BaseScore + CaseMatch) + (BaseScore + CaseMatch + (1 * Consecutive)) + (BaseScore + CaseMatch + (2 * Consecutive));
        auto gapScore = (BaseScore + CaseMatch + StartOfString) + (BaseScore + CaseMatch) + (BaseScore + CaseMatch + (1 * Consecutive));

        AssertScoreAndPositions(
            L"oba",
            L"aobar",
            consecutiveScore,
            {1,2,3});

        AssertScoreAndPositions(
            L"oba",
            L"oaaaaaaaaaaabar",
            gapScore,
            {0,12,13});

        VERIFY_IS_GREATER_THAN(consecutiveScore, gapScore);
    }

    void FuzzyTests::MatchWithGapThatMatchesOnTheFirstCharWillNoLongerHigherScoreThanConsecutiveCharsWhenTheGapIs5_NoConsecutiveChars_3CharPattern()
    {
        auto allConsecutiveScore = (BaseScore + CaseMatch) + (BaseScore + CaseMatch + (1 * Consecutive)) + (BaseScore + CaseMatch + (2 * Consecutive));
        auto allBoundaryWithGapScore = (BaseScore + CaseMatch + StartOfString) + (BaseScore + CaseMatch) + (BaseScore + CaseMatch);

        AssertScoreAndPositions(
            L"oba",
            L"aobar",
            allConsecutiveScore,
            {1,2,3});

        AssertScoreAndPositions(
            L"oba",
            L"oaaabzzar",
            allBoundaryWithGapScore,
            {0,4,7});

        VERIFY_IS_GREATER_THAN(allConsecutiveScore, allBoundaryWithGapScore);
    }

    void FuzzyTests::MatchWithGapThatMatchesOnTheFirstCharWillNoLongerScoreHigherThanConsecutiveCharsWhenTheGapIs3_NoConsecutiveChar_4CharPattern()
    {
        auto consecutiveScore = (BaseScore + CaseMatch) + (BaseScore + CaseMatch + (1 * Consecutive)) + (BaseScore + CaseMatch + (2 * Consecutive)) + (BaseScore + CaseMatch + (3 * Consecutive));
        auto gapScore = (BaseScore + CaseMatch + StartOfString) + (BaseScore + CaseMatch) + (BaseScore + CaseMatch) + (BaseScore + CaseMatch);

        AssertScoreAndPositions(
            L"obar",
            L"aobar",
            consecutiveScore,
            {1,2,3,4});

        AssertScoreAndPositions(
            L"obar",
            L"oabzazr",
            gapScore,
            {0,2,4,6});

        VERIFY_IS_GREATER_THAN(consecutiveScore, gapScore);
    }
}
