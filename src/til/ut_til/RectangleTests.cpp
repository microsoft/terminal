// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "til/rectangle.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class RectangleTests
{
    TEST_CLASS(RectangleTests);

    TEST_METHOD(DefaultConstruct)
    {
        const til::rectangle rc;
        VERIFY_ARE_EQUAL(0, rc._topLeft.x());
        VERIFY_ARE_EQUAL(0, rc._topLeft.y());
        VERIFY_ARE_EQUAL(0, rc._bottomRight.x());
        VERIFY_ARE_EQUAL(0, rc._bottomRight.y());
    }

    TEST_METHOD(RawConstruct)
    {
        const til::rectangle rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(5, rc._topLeft.x());
        VERIFY_ARE_EQUAL(10, rc._topLeft.y());
        VERIFY_ARE_EQUAL(15, rc._bottomRight.x());
        VERIFY_ARE_EQUAL(20, rc._bottomRight.y());
    }

    TEST_METHOD(UnsignedConstruct)
    {
        Log::Comment(L"0.) Normal unsigned construct.");
        {
            const size_t l = 5;
            const size_t t = 10;
            const size_t r = 15;
            const size_t b = 20;

            const til::rectangle rc{ l, t, r, b };
            VERIFY_ARE_EQUAL(5, rc._topLeft.x());
            VERIFY_ARE_EQUAL(10, rc._topLeft.y());
            VERIFY_ARE_EQUAL(15, rc._bottomRight.x());
            VERIFY_ARE_EQUAL(20, rc._bottomRight.y());
        }

        Log::Comment(L"1.) Unsigned construct overflow on left.");
        {
            constexpr size_t l = std::numeric_limits<size_t>().max();
            const size_t t = 10;
            const size_t r = 15;
            const size_t b = 20;

            auto fn = [&]() {
                til::rectangle rc{ l, t, r, b};
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Unsigned construct overflow on top.");
        {
            const size_t l = 5;
            constexpr size_t t = std::numeric_limits<size_t>().max();
            const size_t r = 15;
            const size_t b = 20;

            auto fn = [&]() {
                til::rectangle rc{ l, t, r, b };
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"3.) Unsigned construct overflow on right.");
        {
            const size_t l = 5;
            const size_t t = 10;
            constexpr size_t r = std::numeric_limits<size_t>().max();
            const size_t b = 20;

            auto fn = [&]() {
                til::rectangle rc{ l, t, r, b };
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"4.) Unsigned construct overflow on bottom.");
        {
            const size_t l = 5;
            const size_t t = 10;
            const size_t r = 15;
            constexpr size_t b = std::numeric_limits<size_t>().max();

            auto fn = [&]() {
                til::rectangle rc{ l, t, r, b };
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(SignedConstruct)
    {
        const ptrdiff_t l = 5;
        const ptrdiff_t t = 10;
        const ptrdiff_t r = 15;
        const ptrdiff_t b = 20;

        const til::rectangle rc{ l, t, r, b };
        VERIFY_ARE_EQUAL(5, rc._topLeft.x());
        VERIFY_ARE_EQUAL(10, rc._topLeft.y());
        VERIFY_ARE_EQUAL(15, rc._bottomRight.x());
        VERIFY_ARE_EQUAL(20, rc._bottomRight.y());
    }

    TEST_METHOD(SinglePointConstruct)
    {
        Log::Comment(L"0.) Normal Case");
        {
            const til::rectangle rc{ til::point{4, 8} };
            VERIFY_ARE_EQUAL(4, rc._topLeft.x());
            VERIFY_ARE_EQUAL(8, rc._topLeft.y());
            VERIFY_ARE_EQUAL(5, rc._bottomRight.x());
            VERIFY_ARE_EQUAL(9, rc._bottomRight.y());
        }

        Log::Comment(L"1.) Overflow x-dimension case.");
        {
            auto fn = [&]() {
                constexpr ptrdiff_t x = std::numeric_limits<ptrdiff_t>().max();
                const ptrdiff_t y = 0;
                til::rectangle rc{ til::point{x, y} };
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"1.) Overflow y-dimension case.");
        {
            auto fn = [&]() {
                const ptrdiff_t x = 0;
                constexpr ptrdiff_t y = std::numeric_limits<ptrdiff_t>().max();
                til::rectangle rc{ til::point{x, y} };
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(TwoPointsConstruct)
    {
        const ptrdiff_t l = 5;
        const ptrdiff_t t = 10;
        const ptrdiff_t r = 15;
        const ptrdiff_t b = 20;

        const til::rectangle rc{ til::point{l, t}, til::point{r, b} };
        VERIFY_ARE_EQUAL(5, rc._topLeft.x());
        VERIFY_ARE_EQUAL(10, rc._topLeft.y());
        VERIFY_ARE_EQUAL(15, rc._bottomRight.x());
        VERIFY_ARE_EQUAL(20, rc._bottomRight.y());
    }

    TEST_METHOD(SizeOnlyConstruct)
    {
        // Size will match bottom right point because
        // til::rectangle is exclusive.
        const auto sz = til::size{ 5, 10 };
        const til::rectangle rc{ sz };
        VERIFY_ARE_EQUAL(0, rc._topLeft.x());
        VERIFY_ARE_EQUAL(0, rc._topLeft.y());
        VERIFY_ARE_EQUAL(sz.width(), rc._bottomRight.x()); 
        VERIFY_ARE_EQUAL(sz.height(), rc._bottomRight.y());
    }

    TEST_METHOD(PointAndSizeConstruct)
    {

    }

    TEST_METHOD(SmallRectConstruct)
    {

    }

    TEST_METHOD(ExclusiveCapitalStructConstruct)
    {

    }

    TEST_METHOD(Win32RectConstruct)
    {

    }

    TEST_METHOD(Assignment)
    {

    }

    TEST_METHOD(Equality)
    {

    }

    TEST_METHOD(Inequality)
    {

    }

    TEST_METHOD(Boolean)
    {

    }

    TEST_METHOD(OrUnion)
    {

    }

    TEST_METHOD(AndIntersect)
    {

    }

    TEST_METHOD(MinusSubtract)
    {

    }

    TEST_METHOD(Top)
    {

    }

    TEST_METHOD(TopCast)
    {

    }

    TEST_METHOD(Bottom)
    {

    }

    TEST_METHOD(BottomCast)
    {

    }

    TEST_METHOD(Left)
    {

    }

    TEST_METHOD(LeftCast)
    {

    }

    TEST_METHOD(Right)
    {

    }

    TEST_METHOD(RightCast)
    {

    }

    TEST_METHOD(Width)
    {

    }

    TEST_METHOD(WidthCast)
    {

    }

    TEST_METHOD(Height)
    {

    }

    TEST_METHOD(HeightCast)
    {

    }

    TEST_METHOD(Origin)
    {

    }

    TEST_METHOD(Size)
    {

    }

    TEST_METHOD(Empty)
    {

    }

    TEST_METHOD(CastToSmallRect)
    {

    }

    TEST_METHOD(CastToRect)
    {

    }

    TEST_METHOD(CastToD2D1RectF)
    {

    }
};
