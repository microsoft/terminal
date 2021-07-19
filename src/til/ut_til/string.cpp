// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class StringTests
{
    TEST_CLASS(StringTests);

    TEST_METHOD(VisualizeControlCodes)
    {
        const std::wstring_view input{ L"\u001b[A \u001b[B\x7f" };
        const std::wstring_view expected{ L"\u241b[A\u2423\u241b[B\x2421" };
        const auto actual = til::visualize_control_codes(input);
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(starts_with)
    {
        VERIFY_IS_TRUE(til::starts_with("", ""));

        VERIFY_IS_TRUE(til::starts_with("abc", ""));
        VERIFY_IS_TRUE(til::starts_with("abc", "a"));
        VERIFY_IS_TRUE(til::starts_with("abc", "ab"));
        VERIFY_IS_TRUE(til::starts_with("abc", "abc"));
        VERIFY_IS_FALSE(til::starts_with("abc", "abcd"));

        VERIFY_IS_FALSE(til::starts_with("", "abc"));
        VERIFY_IS_FALSE(til::starts_with("a", "abc"));
        VERIFY_IS_FALSE(til::starts_with("ab", "abc"));
        VERIFY_IS_TRUE(til::starts_with("abc", "abc"));
        VERIFY_IS_TRUE(til::starts_with("abcd", "abc"));
    }

    TEST_METHOD(ends_with)
    {
        VERIFY_IS_TRUE(til::ends_with("", ""));

        VERIFY_IS_TRUE(til::ends_with("abc", ""));
        VERIFY_IS_TRUE(til::ends_with("abc", "c"));
        VERIFY_IS_TRUE(til::ends_with("abc", "bc"));
        VERIFY_IS_TRUE(til::ends_with("abc", "abc"));
        VERIFY_IS_FALSE(til::ends_with("abc", "0abc"));

        VERIFY_IS_FALSE(til::ends_with("", "abc"));
        VERIFY_IS_FALSE(til::ends_with("c", "abc"));
        VERIFY_IS_FALSE(til::ends_with("bc", "abc"));
        VERIFY_IS_TRUE(til::ends_with("abc", "abc"));
        VERIFY_IS_TRUE(til::ends_with("0abc", "abc"));
    }

    TEST_METHOD(tolower_ascii)
    {
        for (wchar_t ch = 0; ch < 128; ++ch)
        {
            VERIFY_ARE_EQUAL(std::towlower(ch), til::tolower_ascii(ch));
        }
    }

    TEST_METHOD(toupper_ascii)
    {
        for (wchar_t ch = 0; ch < 128; ++ch)
        {
            VERIFY_ARE_EQUAL(std::towupper(ch), til::toupper_ascii(ch));
        }
    }

    TEST_METHOD(equals_insensitive_ascii)
    {
        VERIFY_IS_TRUE(til::equals_insensitive_ascii("", ""));
        VERIFY_IS_FALSE(til::equals_insensitive_ascii("", "foo"));
        VERIFY_IS_FALSE(til::equals_insensitive_ascii("foo", "fo"));
        VERIFY_IS_FALSE(til::equals_insensitive_ascii("fooo", "foo"));
        VERIFY_IS_TRUE(til::equals_insensitive_ascii("cOUnterStriKE", "COuntERStRike"));
    }

    TEST_METHOD(prefix_split)
    {
        {
            std::string_view s{ "" };
            VERIFY_ARE_EQUAL("", til::prefix_split(s, ""));
            VERIFY_ARE_EQUAL("", s);
        }
        {
            std::string_view s{ "" };
            VERIFY_ARE_EQUAL("", til::prefix_split(s, " "));
            VERIFY_ARE_EQUAL("", s);
        }
        {
            std::string_view s{ " " };
            VERIFY_ARE_EQUAL(" ", til::prefix_split(s, ""));
            VERIFY_ARE_EQUAL("", s);
        }
        {
            std::string_view s{ "foo" };
            VERIFY_ARE_EQUAL("foo", til::prefix_split(s, ""));
            VERIFY_ARE_EQUAL("", s);
        }
        {
            std::string_view s{ "foo bar baz" };
            VERIFY_ARE_EQUAL("foo", til::prefix_split(s, " "));
            VERIFY_ARE_EQUAL("bar baz", s);
            VERIFY_ARE_EQUAL("bar", til::prefix_split(s, " "));
            VERIFY_ARE_EQUAL("baz", s);
            VERIFY_ARE_EQUAL("baz", til::prefix_split(s, " "));
            VERIFY_ARE_EQUAL("", s);
        }
        {
            std::string_view s{ "foo123barbaz123" };
            VERIFY_ARE_EQUAL("foo", til::prefix_split(s, "123"));
            VERIFY_ARE_EQUAL("barbaz123", s);
            VERIFY_ARE_EQUAL("barbaz", til::prefix_split(s, "123"));
            VERIFY_ARE_EQUAL("", s);
            VERIFY_ARE_EQUAL("", til::prefix_split(s, ""));
            VERIFY_ARE_EQUAL("", s);
        }
    }
};
