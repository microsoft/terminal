// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class SomeTests
{
    TEST_CLASS(SomeTests);

    TEST_METHOD(Fill)
    {
        til::some<int, 4> s;

        const auto val = 12;
        s.fill(val);

        VERIFY_ARE_EQUAL(s.max_size(), s.size());

        for (const auto& i : s)
        {
            VERIFY_ARE_EQUAL(val, i);
        }
    }

    TEST_METHOD(Swap)
    {
        til::some<int, 4> a;
        til::some<int, 4> b;

        const auto aVal = 900;
        a.fill(900);

        const auto bVal = 45;
        b.push_back(45);

        const auto aSize = a.size();
        const auto bSize = b.size();

        a.swap(b);

        VERIFY_ARE_EQUAL(aSize, b.size());
        VERIFY_ARE_EQUAL(bSize, a.size());

        VERIFY_ARE_EQUAL(bVal, a[0]);

        for (const auto& i : b)
        {
            VERIFY_ARE_EQUAL(aVal, i);
        }
    }

    TEST_METHOD(Size)
    {
        til::some<int, 2> c;

        VERIFY_ARE_EQUAL(0, c.size());

        c.push_back(3);
        VERIFY_ARE_EQUAL(1, c.size());

        c.push_back(12);
        VERIFY_ARE_EQUAL(2, c.size());

        c.pop_back();
        VERIFY_ARE_EQUAL(1, c.size());

        c.pop_back();
        VERIFY_ARE_EQUAL(0, c.size());
    }

    TEST_METHOD(MaxSize)
    {
        til::some<int, 2> c;

        VERIFY_ARE_EQUAL(2, c.max_size());

        c.push_back(3);
        VERIFY_ARE_EQUAL(2, c.max_size());

        c.push_back(12);
        VERIFY_ARE_EQUAL(2, c.size());

        c.pop_back();
        VERIFY_ARE_EQUAL(2, c.max_size());

        c.pop_back();
        VERIFY_ARE_EQUAL(2, c.max_size());
    }

    TEST_METHOD(PushBack)
    {
        til::some<int, 1> s;
        s.push_back(12);
        VERIFY_THROWS(s.push_back(12), std::out_of_range);
    }

    TEST_METHOD(PopBack)
    {
        til::some<int, 1> s;
        VERIFY_THROWS(s.pop_back(), std::out_of_range);

        s.push_back(12);
        VERIFY_THROWS(s.push_back(12), std::out_of_range);
    }

    TEST_METHOD(Empty)
    {
        til::some<int, 2> s;
        VERIFY_IS_TRUE(s.empty());
        s.push_back(12);
        VERIFY_IS_FALSE(s.empty());
        s.pop_back();
        VERIFY_IS_TRUE(s.empty());
    }
};
