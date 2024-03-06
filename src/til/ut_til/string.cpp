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

    // Normally this would be the spot where you'd find a TEST_METHOD(to_ulong).
    // I didn't quite trust my coding skills and thus opted to use fuzz-testing.
    // The below function was used to test to_ulong for unsafety and conformance with clang's strtoul.
    // The test was run as:
    //   clang++ -fsanitize=address,undefined,fuzzer -std=c++17 file.cpp
    // and was run for 20min across 16 jobs in parallel.
#if 0
    extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
    {
        while (size > 0 && (isspace(*data) || *data == '+' || *data == '-'))
        {
            --size;
            ++data;
        }

        if (size == 0 || size > 127)
        {
            return 0;
        }

        char narrow_buffer[128];
        wchar_t wide_buffer[128];

        memcpy(narrow_buffer, data, size);
        for (size_t i = 0; i < size; ++i)
        {
            wide_buffer[i] = data[i];
        }

        // strtoul requires a null terminator
        narrow_buffer[size] = 0;
        wide_buffer[size] = 0;

        char* end;
        const auto expected = strtoul(narrow_buffer, &end, 0);
        if (end != narrow_buffer + size || expected >= ULONG_MAX / 16)
        {
            return 0;
        }

        const auto actual = to_ulong({ wide_buffer, size });
        if (expected != actual)
        {
            __builtin_trap();
        }

        return 0;
    }
#endif

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

    TEST_METHOD(prefix_split_char)
    {
        {
            std::string_view s{ "" };
            VERIFY_ARE_EQUAL("", til::prefix_split(s, ' '));
            VERIFY_ARE_EQUAL("", s);
        }
        {
            std::string_view s{ "foo bar baz" };
            VERIFY_ARE_EQUAL("foo", til::prefix_split(s, ' '));
            VERIFY_ARE_EQUAL("bar baz", s);
            VERIFY_ARE_EQUAL("bar", til::prefix_split(s, ' '));
            VERIFY_ARE_EQUAL("baz", s);
            VERIFY_ARE_EQUAL("baz", til::prefix_split(s, ' '));
            VERIFY_ARE_EQUAL("", s);
        }
    }

    TEST_METHOD(CleanPathAndFilename)
    {
        VERIFY_ARE_EQUAL(LR"(CUsersGeddyMusicAnalog Man)", til::clean_filename(LR"(C:\Users\Geddy\Music\"Analog Man")"));
        VERIFY_ARE_EQUAL(LR"(C:\Users\Geddy\Music\Analog Man)", til::clean_path(LR"(C:\Users\Geddy\Music\"Analog Man")"));
    }

    TEST_METHOD(LegalPath)
    {
        VERIFY_IS_TRUE(til::is_legal_path(LR"(C:\Users\Documents and Settings\Users\;\Why not)"));
        VERIFY_IS_FALSE(til::is_legal_path(LR"(C:\Users\Documents and Settings\"Quote-un-quote users")"));
    }
};
