// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

template<>
class WEX::TestExecution::VerifyOutputTraits<std::vector<std::wstring>>
{
public:
    static WEX::Common::NoThrowString ToString(const std::vector<std::wstring>& vec)
    {
        WEX::Common::NoThrowString str;
        str.Append(L"{ ");
        for (size_t i = 0; i < vec.size(); ++i)
        {
            str.Append(i == 0 ? L"\"" : L", \"");
            str.Append(vec[i].c_str());
            str.Append(L"\"");
        }
        str.Append(L" }");
        return str;
    }
};

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

    // Normally this would be the spot where you'd find a TEST_METHOD(parse_u64).
    // I didn't quite trust my coding skills and thus opted to use fuzz-testing.
    // The below function was used to test parse_u64 for unsafety and conformance with clang's strtoul.
    // The test was run as:
    //   clang++ -fsanitize=address,undefined,fuzzer -std=c++17 file.cpp
    // and was run for 20min across 16 jobs in parallel.
#if 0
    template<typename T, typename Traits>
    std::optional<uint64_t> parse_u64(const std::basic_string_view<T, Traits>& str, int base = 0) noexcept
    {
        // ... implementation ...
    }

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
        memcpy(narrow_buffer, data, size);
        // strtoul requires a null terminator
        narrow_buffer[size] = 0;

        char* end;
        const auto val = strtoull(narrow_buffer, &end, 0);
        const auto bad = end != narrow_buffer + size || val == ULLONG_MAX;
        const auto expected = bad ? std::nullopt : std::optional{ val };

        const auto actual = parse_u64(std::string_view{ narrow_buffer, size });
        if (expected != actual && actual.value_or(0) != ULLONG_MAX)
        {
            __builtin_trap();
        }

        return 0;
    }
#endif

    TEST_METHOD(parse_u64_overflow)
    {
        VERIFY_ARE_EQUAL(UINT64_C(18446744073709551614), til::details::parse_u64(std::string_view{ "18446744073709551614" }));
        VERIFY_ARE_EQUAL(UINT64_C(18446744073709551615), til::details::parse_u64(std::string_view{ "18446744073709551615" }));
        VERIFY_ARE_EQUAL(std::nullopt, til::details::parse_u64(std::string_view{ "18446744073709551616" }));
        VERIFY_ARE_EQUAL(std::nullopt, til::details::parse_u64(std::string_view{ "18446744073709551617" }));
        VERIFY_ARE_EQUAL(std::nullopt, til::details::parse_u64(std::string_view{ "88888888888888888888" }));
    }

    TEST_METHOD(parse_unsigned)
    {
        VERIFY_ARE_EQUAL(std::nullopt, til::parse_unsigned<uint32_t>(""));
        VERIFY_ARE_EQUAL(std::nullopt, til::parse_unsigned<uint32_t>("0x"));
        VERIFY_ARE_EQUAL(std::nullopt, til::parse_unsigned<uint32_t>("Z"));
        VERIFY_ARE_EQUAL(std::nullopt, til::parse_unsigned<uint32_t>("0xZ"));
        VERIFY_ARE_EQUAL(std::nullopt, til::parse_unsigned<uint32_t>("0Z"));
        VERIFY_ARE_EQUAL(std::nullopt, til::parse_unsigned<uint32_t>("123abc"));
        VERIFY_ARE_EQUAL(std::nullopt, til::parse_unsigned<uint32_t>("0123abc"));
        VERIFY_ARE_EQUAL(std::nullopt, til::parse_unsigned<uint32_t>("0x100000000"));
        VERIFY_ARE_EQUAL(0u, til::parse_unsigned<uint32_t>("0"));
        VERIFY_ARE_EQUAL(0u, til::parse_unsigned<uint32_t>("0x0"));
        VERIFY_ARE_EQUAL(0123u, til::parse_unsigned<uint32_t>("0123"));
        VERIFY_ARE_EQUAL(123u, til::parse_unsigned<uint32_t>("123"));
        VERIFY_ARE_EQUAL(0x123u, til::parse_unsigned<uint32_t>("0x123"));
        VERIFY_ARE_EQUAL(0x123abcu, til::parse_unsigned<uint32_t>("0x123abc"));
        VERIFY_ARE_EQUAL(0X123ABCu, til::parse_unsigned<uint32_t>("0X123ABC"));
        VERIFY_ARE_EQUAL(UINT32_MAX, til::parse_unsigned<uint32_t>("0xffffffff"));
        VERIFY_ARE_EQUAL(UINT32_MAX, til::parse_unsigned<uint32_t>("4294967295"));
    }

    TEST_METHOD(parse_signed)
    {
        VERIFY_ARE_EQUAL(std::nullopt, til::parse_signed<int32_t>(""));
        VERIFY_ARE_EQUAL(std::nullopt, til::parse_signed<int32_t>("-"));
        VERIFY_ARE_EQUAL(std::nullopt, til::parse_signed<int32_t>("--"));
        VERIFY_ARE_EQUAL(std::nullopt, til::parse_signed<int32_t>("--0"));
        VERIFY_ARE_EQUAL(std::nullopt, til::parse_signed<int32_t>("-0Z"));
        VERIFY_ARE_EQUAL(std::nullopt, til::parse_signed<int32_t>("-123abc"));
        VERIFY_ARE_EQUAL(std::nullopt, til::parse_signed<int32_t>("-0123abc"));
        VERIFY_ARE_EQUAL(std::nullopt, til::parse_signed<int32_t>("0x80000000"));
        VERIFY_ARE_EQUAL(std::nullopt, til::parse_signed<int32_t>("-0x80000001"));
        VERIFY_ARE_EQUAL(0, til::parse_signed<int32_t>("0"));
        VERIFY_ARE_EQUAL(0, til::parse_signed<int32_t>("-0"));
        VERIFY_ARE_EQUAL(0, til::parse_signed<int32_t>("-0x0"));
        VERIFY_ARE_EQUAL(0123, til::parse_signed<int32_t>("0123"));
        VERIFY_ARE_EQUAL(123, til::parse_signed<int32_t>("123"));
        VERIFY_ARE_EQUAL(0x123, til::parse_signed<int32_t>("0x123"));
        VERIFY_ARE_EQUAL(-0123, til::parse_signed<int32_t>("-0123"));
        VERIFY_ARE_EQUAL(-123, til::parse_signed<int32_t>("-123"));
        VERIFY_ARE_EQUAL(-0x123, til::parse_signed<int32_t>("-0x123"));
        VERIFY_ARE_EQUAL(-0x123abc, til::parse_signed<int32_t>("-0x123abc"));
        VERIFY_ARE_EQUAL(-0X123ABC, til::parse_signed<int32_t>("-0X123ABC"));
        VERIFY_ARE_EQUAL(INT32_MIN, til::parse_signed<int32_t>("-0x80000000"));
        VERIFY_ARE_EQUAL(INT32_MIN, til::parse_signed<int32_t>("-2147483648"));
        VERIFY_ARE_EQUAL(INT32_MAX, til::parse_signed<int32_t>("0x7fffffff"));
        VERIFY_ARE_EQUAL(INT32_MAX, til::parse_signed<int32_t>("2147483647"));
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

    TEST_METHOD(split_iterator)
    {
        static constexpr auto split = [](const std::string_view& str, const char needle) {
            std::vector<std::string_view> substrings;
            for (auto&& s : til::split_iterator{ str, needle })
            {
                substrings.emplace_back(s);
            }
            return substrings;
        };

        std::vector<std::string_view> expected;
        std::vector<std::string_view> actual;

        expected = { "foo" };
        actual = split("foo", ' ');
        VERIFY_ARE_EQUAL(expected, actual);

        expected = { "", "foo" };
        actual = split(" foo", ' ');
        VERIFY_ARE_EQUAL(expected, actual);

        expected = { "foo", "" };
        actual = split("foo ", ' ');
        VERIFY_ARE_EQUAL(expected, actual);

        expected = { "foo", "bar", "baz" };
        actual = split("foo bar baz", ' ');
        VERIFY_ARE_EQUAL(expected, actual);

        expected = { "", "", "foo", "", "bar", "", "" };
        actual = split(";;foo;;bar;;", ';');
        VERIFY_ARE_EQUAL(expected, actual);
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

    TEST_METHOD(IterateFontFamilies)
    {
        static constexpr auto expected = [](auto&&... args) {
            return std::vector<std::wstring>{ std::forward<decltype(args)>(args)... };
        };
        static constexpr auto actual = [](std::wstring_view families) {
            std::vector<std::wstring> split;
            til::iterate_font_families(families, [&](std::wstring&& str) {
                split.emplace_back(std::move(str));
            });
            return split;
        };

        VERIFY_ARE_EQUAL(expected(L"foo", L" b  a  r ", LR"(b"az)"), actual(LR"(  foo  ," b  a  r ",b\"az)"));
        VERIFY_ARE_EQUAL(expected(LR"(foo, bar)"), actual(LR"("foo, bar")"));
        VERIFY_ARE_EQUAL(expected(LR"("foo")", LR"('bar')"), actual(LR"('"foo"', "'bar'")"));
        VERIFY_ARE_EQUAL(expected(LR"("foo")", LR"('bar')"), actual(LR"("\"foo\"", '\'bar\'')"));
        VERIFY_ARE_EQUAL(expected(L"foo"), actual(LR"(,,,,foo,,,,)"));
    }
};
