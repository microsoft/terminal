// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "../../types/inc/Utf16Parser.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

static const std::vector<wchar_t> CyrillicChar = { 0x0431 }; // lowercase be
static const std::vector<wchar_t> LatinChar = { 0x0061 }; // uppercase A
static const std::vector<wchar_t> FullWidthChar = { 0xFF2D }; // fullwidth latin small letter m
static const std::vector<wchar_t> GaelicChar = { 0x1E41 }; // latin small letter m with dot above
static const std::vector<wchar_t> HiraganaChar = { 0x3059 }; // hiragana su
static const std::vector<wchar_t> SunglassesEmoji = { 0xD83D, 0xDE0E }; // smiling face with sunglasses emoji

class Utf16ParserTests
{
    TEST_CLASS(Utf16ParserTests);

    TEST_METHOD(CanParseNonSurrogateText)
    {
        const std::vector<std::vector<wchar_t>> expected = { CyrillicChar, LatinChar, FullWidthChar, GaelicChar, HiraganaChar };

        std::wstring wstr;
        for (const auto& charData : expected)
        {
            wstr.push_back(charData.at(0));
        }

        const std::vector<std::vector<wchar_t>> result = Utf16Parser::Parse(wstr);

        VERIFY_ARE_EQUAL(expected.size(), result.size());
        for (size_t i = 0; i < result.size(); ++i)
        {
            const auto& sequence = result.at(i);
            VERIFY_ARE_EQUAL(sequence, expected.at(i));
        }
    }

    TEST_METHOD(CanParseSurrogatePairs)
    {
        const std::wstring wstr{ SunglassesEmoji.begin(), SunglassesEmoji.end() };
        const std::vector<std::vector<wchar_t>> result = Utf16Parser::Parse(wstr);

        VERIFY_ARE_EQUAL(result.size(), 1u);
        VERIFY_ARE_EQUAL(result.at(0).size(), SunglassesEmoji.size());
        for (size_t i = 0; i < SunglassesEmoji.size(); ++i)
        {
            VERIFY_ARE_EQUAL(result.at(0).at(i), SunglassesEmoji.at(i));
        }
    }

    TEST_METHOD(WillDropBadSurrogateCombinations)
    {
        // test dropping of invalid leading surrogates
        std::wstring wstr{ SunglassesEmoji.begin(), SunglassesEmoji.end() };
        wstr += wstr;
        wstr.at(1) = SunglassesEmoji.at(0); // wstr contains 3 leading, 1 trailing surrogate sequence

        std::vector<std::vector<wchar_t>> result = Utf16Parser::Parse(wstr);

        VERIFY_ARE_EQUAL(result.size(), 1u);
        VERIFY_ARE_EQUAL(result.at(0).size(), SunglassesEmoji.size());
        for (size_t i = 0; i < SunglassesEmoji.size(); ++i)
        {
            VERIFY_ARE_EQUAL(result.at(0).at(i), SunglassesEmoji.at(i));
        }

        // test dropping of invalid trailing surrogates
        wstr = { SunglassesEmoji.begin(), SunglassesEmoji.end() };
        wstr += wstr;
        wstr.at(0) = SunglassesEmoji.at(1); // wstr contains 2 trailing, 1 leading, 1 trailing surrogate sequence

        result = Utf16Parser::Parse(wstr);

        VERIFY_ARE_EQUAL(result.size(), 1u);
        VERIFY_ARE_EQUAL(result.at(0).size(), SunglassesEmoji.size());
        for (size_t i = 0; i < SunglassesEmoji.size(); ++i)
        {
            VERIFY_ARE_EQUAL(result.at(0).at(i), SunglassesEmoji.at(i));
        }
    }

    const std::wstring_view Replacement{ &UNICODE_REPLACEMENT, 1 };

    TEST_METHOD(ParseNextLeadOnly)
    {
        std::wstring wstr{ SunglassesEmoji.at(0) };

        const auto expected = Replacement;
        const auto actual = Utf16Parser::ParseNext(wstr);

        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(ParseNextTrailOnly)
    {
        std::wstring wstr{ SunglassesEmoji.at(1) };

        const auto expected = Replacement;
        const auto actual = Utf16Parser::ParseNext(wstr);

        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(ParseNextSingleOnly)
    {
        std::wstring wstr{ CyrillicChar.at(0) };

        const auto expected = std::wstring_view{ CyrillicChar.data(), CyrillicChar.size() };
        const auto actual = Utf16Parser::ParseNext(wstr);

        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(ParseNextLeadLead)
    {
        std::wstring wstr{ SunglassesEmoji.at(0) };
        wstr += SunglassesEmoji.at(0);

        const auto expected = Replacement;
        const auto actual = Utf16Parser::ParseNext(wstr);

        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(ParseNextLeadTrail)
    {
        std::wstring wstr{ SunglassesEmoji.at(0) };
        wstr += SunglassesEmoji.at(1);

        const auto expected = std::wstring_view{ SunglassesEmoji.data(), SunglassesEmoji.size() };
        const auto actual = Utf16Parser::ParseNext(wstr);

        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(ParseNextTrailTrail)
    {
        std::wstring wstr{ SunglassesEmoji.at(1) };
        wstr += SunglassesEmoji.at(1);

        const auto expected = Replacement;
        const auto actual = Utf16Parser::ParseNext(wstr);

        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(ParseNextLeadSingle)
    {
        std::wstring wstr{ SunglassesEmoji.at(0) };
        wstr += LatinChar.at(0);

        const auto expected = std::wstring_view{ LatinChar.data(), LatinChar.size() };
        const auto actual = Utf16Parser::ParseNext(wstr);

        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(ParseNextTrailSingle)
    {
        std::wstring wstr{ SunglassesEmoji.at(1) };
        wstr += LatinChar.at(0);

        const auto expected = std::wstring_view{ LatinChar.data(), LatinChar.size() };
        const auto actual = Utf16Parser::ParseNext(wstr);

        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(ParseNextLeadLeadTrail)
    {
        std::wstring wstr{ SunglassesEmoji.at(0) };
        wstr += SunglassesEmoji.at(0);
        wstr += SunglassesEmoji.at(1);

        const auto expected = std::wstring_view{ SunglassesEmoji.data(), SunglassesEmoji.size() };
        const auto actual = Utf16Parser::ParseNext(wstr);

        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(ParseNextTrailLeadTrail)
    {
        std::wstring wstr{ SunglassesEmoji.at(1) };
        wstr += SunglassesEmoji.at(0);
        wstr += SunglassesEmoji.at(1);

        const auto expected = std::wstring_view{ SunglassesEmoji.data(), SunglassesEmoji.size() };
        const auto actual = Utf16Parser::ParseNext(wstr);

        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(ParseNextSingleLeadTrail)
    {
        std::wstring wstr{ GaelicChar.at(0) };
        wstr += SunglassesEmoji.at(0);
        wstr += SunglassesEmoji.at(1);

        const auto expected = std::wstring_view{ GaelicChar.data(), GaelicChar.size() };
        const auto actual = Utf16Parser::ParseNext(wstr);

        VERIFY_ARE_EQUAL(expected, actual);
    }
};
