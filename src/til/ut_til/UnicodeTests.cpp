// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"

#include <til/unicode.h>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

#define REPLACEMENT L"\xFFFD"
#define LEADING L"\xD801"
#define TRAILING L"\xDC01"
#define PAIR L"\xD801\xDC01"

class UnicodeTests
{
    TEST_CLASS(UnicodeTests);

    TEST_METHOD(utf16_next)
    {
        struct Test
        {
            std::wstring_view input;
            std::wstring_view expected;
        };

        static constexpr std::array tests{
            Test{ L"", REPLACEMENT },
            Test{ L"a", L"a" },
            Test{ L"abc", L"a" },
            Test{ L"a" PAIR, L"a" },
            Test{ L"a" LEADING, L"a" },
            Test{ L"a" TRAILING, L"a" },
            Test{ PAIR L"a", PAIR },
            Test{ LEADING L"a", REPLACEMENT },
            Test{ TRAILING L"a", REPLACEMENT },
        };

        for (const auto& t : tests)
        {
            const auto actual = til::utf16_next(t.input);
            VERIFY_ARE_EQUAL(t.expected, actual);
        }
    }

    TEST_METHOD(utf16_iterator)
    {
        struct Test
        {
            std::wstring_view input;
            til::some<std::wstring_view, 5> expected;
        };

        static constexpr std::array tests{
            Test{ L"", {} },
            Test{ L"a", { L"a" } },
            Test{ L"abc", { L"a", L"b", L"c" } },
            Test{ PAIR L"a" PAIR L"b" PAIR, { PAIR, L"a", PAIR, L"b", PAIR } },
            Test{ LEADING L"a" LEADING L"b" LEADING, { REPLACEMENT, L"a", REPLACEMENT, L"b", REPLACEMENT } },
            Test{ TRAILING L"a" TRAILING L"b" TRAILING, { REPLACEMENT, L"a", REPLACEMENT, L"b", REPLACEMENT } },
            Test{ L"a" TRAILING LEADING L"b", { L"a", REPLACEMENT, REPLACEMENT, L"b" } },
        };

        for (const auto& t : tests)
        {
            auto it = t.expected.begin();
            const auto end = t.expected.end();

            for (const auto& v : til::utf16_iterator{ t.input })
            {
                VERIFY_ARE_NOT_EQUAL(end, it);
                VERIFY_ARE_EQUAL(*it, v);
                ++it;
            }

            VERIFY_ARE_EQUAL(end, it);
        }
    }
};
