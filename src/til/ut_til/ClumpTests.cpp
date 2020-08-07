// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "til/clump.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class ClumpTests
{
    TEST_CLASS(ClumpTests);

    template<typename T, typename T2>
    static void _VerifyContainersEqual(const T& first, const T2& second)
    {
        VERIFY_ARE_EQUAL(gsl::span<const typename T::value_type>(first), gsl::span<const typename T2::value_type>(second));
    }

    static til::clump<int> _basicClump() // {1, 2}, {3}
    {
        static til::clump<int> val = []() {
            til::clump<int> c;
            c.emplace_back(1);
            c.emplace_glom(2);
            c.emplace_back(3);
            return c;
        }();
        return val; // copy
    }

    static til::clump<int> _basicUnitClump() // {1}, {2}, {3}
    {
        static til::clump<int> val = []() {
            til::clump<int> c;
            c.emplace_back(1);
            c.emplace_back(2);
            c.emplace_back(3);
            return c;
        }();
        return val; // copy
    }

    TEST_METHOD(EmptyState)
    {
        til::clump<int> c;

        // empty state check
        VERIFY_ARE_EQUAL(0, c.size());
        VERIFY_IS_TRUE(c.empty());
        VERIFY_ARE_EQUAL(c.begin(), c.end());
    }

    TEST_METHOD(Emplace)
    {
        til::clump<int> c;

        c.emplace_back(1);

        VERIFY_ARE_EQUAL(1, c.size());
        _VerifyContainersEqual(*c.begin(), std::vector<int>{ 1 });

        // Iterator makes sense
        VERIFY_ARE_EQUAL(++c.begin(), c.end());

        c.emplace_glom(2);
        VERIFY_ARE_EQUAL(1, c.size());
        _VerifyContainersEqual(*c.begin(), std::vector<int>{ 1, 2 });

        c.emplace_back(3);
        VERIFY_ARE_EQUAL(2, c.size());
        _VerifyContainersEqual(*c.begin(), std::vector<int>{ 1, 2 });
        _VerifyContainersEqual(*(++c.begin()), std::vector<int>{ 3 });
    }

    TEST_METHOD(Push)
    {
        til::clump<int> c;

        c.push_back(1);

        VERIFY_ARE_EQUAL(1, c.size());
        _VerifyContainersEqual(*c.begin(), std::vector<int>{ 1 });

        // Iterator makes sense
        VERIFY_ARE_EQUAL(++c.begin(), c.end());

        c.push_glom(2);
        VERIFY_ARE_EQUAL(1, c.size());
        _VerifyContainersEqual(*c.begin(), std::vector<int>{ 1, 2 });

        c.push_back(3);
        VERIFY_ARE_EQUAL(2, c.size());
        _VerifyContainersEqual(*c.begin(), std::vector<int>{ 1, 2 });
        _VerifyContainersEqual(*(++c.begin()), std::vector<int>{ 3 });
    }

    TEST_METHOD(Clear)
    {
        auto c{ _basicClump() };
        VERIFY_ARE_NOT_EQUAL(0, c.size());
        VERIFY_IS_FALSE(c.empty());
        VERIFY_ARE_NOT_EQUAL(c.begin(), c.end());

        c.clear();

        VERIFY_ARE_EQUAL(0, c.size());
        VERIFY_IS_TRUE(c.empty());
        VERIFY_ARE_EQUAL(c.begin(), c.end());
    }

    TEST_METHOD(GlomFirst)
    {
        til::clump<int> c;
        c.emplace_glom(1);

        VERIFY_ARE_EQUAL(1, c.size());
        _VerifyContainersEqual(*c.begin(), std::vector<int>{ 1 });
    }

    TEST_METHOD(Back)
    {
        til::clump<int> c;
        c.emplace_back(0);
        c.emplace_glom(0);
        c.back() = 10;

        VERIFY_ARE_EQUAL(1, c.size());
        _VerifyContainersEqual(*c.begin(), std::vector<int>{ 0, 10 });
    }

    TEST_METHOD(Iterator)
    {
        { // jagged clump
            auto c{ _basicClump() };
            const std::array<std::vector<int>, 2> expected{
                std::vector<int>{ 1, 2 },
                std::vector<int>{ 3 }
            };

            size_t i{ 0 };
            for (auto clumped : c)
            {
                _VerifyContainersEqual(clumped, expected[i]);
                ++i;
            }
            VERIFY_ARE_EQUAL(2, i);
        }
        { // unit clump
            auto c{ _basicUnitClump() };
            const std::array<std::vector<int>, 3> expected{
                std::vector<int>{ 1 },
                std::vector<int>{ 2 },
                std::vector<int>{ 3 }
            };

            size_t i{ 0 };
            for (auto clumped : c)
            {
                _VerifyContainersEqual(clumped, expected[i]);
                ++i;
            }
            VERIFY_ARE_EQUAL(3, i);
        }
    }
};
