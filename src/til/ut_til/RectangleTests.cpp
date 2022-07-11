// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

// Ensure the "safety" of til::rect::as_win32_rect
static_assert(
    sizeof(til::rect) == sizeof(RECT) &&
    alignof(til::rect) == alignof(RECT) &&
    offsetof(til::rect, left) == offsetof(RECT, left) &&
    offsetof(til::rect, top) == offsetof(RECT, top) &&
    offsetof(til::rect, right) == offsetof(RECT, right) &&
    offsetof(til::rect, bottom) == offsetof(RECT, bottom));
// Ensure the "safety" of til::rect::as_win32_points
static_assert(
    sizeof(til::rect) == 2 * sizeof(POINT) &&
    alignof(til::rect) == alignof(POINT) &&
    offsetof(til::rect, left) == offsetof(POINT, x) &&
    offsetof(til::rect, top) == offsetof(POINT, y) &&
    offsetof(til::rect, right) == offsetof(POINT, x) + sizeof(POINT) &&
    offsetof(til::rect, bottom) == offsetof(POINT, y) + sizeof(POINT));

class RectangleTests
{
    TEST_CLASS(RectangleTests);

    TEST_METHOD(DefaultConstruct)
    {
        const til::rect rc;
        VERIFY_ARE_EQUAL(0, rc.left);
        VERIFY_ARE_EQUAL(0, rc.top);
        VERIFY_ARE_EQUAL(0, rc.right);
        VERIFY_ARE_EQUAL(0, rc.bottom);
    }

    TEST_METHOD(RawConstruct)
    {
        const til::rect rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(5, rc.left);
        VERIFY_ARE_EQUAL(10, rc.top);
        VERIFY_ARE_EQUAL(15, rc.right);
        VERIFY_ARE_EQUAL(20, rc.bottom);
    }

    TEST_METHOD(SignedConstruct)
    {
        const auto l = 5;
        const auto t = 10;
        const auto r = 15;
        const auto b = 20;

        const til::rect rc{ l, t, r, b };
        VERIFY_ARE_EQUAL(5, rc.left);
        VERIFY_ARE_EQUAL(10, rc.top);
        VERIFY_ARE_EQUAL(15, rc.right);
        VERIFY_ARE_EQUAL(20, rc.bottom);
    }

    TEST_METHOD(TwoPointsConstruct)
    {
        const auto l = 5;
        const auto t = 10;
        const auto r = 15;
        const auto b = 20;

        const til::rect rc{ til::point{ l, t }, til::point{ r, b } };
        VERIFY_ARE_EQUAL(5, rc.left);
        VERIFY_ARE_EQUAL(10, rc.top);
        VERIFY_ARE_EQUAL(15, rc.right);
        VERIFY_ARE_EQUAL(20, rc.bottom);
    }

    TEST_METHOD(SizeOnlyConstruct)
    {
        // Size will match bottom right point because
        // til::rect is exclusive.
        const auto sz = til::size{ 5, 10 };
        const til::rect rc{ sz };
        VERIFY_ARE_EQUAL(0, rc.left);
        VERIFY_ARE_EQUAL(0, rc.top);
        VERIFY_ARE_EQUAL(sz.width, rc.right);
        VERIFY_ARE_EQUAL(sz.height, rc.bottom);
    }

    TEST_METHOD(PointAndSizeConstruct)
    {
        const til::point pt{ 4, 8 };

        Log::Comment(L"Normal Case");
        {
            const til::rect rc{ pt, til::size{ 2, 10 } };
            VERIFY_ARE_EQUAL(4, rc.left);
            VERIFY_ARE_EQUAL(8, rc.top);
            VERIFY_ARE_EQUAL(6, rc.right);
            VERIFY_ARE_EQUAL(18, rc.bottom);
        }

        Log::Comment(L"Overflow x-dimension case.");
        {
            auto fn = [&]() {
                constexpr auto x = std::numeric_limits<til::CoordType>().max();
                const auto y = 0;
                const til::rect rc{ pt, til::size{ x, y } };
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
        }

        Log::Comment(L"Overflow y-dimension case.");
        {
            auto fn = [&]() {
                const auto x = 0;
                constexpr auto y = std::numeric_limits<til::CoordType>().max();
                const til::rect rc{ pt, til::size{ x, y } };
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
        }
    }

    TEST_METHOD(SmallRectConstruct)
    {
        SMALL_RECT sr;
        sr.Left = 5;
        sr.Top = 10;
        sr.Right = 14;
        sr.Bottom = 19;

        const til::rect rc{ til::wrap_small_rect(sr) };
        VERIFY_ARE_EQUAL(5, rc.left);
        VERIFY_ARE_EQUAL(10, rc.top);
        VERIFY_ARE_EQUAL(15, rc.right);
        VERIFY_ARE_EQUAL(20, rc.bottom);
    }

    TEST_METHOD(Win32RectConstruct)
    {
        const RECT win32rc{ 5, 10, 15, 20 };
        const til::rect rc{ win32rc };

        VERIFY_ARE_EQUAL(5, rc.left);
        VERIFY_ARE_EQUAL(10, rc.top);
        VERIFY_ARE_EQUAL(15, rc.right);
        VERIFY_ARE_EQUAL(20, rc.bottom);
    }

    TEST_METHOD(Assignment)
    {
        til::rect a{ 1, 2, 3, 4 };
        const til::rect b{ 5, 6, 7, 8 };

        VERIFY_ARE_EQUAL(1, a.left);
        VERIFY_ARE_EQUAL(2, a.top);
        VERIFY_ARE_EQUAL(3, a.right);
        VERIFY_ARE_EQUAL(4, a.bottom);

        a = b;

        VERIFY_ARE_EQUAL(5, a.left);
        VERIFY_ARE_EQUAL(6, a.top);
        VERIFY_ARE_EQUAL(7, a.right);
        VERIFY_ARE_EQUAL(8, a.bottom);
    }

    TEST_METHOD(Equality)
    {
        Log::Comment(L"Equal.");
        {
            const til::rect a{ 1, 2, 3, 4 };
            const til::rect b{ 1, 2, 3, 4 };
            VERIFY_IS_TRUE(a == b);
        }

        Log::Comment(L"Left A changed.");
        {
            const til::rect a{ 9, 2, 3, 4 };
            const til::rect b{ 1, 2, 3, 4 };
            VERIFY_IS_FALSE(a == b);
        }

        Log::Comment(L"Top A changed.");
        {
            const til::rect a{ 1, 9, 3, 4 };
            const til::rect b{ 1, 2, 3, 4 };
            VERIFY_IS_FALSE(a == b);
        }

        Log::Comment(L"Right A changed.");
        {
            const til::rect a{ 1, 2, 9, 4 };
            const til::rect b{ 1, 2, 3, 4 };
            VERIFY_IS_FALSE(a == b);
        }

        Log::Comment(L"Bottom A changed.");
        {
            const til::rect a{ 1, 2, 3, 9 };
            const til::rect b{ 1, 2, 3, 4 };
            VERIFY_IS_FALSE(a == b);
        }

        Log::Comment(L"Left B changed.");
        {
            const til::rect a{ 1, 2, 3, 4 };
            const til::rect b{ 9, 2, 3, 4 };
            VERIFY_IS_FALSE(a == b);
        }

        Log::Comment(L"Top B changed.");
        {
            const til::rect a{ 1, 2, 3, 4 };
            const til::rect b{ 1, 9, 3, 4 };
            VERIFY_IS_FALSE(a == b);
        }

        Log::Comment(L"Right B changed.");
        {
            const til::rect a{ 1, 2, 3, 4 };
            const til::rect b{ 1, 2, 9, 4 };
            VERIFY_IS_FALSE(a == b);
        }

        Log::Comment(L"Bottom B changed.");
        {
            const til::rect a{ 1, 2, 3, 4 };
            const til::rect b{ 1, 2, 3, 9 };
            VERIFY_IS_FALSE(a == b);
        }
    }

    TEST_METHOD(Inequality)
    {
        Log::Comment(L"Equal.");
        {
            const til::rect a{ 1, 2, 3, 4 };
            const til::rect b{ 1, 2, 3, 4 };
            VERIFY_IS_FALSE(a != b);
        }

        Log::Comment(L"Left A changed.");
        {
            const til::rect a{ 9, 2, 3, 4 };
            const til::rect b{ 1, 2, 3, 4 };
            VERIFY_IS_TRUE(a != b);
        }

        Log::Comment(L"Top A changed.");
        {
            const til::rect a{ 1, 9, 3, 4 };
            const til::rect b{ 1, 2, 3, 4 };
            VERIFY_IS_TRUE(a != b);
        }

        Log::Comment(L"Right A changed.");
        {
            const til::rect a{ 1, 2, 9, 4 };
            const til::rect b{ 1, 2, 3, 4 };
            VERIFY_IS_TRUE(a != b);
        }

        Log::Comment(L"Bottom A changed.");
        {
            const til::rect a{ 1, 2, 3, 9 };
            const til::rect b{ 1, 2, 3, 4 };
            VERIFY_IS_TRUE(a != b);
        }

        Log::Comment(L"Left B changed.");
        {
            const til::rect a{ 1, 2, 3, 4 };
            const til::rect b{ 9, 2, 3, 4 };
            VERIFY_IS_TRUE(a != b);
        }

        Log::Comment(L"Top B changed.");
        {
            const til::rect a{ 1, 2, 3, 4 };
            const til::rect b{ 1, 9, 3, 4 };
            VERIFY_IS_TRUE(a != b);
        }

        Log::Comment(L"Right B changed.");
        {
            const til::rect a{ 1, 2, 3, 4 };
            const til::rect b{ 1, 2, 9, 4 };
            VERIFY_IS_TRUE(a != b);
        }

        Log::Comment(L"Bottom B changed.");
        {
            const til::rect a{ 1, 2, 3, 4 };
            const til::rect b{ 1, 2, 3, 9 };
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

        til::CoordType left, top, right, bottom;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"left", left));
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"top", top));
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"right", right));
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"bottom", bottom));

        const auto expected = left < right && top < bottom;
        const til::rect actual{ left, top, right, bottom };
        VERIFY_ARE_EQUAL(expected, (bool)actual);
    }

    TEST_METHOD(OrUnion)
    {
        const til::rect one{ 4, 6, 10, 14 };
        const til::rect two{ 5, 2, 13, 10 };

        const til::rect expected{ 4, 2, 13, 14 };
        const auto actual = one | two;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(OrUnionInplace)
    {
        til::rect one{ 4, 6, 10, 14 };
        const til::rect two{ 5, 2, 13, 10 };

        const til::rect expected{ 4, 2, 13, 14 };
        one |= two;
        VERIFY_ARE_EQUAL(expected, one);
    }

    TEST_METHOD(AndIntersect)
    {
        const til::rect one{ 4, 6, 10, 14 };
        const til::rect two{ 5, 2, 13, 10 };

        const til::rect expected{ 5, 6, 10, 10 };
        const auto actual = one & two;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(AndIntersectInplace)
    {
        til::rect one{ 4, 6, 10, 14 };
        const til::rect two{ 5, 2, 13, 10 };

        const til::rect expected{ 5, 6, 10, 10 };
        one &= two;
        VERIFY_ARE_EQUAL(expected, one);
    }

    TEST_METHOD(MinusSubtractSame)
    {
        const til::rect original{ 0, 0, 10, 10 };
        const auto removal = original;

        // Since it's the same rectangle, nothing's left. We should get no results.
        const til::some<til::rect, 4> expected;
        const auto actual = original - removal;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(MinusSubtractNoOverlap)
    {
        const til::rect original{ 0, 0, 10, 10 };
        const til::rect removal{ 12, 12, 15, 15 };

        // Since they don't overlap, we expect the original to be given back.
        const til::some<til::rect, 4> expected{ original };
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

        const til::rect original{ 0, 0, 10, 10 };
        const til::rect removal{ -12, 3, 15, 15 };

        const til::some<til::rect, 4> expected{
            til::rect{ original.left, original.top, original.right, removal.top }
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

        const til::rect original{ 0, 0, 10, 10 };
        const til::rect removal{ 3, 3, 15, 15 };

        const til::some<til::rect, 4> expected{
            til::rect{ original.left, original.top, original.right, removal.top },
            til::rect{ original.left, removal.top, removal.left, original.bottom }
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

        const til::rect original{ 0, 0, 10, 10 };
        const til::rect removal{ 3, 3, 15, 6 };

        const til::some<til::rect, 4> expected{
            til::rect{ original.left, original.top, original.right, removal.top },
            til::rect{ original.left, removal.bottom, original.right, original.bottom },
            til::rect{ original.left, removal.top, removal.left, removal.bottom }
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

        const til::rect original{ 0, 0, 10, 10 };
        const til::rect removal{ 3, 3, 6, 6 };

        const til::some<til::rect, 4> expected{
            til::rect{ original.left, original.top, original.right, removal.top },
            til::rect{ original.left, removal.bottom, original.right, original.bottom },
            til::rect{ original.left, removal.top, removal.left, removal.bottom },
            til::rect{ removal.right, removal.top, original.right, removal.bottom }
        };
        const auto actual = original - removal;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(AdditionPoint)
    {
        const til::rect start{ 10, 20, 30, 40 };
        const til::point pt{ 3, 7 };
        const til::rect expected{ 10 + 3, 20 + 7, 30 + 3, 40 + 7 };
        const auto actual = start + pt;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(AdditionPointInplace)
    {
        til::rect start{ 10, 20, 30, 40 };
        const til::point pt{ 3, 7 };
        const til::rect expected{ 10 + 3, 20 + 7, 30 + 3, 40 + 7 };
        start += pt;
        VERIFY_ARE_EQUAL(expected, start);
    }

    TEST_METHOD(SubtractionPoint)
    {
        const til::rect start{ 10, 20, 30, 40 };
        const til::point pt{ 3, 7 };
        const til::rect expected{ 10 - 3, 20 - 7, 30 - 3, 40 - 7 };
        const auto actual = start - pt;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(SubtractionPointInplace)
    {
        til::rect start{ 10, 20, 30, 40 };
        const til::point pt{ 3, 7 };
        const til::rect expected{ 10 - 3, 20 - 7, 30 - 3, 40 - 7 };
        start -= pt;
        VERIFY_ARE_EQUAL(expected, start);
    }

    TEST_METHOD(ScaleUpSize)
    {
        const til::rect start{ 10, 20, 30, 40 };

        Log::Comment(L"Multiply by size to scale from cells to pixels");
        {
            const til::size scale{ 3, 7 };
            const til::rect expected{ 10 * 3, 20 * 7, 30 * 3, 40 * 7 };
            const auto actual = start.scale_up(scale);
            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"Multiply by size with width way too big.");
        {
            const til::size scale{ std::numeric_limits<til::CoordType>().max(), static_cast<til::CoordType>(7) };

            auto fn = [&]() {
                const auto actual = start.scale_up(scale);
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
        }

        Log::Comment(L"Multiply by size with height way too big.");
        {
            const til::size scale{ static_cast<til::CoordType>(3), std::numeric_limits<til::CoordType>().max() };

            auto fn = [&]() {
                const auto actual = start.scale_up(scale);
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
        }
    }

    TEST_METHOD(ScaleDownSize)
    {
        const til::rect start{ 10, 20, 29, 40 };

        Log::Comment(L"Division by size to scale from pixels to cells");
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
            const til::rect expected{ 3, 2, 10, 6 };
            const auto actual = start.scale_down(scale);
            VERIFY_ARE_EQUAL(expected, actual);
        }
    }

    TEST_METHOD(Top)
    {
        const til::rect rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(rc.top, rc.top);
    }

    TEST_METHOD(TopCast)
    {
        const til::rect rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(static_cast<SHORT>(rc.top), rc.narrow_top<SHORT>());
    }

    TEST_METHOD(Bottom)
    {
        const til::rect rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(rc.bottom, rc.bottom);
    }

    TEST_METHOD(BottomCast)
    {
        const til::rect rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(static_cast<SHORT>(rc.bottom), rc.narrow_bottom<SHORT>());
    }

    TEST_METHOD(Left)
    {
        const til::rect rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(rc.left, rc.left);
    }

    TEST_METHOD(LeftCast)
    {
        const til::rect rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(static_cast<SHORT>(rc.left), rc.narrow_left<SHORT>());
    }

    TEST_METHOD(Right)
    {
        const til::rect rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(rc.right, rc.right);
    }

    TEST_METHOD(RightCast)
    {
        const til::rect rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(static_cast<SHORT>(rc.right), rc.narrow_right<SHORT>());
    }

    TEST_METHOD(Width)
    {
        Log::Comment(L"Width that should be in bounds.");
        {
            const til::rect rc{ 5, 10, 15, 20 };
            VERIFY_ARE_EQUAL(15 - 5, rc.width());
        }

        Log::Comment(L"Width that should go out of bounds on subtraction.");
        {
            constexpr auto bigVal = std::numeric_limits<til::CoordType>().min();
            const auto normalVal = 5;
            const til::rect rc{ normalVal, normalVal, bigVal, normalVal };

            auto fn = [&]() {
                rc.width();
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
        }
    }

    TEST_METHOD(WidthCast)
    {
        const auto expected = 15 - 5;
        const til::rect rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(expected, rc.narrow_width<SHORT>());
    }

    TEST_METHOD(Height)
    {
        Log::Comment(L"Height that should be in bounds.");
        {
            const til::rect rc{ 5, 10, 15, 20 };
            VERIFY_ARE_EQUAL(20 - 10, rc.height());
        }

        Log::Comment(L"Height that should go out of bounds on subtraction.");
        {
            constexpr auto bigVal = std::numeric_limits<til::CoordType>().min();
            const auto normalVal = 5;
            const til::rect rc{ normalVal, normalVal, normalVal, bigVal };

            auto fn = [&]() {
                rc.height();
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
        }
    }

    TEST_METHOD(HeightCast)
    {
        const auto expected = 20 - 10;
        const til::rect rc{ 5, 10, 15, 20 };
        VERIFY_ARE_EQUAL(expected, rc.narrow_height<SHORT>());
    }

    TEST_METHOD(Origin)
    {
        const til::rect rc{ 5, 10, 15, 20 };
        const til::point expected{ 5, 10 };
        VERIFY_ARE_EQUAL(expected, rc.origin());
    }

    TEST_METHOD(Size)
    {
        const til::rect rc{ 5, 10, 15, 20 };
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

        til::CoordType left, top, right, bottom;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"left", left));
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"top", top));
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"right", right));
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"bottom", bottom));

        const auto expected = !(left < right && top < bottom);
        const til::rect actual{ left, top, right, bottom };
        VERIFY_ARE_EQUAL(expected, actual.empty());
    }

    TEST_METHOD(ContainsPoint)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:x", L"{-1000,0,4,5,6,14,15,16,1000}")
            TEST_METHOD_PROPERTY(L"Data:y", L"{-1000,0,9,10,11,19,20,21,1000}")
        END_TEST_METHOD_PROPERTIES()

        til::CoordType x, y;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"x", x));
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"y", y));

        const til::rect rc{ 5, 10, 15, 20 };
        const til::point pt{ x, y };

        const auto xInBounds = x >= 5 && x < 15;
        const auto yInBounds = y >= 10 && y < 20;
        const auto expected = xInBounds && yInBounds;
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

    TEST_METHOD(ContainsRectangle)
    {
        const til::rect rc{ 5, 10, 15, 20 }; // 10x10 rectangle.

        const til::rect fitsInside{ 8, 12, 10, 18 };
        const til::rect spillsOut{ 0, 0, 50, 50 };
        const til::rect sticksOut{ 14, 12, 30, 13 };

        VERIFY_IS_TRUE(rc.contains(rc), L"We contain ourself.");
        VERIFY_IS_TRUE(rc.contains(fitsInside), L"We fully contain a smaller rectangle.");
        VERIFY_IS_FALSE(rc.contains(spillsOut), L"We do not fully contain rectangle larger than us.");
        VERIFY_IS_FALSE(rc.contains(sticksOut), L"We do not contain a rectangle that is smaller, but sticks out our edge.");
    }

    TEST_METHOD(IndexOfPoint)
    {
        const til::rect rc{ 5, 10, 15, 20 };

        Log::Comment(L"Normal in bounds.");
        {
            const til::point pt{ 7, 17 };
            const auto expected = 72;
            VERIFY_ARE_EQUAL(expected, rc.index_of(pt));
        }

        Log::Comment(L"Out of bounds.");
        {
            auto fn = [&]() {
                const til::point pt{ 1, 1 };
                rc.index_of(pt);
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_INVALIDARG; });
        }

        Log::Comment(L"Overflow.");
        {
            auto fn = [&]() {
                constexpr const auto min = static_cast<til::CoordType>(0);
                constexpr const auto max = std::numeric_limits<til::CoordType>().max();
                const til::rect bigRc{ min, min, max, max };
                const til::point pt{ max - 1, max - 1 };
                bigRc.index_of(pt);
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
        }
    }

    TEST_METHOD(PointAtIndex)
    {
        const til::rect rc{ 5, 10, 15, 20 };

        Log::Comment(L"Normal in bounds.");
        {
            const til::point expected{ 7, 17 };
            VERIFY_ARE_EQUAL(expected, rc.point_at(72));
        }

        Log::Comment(L"Out of bounds too high.");
        {
            auto fn = [&]() {
                rc.point_at(1000);
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_INVALIDARG; });
        }
    }

    TEST_METHOD(CastToSmallRect)
    {
        Log::Comment(L"Typical situation.");
        {
            const til::rect rc{ 5, 10, 15, 20 };
            const auto val = til::unwrap_small_rect(rc.to_inclusive_rect());
            VERIFY_ARE_EQUAL(5, val.Left);
            VERIFY_ARE_EQUAL(10, val.Top);
            VERIFY_ARE_EQUAL(14, val.Right);
            VERIFY_ARE_EQUAL(19, val.Bottom);
        }

        Log::Comment(L"Overflow on left.");
        {
            constexpr auto l = std::numeric_limits<til::CoordType>().max();
            const auto t = 10;
            const auto r = 15;
            const auto b = 20;
            const til::rect rc{ l, t, r, b };

            auto fn = [&]() {
                const auto val = til::unwrap_small_rect(rc.to_inclusive_rect());
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
        }

        Log::Comment(L"Overflow on top.");
        {
            const auto l = 5;
            constexpr auto t = std::numeric_limits<til::CoordType>().max();
            const auto r = 15;
            const auto b = 20;
            const til::rect rc{ l, t, r, b };

            auto fn = [&]() {
                const auto val = til::unwrap_small_rect(rc.to_inclusive_rect());
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
        }

        Log::Comment(L"Overflow on right.");
        {
            const auto l = 5;
            const auto t = 10;
            constexpr auto r = std::numeric_limits<til::CoordType>().max();
            const auto b = 20;
            const til::rect rc{ l, t, r, b };

            auto fn = [&]() {
                const auto val = til::unwrap_small_rect(rc.to_inclusive_rect());
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
        }

        Log::Comment(L"Overflow on bottom.");
        {
            const auto l = 5;
            const auto t = 10;
            const auto r = 15;
            constexpr auto b = std::numeric_limits<til::CoordType>().max();
            const til::rect rc{ l, t, r, b };

            auto fn = [&]() {
                const auto val = til::unwrap_small_rect(rc.to_inclusive_rect());
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
        }
    }

    TEST_METHOD(CastToRect)
    {
        Log::Comment(L"Typical situation.");
        {
            const til::rect rc{ 5, 10, 15, 20 };
            auto val = rc.to_win32_rect();
            VERIFY_ARE_EQUAL(5, val.left);
            VERIFY_ARE_EQUAL(10, val.top);
            VERIFY_ARE_EQUAL(15, val.right);
            VERIFY_ARE_EQUAL(20, val.bottom);
        }

        Log::Comment(L"Fit max left into RECT (may overflow).");
        {
            constexpr auto l = std::numeric_limits<til::CoordType>().max();
            const auto t = 10;
            const auto r = 15;
            const auto b = 20;
            const til::rect rc{ l, t, r, b };

            // On some platforms, til::CoordType will fit inside l/t/r/b
            const auto overflowExpected = l > std::numeric_limits<decltype(RECT::left)>().max();

            if (overflowExpected)
            {
                auto fn = [&]() {
                    auto val = rc.to_win32_rect();
                };

                VERIFY_THROWS(fn(), gsl::narrowing_error);
            }
            else
            {
                auto val = rc.to_win32_rect();
                VERIFY_ARE_EQUAL(l, val.left);
            }
        }

        Log::Comment(L"Fit max top into RECT (may overflow).");
        {
            const auto l = 5;
            constexpr auto t = std::numeric_limits<til::CoordType>().max();
            const auto r = 15;
            const auto b = 20;
            const til::rect rc{ l, t, r, b };

            // On some platforms, til::CoordType will fit inside l/t/r/b
            const auto overflowExpected = t > std::numeric_limits<decltype(RECT::top)>().max();

            if (overflowExpected)
            {
                auto fn = [&]() {
                    auto val = rc.to_win32_rect();
                };

                VERIFY_THROWS(fn(), gsl::narrowing_error);
            }
            else
            {
                auto val = rc.to_win32_rect();
                VERIFY_ARE_EQUAL(t, val.top);
            }
        }

        Log::Comment(L"Fit max right into RECT (may overflow).");
        {
            const auto l = 5;
            const auto t = 10;
            constexpr auto r = std::numeric_limits<til::CoordType>().max();
            const auto b = 20;
            const til::rect rc{ l, t, r, b };

            // On some platforms, til::CoordType will fit inside l/t/r/b
            const auto overflowExpected = r > std::numeric_limits<decltype(RECT::right)>().max();

            if (overflowExpected)
            {
                auto fn = [&]() {
                    auto val = rc.to_win32_rect();
                };

                VERIFY_THROWS(fn(), gsl::narrowing_error);
            }
            else
            {
                auto val = rc.to_win32_rect();
                VERIFY_ARE_EQUAL(r, val.right);
            }
        }

        Log::Comment(L"Fit max bottom into RECT (may overflow).");
        {
            const auto l = 5;
            const auto t = 10;
            const auto r = 15;
            constexpr auto b = std::numeric_limits<til::CoordType>().max();
            const til::rect rc{ l, t, r, b };

            // On some platforms, til::CoordType will fit inside l/t/r/b
            const auto overflowExpected = b > std::numeric_limits<decltype(RECT::bottom)>().max();

            if (overflowExpected)
            {
                auto fn = [&]() {
                    auto val = rc.to_win32_rect();
                };

                VERIFY_THROWS(fn(), gsl::narrowing_error);
            }
            else
            {
                auto val = rc.to_win32_rect();
                VERIFY_ARE_EQUAL(b, val.bottom);
            }
        }
    }

    TEST_METHOD(CastToD2D1RectF)
    {
        Log::Comment(L"Typical situation.");
        {
            const til::rect rc{ 5, 10, 15, 20 };
            auto val = rc.to_d2d_rect();
            VERIFY_ARE_EQUAL(5, val.left);
            VERIFY_ARE_EQUAL(10, val.top);
            VERIFY_ARE_EQUAL(15, val.right);
            VERIFY_ARE_EQUAL(20, val.bottom);
        }

        // All til::CoordTypes fit into a float, so there's no exception tests.
    }

    TEST_METHOD(CastToWindowsFoundationRect)
    {
        Log::Comment(L"Typical situation.");
        {
            const til::rect rc{ 5, 10, 15, 20 };
            auto val = rc.to_winrt_rect();
            VERIFY_ARE_EQUAL(5.f, val.X);
            VERIFY_ARE_EQUAL(10.f, val.Y);
            VERIFY_ARE_EQUAL(10.f, val.Width);
            VERIFY_ARE_EQUAL(10.f, val.Height);
        }

        // All til::CoordTypes fit into a float, so there's no exception tests.
        // The only other exceptions come from things that don't fit into width() or height()
        // and those have explicit tests elsewhere in this file.
    }

#pragma region iterator
    TEST_METHOD(Begin)
    {
        const til::rect rc{ 5, 10, 15, 20 };
        const til::point expected{ rc.left, rc.top };
        const auto it = rc.begin();

        VERIFY_ARE_EQUAL(expected, *it);
    }

    TEST_METHOD(End)
    {
        const til::rect rc{ 5, 10, 15, 20 };
        const til::point expected{ rc.left, rc.bottom };
        const auto it = rc.end();

        VERIFY_ARE_EQUAL(expected, *it);
    }

    TEST_METHOD(ConstIteratorIncrement)
    {
        const til::rect rc{ til::size{ 2, 2 } };

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
        const til::rect rc{ 5, 10, 15, 20 };

        VERIFY_IS_TRUE(rc.begin() == rc.begin());
        VERIFY_IS_FALSE(rc.begin() == rc.end());
    }

    TEST_METHOD(ConstIteratorInequality)
    {
        const til::rect rc{ 5, 10, 15, 20 };

        VERIFY_IS_FALSE(rc.begin() != rc.begin());
        VERIFY_IS_TRUE(rc.begin() != rc.end());
    }

    TEST_METHOD(ConstIteratorLessThan)
    {
        const til::rect rc{ 5, 10, 15, 20 };

        VERIFY_IS_TRUE(rc.begin() < rc.end());
        VERIFY_IS_FALSE(rc.end() < rc.begin());
    }

    TEST_METHOD(ConstIteratorGreaterThan)
    {
        const til::rect rc{ 5, 10, 15, 20 };

        VERIFY_IS_TRUE(rc.end() > rc.begin());
        VERIFY_IS_FALSE(rc.begin() > rc.end());
    }

#pragma endregion

    TEST_METHOD(CastFromFloatWithMathTypes)
    {
        Log::Comment(L"Ceiling");
        {
            {
                til::rect converted{ til::math::ceiling, 1.f, 2.f, 3.f, 4.f };
                VERIFY_ARE_EQUAL((til::rect{ 1, 2, 3, 4 }), converted);
            }
            {
                til::rect converted{ til::math::ceiling, 1.6f, 2.4f, 3.2f, 4.8f };
                VERIFY_ARE_EQUAL((til::rect{ 2, 3, 4, 5 }), converted);
            }
            {
                til::rect converted{ til::math::ceiling, 3., 4., 5., 6. };
                VERIFY_ARE_EQUAL((til::rect{ 3, 4, 5, 6 }), converted);
            }
            {
                til::rect converted{ til::math::ceiling, 3.6, 4.4, 5.7, 6.3 };
                VERIFY_ARE_EQUAL((til::rect{ 4, 5, 6, 7 }), converted);
            }
        }

        Log::Comment(L"Flooring");
        {
            {
                til::rect converted{ til::math::flooring, 1.f, 2.f, 3.f, 4.f };
                VERIFY_ARE_EQUAL((til::rect{ 1, 2, 3, 4 }), converted);
            }
            {
                til::rect converted{ til::math::flooring, 1.6f, 2.4f, 3.2f, 4.8f };
                VERIFY_ARE_EQUAL((til::rect{ 1, 2, 3, 4 }), converted);
            }
            {
                til::rect converted{ til::math::flooring, 3., 4., 5., 6. };
                VERIFY_ARE_EQUAL((til::rect{ 3, 4, 5, 6 }), converted);
            }
            {
                til::rect converted{ til::math::flooring, 3.6, 4.4, 5.7, 6.3 };
                VERIFY_ARE_EQUAL((til::rect{ 3, 4, 5, 6 }), converted);
            }
        }

        Log::Comment(L"Rounding");
        {
            {
                til::rect converted{ til::math::rounding, 1.f, 2.f, 3.f, 4.f };
                VERIFY_ARE_EQUAL((til::rect{ 1, 2, 3, 4 }), converted);
            }
            {
                til::rect converted{ til::math::rounding, 1.6f, 2.4f, 3.2f, 4.8f };
                VERIFY_ARE_EQUAL((til::rect{ 2, 2, 3, 5 }), converted);
            }
            {
                til::rect converted{ til::math::rounding, 3., 4., 5., 6. };
                VERIFY_ARE_EQUAL((til::rect{ 3, 4, 5, 6 }), converted);
            }
            {
                til::rect converted{ til::math::rounding, 3.6, 4.4, 5.7, 6.3 };
                VERIFY_ARE_EQUAL((til::rect{ 4, 4, 6, 6 }), converted);
            }
        }
    }
};
