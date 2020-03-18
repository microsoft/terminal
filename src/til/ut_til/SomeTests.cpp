// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class SomeTests
{
    TEST_CLASS(SomeTests);

    TEST_METHOD(Construct)
    {
        Log::Comment(L"Default Constructor");
        til::some<int, 2> s;

        Log::Comment(L"Valid Initializer List Constructor");
        til::some<int, 2> t{ 1 };
        til::some<int, 2> u{ 1, 2 };

        Log::Comment(L"Invalid Initializer List Constructor");
        auto f = []() {
            til::some<int, 2> v{ 1, 2, 3 };
        };

        VERIFY_THROWS(f(), std::invalid_argument);
    }

    TEST_METHOD(Equality)
    {
        til::some<int, 2> a{ 1, 2 };
        til::some<int, 2> b{ 1, 2 };
        VERIFY_IS_TRUE(a == b);

        til::some<int, 2> c{ 3, 2 };
        VERIFY_IS_FALSE(a == c);

        til::some<int, 2> d{ 2, 3 };
        VERIFY_IS_FALSE(a == d);

        til::some<int, 2> e{ 1 };
        VERIFY_IS_FALSE(a == e);
    }

    TEST_METHOD(Inequality)
    {
        til::some<int, 2> a{ 1, 2 };
        til::some<int, 2> b{ 1, 2 };
        VERIFY_IS_FALSE(a != b);

        til::some<int, 2> c{ 3, 2 };
        VERIFY_IS_TRUE(a != c);

        til::some<int, 2> d{ 2, 3 };
        VERIFY_IS_TRUE(a != d);

        til::some<int, 2> e{ 1 };
        VERIFY_IS_TRUE(a != e);
    }

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

        VERIFY_ARE_EQUAL(0u, c.size());

        c.push_back(3);
        VERIFY_ARE_EQUAL(1u, c.size());

        c.push_back(12);
        VERIFY_ARE_EQUAL(2u, c.size());

        c.pop_back();
        VERIFY_ARE_EQUAL(1u, c.size());

        c.pop_back();
        VERIFY_ARE_EQUAL(0u, c.size());
    }

    TEST_METHOD(MaxSize)
    {
        til::some<int, 2> c;

        VERIFY_ARE_EQUAL(2u, c.max_size());

        c.push_back(3);
        VERIFY_ARE_EQUAL(2u, c.max_size());

        c.push_back(12);
        VERIFY_ARE_EQUAL(2u, c.size());

        c.pop_back();
        VERIFY_ARE_EQUAL(2u, c.max_size());

        c.pop_back();
        VERIFY_ARE_EQUAL(2u, c.max_size());
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

    TEST_METHOD(Clear)
    {
        til::some<int, 2> s;
        VERIFY_IS_TRUE(s.empty());
        s.push_back(12);
        VERIFY_IS_FALSE(s.empty());
        VERIFY_ARE_EQUAL(1u, s.size());
        s.clear();
        VERIFY_IS_TRUE(s.empty());
        VERIFY_ARE_EQUAL(0u, s.size());
    }

    TEST_METHOD(ClearFreesMembers)
    {
        til::some<std::shared_ptr<int>, 2> s;

        auto a = std::make_shared<int>(4);
        auto weakA = std::weak_ptr<int>(a);

        auto b = std::make_shared<int>(6);
        auto weakB = std::weak_ptr<int>(b);

        s.push_back(std::move(a));
        s.push_back(std::move(b));

        VERIFY_IS_FALSE(weakA.expired());
        VERIFY_IS_FALSE(weakB.expired());

        s.clear();

        VERIFY_IS_TRUE(weakA.expired());
        VERIFY_IS_TRUE(weakB.expired());
    }

    TEST_METHOD(Data)
    {
        til::some<int, 2> s;
        const auto one = 1;
        const auto two = 2;
        s.push_back(one);
        s.push_back(two);

        auto data = s.data();

        VERIFY_ARE_EQUAL(one, *data);
        VERIFY_ARE_EQUAL(two, *(data + 1));
    }

    TEST_METHOD(FrontBack)
    {
        til::some<int, 2> s;
        const auto one = 1;
        const auto two = 2;
        s.push_back(one);
        s.push_back(two);

        VERIFY_ARE_EQUAL(one, s.front());
        VERIFY_ARE_EQUAL(two, s.back());
    }

    TEST_METHOD(Indexing)
    {
        const auto one = 14;
        const auto two = 28;

        til::some<int, 2> s;
        VERIFY_THROWS(s.at(0), std::out_of_range);
        VERIFY_THROWS(s.at(1), std::out_of_range);
        auto a = s[0];
        a = s[1];

        s.push_back(one);
        VERIFY_ARE_EQUAL(one, s.at(0));
        VERIFY_ARE_EQUAL(one, s[0]);
        VERIFY_THROWS(s.at(1), std::out_of_range);
        a = s[1];

        s.push_back(two);
        VERIFY_ARE_EQUAL(one, s.at(0));
        VERIFY_ARE_EQUAL(one, s[0]);
        VERIFY_ARE_EQUAL(two, s.at(1));
        VERIFY_ARE_EQUAL(two, s[1]);

        s.pop_back();
        VERIFY_ARE_EQUAL(one, s.at(0));
        VERIFY_ARE_EQUAL(one, s[0]);
        VERIFY_THROWS(s.at(1), std::out_of_range);
        a = s[1];

        s.pop_back();
        VERIFY_THROWS(s.at(0), std::out_of_range);
        VERIFY_THROWS(s.at(1), std::out_of_range);
        a = s[0];
        a = s[1];
    }

    TEST_METHOD(ForwardIter)
    {
        const int vals[] = { 17, 99 };
        const int valLength = ARRAYSIZE(vals);

        til::some<int, 2> s;
        VERIFY_ARE_EQUAL(s.begin(), s.end());
        VERIFY_ARE_EQUAL(s.cbegin(), s.cend());
        VERIFY_ARE_EQUAL(s.begin(), s.cbegin());
        VERIFY_ARE_EQUAL(s.end(), s.cend());

        s.push_back(vals[0]);
        s.push_back(vals[1]);

        VERIFY_ARE_EQUAL(s.begin() + valLength, s.end());
        VERIFY_ARE_EQUAL(s.cbegin() + valLength, s.cend());

        auto count = 0;
        for (const auto& i : s)
        {
            VERIFY_ARE_EQUAL(vals[count], i);
            ++count;
        }
        VERIFY_ARE_EQUAL(valLength, count);

        count = 0;
        for (auto i = s.cbegin(); i < s.cend(); ++i)
        {
            VERIFY_ARE_EQUAL(vals[count], *i);
            ++count;
        }
        VERIFY_ARE_EQUAL(valLength, count);

        count = 0;
        for (auto i = s.begin(); i < s.end(); ++i)
        {
            VERIFY_ARE_EQUAL(vals[count], *i);
            ++count;
        }
        VERIFY_ARE_EQUAL(valLength, count);
    }

    TEST_METHOD(ReverseIter)
    {
        const int vals[] = { 17, 99 };
        const int valLength = ARRAYSIZE(vals);

        til::some<int, 2> s;
        VERIFY_ARE_EQUAL(s.rbegin(), s.rend());
        VERIFY_ARE_EQUAL(s.crbegin(), s.crend());
        VERIFY_ARE_EQUAL(s.rbegin(), s.crbegin());
        VERIFY_ARE_EQUAL(s.rend(), s.crend());

        s.push_back(vals[0]);
        s.push_back(vals[1]);

        VERIFY_ARE_EQUAL(s.rbegin() + valLength, s.rend());
        VERIFY_ARE_EQUAL(s.crbegin() + valLength, s.crend());

        auto count = 0;
        for (auto i = s.crbegin(); i < s.crend(); ++i)
        {
            VERIFY_ARE_EQUAL(vals[valLength - count - 1], *i);
            ++count;
        }
        VERIFY_ARE_EQUAL(valLength, count);

        count = 0;
        for (auto i = s.rbegin(); i < s.rend(); ++i)
        {
            VERIFY_ARE_EQUAL(vals[valLength - count - 1], *i);
            ++count;
        }
        VERIFY_ARE_EQUAL(valLength, count);
    }
};
