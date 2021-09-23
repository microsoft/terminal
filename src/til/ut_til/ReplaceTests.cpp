// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class ReplaceTests
{
    TEST_CLASS(ReplaceTests);

    TEST_METHOD(ReplaceStrings);
    TEST_METHOD(ReplaceStringAndViews);
    TEST_METHOD(ReplaceStringsInplace);
    TEST_METHOD(ReplaceStringAndViewsInplace);

    TEST_METHOD(ReplaceWstrings);
    TEST_METHOD(ReplaceWstringAndViews);
    TEST_METHOD(ReplaceWstringsInplace);
    TEST_METHOD(ReplaceWstringAndViewsInplace);

    // There are explicitly no winrt::hstring tests here, because it's capital-H
    // hard to get the winrt hstring header included in this project without
    // pulling in all of the winrt machinery.
};

void ReplaceTests::ReplaceStrings()
{
    std::string foo{ "foo" };

    auto temp1 = til::replace_needle_in_haystack(foo, "f", "b");
    VERIFY_ARE_EQUAL("boo", temp1);

    auto temp2 = til::replace_needle_in_haystack(temp1, "o", "00");
    VERIFY_ARE_EQUAL("b0000", temp2);
}

void ReplaceTests::ReplaceStringAndViews()
{
    std::string foo{ "foo" };
    std::string_view f{ "f" };
    std::string_view b{ "b" };
    std::string_view o{ "o" };
    std::string_view zeroZero{ "00" };

    auto temp1 = til::replace_needle_in_haystack(foo, f, b);
    VERIFY_ARE_EQUAL("boo", temp1);

    auto temp2 = til::replace_needle_in_haystack(temp1, o, zeroZero);
    VERIFY_ARE_EQUAL("b0000", temp2);
}

void ReplaceTests::ReplaceStringsInplace()
{
    std::string foo{ "foo" };

    til::replace_needle_in_haystack_inplace(foo, "f", "b");
    VERIFY_ARE_EQUAL("boo", foo);

    til::replace_needle_in_haystack_inplace(foo, "o", "00");
    VERIFY_ARE_EQUAL("b0000", foo);
}

void ReplaceTests::ReplaceStringAndViewsInplace()
{
    std::string foo{ "foo" };
    std::string_view f{ "f" };
    std::string_view b{ "b" };
    std::string_view o{ "o" };
    std::string_view zeroZero{ "00" };

    til::replace_needle_in_haystack_inplace(foo, f, b);
    VERIFY_ARE_EQUAL("boo", foo);

    til::replace_needle_in_haystack_inplace(foo, o, zeroZero);
    VERIFY_ARE_EQUAL("b0000", foo);
}

void ReplaceTests::ReplaceWstrings()
{
    std::wstring foo{ L"foo" };

    auto temp1 = til::replace_needle_in_haystack(foo, L"f", L"b");
    VERIFY_ARE_EQUAL(L"boo", temp1);

    auto temp2 = til::replace_needle_in_haystack(temp1, L"o", L"00");
    VERIFY_ARE_EQUAL(L"b0000", temp2);
}

void ReplaceTests::ReplaceWstringAndViews()
{
    std::wstring foo{ L"foo" };
    std::wstring_view f{ L"f" };
    std::wstring_view b{ L"b" };
    std::wstring_view o{ L"o" };
    std::wstring_view zeroZero{ L"00" };

    auto temp1 = til::replace_needle_in_haystack(foo, f, b);
    VERIFY_ARE_EQUAL(L"boo", temp1);

    auto temp2 = til::replace_needle_in_haystack(temp1, o, zeroZero);
    VERIFY_ARE_EQUAL(L"b0000", temp2);
}

void ReplaceTests::ReplaceWstringsInplace()
{
    std::wstring foo{ L"foo" };

    til::replace_needle_in_haystack_inplace(foo, L"f", L"b");
    VERIFY_ARE_EQUAL(L"boo", foo);

    til::replace_needle_in_haystack_inplace(foo, L"o", L"00");
    VERIFY_ARE_EQUAL(L"b0000", foo);
}

void ReplaceTests::ReplaceWstringAndViewsInplace()
{
    std::wstring foo{ L"foo" };
    std::wstring_view f{ L"f" };
    std::wstring_view b{ L"b" };
    std::wstring_view o{ L"o" };
    std::wstring_view zeroZero{ L"00" };

    til::replace_needle_in_haystack_inplace(foo, f, b);
    VERIFY_ARE_EQUAL(L"boo", foo);

    til::replace_needle_in_haystack_inplace(foo, o, zeroZero);
    VERIFY_ARE_EQUAL(L"b0000", foo);
}
