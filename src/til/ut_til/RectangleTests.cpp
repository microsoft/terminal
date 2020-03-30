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

    TEST_METHOD(OrUnionInplace)
    {
        til::rectangle one{ 4, 6, 10, 14 };
        const til::rectangle two{ 5, 2, 13, 10 };

        const til::rectangle expected{ 4, 2, 13, 14 };
        one |= two;
        VERIFY_ARE_EQUAL(expected, one);
    }

    TEST_METHOD(AndIntersect)
    {
        const til::rectangle one{ 4, 6, 10, 14 };
        const til::rectangle two{ 5, 2, 13, 10 };

        const til::rectangle expected{ 5, 6, 10, 10 };
        const auto actual = one & two;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(AndIntersectInplace)
    {
        til::rectangle one{ 4, 6, 10, 14 };
        const til::rectangle two{ 5, 2, 13, 10 };

        const til::rectangle expected{ 5, 6, 10, 10 };
        one &= two;
        VERIFY_ARE_EQUAL(expected, one);
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

    TEST_METHOD(AdditionPoint)
    {
        const til::rectangle start{ 10, 20, 30, 40 };
        const til::point pt{ 3, 7 };
        const til::rectangle expected{ 10 + 3, 20 + 7, 30 + 3, 40 + 7 };
        const auto actual = start + pt;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(AdditionPointInplace)
    {
        til::rectangle start{ 10, 20, 30, 40 };
        const til::point pt{ 3, 7 };
        const til::rectangle expected{ 10 + 3, 20 + 7, 30 + 3, 40 + 7 };
        start += pt;
        VERIFY_ARE_EQUAL(expected, start);
    }

    TEST_METHOD(SubtractionPoint)
    {
        const til::rectangle start{ 10, 20, 30, 40 };
        const til::point pt{ 3, 7 };
        const til::rectangle expected{ 10 - 3, 20 - 7, 30 - 3, 40 - 7 };
        const auto actual = start - pt;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(SubtractionPointInplace)
    {
        til::rectangle start{ 10, 20, 30, 40 };
        const til::point pt{ 3, 7 };
        const til::rectangle expected{ 10 - 3, 20 - 7, 30 - 3, 40 - 7 };
        start -= pt;
        VERIFY_ARE_EQUAL(expected, start);
    }

    TEST_METHOD(AdditionSize)
    {
        const til::rectangle start{ 10, 20, 30, 40 };

        Log::Comment(L"1.) Add size to bottom and right");
        {
            const til::size scale{ 3, 7 };
            const til::rectangle expected{ 10, 20, 33, 47 };
            const auto actual = start + scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"2.) Add size to top and left");
        {
            const til::size scale{ -3, -7 };
            const til::rectangle expected{ 7, 13, 30, 40 };
            const auto actual = start + scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"3.) Add size to bottom and left");
        {
            const til::size scale{ -3, 7 };
            const til::rectangle expected{ 7, 20, 30, 47 };
            const auto actual = start + scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"4.) Add size to top and right");
        {
            const til::size scale{ 3, -7 };
            const til::rectangle expected{ 10, 13, 33, 40 };
            const auto actual = start + scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }
    }

    TEST_METHOD(AdditionSizeInplace)
    {
        const til::rectangle start{ 10, 20, 30, 40 };

        Log::Comment(L"1.) Add size to bottom and right");
        {
            auto actual = start;
            const til::size scale{ 3, 7 };
            const til::rectangle expected{ 10, 20, 33, 47 };
            actual += scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"2.) Add size to top and left");
        {
            auto actual = start;
            const til::size scale{ -3, -7 };
            const til::rectangle expected{ 7, 13, 30, 40 };
            actual += scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"3.) Add size to bottom and left");
        {
            auto actual = start;
            const til::size scale{ -3, 7 };
            const til::rectangle expected{ 7, 20, 30, 47 };
            actual += scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"4.) Add size to top and right");
        {
            auto actual = start;
            const til::size scale{ 3, -7 };
            const til::rectangle expected{ 10, 13, 33, 40 };
            actual += scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }
    }

    TEST_METHOD(SubtractionSize)
    {
        const til::rectangle start{ 10, 20, 30, 40 };

        Log::Comment(L"1.) Subtract size from bottom and right");
        {
            const til::size scale{ 3, 7 };
            const til::rectangle expected{ 10, 20, 27, 33 };
            const auto actual = start - scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"2.) Subtract size from top and left");
        {
            const til::size scale{ -3, -7 };
            const til::rectangle expected{ 13, 27, 30, 40 };
            const auto actual = start - scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"3.) Subtract size from bottom and left");
        {
            const til::size scale{ -3, 7 };
            const til::rectangle expected{ 13, 20, 30, 33 };
            const auto actual = start - scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"4.) Subtract size from top and right");
        {
            const til::size scale{ 3, -6 };
            const til::rectangle expected{ 10, 26, 27, 40 };
            const auto actual = start - scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }
    }

    TEST_METHOD(SubtractionSizeInplace)
    {
        const til::rectangle start{ 10, 20, 30, 40 };

        Log::Comment(L"1.) Subtract size from bottom and right");
        {
            auto actual = start;
            const til::size scale{ 3, 7 };
            const til::rectangle expected{ 10, 20, 27, 33 };
            actual -= scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"2.) Subtract size from top and left");
        {
            auto actual = start;
            const til::size scale{ -3, -7 };
            const til::rectangle expected{ 13, 27, 30, 40 };
            actual -= scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"3.) Subtract size from bottom and left");
        {
            auto actual = start;
            const til::size scale{ -3, 7 };
            const til::rectangle expected{ 13, 20, 30, 33 };
            actual -= scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"4.) Subtract size from top and right");
        {
            auto actual = start;
            const til::size scale{ 3, -6 };
            const til::rectangle expected{ 10, 26, 27, 40 };
            actual -= scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }
    }

    TEST_METHOD(MultiplicationSize)
    {
        const til::rectangle start{ 10, 20, 30, 40 };

        Log::Comment(L"1.) Multiply by size to scale from cells to pixels");
        {
            const til::size scale{ 3, 7 };
            const til::rectangle expected{ 10 * 3, 20 * 7, 30 * 3, 40 * 7 };
            const auto actual = start * scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"2.) Multiply by size with width way too big.");
        {
            const til::size scale{ std::numeric_limits<ptrdiff_t>().max(), static_cast<ptrdiff_t>(7) };

            auto fn = [&]() {
                const auto actual = start * scale;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"3.) Multiply by size with height way too big.");
        {
            const til::size scale{ static_cast<ptrdiff_t>(3), std::numeric_limits<ptrdiff_t>().max() };

            auto fn = [&]() {
                const auto actual = start * scale;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(MultiplicationSizeInplace)
    {
        const til::rectangle start{ 10, 20, 30, 40 };

        Log::Comment(L"1.) Multiply by size to scale from cells to pixels");
        {
            const til::size scale{ 3, 7 };
            const til::rectangle expected{ 10 * 3, 20 * 7, 30 * 3, 40 * 7 };
            auto actual = start;
            actual *= scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"2.) Multiply by size with width way too big.");
        {
            const til::size scale{ std::numeric_limits<ptrdiff_t>().max(), static_cast<ptrdiff_t>(7) };

            auto fn = [&]() {
                auto actual = start;
                actual *= scale;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"3.) Multiply by size with height way too big.");
        {
            const til::size scale{ static_cast<ptrdiff_t>(3), std::numeric_limits<ptrdiff_t>().max() };

            auto fn = [&]() {
                auto actual = start;
                actual *= scale;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(DivisionSize)
    {
        const til::rectangle start{ 10, 20, 29, 40 };

        Log::Comment(L"0.) Division by size to scale from pixels to cells");
        {
            const til::size scale{ 3, 7 };

            // Division is special. The top and left round down.
            // The bottom and right round up. This is to ensure that the cells
            // the smaller rectangle represents fully cover all the pixels
            // of the larger rectangle.
            // L: 10 / 3 = 3.333 --> round down --> 3
            // T: 20 / 7 = 2.857 --> round down --> 2
            // R: 29 / 3 = 9.667 --> round up ----> 10
            // B: 40 / 7 = 5.714 --> round up ----> 6
            const til::rectangle expected{ 3, 2, 10, 6 };
            const auto actual = start / scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }
    }

    TEST_METHOD(DivisionSizeInplace)
    {
        const til::rectangle start{ 10, 20, 29, 40 };

        Log::Comment(L"0.) Division by size to scale from pixels to cells");
        {
            const til::size scale{ 3, 7 };

            // Division is special. The top and left round down.
            // The bottom and right round up. This is to ensure that the cells
            // the smaller rectangle represents fully cover all the pixels
            // of the larger rectangle.
            // L: 10 / 3 = 3.333 --> round down --> 3
            // T: 20 / 7 = 2.857 --> round down --> 2
            // R: 29 / 3 = 9.667 --> round up ----> 10
            // B: 40 / 7 = 5.714 --> round up ----> 6
            const til::rectangle expected{ 3, 2, 10, 6 };
            auto actual = start;
            actual /= scale;
            VERIFY_ARE_EQUAL(expected, actual);
        }
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

    TEST_METHOD(ContainsPoint)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:x", L"{-1000,0,4,5,6,14,15,16,1000}")
            TEST_METHOD_PROPERTY(L"Data:y", L"{-1000,0,9,10,11,19,20,21,1000}")
        END_TEST_METHOD_PROPERTIES()

        ptrdiff_t x, y;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"x", x));
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"y", y));

        const til::rectangle rc{ 5, 10, 15, 20 };
        const til::point pt{ x, y };

        const bool xInBounds = x >= 5 && x < 15;
        const bool yInBounds = y >= 10 && y < 20;
        const bool expected = xInBounds && yInBounds;
        if (expected)
        {
            Log::Comment(L"Expected in bounds.");
        }
        else
        {
            Log::Comment(L"Expected OUT of bounds.");
        }

        VERIFY_ARE_EQUAL(expected, rc.contains(pt));
    }

    TEST_METHOD(ContainsIndex)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:idx", L"{-1000,-1,0, 1,50,99,100,101, 1000}")
        END_TEST_METHOD_PROPERTIES()

        ptrdiff_t idx;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"idx", idx));

        const til::rectangle rc{ 5, 10, 15, 20 }; // 10x10 rectangle.
        const ptrdiff_t area = (15 - 5) * (20 - 10);
        const bool expected = idx >= 0 && idx < area;
        if (expected)
        {
            Log::Comment(L"Expected in bounds.");
        }
        else
        {
            Log::Comment(L"Expected OUT of bounds.");
        }

        VERIFY_ARE_EQUAL(expected, rc.contains(idx));
    }

    TEST_METHOD(ContainsRectangle)
    {
        const til::rectangle rc{ 5, 10, 15, 20 }; // 10x10 rectangle.

        const til::rectangle fitsInside{ 8, 12, 10, 18 };
        const til::rectangle spillsOut{ 0, 0, 50, 50 };
        const til::rectangle sticksOut{ 14, 12, 30, 13 };

        VERIFY_IS_TRUE(rc.contains(rc), L"We contain ourself.");
        VERIFY_IS_TRUE(rc.contains(fitsInside), L"We fully contain a smaller rectangle.");
        VERIFY_IS_FALSE(rc.contains(spillsOut), L"We do not fully contain rectangle larger than us.");
        VERIFY_IS_FALSE(rc.contains(sticksOut), L"We do not contain a rectangle that is smaller, but sticks out our edge.");
    }

    TEST_METHOD(IndexOfPoint)
    {
        const til::rectangle rc{ 5, 10, 15, 20 };

        Log::Comment(L"0.) Normal in bounds.");
        {
            const til::point pt{ 7, 17 };
            const ptrdiff_t expected = 72;
            VERIFY_ARE_EQUAL(expected, rc.index_of(pt));
        }

        Log::Comment(L"1.) Out of bounds.");
        {
            auto fn = [&]() {
                const til::point pt{ 1, 1 };
                rc.index_of(pt);
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_INVALIDARG; });
        }

        Log::Comment(L"2.) Overflow.");
        {
            auto fn = [&]() {
                constexpr const ptrdiff_t min = static_cast<ptrdiff_t>(0);
                constexpr const ptrdiff_t max = std::numeric_limits<ptrdiff_t>().max();
                const til::rectangle bigRc{ min, min, max, max };
                const til::point pt{ max - 1, max - 1 };
                bigRc.index_of(pt);
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(PointAtIndex)
    {
        const til::rectangle rc{ 5, 10, 15, 20 };

        Log::Comment(L"0.) Normal in bounds.");
        {
            const ptrdiff_t index = 72;
            const til::point expected{ 7, 17 };

            VERIFY_ARE_EQUAL(expected, rc.point_at(index));
        }

        Log::Comment(L"1.) Out of bounds too low.");
        {
            auto fn = [&]() {
                const ptrdiff_t index = -1;
                rc.point_at(index);
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_INVALIDARG; });
        }

        Log::Comment(L"2.) Out of bounds too high.");
        {
            auto fn = [&]() {
                const ptrdiff_t index = 1000;
                rc.point_at(index);
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_INVALIDARG; });
        }
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

#pragma region iterator
    TEST_METHOD(Begin)
    {
        const til::rectangle rc{ 5, 10, 15, 20 };
        const til::point expected{ rc.left(), rc.top() };
        const auto it = rc.begin();

        VERIFY_ARE_EQUAL(expected, *it);
    }

    TEST_METHOD(End)
    {
        const til::rectangle rc{ 5, 10, 15, 20 };
        const til::point expected{ rc.left(), rc.bottom() };
        const auto it = rc.end();

        VERIFY_ARE_EQUAL(expected, *it);
    }

    TEST_METHOD(ConstIteratorIncrement)
    {
        const til::rectangle rc{ til::size{ 2, 2 } };

        auto it = rc.begin();
        auto expected = til::point{ 0, 0 };
        VERIFY_ARE_EQUAL(expected, *it);

        ++it;
        expected = til::point{ 1, 0 };
        VERIFY_ARE_EQUAL(expected, *it);

        ++it;
        expected = til::point{ 0, 1 };
        VERIFY_ARE_EQUAL(expected, *it);

        ++it;
        expected = til::point{ 1, 1 };
        VERIFY_ARE_EQUAL(expected, *it);

        ++it;
        expected = til::point{ 0, 2 };
        VERIFY_ARE_EQUAL(expected, *it);
        VERIFY_ARE_EQUAL(expected, *rc.end());

        // We wouldn't normally walk one past, but validate it keeps going
        // like any STL iterator would.
        ++it;
        expected = til::point{ 1, 2 };
        VERIFY_ARE_EQUAL(expected, *it);
    }

    TEST_METHOD(ConstIteratorEquality)
    {
        const til::rectangle rc{ 5, 10, 15, 20 };

        VERIFY_IS_TRUE(rc.begin() == rc.begin());
        VERIFY_IS_FALSE(rc.begin() == rc.end());
    }

    TEST_METHOD(ConstIteratorInequality)
    {
        const til::rectangle rc{ 5, 10, 15, 20 };

        VERIFY_IS_FALSE(rc.begin() != rc.begin());
        VERIFY_IS_TRUE(rc.begin() != rc.end());
    }

    TEST_METHOD(ConstIteratorLessThan)
    {
        const til::rectangle rc{ 5, 10, 15, 20 };

        VERIFY_IS_TRUE(rc.begin() < rc.end());
        VERIFY_IS_FALSE(rc.end() < rc.begin());
    }

    TEST_METHOD(ConstIteratorGreaterThan)
    {
        const til::rectangle rc{ 5, 10, 15, 20 };

        VERIFY_IS_TRUE(rc.end() > rc.begin());
        VERIFY_IS_FALSE(rc.begin() > rc.end());
    }

#pragma endregion
};
