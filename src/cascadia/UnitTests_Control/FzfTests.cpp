#include "pch.h"
#pragma warning( push )
#pragma warning( disable : 4189 )
#pragma warning( disable : 4100 )

#include "../../inc/TestUtils.h"

#include <icu.h>
#include <vector>

#include "../fzf/fzf.h"

using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace fzfUnitTests
{
    class FzfTests
    {
    public:
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

        static int8_t max_i8(int8_t a, int8_t b)
        {
            return a > b ? a : b;
        }

        BEGIN_TEST_CLASS(FzfTests)
        END_TEST_CLASS()

        TEST_METHOD(FuzzyMatchV2_Case1);
        TEST_METHOD(FuzzyMatchV2_Case2);
        TEST_METHOD(FuzzyMatchV2_Case3);
        TEST_METHOD(FuzzyMatchV2_Case4);
        TEST_METHOD(FuzzyMatchV2_Case5);
        TEST_METHOD(FuzzyMatchV2_Case6);
        TEST_METHOD(FuzzyMatchV2_Case7);
        TEST_METHOD(FuzzyMatchV2_Case8);
        TEST_METHOD(FuzzyMatchV2_Case9);
        TEST_METHOD(FuzzyMatchV2_Case10);
        TEST_METHOD(FuzzyMatchV2_Case11);
        TEST_METHOD(FuzzyMatchV2_Case12);
        TEST_METHOD(FuzzyMatchV2_Case13);
        TEST_METHOD(FuzzyMatchV2_Case14);
        TEST_METHOD(FuzzyMatchV2_Case15);
        TEST_METHOD(FuzzyMatchV2_Case16);
        TEST_METHOD(FuzzyMatchV2_Case17);
        TEST_METHOD(FuzzyMatchV2_Case18);
        TEST_METHOD(FuzzyMatchV2_Case19);
        TEST_METHOD(FuzzyMatchV2_Case20);
        TEST_METHOD(FuzzyMatchV2_Case21);
        TEST_METHOD(FuzzyMatchV2_Case22);
        TEST_METHOD(FuzzyMatchV2_Case23);
        TEST_METHOD(FuzzyMatchV2_UnicodeCase1);
        TEST_METHOD(FuzzyMatchV2_UnicodeCase2);
        TEST_METHOD(FuzzyMatchV2_UnicodeCase3);
        TEST_METHOD(PrefixMatch_Case1);
        TEST_METHOD(PrefixMatch_Case2);
        TEST_METHOD(PrefixMatch_Case3);
        TEST_METHOD(PrefixMatchUnicode_Case1);
        TEST_METHOD(PrefixMatchUnicode_Case2);
        TEST_METHOD(PrefixMatchUnicode_Case3);
        TEST_METHOD(SufixMatch_Case1);
        TEST_METHOD(SufixMatch_Case2);
        TEST_METHOD(SufixMatch_Case3);
        TEST_METHOD(SufixMatchUnicode_Case1);
        TEST_METHOD(SufixMatchUnicode_Case2);
        TEST_METHOD(SufixMatchUnicode_Case3);
        TEST_METHOD(EqualMatch_Case1);
        TEST_METHOD(EqualMatch_Case2);
        TEST_METHOD(EqualMatch_Case3);
        TEST_METHOD(EqualMatchUnicode_CaseRespect_1);
        TEST_METHOD(EqualMatchUnicode_CaseRespect_2);
        TEST_METHOD(EqualMatchUnicode_CaseRespect_3);
        TEST_METHOD(EqualMatchUnicode_CaseRespect_4);
        TEST_METHOD(EqualMatchUnicode_CaseIngnore_1);
        TEST_METHOD(EqualMatchUnicode_CaseIgnore_2);
        TEST_METHOD(EqualMatchUnicode_CaseIgnore_3);
        TEST_METHOD(EqualMatchUnicode_CaseIgnore_4);
        TEST_METHOD(EqualMatchUnicode_CaseSmart_1);
        TEST_METHOD(EqualMatchUnicode_CaseSmart2);
        TEST_METHOD(EqualMatchUnicode_CaseSmart3);
        TEST_METHOD(EqualMatchUnicode_CaseSmart4);
        TEST_METHOD(PatternMatch_empty);
        TEST_METHOD(PatternMatch_simple);
        TEST_METHOD(PatternMatch_withEscapedSpace);
        TEST_METHOD(PatternMatch_withComplexEscapedSpace);
        TEST_METHOD(PatternMatch_withSpaceAndNormalSpace);
        TEST_METHOD(PatternMatch_invert);
        TEST_METHOD(PatternMatch_invertMultiple);
        TEST_METHOD(PatternMatch_smartCase);
        TEST_METHOD(PatternMatch_smartCase2);
        TEST_METHOD(PatternMatch_simpleOr);
        TEST_METHOD(PatternMatch_complexAnd);
        TEST_METHOD(Integration_Case1);
        TEST_METHOD(Integration_Case2);
        TEST_METHOD(Integration_Case3);
        TEST_METHOD(Integration_Case4);
        TEST_METHOD(Integration_Case5);
        TEST_METHOD(Integration_Case6);

    private:
        ufzf_pattern_t* PatternTest2(std::wstring patternString, fzf_case_types caseType, int32_t patternSize, int32_t cap, bool onlyInv)
        {
            fzf_slab_t* slab = fzf_make_default_slab();
            UErrorCode status = U_ZERO_ERROR;
            const UChar* uPattern = reinterpret_cast<const UChar*>(patternString.data());
            VERIFY_IS_FALSE(U_FAILURE(status));
            ufzf_pattern_t* pattern = ufzf_parse_pattern(caseType, false, uPattern, true);
            VERIFY_ARE_EQUAL(patternSize, pattern->size);
            VERIFY_ARE_EQUAL(cap, pattern->cap);
            VERIFY_ARE_EQUAL(onlyInv, pattern->only_inv);

            fzf_free_slab(slab);
            return pattern;
        }
        void ScoreInputTest(std::wstring patternString, fzf_case_types caseType, std::wstring input, int32_t expectedScore, std::vector<int32_t> expectedPos)
        {
            fzf_slab_t* slab = fzf_make_default_slab();
            UErrorCode status = U_ZERO_ERROR;
            const UChar* icuPatternBuf = reinterpret_cast<const UChar*>(patternString.data());
            ufzf_pattern_t* pattern = ufzf_parse_pattern(caseType, false, icuPatternBuf, true);
            const UChar* uInput = reinterpret_cast<const UChar*>(input.c_str());
            auto utInput = utext_openUChars(nullptr, uInput, input.length(), &status);

            AssertScore(slab, pattern, utInput, expectedScore);
            AssertPos(slab, pattern, utInput, expectedPos);

            ufzf_free_pattern(pattern);
            fzf_free_slab(slab);
            utext_close(utInput);
        }
        void ScoreInputTest(std::wstring patternString, fzf_case_types caseType, std::wstring input, int32_t expectedScore)
        {
            fzf_slab_t* slab = fzf_make_default_slab();
            UErrorCode status = U_ZERO_ERROR;
            const UChar* icuPatternBuf = reinterpret_cast<const UChar*>(patternString.data());
            ufzf_pattern_t* pattern = ufzf_parse_pattern(caseType, false, icuPatternBuf, true);
            const UChar* uInput = reinterpret_cast<const UChar*>(input.c_str());
            auto utInput = utext_openUChars(nullptr, uInput, input.length(), &status);

            AssertScore(slab, pattern, utInput, expectedScore);

            ufzf_free_pattern(pattern);
            fzf_free_slab(slab);
            utext_close(utInput);
        }
        void AssertScore(fzf_slab_t* slab, ufzf_pattern_t* pattern, UText* utInput, int32_t expectedScore)
        {
            auto result = ufzf_get_score(utInput, pattern, slab);
            VERIFY_ARE_EQUAL(expectedScore, result);
        }
        void AssertPos(fzf_slab_t* slab, ufzf_pattern_t* pattern, UText* utInput, std::vector<int32_t> expectedPos)
        {
            auto pos = ufzf_get_positions(utInput, pattern, slab);
            VERIFY_ARE_EQUAL(expectedPos.size(), pos->size);
            for (auto i = 0; i < expectedPos.size(); i++)
            {
                VERIFY_ARE_EQUAL(expectedPos[i], static_cast<int32_t>(pos->data[i]));
            }
            fzf_free_positions(pos);
        }
        void TestScoreMultipleInput(std::wstring patternString, std::vector<std::wstring> input, std::vector<int32_t> expected)
        {
            UErrorCode status = U_ZERO_ERROR;
            const UChar* uPattern = reinterpret_cast<const UChar*>(patternString.c_str());
            VERIFY_IS_FALSE(U_FAILURE(status));
            fzf_slab_t* slab = fzf_make_default_slab();
            ufzf_pattern_t* pattern = ufzf_parse_pattern(CaseSmart, false, uPattern, true);
            for (size_t i = 0; i < input.size(); ++i)
            {
                const UChar* uInput = reinterpret_cast<const UChar*>(input[i].data());
                auto utInput = utext_openUChars(nullptr, uInput, input[i].length(), &status);
                VERIFY_ARE_EQUAL(expected[i], ufzf_get_score(utInput, pattern, slab));
            }
            ufzf_free_pattern(pattern);
            fzf_free_slab(slab);
        }

        void AssertTermString(std::wstring expected, ufzf_term_t* term)
        {
            std::wstring actualWString;

            auto actual = term->ptr;

            utext_setNativeIndex(actual, 0);
            UChar32 c = utext_next32(actual);
            while (c != U_SENTINEL)
            {
                if (c <= 0xFFFF)
                {
                    actualWString += static_cast<wchar_t>(c);
                }
                else
                {
                    c -= 0x10000;
                    actualWString += static_cast<wchar_t>((c >> 10) + 0xD800);
                    actualWString += static_cast<wchar_t>((c & 0x3FF) + 0xDC00);
                }
                c = utext_next32(actual);
            }

            VERIFY_ARE_EQUAL(expected, actualWString);
        }
    };

    void FzfTests::FuzzyMatchV2_Case1()
    {
        ScoreInputTest(L"So", CaseRespect, L"So Danco Samba", 56, { 1, 0 });
    }
    void FzfTests::FuzzyMatchV2_Case2()
    {
        ScoreInputTest(L"sodc", CaseIgnore, L"So Danco Samba", 89, { 6, 3, 1, 0 });
    }
    void FzfTests::FuzzyMatchV2_Case3()
    {
        ScoreInputTest(L"danco", CaseIgnore, L"Danco", 128, { 4, 3, 2, 1, 0 });
    }
    void FzfTests::FuzzyMatchV2_Case4()
    {
        ScoreInputTest(L"obz", CaseIgnore, L"fooBarbaz1", ScoreMatch * 3 + BonusCamel123 + ScoreGapStart + ScoreGapExtension * 3, { 8, 3, 2 });
    }
    void FzfTests::FuzzyMatchV2_Case5()
    {
        ScoreInputTest(L"fbb", CaseIgnore, L"foo bar baz", ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusBoundary * 2 + 2 * ScoreGapStart + 4 * ScoreGapExtension, { 8, 4, 0 });
    }
    void FzfTests::FuzzyMatchV2_Case6()
    {
        ScoreInputTest(L"rdoc", CaseIgnore, L"/AutomatorDocument.icns", ScoreMatch * 4 + BonusCamel123 + BonusConsecutive * 2);
    }
    void FzfTests::FuzzyMatchV2_Case7()
    {
        ScoreInputTest(L"zshc", CaseIgnore, L"/man1/zshcompctl.1", ScoreMatch * 4 + BonusBoundary * BonusFirstCharMultiplier + BonusBoundary * 3);
    }
    void FzfTests::FuzzyMatchV2_Case8()
    {
        ScoreInputTest(L"zshc", CaseIgnore, L"/.oh-my-zsh/cache", ScoreMatch * 4 + BonusBoundary * BonusFirstCharMultiplier + BonusBoundary * 3 + ScoreGapStart);
    }
    void FzfTests::FuzzyMatchV2_Case9()
    {
        ScoreInputTest(L"12356", CaseIgnore, L"ab0123 456", ScoreMatch * 5 + BonusConsecutive * 3 + ScoreGapStart + ScoreGapExtension);
    }
    void FzfTests::FuzzyMatchV2_Case10()
    {
        ScoreInputTest(L"12356", CaseIgnore, L"abc123 456", ScoreMatch * 5 + BonusCamel123 * BonusFirstCharMultiplier + BonusCamel123 * 2 + BonusConsecutive + ScoreGapStart + ScoreGapExtension);
    }
    void FzfTests::FuzzyMatchV2_Case11()
    {
        ScoreInputTest(L"fbb", CaseIgnore, L"foo/bar/baz", ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusBoundary * 2 + 2 * ScoreGapStart + 4 * ScoreGapExtension);
    }
    void FzfTests::FuzzyMatchV2_Case12()
    {
        ScoreInputTest(L"fbb", CaseIgnore, L"fooBarBaz", ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusCamel123 * 2 + 2 * ScoreGapStart + 2 * ScoreGapExtension);
    }
    void FzfTests::FuzzyMatchV2_Case13()
    {
        ScoreInputTest(L"fbb", CaseIgnore, L"foo barbaz", ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusBoundary + ScoreGapStart * 2 + ScoreGapExtension * 3);
    }
    void FzfTests::FuzzyMatchV2_Case14()
    {
        ScoreInputTest(L"foob", CaseIgnore, L"fooBar Baz", ScoreMatch * 4 + BonusBoundary * BonusFirstCharMultiplier + BonusBoundary * 3);
    }
    void FzfTests::FuzzyMatchV2_Case15()
    {
        ScoreInputTest(L"foo-b", CaseIgnore, L"xFoo-Bar Baz", ScoreMatch * 5 + BonusCamel123 * BonusFirstCharMultiplier + BonusCamel123 * 2 + BonusNonWord + BonusBoundary);
    }
    void FzfTests::FuzzyMatchV2_Case16()
    {
        ScoreInputTest(L"oBz", CaseRespect, L"fooBarbaz", ScoreMatch * 3 + BonusCamel123 + ScoreGapStart + ScoreGapExtension * 3);
    }
    void FzfTests::FuzzyMatchV2_Case17()
    {
        ScoreInputTest(L"FBB", CaseRespect, L"Foo/Bar/Baz", ScoreMatch * 3 + BonusBoundary * (BonusFirstCharMultiplier + 2) + ScoreGapStart * 2 + ScoreGapExtension * 4);
    }
    void FzfTests::FuzzyMatchV2_Case18()
    {
        ScoreInputTest(L"FBB", CaseRespect, L"FooBarBaz", ScoreMatch * 3 + BonusBoundary * BonusFirstCharMultiplier + BonusCamel123 * 2 + ScoreGapStart * 2 + ScoreGapExtension * 2);
    }
    void FzfTests::FuzzyMatchV2_Case19()
    {
        ScoreInputTest(L"FooB", CaseRespect, L"FooBar Baz", ScoreMatch * 4 + BonusBoundary * BonusFirstCharMultiplier + BonusBoundary * 2 + max_i8(BonusCamel123, BonusBoundary));
    }
    void FzfTests::FuzzyMatchV2_Case20()
    {
        ScoreInputTest(L"o-ba", CaseRespect, L"foo-bar", ScoreMatch * 4 + BonusBoundary * 3);
    }
    void FzfTests::FuzzyMatchV2_Case21()
    {
        ScoreInputTest(L"oBZ", CaseRespect, L"fooBarbaz", 0);
    }
    void FzfTests::FuzzyMatchV2_Case22()
    {
        ScoreInputTest(L"fbb", CaseRespect, L"Foo Bar Baz", 0);
    }
    void FzfTests::FuzzyMatchV2_Case23()
    {
        ScoreInputTest(L"fooBarbazz", CaseRespect, L"fooBarbaz", 0);
    }
    //\U0001F600: 😀 \u01C5: ǅ \u00CF: Ĩ
    void FzfTests::FuzzyMatchV2_UnicodeCase1()
    {
        ScoreInputTest(L"\U0001F600", CaseRespect, L"\U0001F600 Danco", 56, { 1, 0 });
    }
    void FzfTests::FuzzyMatchV2_UnicodeCase2()
    {
        ScoreInputTest(L"\U0001F600\u01C5", CaseRespect, L"\U0001F600\u01C5 Danco", 80, { 2, 1, 0 });
    }
    void FzfTests::FuzzyMatchV2_UnicodeCase3()
    {
        ScoreInputTest(L"\U0001F600\u01C5\u00CF", CaseRespect, L"\U0001F600\u01C5 D\u00CFnco", 92, { 5, 2, 1, 0 });
    }
    void FzfTests::PrefixMatch_Case1()
    {
        ScoreInputTest(L"^So", CaseRespect, L"So Danco Samba", 56);
    }
    void FzfTests::PrefixMatch_Case2()
    {
        ScoreInputTest(L"^sodc", CaseRespect, L"So Danco Samba", 0);
    }
    void FzfTests::PrefixMatch_Case3()
    {
        ScoreInputTest(L"^danco", CaseRespect, L"Danco", 0);
    }
    void FzfTests::PrefixMatchUnicode_Case1()
    {
        ScoreInputTest(L"^\u00CFSo", CaseRespect, L"\u00CFSo Danco Samba", 80);
    }
    void FzfTests::PrefixMatchUnicode_Case2()
    {
        ScoreInputTest(L"^\u00CFsodc", CaseRespect, L"\u00CF So Danco Samba", 0);
    }
    void FzfTests::PrefixMatchUnicode_Case3()
    {
        ScoreInputTest(L"^\u00CFdanco", CaseRespect, L"\u00CFDanco", 0);
    }
    void FzfTests::SufixMatch_Case1()
    {
        ScoreInputTest(L"So$", CaseRespect, L"So Danco Samba", 0);
    }
    void FzfTests::SufixMatch_Case2()
    {
        ScoreInputTest(L"sodc$", CaseIgnore, L"So Danco Samba", 0);
    }
    void FzfTests::SufixMatch_Case3()
    {
        ScoreInputTest(L"danco$", CaseIgnore, L"Danco", 128);
    }
    void FzfTests::SufixMatchUnicode_Case1()
    {
        ScoreInputTest(L"\u00CFSo$", CaseRespect, L"\u00CFSo Danco Samba", 0);
    }
    void FzfTests::SufixMatchUnicode_Case2()
    {
        ScoreInputTest(L"\u00CFsodc$", CaseIgnore, L"\u00CFSo Danco Samba", 0);
    }
    void FzfTests::SufixMatchUnicode_Case3()
    {
        ScoreInputTest(L"samb\u00CFa$", CaseIgnore, L"Danco Samb\u00CFa", 152);
    }
    void FzfTests::EqualMatch_Case1()
    {
        ScoreInputTest(L"'So", CaseRespect, L"So Danco Samba", 56);
    }
    void FzfTests::EqualMatch_Case2()
    {
        ScoreInputTest(L"'sodc", CaseIgnore, L"So Danco Samba", 0);
    }
    void FzfTests::EqualMatch_Case3()
    {
        ScoreInputTest(L"'danco", CaseIgnore, L"Danco", 128);
    }
    //\u00EF:ï (lowerCase) \u00CF:Ï (upperCase)
    void FzfTests::EqualMatchUnicode_CaseRespect_1()
    {
        ScoreInputTest(L"'so\u00CF", CaseRespect, L"so\u00CF Danco Samba", 80);
    }
    void FzfTests::EqualMatchUnicode_CaseRespect_2()
    {
        ScoreInputTest(L"'so\u00EF", CaseRespect, L"so\u00CF Danco Samba", 0);
    }
    void FzfTests::EqualMatchUnicode_CaseRespect_3()
    {
        ScoreInputTest(L"'so\u00CF", CaseRespect, L"so\u00EF Danco Samba", 0);
    }
    void FzfTests::EqualMatchUnicode_CaseRespect_4()
    {
        ScoreInputTest(L"'so\u00EF", CaseRespect, L"so\u00EF Danco Samba", 80);
    }
    void FzfTests::EqualMatchUnicode_CaseIngnore_1()
    {
        ScoreInputTest(L"'danco\u00CF", CaseIgnore, L"danco\u00EF", 152);
    }
    void FzfTests::EqualMatchUnicode_CaseIgnore_2()
    {
        ScoreInputTest(L"'danco\u00EF", CaseIgnore, L"danco\u00CF", 152);
    }
    void FzfTests::EqualMatchUnicode_CaseIgnore_3()
    {
        ScoreInputTest(L"'danco\u00CF", CaseIgnore, L"danco\u00CF", 152);
    }
    void FzfTests::EqualMatchUnicode_CaseIgnore_4()
    {
        ScoreInputTest(L"'danco\u00EF", CaseIgnore, L"danco\u00EF", 152);
    }
    void FzfTests::EqualMatchUnicode_CaseSmart_1()
    {
        ScoreInputTest(L"'danco\u00CF", CaseSmart, L"danco\u00CF", 152);
    }
    void FzfTests::EqualMatchUnicode_CaseSmart2()
    {
        ScoreInputTest(L"'danco\u00EF", CaseSmart, L"danco\u00CF", 152);
    }
    void FzfTests::EqualMatchUnicode_CaseSmart3()
    {
        ScoreInputTest(L"'danco\u00CF", CaseSmart, L"danco\u00EF", 0);
    }
    void FzfTests::EqualMatchUnicode_CaseSmart4()
    {
        ScoreInputTest(L"'danco\u00CF", CaseSmart, L"danco\u00CF", 152);
    }
    void FzfTests::PatternMatch_empty()
    {
        auto pattern = PatternTest2(L"", CaseSmart, 0, 0, false);
        ufzf_free_pattern(pattern);
    }
    void FzfTests::PatternMatch_simple()
    {
        auto pattern = PatternTest2(L"lua", CaseSmart, 1, 1, false);
        ufzf_free_pattern(pattern);
    }
    void FzfTests::PatternMatch_withEscapedSpace()
    {
        auto pattern = PatternTest2(L"file\\ ", CaseSmart, 1, 1, false);

        VERIFY_ARE_EQUAL(1, pattern->ptr[0]->size);
        VERIFY_ARE_EQUAL(1, pattern->ptr[0]->cap);

        VERIFY_IS_TRUE(ufzf_fuzzy_match_v2 == pattern->ptr[0]->ptr[0].fn);
        AssertTermString(L"file ", &pattern->ptr[0]->ptr[0]);
        VERIFY_IS_FALSE(pattern->ptr[0]->ptr[0].case_sensitive);

        ufzf_free_pattern(pattern);
    }
    void FzfTests::PatternMatch_withComplexEscapedSpace()
    {
        auto pattern = PatternTest2(L"file\\ with\\ space", CaseSmart, 1, 1, false);

        VERIFY_ARE_EQUAL(1, pattern->ptr[0]->size);
        VERIFY_ARE_EQUAL(1, pattern->ptr[0]->cap);

        VERIFY_IS_TRUE(ufzf_fuzzy_match_v2 == pattern->ptr[0]->ptr[0].fn);
        AssertTermString(L"file with space", &pattern->ptr[0]->ptr[0]);
        VERIFY_IS_FALSE(pattern->ptr[0]->ptr[0].case_sensitive);

        ufzf_free_pattern(pattern);
    }
    void FzfTests::PatternMatch_withSpaceAndNormalSpace()
    {
        auto pattern = PatternTest2(L"file\\  new", CaseSmart, 2, 2, false);

        VERIFY_ARE_EQUAL(1, pattern->ptr[0]->size);
        VERIFY_ARE_EQUAL(1, pattern->ptr[0]->cap);
        VERIFY_ARE_EQUAL(1, pattern->ptr[1]->size);
        VERIFY_ARE_EQUAL(1, pattern->ptr[1]->cap);

        VERIFY_IS_TRUE(ufzf_fuzzy_match_v2 == pattern->ptr[0]->ptr[0].fn);
        AssertTermString(L"file ", &pattern->ptr[0]->ptr[0]);
        VERIFY_IS_FALSE(pattern->ptr[0]->ptr[0].case_sensitive);

        VERIFY_IS_TRUE(ufzf_fuzzy_match_v2 == pattern->ptr[1]->ptr[0].fn);
        AssertTermString(L"new", &pattern->ptr[1]->ptr[0]);
        VERIFY_IS_FALSE(pattern->ptr[1]->ptr[0].case_sensitive);

        ufzf_free_pattern(pattern);
    }
    void FzfTests::PatternMatch_invert()
    {
        auto pattern = PatternTest2(L"!Lua", CaseSmart, 1, 1, true);

        VERIFY_ARE_EQUAL(1, pattern->ptr[0]->size);
        VERIFY_ARE_EQUAL(1, pattern->ptr[0]->cap);

        VERIFY_IS_TRUE(ufzf_exact_match_naive == pattern->ptr[0]->ptr[0].fn);
        AssertTermString(L"Lua", &pattern->ptr[0]->ptr[0]);
        VERIFY_IS_TRUE(pattern->ptr[0]->ptr[0].case_sensitive);

        ufzf_free_pattern(pattern);
    }
    void FzfTests::PatternMatch_invertMultiple()
    {
        auto pattern = PatternTest2(L"!fzf !test", CaseSmart, 2, 2, true);

        VERIFY_ARE_EQUAL(1, pattern->ptr[0]->size);
        VERIFY_ARE_EQUAL(1, pattern->ptr[0]->cap);
        VERIFY_ARE_EQUAL(1, pattern->ptr[1]->size);
        VERIFY_ARE_EQUAL(1, pattern->ptr[1]->cap);

        VERIFY_IS_TRUE(ufzf_exact_match_naive == pattern->ptr[0]->ptr[0].fn);
        AssertTermString(L"fzf", &pattern->ptr[0]->ptr[0]);
        VERIFY_IS_FALSE(pattern->ptr[0]->ptr[0].case_sensitive);
        VERIFY_IS_TRUE(pattern->ptr[0]->ptr[0].inv);

        VERIFY_IS_TRUE(ufzf_exact_match_naive == pattern->ptr[1]->ptr[0].fn);
        AssertTermString(L"test", &pattern->ptr[1]->ptr[0]);
        VERIFY_IS_FALSE(pattern->ptr[1]->ptr[0].case_sensitive);
        VERIFY_IS_TRUE(pattern->ptr[1]->ptr[0].inv);

        ufzf_free_pattern(pattern);
    }
    void FzfTests::PatternMatch_smartCase()
    {
        auto pattern = PatternTest2(L"Lua", CaseSmart, 1, 1, false);

        VERIFY_ARE_EQUAL(1, pattern->ptr[0]->size);
        VERIFY_ARE_EQUAL(1, pattern->ptr[0]->cap);

        VERIFY_IS_TRUE(ufzf_fuzzy_match_v2 == pattern->ptr[0]->ptr[0].fn);
        AssertTermString(L"Lua", &pattern->ptr[0]->ptr[0]);
        VERIFY_IS_TRUE(pattern->ptr[0]->ptr[0].case_sensitive);

        ufzf_free_pattern(pattern);
    }
    void FzfTests::PatternMatch_smartCase2()
    {
        auto pattern = PatternTest2(L"lua", CaseSmart, 1, 1, false);

        VERIFY_ARE_EQUAL(1, pattern->ptr[0]->size);
        VERIFY_ARE_EQUAL(1, pattern->ptr[0]->cap);

        VERIFY_IS_TRUE(ufzf_fuzzy_match_v2 == pattern->ptr[0]->ptr[0].fn);
        AssertTermString(L"lua", &pattern->ptr[0]->ptr[0]);
        VERIFY_IS_FALSE(pattern->ptr[0]->ptr[0].case_sensitive);

        ufzf_free_pattern(pattern);
    }
    void FzfTests::PatternMatch_simpleOr()
    {
        auto pattern = PatternTest2(L"'src | ^Lua", CaseSmart, 1, 1, false);

        VERIFY_ARE_EQUAL(2, pattern->ptr[0]->size);
        VERIFY_ARE_EQUAL(2, pattern->ptr[0]->cap);

        VERIFY_IS_TRUE(ufzf_exact_match_naive == pattern->ptr[0]->ptr[0].fn);
        AssertTermString(L"src", &pattern->ptr[0]->ptr[0]);
        VERIFY_IS_FALSE(pattern->ptr[0]->ptr[0].case_sensitive);

        VERIFY_IS_TRUE(ufzf_prefix_match == pattern->ptr[0]->ptr[1].fn);
        AssertTermString(L"Lua", &pattern->ptr[0]->ptr[1]);
        VERIFY_IS_TRUE(pattern->ptr[0]->ptr[1].case_sensitive);

        ufzf_free_pattern(pattern);
    }
    void FzfTests::PatternMatch_complexAnd()
    {
        auto pattern = PatternTest2(L".lua$ 'previewer !'term !asdf", CaseSmart, 4, 4, false);

        VERIFY_ARE_EQUAL(1, pattern->ptr[0]->size);
        VERIFY_ARE_EQUAL(1, pattern->ptr[0]->cap);
        VERIFY_ARE_EQUAL(1, pattern->ptr[1]->size);
        VERIFY_ARE_EQUAL(1, pattern->ptr[1]->cap);
        VERIFY_ARE_EQUAL(1, pattern->ptr[2]->size);
        VERIFY_ARE_EQUAL(1, pattern->ptr[2]->cap);
        VERIFY_ARE_EQUAL(1, pattern->ptr[3]->size);
        VERIFY_ARE_EQUAL(1, pattern->ptr[3]->cap);

        VERIFY_IS_TRUE(ufzf_suffix_match == pattern->ptr[0]->ptr[0].fn);
        AssertTermString(L".lua", &pattern->ptr[0]->ptr[0]);
        VERIFY_IS_FALSE(pattern->ptr[0]->ptr[0].case_sensitive);
        VERIFY_IS_FALSE(pattern->ptr[0]->ptr[0].inv);

        VERIFY_IS_TRUE(ufzf_exact_match_naive == pattern->ptr[1]->ptr[0].fn);
        AssertTermString(L"previewer", &pattern->ptr[1]->ptr[0]);
        VERIFY_IS_FALSE(pattern->ptr[1]->ptr[0].case_sensitive);
        VERIFY_IS_FALSE(pattern->ptr[1]->ptr[0].inv);

        VERIFY_IS_TRUE(ufzf_fuzzy_match_v2 == pattern->ptr[2]->ptr[0].fn);
        AssertTermString(L"term", &pattern->ptr[2]->ptr[0]);
        VERIFY_IS_FALSE(pattern->ptr[2]->ptr[0].case_sensitive);
        VERIFY_IS_TRUE(pattern->ptr[2]->ptr[0].inv);

        VERIFY_IS_TRUE(ufzf_exact_match_naive == pattern->ptr[3]->ptr[0].fn);
        AssertTermString(L"asdf", &pattern->ptr[3]->ptr[0]);
        VERIFY_IS_FALSE(pattern->ptr[3]->ptr[0].case_sensitive);
        VERIFY_IS_TRUE(pattern->ptr[3]->ptr[0].inv);

        ufzf_free_pattern(pattern);
    }
    void FzfTests::Integration_Case1()
    {
        std::vector<std::wstring> input = { L"fzf", L"main.c", L"src/fzf", L"fz/noooo" };
        std::vector<int32_t> expected = { 0, 1, 0, 1 };
        TestScoreMultipleInput(L"!fzf", input, expected);
        auto pattern = PatternTest2(L"lua", CaseSmart, 1, 1, false);
        ufzf_free_pattern(pattern);
    }
    void FzfTests::Integration_Case2()
    {
        std::vector<std::wstring> input = { L"src/fzf.c", L"README.md", L"lua/asdf", L"test/test.c" };
        std::vector<int32_t> expected = { 0, 1, 1, 0 };
        TestScoreMultipleInput(L"!fzf !test", input, expected);
    }
    void FzfTests::Integration_Case3()
    {
        std::vector<std::wstring> input = { L"file ", L"file lua", L"lua" };
        std::vector<int32_t> expected = { 0, 200, 0 };
        TestScoreMultipleInput(L"file\\ lua", input, expected);
    }
    void FzfTests::Integration_Case4()
    {
        std::vector<std::wstring> input = { L"file with space", L"file lua", L"lua", L"src", L"test" };
        std::vector<int32_t> expected = { 32, 32, 0, 0, 0 };
        TestScoreMultipleInput(L"\\ ", input, expected);
    }
    void FzfTests::Integration_Case5()
    {
        std::vector<std::wstring> input = { L"src/fzf.h", L"README.md", L"build/fzf", L"lua/fzf_lib.lua", L"Lua/fzf_lib.lua" };
        std::vector<int32_t> expected = { 80, 0, 0, 0, 80 };
        TestScoreMultipleInput(L"'src | ^Lua", input, expected);
    }
    void FzfTests::Integration_Case6()
    {
        std::vector<std::wstring> input = { L"lua/random_previewer", L"README.md", L"previewers/utils.lua", L"previewers/buffer.lua", L"previewers/term.lua" };
        std::vector<int32_t> expected = { 0, 0, 328, 328, 0 };
        TestScoreMultipleInput(L".lua$ 'previewer !'term", input, expected);
    }
}
#pragma warning(pop)
