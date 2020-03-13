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
                const til::rectangle rc{ l, t, r, b };
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
            const til::rectangle rc{ til::point{ 4, 8 } };
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
                const til::rectangle rc{ til::point{ x, y } };
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"1.) Overflow y-dimension case.");
        {
            auto fn = [&]() {
                const ptrdiff_t x = 0;
                constexpr ptrdiff_t y = std::numeric_limits<ptrdiff_t>().max();
                const til::rectangle rc{ til::point{ x, y } };
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

        const til::rectangle rc{ til::point{ l, t }, til::point{ r, b } };
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
            const til::rectangle rc{ pt, til::size{ 2, 10 } };
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
                const til::rectangle rc{ pt, til::size{ x, y } };
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"1.) Overflow y-dimension case.");
        {
            auto fn = [&]() {
                const ptrdiff_t x = 0;
                constexpr ptrdiff_t y = std::numeric_limits<ptrdiff_t>().max();
                const til::rectangle rc{ pt, til::size{ x, y } };
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(SmallRectConstruct)
    {
        SMALL_RECT sr;
        sr.Left = 5;
        sr.Top = 10;
        sr.Right = 14;
        sr.Bottom = 19;

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
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:left", L"{0,10}")
            TEST_METHOD_PROPERTY(L"Data:top", L"{0,10}")
            TEST_METHOD_PROPERTY(L"Data:right", L"{0,10}")
            TEST_METHOD_PROPERTY(L"Data:bottom", L"{0,10}")
        END_TEST_METHOD_PROPERTIES()

        ptrdiff_t left, top, right, bottom;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"left", left));
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"top", top));
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"right", right));
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"bottom", bottom));

        const bool expected = left < right && top < bottom;
        const til::rectangle actual{ left, top, right, bottom };
        VERIFY_ARE_EQUAL(expected, (bool)actual);
    }

    TEST_METHOD(OrUnion)
    {
        const til::rectangle one{ 4, 6, 10, 14 };
        const til::rectangle two{ 5, 2, 13, 10 };

        const til::rectangle expected{ 4, 2, 13, 14 };
        const auto actual = one | two;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(AndIntersect)
    {
        const til::rectangle one{ 4, 6, 10, 14 };
        const til::rectangle two{ 5, 2, 13, 10 };

        const til::rectangle expected{ 5, 6, 10, 10 };
        const auto actual = one & two;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(MinusSubtractSame)
    {
        const til::rectangle original{ 0, 0, 10, 10 };
        const auto removal = original;

        // Since it's the same rectangle, nothing's left. We should get no results.
        const til::some<til::rectangle, 4> expected;
        const auto actual = original - removal;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(MinusSubtractNoOverlap)
    {
        const til::rectangle original{ 0, 0, 10, 10 };
        const til::rectangle removal{ 12, 12, 15, 15 };

        // Since they don't overlap, we expect the original to be given back.
        const til::some<til::rectangle, 4> expected{ original };
        const auto actual = original - removal;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(MinusSubtractOne)
    {
        //                +--------+
        //                | result |
        //                |        |
        //   +-------------------------------------+
        //   |            |        |               |
        //   |            |        |               |
        //   |            |original|               |
        //   |            |        |               |
        //   |            |        |               |
        //   |            +--------+               |
        //   |                                     |
        //   |                                     |
        //   |        removal                      |
        //   |                                     |
        //   +-------------------------------------+

        const til::rectangle original{ 0, 0, 10, 10 };
        const til::rectangle removal{ -12, 3, 15, 15 };

        const til::some<til::rectangle, 4> expected{
            til::rectangle{ original.left(), original.top(), original.right(), removal.top() }
        };
        const auto actual = original - removal;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(MinusSubtractTwo)
    {
        //    +--------+
        //    |result0 |
        //    |        |
        //    |~~~~+-----------------+
        //    |res1|   |             |
        //    |    |   |             |
        //    |original|             |
        //    |    |   |             |
        //    |    |   |             |
        //    +--------+             |
        //         |                 |
        //         |                 |
        //         |   removal       |
        //         +-----------------+

        const til::rectangle original{ 0, 0, 10, 10 };
        const til::rectangle removal{ 3, 3, 15, 15 };

        const til::some<til::rectangle, 4> expected{
            til::rectangle{ original.left(), original.top(), original.right(), removal.top() },
            til::rectangle{ original.left(), removal.top(), removal.left(), original.bottom() }
        };
        const auto actual = original - removal;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(MinusSubtractThree)
    {
        //    +--------+
        //    |result0 |
        //    |        |
        //    |~~~~+---------------------------+
        //    |res2|   |     removal           |
        //    |original|                       |
        //    |~~~~+---------------------------+
        //    |result1 |
        //    |        |
        //    +--------+

        const til::rectangle original{ 0, 0, 10, 10 };
        const til::rectangle removal{ 3, 3, 15, 6 };

        const til::some<til::rectangle, 4> expected{
            til::rectangle{ original.left(), original.top(), original.right(), removal.top() },
            til::rectangle{ original.left(), removal.bottom(), original.right(), original.bottom() },
            til::rectangle{ original.left(), removal.top(), removal.left(), removal.bottom() }
        };
        const auto actual = original - removal;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(MinusSubtractFour)
    {
        //     (original)---+
        //                  |
        //                  v
        //    + --------------------------+
        //    |         result0           |
        //    |   o         r         i   |
        //    |                           |
        //    |~~~~~~~+-----------+~~~~~~~|
        //    | res2  |           | res3  |
        //    |   g   |  removal  |   i   |
        //    |       |           |       |
        //    |~~~~~~~+-----------+~~~~~~~|
        //    |          result1          |
        //    |   n         a         l   |
        //    |                           |
        //    +---------------------------+

        const til::rectangle original{ 0, 0, 10, 10 };
        const til::rectangle removal{ 3, 3, 6, 6 };

        const til::some<til::rectangle, 4> expected{
            til::rectangle{ original.left(), original.top(), original.right(), removal.top() },
            til::rectangle{ original.left(), removal.bottom(), original.right(), original.bottom() },
            til::rectangle{ original.left(), removal.top(), removal.left(), removal.bottom() },
            til::rectangle{ removal.right(), removal.top(), original.right(), removal.bottom() }
        };
        const auto actual = original - removal;
        VERIFY_ARE_EQUAL(expected, actual);
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
        Log::Comment(L"0.) Width that should be in bounds.");
        {
            const til::rectangle rc{ 5, 10, 15, 20 };
            VERIFY_ARE_EQUAL(15 - 5, rc.width());
        }

        Log::Comment(L"1.) Width that should go out of bounds on subtraction.");
        {
            constexpr ptrdiff_t bigVal = std::numeric_limits<ptrdiff_t>().min();
            const ptrdiff_t normalVal = 5;
            const til::rectangle rc{ normalVal, normalVal, bigVal, normalVal };

            auto fn = [&]() {
                rc.width();
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(WidthCast)
    {
        const SHORT expected = 15 - 5;
        const til::rectangle rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(expected, rc.width<SHORT>());
    }

    TEST_METHOD(Height)
    {
        Log::Comment(L"0.) Height that should be in bounds.");
        {
            const til::rectangle rc{ 5, 10, 15, 20 };
            VERIFY_ARE_EQUAL(20 - 10, rc.height());
        }

        Log::Comment(L"1.) Height that should go out of bounds on subtraction.");
        {
            constexpr ptrdiff_t bigVal = std::numeric_limits<ptrdiff_t>().min();
            const ptrdiff_t normalVal = 5;
            const til::rectangle rc{ normalVal, normalVal, normalVal, bigVal };

            auto fn = [&]() {
                rc.height();
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(HeightCast)
    {
        const SHORT expected = 20 - 10;
        const til::rectangle rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(expected, rc.height<SHORT>());
    }

    TEST_METHOD(Origin)
    {
        const til::rectangle rc{ 5, 10, 15, 20 };
        const til::point expected{ rc._topLeft };
        VERIFY_ARE_EQUAL(expected, rc.origin());
    }

    TEST_METHOD(Size)
    {
        const til::rectangle rc{ 5, 10, 15, 20 };
        const til::size expected{ 10, 10 };
        VERIFY_ARE_EQUAL(expected, rc.size());
    }

    TEST_METHOD(Empty)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:left", L"{0,10}")
            TEST_METHOD_PROPERTY(L"Data:top", L"{0,10}")
            TEST_METHOD_PROPERTY(L"Data:right", L"{0,10}")
            TEST_METHOD_PROPERTY(L"Data:bottom", L"{0,10}")
        END_TEST_METHOD_PROPERTIES()

        ptrdiff_t left, top, right, bottom;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"left", left));
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"top", top));
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"right", right));
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"bottom", bottom));

        const bool expected = !(left < right && top < bottom);
        const til::rectangle actual{ left, top, right, bottom };
        VERIFY_ARE_EQUAL(expected, actual.empty());
    }

    TEST_METHOD(CastToSmallRect)
    {
        Log::Comment(L"0.) Typical situation.");
        {
            const til::rectangle rc{ 5, 10, 15, 20 };
            SMALL_RECT val = rc;
            VERIFY_ARE_EQUAL(5, val.Left);
            VERIFY_ARE_EQUAL(10, val.Top);
            VERIFY_ARE_EQUAL(14, val.Right);
            VERIFY_ARE_EQUAL(19, val.Bottom);
        }

        Log::Comment(L"1.) Overflow on left.");
        {
            constexpr ptrdiff_t l = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t t = 10;
            const ptrdiff_t r = 15;
            const ptrdiff_t b = 20;
            const til::rectangle rc{ l, t, r, b };

            auto fn = [&]() {
                SMALL_RECT val = rc;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Overflow on top.");
        {
            const ptrdiff_t l = 5;
            constexpr ptrdiff_t t = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t r = 15;
            const ptrdiff_t b = 20;
            const til::rectangle rc{ l, t, r, b };

            auto fn = [&]() {
                SMALL_RECT val = rc;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"3.) Overflow on right.");
        {
            const ptrdiff_t l = 5;
            const ptrdiff_t t = 10;
            constexpr ptrdiff_t r = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t b = 20;
            const til::rectangle rc{ l, t, r, b };

            auto fn = [&]() {
                SMALL_RECT val = rc;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"4.) Overflow on bottom.");
        {
            const ptrdiff_t l = 5;
            const ptrdiff_t t = 10;
            const ptrdiff_t r = 15;
            constexpr ptrdiff_t b = std::numeric_limits<ptrdiff_t>().max();
            const til::rectangle rc{ l, t, r, b };

            auto fn = [&]() {
                SMALL_RECT val = rc;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(CastToRect)
    {
        Log::Comment(L"0.) Typical situation.");
        {
            const til::rectangle rc{ 5, 10, 15, 20 };
            RECT val = rc;
            VERIFY_ARE_EQUAL(5, val.left);
            VERIFY_ARE_EQUAL(10, val.top);
            VERIFY_ARE_EQUAL(15, val.right);
            VERIFY_ARE_EQUAL(20, val.bottom);
        }

        Log::Comment(L"1.) Fit max left into RECT (may overflow).");
        {
            constexpr ptrdiff_t l = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t t = 10;
            const ptrdiff_t r = 15;
            const ptrdiff_t b = 20;
            const til::rectangle rc{ l, t, r, b };

            // On some platforms, ptrdiff_t will fit inside l/t/r/b
            const bool overflowExpected = l > std::numeric_limits<decltype(RECT::left)>().max();

            if (overflowExpected)
            {
                auto fn = [&]() {
                    RECT val = rc;
                };

                VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
            }
            else
            {
                RECT val = rc;
                VERIFY_ARE_EQUAL(l, val.left);
            }
        }

        Log::Comment(L"2.) Fit max top into RECT (may overflow).");
        {
            const ptrdiff_t l = 5;
            constexpr ptrdiff_t t = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t r = 15;
            const ptrdiff_t b = 20;
            const til::rectangle rc{ l, t, r, b };

            // On some platforms, ptrdiff_t will fit inside l/t/r/b
            const bool overflowExpected = t > std::numeric_limits<decltype(RECT::top)>().max();

            if (overflowExpected)
            {
                auto fn = [&]() {
                    RECT val = rc;
                };

                VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
            }
            else
            {
                RECT val = rc;
                VERIFY_ARE_EQUAL(t, val.top);
            }
        }

        Log::Comment(L"3.) Fit max right into RECT (may overflow).");
        {
            const ptrdiff_t l = 5;
            const ptrdiff_t t = 10;
            constexpr ptrdiff_t r = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t b = 20;
            const til::rectangle rc{ l, t, r, b };

            // On some platforms, ptrdiff_t will fit inside l/t/r/b
            const bool overflowExpected = r > std::numeric_limits<decltype(RECT::right)>().max();

            if (overflowExpected)
            {
                auto fn = [&]() {
                    RECT val = rc;
                };

                VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
            }
            else
            {
                RECT val = rc;
                VERIFY_ARE_EQUAL(r, val.right);
            }
        }

        Log::Comment(L"4.) Fit max bottom into RECT (may overflow).");
        {
            const ptrdiff_t l = 5;
            const ptrdiff_t t = 10;
            const ptrdiff_t r = 15;
            constexpr ptrdiff_t b = std::numeric_limits<ptrdiff_t>().max();
            const til::rectangle rc{ l, t, r, b };

            // On some platforms, ptrdiff_t will fit inside l/t/r/b
            const bool overflowExpected = b > std::numeric_limits<decltype(RECT::bottom)>().max();

            if (overflowExpected)
            {
                auto fn = [&]() {
                    RECT val = rc;
                };

                VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
            }
            else
            {
                RECT val = rc;
                VERIFY_ARE_EQUAL(b, val.bottom);
            }
        }
    }

    TEST_METHOD(CastToD2D1RectF)
    {
        Log::Comment(L"0.) Typical situation.");
        {
            const til::rectangle rc{ 5, 10, 15, 20 };
            D2D1_RECT_F val = rc;
            VERIFY_ARE_EQUAL(5, val.left);
            VERIFY_ARE_EQUAL(10, val.top);
            VERIFY_ARE_EQUAL(15, val.right);
            VERIFY_ARE_EQUAL(20, val.bottom);
        }

        // All ptrdiff_ts fit into a float, so there's no exception tests.
    }
};
