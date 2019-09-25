// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"
#include "CommonState.hpp"

#include "../types/inc/CodepointWidthDetector.hpp"

using namespace WEX::Logging;

static constexpr std::wstring_view emoji = L"\xD83E\xDD22"; // U+1F922 nauseated face

static constexpr std::wstring_view ambiguous = L"\x414"; // U+0414 cyrillic capital de

// codepoint and utf16 encoded string
static const std::vector<std::tuple<unsigned int, std::wstring, CodepointWidth>> testData = {
    { 0x7, L"\a", CodepointWidth::Narrow }, // BEL
    { 0x20, L" ", CodepointWidth::Narrow },
    { 0x39, L"9", CodepointWidth::Narrow },
    { 0x414, L"\x414", CodepointWidth::Ambiguous }, // U+0414 cyrillic capital de
    { 0x1104, L"\x1104", CodepointWidth::Wide }, // U+1104 hangul choseong ssangtikeut
    { 0x306A, L"\x306A", CodepointWidth::Wide }, // U+306A hiragana na
    { 0x30CA, L"\x30CA", CodepointWidth::Wide }, // U+30CA katakana na
    { 0x72D7, L"\x72D7", CodepointWidth::Wide }, // U+72D7
    { 0x1F47E, L"\xD83D\xDC7E", CodepointWidth::Wide }, // U+1F47E alien monster
    { 0x1F51C, L"\xD83D\xDD1C", CodepointWidth::Wide } // U+1F51C SOON
};

class CodepointWidthDetectorTests
{
    TEST_CLASS(CodepointWidthDetectorTests);

    TEST_METHOD(CanLookUpEmoji)
    {
        CodepointWidthDetector widthDetector;
        VERIFY_IS_TRUE(widthDetector.IsWide(emoji));
    }

    TEST_METHOD(CanExtractCodepoint)
    {
        CodepointWidthDetector widthDetector;
        for (const auto& data : testData)
        {
            const auto& expected = std::get<0>(data);
            const auto& wstr = std::get<1>(data);
            const auto result = widthDetector._extractCodepoint({ wstr.c_str(), wstr.size() });
            VERIFY_ARE_EQUAL(result, expected);
        }
    }

    TEST_METHOD(CanGetWidths)
    {
        CodepointWidthDetector widthDetector;
        for (const auto& data : testData)
        {
            const auto& expected = std::get<2>(data);
            const auto& wstr = std::get<1>(data);
            const auto result = widthDetector.GetWidth({ wstr.c_str(), wstr.size() });
            VERIFY_ARE_EQUAL(result, expected);
        }
    }

    static bool FallbackMethod(const std::wstring_view glyph)
    {
        if (glyph.size() < 1)
        {
            return false;
        }
        else
        {
            return (glyph.at(0) % 2) == 1;
        }
    }

    TEST_METHOD(AmbiguousCache)
    {
        // Set up a detector with fallback.
        CodepointWidthDetector widthDetector;
        widthDetector.SetFallbackMethod(std::bind(&FallbackMethod, std::placeholders::_1));

        // Ensure fallback cache is empty.
        VERIFY_ARE_EQUAL(0u, widthDetector._fallbackCache.size());

        // Lookup ambiguous width character.
        widthDetector.IsWide(ambiguous);

        // Cache should hold it.
        VERIFY_ARE_EQUAL(1u, widthDetector._fallbackCache.size());

        // Cached item should match what we expect
        const auto it = widthDetector._fallbackCache.begin();
        VERIFY_ARE_EQUAL(ambiguous, it->first);
        VERIFY_ARE_EQUAL(FallbackMethod(ambiguous), it->second);

        // Cache should empty when font changes.
        widthDetector.NotifyFontChanged();
        VERIFY_ARE_EQUAL(0u, widthDetector._fallbackCache.size());
    }
};
