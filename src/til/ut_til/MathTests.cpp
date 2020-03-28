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

    template<typename TG, typename TX = ptrdiff_t>
    struct TestCase
    {
        TG given;
        TX expected;
    };

    template<class TilMath, typename TG, typename TX, int N>
    static void _RunCases(TilMath, const std::array<TestCase<TG, TX>, N>& cases)
    {
        for (const auto& tc : cases)
        {
            VERIFY_ARE_EQUAL(tc.expected, TilMath::template cast<decltype(tc.expected)>(tc.given));
        }
    }

    TEST_METHOD(Truncating)
    {
        std::array<TestCase<long double, ptrdiff_t>, 8> cases{
            TestCase<long double, ptrdiff_t>{ 1., 1 },
            { 1.9, 1 },
            { -7.1, -7 },
            { -8.5, -8 },
            { PTRDIFF_MAX + 0.5, PTRDIFF_MAX },
            { PTRDIFF_MIN - 0.5, PTRDIFF_MIN },
            { INFINITY, PTRDIFF_MAX },
            { -INFINITY, PTRDIFF_MIN },
        };

        _RunCases(til::math::truncating, cases);

        const auto fn = []() {
            const auto v = til::math::details::truncating_t::cast<ptrdiff_t>(NAN);
        };
        VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
    }

    TEST_METHOD(Ceiling)
    {
        std::array<TestCase<long double, ptrdiff_t>, 8> cases{
            TestCase<long double, ptrdiff_t>{ 1., 1 },
            { 1.9, 2 },
            { -7.1, -7 },
            { -8.5, -8 },
            { PTRDIFF_MAX + 0.5, PTRDIFF_MAX },
            { PTRDIFF_MIN - 0.5, PTRDIFF_MIN },
            { INFINITY, PTRDIFF_MAX },
            { -INFINITY, PTRDIFF_MIN },
        };

        _RunCases(til::math::ceiling, cases);

        const auto fn = []() {
            const auto v = til::math::details::ceiling_t::cast<ptrdiff_t>(NAN);
        };
        VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
    }

    TEST_METHOD(Flooring)
    {
        std::array<TestCase<long double, ptrdiff_t>, 8> cases{
            TestCase<long double, ptrdiff_t>{ 1., 1 },
            { 1.9, 1 },
            { -7.1, -8 },
            { -8.5, -9 },
            { PTRDIFF_MAX + 0.5, PTRDIFF_MAX },
            { PTRDIFF_MIN - 0.5, PTRDIFF_MIN },
            { INFINITY, PTRDIFF_MAX },
            { -INFINITY, PTRDIFF_MIN },
        };

        _RunCases(til::math::flooring, cases);

        const auto fn = []() {
            const auto v = til::math::details::flooring_t::cast<ptrdiff_t>(NAN);
        };
        VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
    }

    TEST_METHOD(Rounding)
    {
        std::array<TestCase<long double, ptrdiff_t>, 8> cases{
            TestCase<long double, ptrdiff_t>{ 1., 1 },
            { 1.9, 2 },
            { -7.1, -7 },
            { -8.5, -9 },
            { PTRDIFF_MAX + 0.5, PTRDIFF_MAX },
            { PTRDIFF_MIN - 0.5, PTRDIFF_MIN },
            { INFINITY, PTRDIFF_MAX },
            { -INFINITY, PTRDIFF_MIN },
        };

        _RunCases(til::math::rounding, cases);

        const auto fn = []() {
            const auto v = til::math::details::rounding_t::cast<ptrdiff_t>(NAN);
        };
        VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
    }

    TEST_METHOD(NormalIntegers)
    {
        std::array<TestCase<ptrdiff_t, int>, 4> cases{
            TestCase<ptrdiff_t, int>{ 1, 1 },
            { -1, -1 },
            { PTRDIFF_MAX, INT_MAX },
            { PTRDIFF_MIN, INT_MIN },
        };

        _RunCases(til::math::rounding, cases);
    }
};
