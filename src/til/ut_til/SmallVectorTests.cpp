// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <til/small_vector.h>

using namespace std::literals;
using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

// This test code was taken from our STL's tests/tr1/tests/vector/test.cpp.
// Some minor parts were removed or rewritten to fit our spell checker as well as the
// slightly more minimalistic small_vector API which doesn't implement all of std::vector.

struct Copyable_int
{
    Copyable_int(int v = 0) noexcept :
        val(v)
    {
    }

    Copyable_int(const Copyable_int& x) noexcept :
        val(x.val)
    {
    }

    Copyable_int& operator=(const Copyable_int& x) noexcept
    {
        val = x.val;
        return *this;
    }

    Copyable_int(Copyable_int&& right) noexcept :
        val(right.val)
    {
        right.val = -1;
    }

    Copyable_int& operator=(Copyable_int&& right) noexcept
    {
        if (this != &right)
        {
            val = right.val;
            right.val = -1;
        }
        return *this;
    }

    bool operator==(const Copyable_int& x) const
    {
        return val == x.val;
    }

    int val;
};

struct Movable_int : Copyable_int
{
    Movable_int(int v = 0) noexcept :
        Copyable_int(v)
    {
    }

    Movable_int(int v1, int v2) noexcept :
        Copyable_int(v2 + (v1 << 4))
    {
    }

    Movable_int(int v1, int v2, int v3) noexcept :
        Copyable_int(v3 + (v2 << 4) + (v1 << 8))
    {
    }

    Movable_int(int v1, int v2, int v3, int v4) noexcept :
        Copyable_int(v4 + (v3 << 4) + (v2 << 8) + (v1 << 12))
    {
    }

    Movable_int(int v1, int v2, int v3, int v4, int v5) noexcept :
        Copyable_int(v5 + (v4 << 4) + (v3 << 8) + (v2 << 12) + (v1 << 16))
    {
    }

    Movable_int(const Movable_int&) = delete;
    Movable_int& operator=(const Movable_int&) = delete;

    Movable_int(Movable_int&& right) noexcept :
        Copyable_int(right.val)
    {
        right.val = -1;
    }

    Movable_int& operator=(Movable_int&& right) noexcept
    {
        if (this != &right)
        {
            val = right.val;
            right.val = -1;
        }
        return *this;
    }
};

class SmallVectorTests
{
    TEST_CLASS(SmallVectorTests)

    TEST_METHOD(Simple)
    {
        char ch = '\0';
        char carr[] = "abc";

        typedef til::small_vector<char, 3> container;
        container::pointer p_ptr = (char*)nullptr;
        container::const_pointer p_const_ptr = (const char*)nullptr;
        container::reference p_ref = ch;
        container::const_reference p_cref = (const char&)ch;
        container::value_type* p_val = (char*)nullptr;
        container::size_type* p_size = (std::size_t*)nullptr;
        container::difference_type* p_diff = (std::ptrdiff_t*)nullptr;

        p_ptr = p_ptr;
        p_const_ptr = p_const_ptr;
        p_ref = p_cref;
        p_size = p_size;
        p_diff = p_diff;
        p_val = p_val;

        container v0;
        VERIFY_IS_TRUE(v0.empty());
        VERIFY_ARE_EQUAL(v0.size(), 0u);

        container v1(5), v1a(6, 'x'), v1b(7, 'y');
        VERIFY_ARE_EQUAL(v1.size(), 5u);
        VERIFY_ARE_EQUAL(v1.back(), '\0');
        VERIFY_ARE_EQUAL(v1a.size(), 6u);
        VERIFY_ARE_EQUAL(v1a.back(), 'x');
        VERIFY_ARE_EQUAL(v1b.size(), 7u);
        VERIFY_ARE_EQUAL(v1b.back(), 'y');

        container v2(v1a);
        VERIFY_ARE_EQUAL(v2.size(), 6u);
        VERIFY_ARE_EQUAL(v2.front(), 'x');

        container v2a(v2);
        VERIFY_ARE_EQUAL(v2a.size(), 6u);
        VERIFY_ARE_EQUAL(v2a.front(), 'x');

        container v3(v1a.begin(), v1a.end());
        VERIFY_ARE_EQUAL(v3.size(), 6u);
        VERIFY_ARE_EQUAL(v3.front(), 'x');

        const container v4(v1a.begin(), v1a.end());
        VERIFY_ARE_EQUAL(v4.size(), 6u);
        VERIFY_ARE_EQUAL(v4.front(), 'x');
        v0 = v4;
        VERIFY_ARE_EQUAL(v0.size(), 6u);
        VERIFY_ARE_EQUAL(v0.front(), 'x');
        VERIFY_ARE_EQUAL(v0[0], 'x');
        VERIFY_ARE_EQUAL(v0.at(5), 'x');

        v0.reserve(12);
        VERIFY_IS_GREATER_THAN_OR_EQUAL(v0.capacity(), 12u);
        v0.resize(8);
        VERIFY_ARE_EQUAL(v0.size(), 8u);
        VERIFY_ARE_EQUAL(v0.back(), '\0');
        v0.resize(10, 'z');
        VERIFY_ARE_EQUAL(v0.size(), 10u);
        VERIFY_ARE_EQUAL(v0.back(), 'z');
        VERIFY_IS_LESS_THAN_OR_EQUAL(v0.size(), v0.max_size());

        container* p_cont = &v0;
        p_cont = p_cont; // to quiet diagnostics

        { // check iterator generators
            container::iterator p_it(v0.begin());
            container::const_iterator p_cit(v4.begin());
            container::reverse_iterator p_rit(v0.rbegin());
            container::const_reverse_iterator p_crit(v4.rbegin());
            VERIFY_ARE_EQUAL(*p_it, 'x');
            VERIFY_ARE_EQUAL(*--(p_it = v0.end()), 'z');
            VERIFY_ARE_EQUAL(*p_cit, 'x');
            VERIFY_ARE_EQUAL(*--(p_cit = v4.end()), 'x');
            VERIFY_ARE_EQUAL(*p_rit, 'z');
            VERIFY_ARE_EQUAL(*--(p_rit = v0.rend()), 'x');
            VERIFY_ARE_EQUAL(*p_crit, 'x');
            VERIFY_ARE_EQUAL(*--(p_crit = v4.rend()), 'x');

            container::const_iterator p_it1 = container::const_iterator();
            container::const_iterator p_it2 = container::const_iterator();
            VERIFY_ARE_EQUAL(p_it1, p_it2); // check null forward iterator comparisons
        }

        { // check const iterators generators
            container::const_iterator p_it(v0.cbegin());
            container::const_iterator p_cit(v4.cbegin());
            container::const_reverse_iterator p_rit(v0.crbegin());
            container::const_reverse_iterator p_crit(v4.crbegin());
            VERIFY_ARE_EQUAL(*p_it, 'x');
            VERIFY_ARE_EQUAL(*--(p_it = v0.cend()), 'z');
            VERIFY_ARE_EQUAL(*p_cit, 'x');
            VERIFY_ARE_EQUAL(*--(p_cit = v4.cend()), 'x');
            VERIFY_ARE_EQUAL(*p_rit, 'z');
            VERIFY_ARE_EQUAL(*--(p_rit = v0.crend()), 'x');
            VERIFY_ARE_EQUAL(*p_crit, 'x');
            VERIFY_ARE_EQUAL(*--(p_crit = v4.crend()), 'x');

            container::const_iterator p_it1 = container::const_iterator();
            container::const_iterator p_it2 = container::const_iterator();
            VERIFY_ARE_EQUAL(p_it1, p_it2); // check null forward iterator comparisons
        }

        VERIFY_ARE_EQUAL(v0.front(), 'x');
        VERIFY_ARE_EQUAL(v4.front(), 'x');

        v0.push_back('a');
        VERIFY_ARE_EQUAL(v0.back(), 'a');
        v0.pop_back();
        VERIFY_ARE_EQUAL(v0.back(), 'z');
        VERIFY_ARE_EQUAL(v4.back(), 'x');

        {
            container v5;
            v5.resize(10);
            VERIFY_ARE_EQUAL(v5.size(), 10u);
            VERIFY_ARE_EQUAL(v5[9], 0);

            container v6(20, 'x');
            container v7(std::move(v6));
            VERIFY_ARE_EQUAL(v6.size(), 0u);
            VERIFY_ARE_EQUAL(v7.size(), 20u);

            container v8;
            v8 = std::move(v7);
            VERIFY_ARE_EQUAL(v7.size(), 0u);
            VERIFY_ARE_EQUAL(v8.size(), 20u);

            til::small_vector<Movable_int, 3> v9;
            v9.resize(10);
            VERIFY_ARE_EQUAL(v9.size(), 10u);
            VERIFY_ARE_EQUAL(v9[9].val, 0);

            til::small_vector<Movable_int, 3> v10;
            Movable_int mi1(1);
            v10.push_back(std::move(mi1));
            VERIFY_ARE_EQUAL(mi1.val, -1);
            VERIFY_ARE_EQUAL(v10[0].val, 1);

            Movable_int mi3(3);
            v10.insert(v10.begin(), std::move(mi3));
            VERIFY_ARE_EQUAL(mi3.val, -1);
            VERIFY_ARE_EQUAL(v10[0].val, 3);
            VERIFY_ARE_EQUAL(v10[1].val, 1);

            v10.emplace_back();
            VERIFY_ARE_EQUAL(v10.back().val, 0);
            v10.emplace_back(2);
            VERIFY_ARE_EQUAL(v10.back().val, 2);
            v10.emplace_back(3, 2);
            VERIFY_ARE_EQUAL(v10.back().val, 0x32);
            v10.emplace_back(4, 3, 2);
            VERIFY_ARE_EQUAL(v10.back().val, 0x432);
            v10.emplace_back(5, 4, 3, 2);
            VERIFY_ARE_EQUAL(v10.back().val, 0x5432);
            v10.emplace_back(6, 5, 4, 3, 2);
            VERIFY_ARE_EQUAL(v10.back().val, 0x65432);
        }

        { // check for lvalue stealing
            til::small_vector<Copyable_int, 3> v11;
            Copyable_int ci1(1);
            v11.push_back(ci1);
            VERIFY_ARE_EQUAL(ci1.val, 1);
            VERIFY_ARE_EQUAL(v11[0].val, 1);

            Copyable_int ci3(3);
            v11.insert(v11.begin(), ci3);
            VERIFY_ARE_EQUAL(ci3.val, 3);
            VERIFY_ARE_EQUAL(v11[0].val, 3);
            VERIFY_ARE_EQUAL(v11[1].val, 1);

            til::small_vector<Copyable_int, 3> v12(v11);
            VERIFY_ARE_EQUAL(v11, v12);
            v11 = v12;
            VERIFY_ARE_EQUAL(v11, v12);
        }

        { // check front/back
            container::iterator p_it;
            v0.clear();
            v0.insert(v0.begin(), v4.begin(), v4.end());
            VERIFY_ARE_EQUAL(v0.size(), v4.size());
            VERIFY_ARE_EQUAL(v0.front(), v4.front());
            v0.clear();
            v0.insert(v0.begin(), 4, 'w');
            VERIFY_ARE_EQUAL(v0.size(), 4u);
            VERIFY_ARE_EQUAL(v0.front(), 'w');
            VERIFY_ARE_EQUAL(*v0.insert(v0.begin(), 'a'), 'a');
            VERIFY_ARE_EQUAL(v0.front(), 'a');
            VERIFY_ARE_EQUAL(*++(p_it = v0.begin()), 'w');
            VERIFY_ARE_EQUAL(*v0.insert(v0.begin(), 2, 'b'), 'b');
            VERIFY_ARE_EQUAL(v0.front(), 'b');
            VERIFY_ARE_EQUAL(*++(p_it = v0.begin()), 'b');
            VERIFY_ARE_EQUAL(*++ ++(p_it = v0.begin()), 'a');
            VERIFY_ARE_EQUAL(*v0.insert(v0.end(), v4.begin(), v4.end()), *v4.begin());
            VERIFY_ARE_EQUAL(v0.back(), v4.back());
            VERIFY_ARE_EQUAL(*v0.insert(v0.end(), carr, carr + 3), *carr);
            VERIFY_ARE_EQUAL(v0.back(), 'c');
            v0.erase(v0.begin());
            VERIFY_ARE_EQUAL(v0.front(), 'b');
            VERIFY_ARE_EQUAL(*++(p_it = v0.begin()), 'a');
            v0.erase(v0.begin(), ++(p_it = v0.begin()));
            VERIFY_ARE_EQUAL(v0.front(), 'a');
        }

        { // test added C++11 functionality
            container v0x;

            v0x.push_back('a');
            VERIFY_ARE_EQUAL(*v0x.data(), 'a');

            v0x.shrink_to_fit();
            VERIFY_ARE_EQUAL(*v0x.data(), 'a');
        }

        {
            std::initializer_list<char> init{ 'a', 'b', 'c' };
            container v11(init);
            VERIFY_ARE_EQUAL(v11.size(), 3u);
            VERIFY_ARE_EQUAL(v11[2], 'c');

            v11.clear();
            v11 = init;
            VERIFY_ARE_EQUAL(v11.size(), 3u);
            VERIFY_ARE_EQUAL(v11[2], 'c');

            v11.insert(v11.begin() + 1, init);
            VERIFY_ARE_EQUAL(v11.size(), 6u);
            VERIFY_ARE_EQUAL(v11[2], 'b');

            v11.clear();
            v11.insert(v11.begin(), init);
            VERIFY_ARE_EQUAL(v11.size(), 3u);
            VERIFY_ARE_EQUAL(v11[2], 'c');
        }

        v0.clear();
        VERIFY_IS_TRUE(v0.empty());
        std::swap(v0, v1);
        VERIFY_IS_FALSE(v0.empty());
        VERIFY_IS_TRUE(v1.empty());
        std::swap(v0, v1);
        VERIFY_IS_TRUE(v0.empty());
        VERIFY_IS_FALSE(v1.empty());
        VERIFY_ARE_EQUAL(v1, v1);
        VERIFY_ARE_NOT_EQUAL(v0, v1);
    }

    TEST_METHOD(InsertTrivialType)
    {
        til::small_vector<int, 5> actual{ { 0, 1, 2, 4 } };
        actual.insert(actual.end() - 1, 3, 3);
        actual.insert(actual.end(), 2, 5);
        actual.insert(actual.begin(), 2, -1);

        til::small_vector<int, 5> expected{ { -1, -1, 0, 1, 2, 3, 3, 3, 4, 5, 5 } };
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(CopyOntoItself)
    {
        til::small_vector<Copyable_int, 5> actual(3);
        actual.operator=(actual);

        const til::small_vector<Copyable_int, 5> expected(3);
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(MoveOntoItself)
    {
        til::small_vector<Movable_int, 5> actual;
        actual.resize(3);
        actual.operator=(std::move(actual));

        til::small_vector<Movable_int, 5> expected;
        expected.resize(3);
        VERIFY_ARE_EQUAL(expected, actual);
    }
};
