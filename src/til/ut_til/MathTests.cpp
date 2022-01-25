// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "til/math.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class MathTests
{
    TEST_CLASS(MathTests);

    using FloatType = double;
    using IntegralType = int;
    using TargetType = int;

    static constexpr auto nan = std::numeric_limits<FloatType>::quiet_NaN();
    static constexpr auto infinity = std::numeric_limits<FloatType>::infinity();

    template<typename TG>
    struct TestCase
    {
        TG given;
        TargetType expected;
        bool throws = false;
    };

    template<typename TG, typename TilMath>
    static void _RunCases(TilMath, const std::initializer_list<TestCase<TG>>& cases)
    {
        for (const auto& tc : cases)
        {
            if (tc.throws)
            {
                VERIFY_THROWS(TilMath::template cast<decltype(tc.expected)>(tc.given), gsl::narrowing_error);
            }
            else
            {
                VERIFY_ARE_EQUAL(tc.expected, TilMath::template cast<decltype(tc.expected)>(tc.given));
            }
        }
    }

    TEST_METHOD(Ceiling)
    {
        _RunCases<FloatType>(
            til::math::ceiling,
            {
                { 1., 1 },
                { 1.9, 2 },
                { -7.1, -7 },
                { -8.5, -8 },
                { INT_MAX - 0.1, INT_MAX },
                { INT_MIN - 0.1, INT_MIN },
                { INT_MAX + 1.1, 0, true },
                { INT_MIN - 1.1, 0, true },
                { infinity, 0, true },
                { -infinity, 0, true },
                { nan, 0, true },
            });
    }

    TEST_METHOD(Flooring)
    {
        _RunCases<FloatType>(
            til::math::flooring,
            {
                { 1., 1 },
                { 1.9, 1 },
                { -7.1, -8 },
                { -8.5, -9 },
                { INT_MAX + 0.1, INT_MAX },
                { INT_MIN + 0.1, INT_MIN },
                { INT_MAX + 1.1, 0, true },
                { INT_MIN - 1.1, 0, true },
                { infinity, 0, true },
                { -infinity, 0, true },
                { nan, 0, true },
            });
    }

    TEST_METHOD(Rounding)
    {
        _RunCases<FloatType>(
            til::math::rounding,
            {
                { 1., 1 },
                { 1.9, 2 },
                { -7.1, -7 },
                { -8.5, -9 },
                { INT_MAX + 0.1, INT_MAX },
                { INT_MIN - 0.1, INT_MIN },
                { INT_MAX + 1.1, 0, true },
                { INT_MIN - 1.1, 0, true },
                { infinity, 0, true },
                { -infinity, 0, true },
                { nan, 0, true },
            });
    }

    TEST_METHOD(NormalIntegers)
    {
        _RunCases<IntegralType>(
            til::math::rounding,
            {
                { 1, 1 },
                { -1, -1 },
                { INT_MAX, INT_MAX },
                { INT_MIN, INT_MIN },
            });
    }
};
