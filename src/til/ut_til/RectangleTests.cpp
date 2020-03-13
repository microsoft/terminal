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
                const til::rectangle rc{ l, t, r, b};
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
                const til::rectangle rc{ l, t, r, b };
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
                const til::rectangle rc{ l, t, r, b };
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
                const til::rectangle rc{ l, t, r, b };
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
                const til::rectangle rc{ til::point{x, y} };
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"1.) Overflow y-dimension case.");
        {
            auto fn = [&]() {
                const ptrdiff_t x = 0;
                constexpr ptrdiff_t y = std::numeric_limits<ptrdiff_t>().max();
                const til::rectangle rc{ til::point{x, y} };
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
        const til::point pt{ 4, 8 };

        Log::Comment(L"0.) Normal Case");
        {
            const til::rectangle rc{pt, til::size{2, 10} };
            VERIFY_ARE_EQUAL(4, rc._topLeft.x());
            VERIFY_ARE_EQUAL(8, rc._topLeft.y());
            VERIFY_ARE_EQUAL(6, rc._bottomRight.x());
            VERIFY_ARE_EQUAL(18, rc._bottomRight.y());
        }

        Log::Comment(L"1.) Overflow x-dimension case.");
        {
            auto fn = [&]() {
                constexpr ptrdiff_t x = std::numeric_limits<ptrdiff_t>().max();
                const ptrdiff_t y = 0;
                const til::rectangle rc{ pt, til::size{x, y} };
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"1.) Overflow y-dimension case.");
        {
            auto fn = [&]() {
                const ptrdiff_t x = 0;
                constexpr ptrdiff_t y = std::numeric_limits<ptrdiff_t>().max();
                const til::rectangle rc{ pt, til::size{x, y} };
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(SmallRectConstruct)
    {
        SMALL_RECT sr;
        sr.Top = 5;
        sr.Left = 10;
        sr.Bottom = 14;
        sr.Right = 19;

        const til::rectangle rc{ sr };
        VERIFY_ARE_EQUAL(5, rc._topLeft.x());
        VERIFY_ARE_EQUAL(10, rc._topLeft.y());
        VERIFY_ARE_EQUAL(15, rc._bottomRight.x());
        VERIFY_ARE_EQUAL(20, rc._bottomRight.y());
    }

    TEST_METHOD(ExclusiveCapitalStructConstruct)
    {
        struct TestStruct
        {
            char Left;
            char Top;
            char Right;
            char Bottom;
        };

        const TestStruct ts{ 1, 2, 3, 4 };

        const til::rectangle rc{ ts };

        VERIFY_ARE_EQUAL(1, rc._topLeft.x());
        VERIFY_ARE_EQUAL(2, rc._topLeft.y());
        VERIFY_ARE_EQUAL(3, rc._bottomRight.x());
        VERIFY_ARE_EQUAL(4, rc._bottomRight.y());
    }

    TEST_METHOD(Win32RectConstruct)
    {
        const RECT win32rc{ 5, 10, 15, 20 };
        const til::rectangle rc{ win32rc };

        VERIFY_ARE_EQUAL(5, rc._topLeft.x());
        VERIFY_ARE_EQUAL(10, rc._topLeft.y());
        VERIFY_ARE_EQUAL(15, rc._bottomRight.x());
        VERIFY_ARE_EQUAL(20, rc._bottomRight.y());
    }

    TEST_METHOD(Assignment)
    {
        til::rectangle a{ 1, 2, 3, 4 };
        const til::rectangle b{ 5, 6, 7, 8 };

        VERIFY_ARE_EQUAL(1, a._topLeft.x());
        VERIFY_ARE_EQUAL(2, a._topLeft.y());
        VERIFY_ARE_EQUAL(3, a._bottomRight.x());
        VERIFY_ARE_EQUAL(4, a._bottomRight.y());

        a = b;

        VERIFY_ARE_EQUAL(5, a._topLeft.x());
        VERIFY_ARE_EQUAL(6, a._topLeft.y());
        VERIFY_ARE_EQUAL(7, a._bottomRight.x());
        VERIFY_ARE_EQUAL(8, a._bottomRight.y());
    }

    TEST_METHOD(Equality)
    {
        Log::Comment(L"0.) Equal.");
        {
            const til::rectangle a{ 1, 2, 3, 4 };
            const til::rectangle b{ 1, 2, 3, 4 };
            VERIFY_IS_TRUE(a == b);
        }

        Log::Comment(L"1.) Left A changed.");
        {
            const til::rectangle a{ 9, 2, 3, 4 };
            const til::rectangle b{ 1, 2, 3, 4 };
            VERIFY_IS_FALSE(a == b);
        }

        Log::Comment(L"2.) Top A changed.");
        {
            const til::rectangle a{ 1, 9, 3, 4 };
            const til::rectangle b{ 1, 2, 3, 4 };
            VERIFY_IS_FALSE(a == b);
        }

        Log::Comment(L"3.) Right A changed.");
        {
            const til::rectangle a{ 1, 2, 9, 4 };
            const til::rectangle b{ 1, 2, 3, 4 };
            VERIFY_IS_FALSE(a == b);
        }

        Log::Comment(L"4.) Bottom A changed.");
        {
            const til::rectangle a{ 1, 2, 3, 9 };
            const til::rectangle b{ 1, 2, 3, 4 };
            VERIFY_IS_FALSE(a == b);
        }

        Log::Comment(L"5.) Left B changed.");
        {
            const til::rectangle a{ 1, 2, 3, 4 };
            const til::rectangle b{ 9, 2, 3, 4 };
            VERIFY_IS_FALSE(a == b);
        }

        Log::Comment(L"6.) Top B changed.");
        {
            const til::rectangle a{ 1, 2, 3, 4 };
            const til::rectangle b{ 1, 9, 3, 4 };
            VERIFY_IS_FALSE(a == b);
        }

        Log::Comment(L"7.) Right B changed.");
        {
            const til::rectangle a{ 1, 2, 3, 4 };
            const til::rectangle b{ 1, 2, 9, 4 };
            VERIFY_IS_FALSE(a == b);
        }

        Log::Comment(L"8.) Bottom B changed.");
        {
            const til::rectangle a{ 1, 2, 3, 4 };
            const til::rectangle b{ 1, 2, 3, 9 };
            VERIFY_IS_FALSE(a == b);
        }
    }

    TEST_METHOD(Inequality)
    {
        Log::Comment(L"0.) Equal.");
        {
            const til::rectangle a{ 1, 2, 3, 4 };
            const til::rectangle b{ 1, 2, 3, 4 };
            VERIFY_IS_FALSE(a != b);
        }

        Log::Comment(L"1.) Left A changed.");
        {
            const til::rectangle a{ 9, 2, 3, 4 };
            const til::rectangle b{ 1, 2, 3, 4 };
            VERIFY_IS_TRUE(a != b);
        }

        Log::Comment(L"2.) Top A changed.");
        {
            const til::rectangle a{ 1, 9, 3, 4 };
            const til::rectangle b{ 1, 2, 3, 4 };
            VERIFY_IS_TRUE(a != b);
        }

        Log::Comment(L"3.) Right A changed.");
        {
            const til::rectangle a{ 1, 2, 9, 4 };
            const til::rectangle b{ 1, 2, 3, 4 };
            VERIFY_IS_TRUE(a != b);
        }

        Log::Comment(L"4.) Bottom A changed.");
        {
            const til::rectangle a{ 1, 2, 3, 9 };
            const til::rectangle b{ 1, 2, 3, 4 };
            VERIFY_IS_TRUE(a != b);
        }

        Log::Comment(L"5.) Left B changed.");
        {
            const til::rectangle a{ 1, 2, 3, 4 };
            const til::rectangle b{ 9, 2, 3, 4 };
            VERIFY_IS_TRUE(a != b);
        }

        Log::Comment(L"6.) Top B changed.");
        {
            const til::rectangle a{ 1, 2, 3, 4 };
            const til::rectangle b{ 1, 9, 3, 4 };
            VERIFY_IS_TRUE(a != b);
        }

        Log::Comment(L"7.) Right B changed.");
        {
            const til::rectangle a{ 1, 2, 3, 4 };
            const til::rectangle b{ 1, 2, 9, 4 };
            VERIFY_IS_TRUE(a != b);
        }

        Log::Comment(L"8.) Bottom B changed.");
        {
            const til::rectangle a{ 1, 2, 3, 4 };
            const til::rectangle b{ 1, 2, 3, 9 };
            VERIFY_IS_TRUE(a != b);
        }
    }

    TEST_METHOD(Boolean)
    {
        const til::rectangle empty;
        VERIFY_IS_FALSE(empty);
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
        const til::rectangle rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(rc._topLeft.y(), rc.top());
    }

    TEST_METHOD(TopCast)
    {
        const til::rectangle rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(static_cast<SHORT>(rc._topLeft.y()), rc.top<SHORT>());
    }

    TEST_METHOD(Bottom)
    {
        const til::rectangle rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(rc._bottomRight.y(), rc.bottom());
    }

    TEST_METHOD(BottomCast)
    {
        const til::rectangle rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(static_cast<SHORT>(rc._bottomRight.y()), rc.bottom<SHORT>());
    }

    TEST_METHOD(Left)
    {
        const til::rectangle rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(rc._topLeft.x(), rc.left());
    }

    TEST_METHOD(LeftCast)
    {
        const til::rectangle rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(static_cast<SHORT>(rc._topLeft.x()), rc.left<SHORT>());
    }

    TEST_METHOD(Right)
    {
        const til::rectangle rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(rc._bottomRight.x(), rc.right());
    }

    TEST_METHOD(RightCast)
    {
        const til::rectangle rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(static_cast<SHORT>(rc._bottomRight.x()), rc.right<SHORT>());
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
