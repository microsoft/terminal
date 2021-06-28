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

    TEST_METHOD(StartsWith)
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

    TEST_METHOD(EndsWith)
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
};
