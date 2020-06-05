// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "til/point.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class PointTests
{
    TEST_CLASS(PointTests);

    TEST_METHOD(DefaultConstruct)
    {
        const til::point pt;
        VERIFY_ARE_EQUAL(0, pt._x);
        VERIFY_ARE_EQUAL(0, pt._y);
    }

    TEST_METHOD(RawConstruct)
    {
        const til::point pt{ 5, 10 };
        VERIFY_ARE_EQUAL(5, pt._x);
        VERIFY_ARE_EQUAL(10, pt._y);
    }

    TEST_METHOD(RawFloatingConstruct)
    {
        const til::point pt{ til::math::rounding, 3.2f, 7.6f };
        VERIFY_ARE_EQUAL(3, pt._x);
        VERIFY_ARE_EQUAL(8, pt._y);
    }

    TEST_METHOD(UnsignedConstruct)
    {
        Log::Comment(L"0.) Normal unsigned construct.");
        {
            const size_t x = 5;
            const size_t y = 10;

            const til::point pt{ x, y };
            VERIFY_ARE_EQUAL(5, pt._x);
            VERIFY_ARE_EQUAL(10, pt._y);
        }

        Log::Comment(L"1.) Unsigned construct overflow on x.");
        {
            constexpr size_t x = std::numeric_limits<size_t>().max();
            const size_t y = 10;

            auto fn = [&]() {
                til::point pt{ x, y };
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Unsigned construct overflow on y.");
        {
            constexpr size_t y = std::numeric_limits<size_t>().max();
            const size_t x = 10;

            auto fn = [&]() {
                til::point pt{ x, y };
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(SignedConstruct)
    {
        const ptrdiff_t x = -5;
        const ptrdiff_t y = -10;

        const til::point pt{ x, y };
        VERIFY_ARE_EQUAL(x, pt._x);
        VERIFY_ARE_EQUAL(y, pt._y);
    }

    TEST_METHOD(CoordConstruct)
    {
        COORD coord{ -5, 10 };

        const til::point pt{ coord };
        VERIFY_ARE_EQUAL(coord.X, pt._x);
        VERIFY_ARE_EQUAL(coord.Y, pt._y);
    }

    TEST_METHOD(PointConstruct)
    {
        POINT point{ 5, -10 };

        const til::point pt{ point };
        VERIFY_ARE_EQUAL(point.x, pt._x);
        VERIFY_ARE_EQUAL(point.y, pt._y);
    }

    TEST_METHOD(Equality)
    {
        Log::Comment(L"0.) Equal.");
        {
            const til::point s1{ 5, 10 };
            const til::point s2{ 5, 10 };
            VERIFY_IS_TRUE(s1 == s2);
        }

        Log::Comment(L"1.) Left Width changed.");
        {
            const til::point s1{ 4, 10 };
            const til::point s2{ 5, 10 };
            VERIFY_IS_FALSE(s1 == s2);
        }

        Log::Comment(L"2.) Right Width changed.");
        {
            const til::point s1{ 5, 10 };
            const til::point s2{ 6, 10 };
            VERIFY_IS_FALSE(s1 == s2);
        }

        Log::Comment(L"3.) Left Height changed.");
        {
            const til::point s1{ 5, 9 };
            const til::point s2{ 5, 10 };
            VERIFY_IS_FALSE(s1 == s2);
        }

        Log::Comment(L"4.) Right Height changed.");
        {
            const til::point s1{ 5, 10 };
            const til::point s2{ 5, 11 };
            VERIFY_IS_FALSE(s1 == s2);
        }
    }

    TEST_METHOD(Inequality)
    {
        Log::Comment(L"0.) Equal.");
        {
            const til::point s1{ 5, 10 };
            const til::point s2{ 5, 10 };
            VERIFY_IS_FALSE(s1 != s2);
        }

        Log::Comment(L"1.) Left Width changed.");
        {
            const til::point s1{ 4, 10 };
            const til::point s2{ 5, 10 };
            VERIFY_IS_TRUE(s1 != s2);
        }

        Log::Comment(L"2.) Right Width changed.");
        {
            const til::point s1{ 5, 10 };
            const til::point s2{ 6, 10 };
            VERIFY_IS_TRUE(s1 != s2);
        }

        Log::Comment(L"3.) Left Height changed.");
        {
            const til::point s1{ 5, 9 };
            const til::point s2{ 5, 10 };
            VERIFY_IS_TRUE(s1 != s2);
        }

        Log::Comment(L"4.) Right Height changed.");
        {
            const til::point s1{ 5, 10 };
            const til::point s2{ 5, 11 };
            VERIFY_IS_TRUE(s1 != s2);
        }
    }

    TEST_METHOD(Addition)
    {
        Log::Comment(L"0.) Addition of two things that should be in bounds.");
        {
            const til::point pt{ 5, 10 };
            const til::point pt2{ 23, 47 };

            const til::point expected{ pt.x() + pt2.x(), pt.y() + pt2.y() };

            VERIFY_ARE_EQUAL(expected, pt + pt2);
        }

        Log::Comment(L"1.) Addition results in value that is too large (x).");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::point pt{ bigSize, static_cast<ptrdiff_t>(0) };
            const til::point pt2{ 1, 1 };

            auto fn = [&]() {
                pt + pt2;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Addition results in value that is too large (y).");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::point pt{ static_cast<ptrdiff_t>(0), bigSize };
            const til::point pt2{ 1, 1 };

            auto fn = [&]() {
                pt + pt2;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(AdditionInplace)
    {
        Log::Comment(L"0.) Addition of two things that should be in bounds.");
        {
            const til::point pt{ 5, 10 };
            const til::point pt2{ 23, 47 };

            const til::point expected{ pt.x() + pt2.x(), pt.y() + pt2.y() };

            auto actual = pt;
            actual += pt2;

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"1.) Addition results in value that is too large (x).");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::point pt{ bigSize, static_cast<ptrdiff_t>(0) };
            const til::point pt2{ 1, 1 };

            auto fn = [&]() {
                auto actual = pt;
                actual += pt2;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Addition results in value that is too large (y).");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::point pt{ static_cast<ptrdiff_t>(0), bigSize };
            const til::point pt2{ 1, 1 };

            auto fn = [&]() {
                auto actual = pt;
                actual += pt2;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(Subtraction)
    {
        Log::Comment(L"0.) Subtraction of two things that should be in bounds.");
        {
            const til::point pt{ 5, 10 };
            const til::point pt2{ 23, 47 };

            const til::point expected{ pt.x() - pt2.x(), pt.y() - pt2.y() };

            VERIFY_ARE_EQUAL(expected, pt - pt2);
        }

        Log::Comment(L"1.) Subtraction results in value that is too small (x).");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::point pt{ bigSize, static_cast<ptrdiff_t>(0) };
            const til::point pt2{ -2, -2 };

            auto fn = [&]() {
                pt2 - pt;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Subtraction results in value that is too small (y).");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::point pt{ static_cast<ptrdiff_t>(0), bigSize };
            const til::point pt2{ -2, -2 };

            auto fn = [&]() {
                pt2 - pt;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(SubtractionInplace)
    {
        Log::Comment(L"0.) Subtraction of two things that should be in bounds.");
        {
            const til::point pt{ 5, 10 };
            const til::point pt2{ 23, 47 };

            const til::point expected{ pt.x() - pt2.x(), pt.y() - pt2.y() };

            auto actual = pt;
            actual -= pt2;

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"1.) Subtraction results in value that is too small (x).");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::point pt{ bigSize, static_cast<ptrdiff_t>(0) };
            const til::point pt2{ -2, -2 };

            auto fn = [&]() {
                auto actual = pt2;
                actual -= pt;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Subtraction results in value that is too small (y).");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::point pt{ static_cast<ptrdiff_t>(0), bigSize };
            const til::point pt2{ -2, -2 };

            auto fn = [&]() {
                auto actual = pt2;
                actual -= pt;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(Multiplication)
    {
        Log::Comment(L"0.) Multiplication of two things that should be in bounds.");
        {
            const til::point pt{ 5, 10 };
            const til::point pt2{ 23, 47 };

            const til::point expected{ pt.x() * pt2.x(), pt.y() * pt2.y() };

            VERIFY_ARE_EQUAL(expected, pt * pt2);
        }

        Log::Comment(L"1.) Multiplication results in value that is too large (x).");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::point pt{ bigSize, static_cast<ptrdiff_t>(0) };
            const til::point pt2{ 10, 10 };

            auto fn = [&]() {
                pt* pt2;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Multiplication results in value that is too large (y).");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::point pt{ static_cast<ptrdiff_t>(0), bigSize };
            const til::point pt2{ 10, 10 };

            auto fn = [&]() {
                pt* pt2;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(MultiplicationInplace)
    {
        Log::Comment(L"0.) Multiplication of two things that should be in bounds.");
        {
            const til::point pt{ 5, 10 };
            const til::point pt2{ 23, 47 };

            const til::point expected{ pt.x() * pt2.x(), pt.y() * pt2.y() };

            auto actual = pt;
            actual *= pt2;

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"1.) Multiplication results in value that is too large (x).");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::point pt{ bigSize, static_cast<ptrdiff_t>(0) };
            const til::point pt2{ 10, 10 };

            auto fn = [&]() {
                auto actual = pt;
                actual *= pt2;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Multiplication results in value that is too large (y).");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::point pt{ static_cast<ptrdiff_t>(0), bigSize };
            const til::point pt2{ 10, 10 };

            auto fn = [&]() {
                auto actual = pt;
                actual *= pt2;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(ScaleByFloat)
    {
        Log::Comment(L"0.) Scale that should be in bounds.");
        {
            const til::point pt{ 5, 10 };
            const float scale = 1.783f;

            const til::point expected{ static_cast<ptrdiff_t>(ceil(5 * scale)), static_cast<ptrdiff_t>(ceil(10 * scale)) };

            const auto actual = pt.scale(til::math::ceiling, scale);

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"1.) Scale results in value that is too large.");
        {
            const til::point pt{ 5, 10 };
            constexpr float scale = std::numeric_limits<float>().max();

            auto fn = [&]() {
                pt.scale(til::math::ceiling, scale);
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(Division)
    {
        Log::Comment(L"0.) Division of two things that should be in bounds.");
        {
            const til::point pt{ 555, 510 };
            const til::point pt2{ 23, 47 };

            const til::point expected{ pt.x() / pt2.x(), pt.y() / pt2.y() };

            VERIFY_ARE_EQUAL(expected, pt / pt2);
        }

        Log::Comment(L"1.) Division by zero");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::point pt{ bigSize, static_cast<ptrdiff_t>(0) };
            const til::point pt2{ 1, 1 };

            auto fn = [&]() {
                pt2 / pt;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(DivisionInplace)
    {
        Log::Comment(L"0.) Division of two things that should be in bounds.");
        {
            const til::point pt{ 555, 510 };
            const til::point pt2{ 23, 47 };

            const til::point expected{ pt.x() / pt2.x(), pt.y() / pt2.y() };
            auto actual = pt;
            actual /= pt2;

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"1.) Division by zero");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::point pt{ bigSize, static_cast<ptrdiff_t>(0) };
            const til::point pt2{ 1, 1 };

            auto fn = [&]() {
                auto actual = pt2;
                actual /= pt;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(X)
    {
        const til::point pt{ 5, 10 };
        VERIFY_ARE_EQUAL(pt._x, pt.x());
    }

    TEST_METHOD(XCast)
    {
        const til::point pt{ 5, 10 };
        VERIFY_ARE_EQUAL(static_cast<SHORT>(pt._x), pt.x<SHORT>());
    }

    TEST_METHOD(Y)
    {
        const til::point pt{ 5, 10 };
        VERIFY_ARE_EQUAL(pt._y, pt.y());
    }

    TEST_METHOD(YCast)
    {
        const til::point pt{ 5, 10 };
        VERIFY_ARE_EQUAL(static_cast<SHORT>(pt._x), pt.x<SHORT>());
    }

    TEST_METHOD(CastToCoord)
    {
        Log::Comment(L"0.) Typical situation.");
        {
            const til::point pt{ 5, 10 };
            COORD val = pt;
            VERIFY_ARE_EQUAL(5, val.X);
            VERIFY_ARE_EQUAL(10, val.Y);
        }

        Log::Comment(L"1.) Overflow on x.");
        {
            constexpr ptrdiff_t x = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t y = 10;
            const til::point pt{ x, y };

            auto fn = [&]() {
                COORD val = pt;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Overflow on y.");
        {
            constexpr ptrdiff_t y = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t x = 10;
            const til::point pt{ x, y };

            auto fn = [&]() {
                COORD val = pt;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(CastToPoint)
    {
        Log::Comment(L"0.) Typical situation.");
        {
            const til::point pt{ 5, 10 };
            POINT val = pt;
            VERIFY_ARE_EQUAL(5, val.x);
            VERIFY_ARE_EQUAL(10, val.y);
        }

        Log::Comment(L"1.) Fit max x into POINT (may overflow).");
        {
            constexpr ptrdiff_t x = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t y = 10;
            const til::point pt{ x, y };

            // On some platforms, ptrdiff_t will fit inside x/y
            const bool overflowExpected = x > std::numeric_limits<decltype(POINT::x)>().max();

            if (overflowExpected)
            {
                auto fn = [&]() {
                    POINT val = pt;
                };

                VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
            }
            else
            {
                POINT val = pt;
                VERIFY_ARE_EQUAL(x, val.x);
            }
        }

        Log::Comment(L"2.) Fit max y into POINT (may overflow).");
        {
            constexpr ptrdiff_t y = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t x = 10;
            const til::point pt{ x, y };

            // On some platforms, ptrdiff_t will fit inside x/y
            const bool overflowExpected = y > std::numeric_limits<decltype(POINT::y)>().max();

            if (overflowExpected)
            {
                auto fn = [&]() {
                    POINT val = pt;
                };

                VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
            }
            else
            {
                POINT val = pt;
                VERIFY_ARE_EQUAL(y, val.y);
            }
        }
    }

    TEST_METHOD(CastToD2D1Point2F)
    {
        Log::Comment(L"0.) Typical situation.");
        {
            const til::point pt{ 5, 10 };
            D2D1_POINT_2F val = pt;
            VERIFY_ARE_EQUAL(5, val.x);
            VERIFY_ARE_EQUAL(10, val.y);
        }

        // All ptrdiff_ts fit into a float, so there's no exception tests.
    }

    TEST_METHOD(Scaling)
    {
        Log::Comment(L"0.) Multiplication of two things that should be in bounds.");
        {
            const til::point pt{ 5, 10 };
            const int scale = 23;

            const til::point expected{ pt.x() * scale, pt.y() * scale };

            VERIFY_ARE_EQUAL(expected, pt * scale);
        }

        Log::Comment(L"1.) Multiplication results in value that is too large (x).");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::point pt{ bigSize, static_cast<ptrdiff_t>(0) };
            const int scale = 10;

            auto fn = [&]() {
                pt* scale;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Multiplication results in value that is too large (y).");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::point pt{ static_cast<ptrdiff_t>(0), bigSize };
            const int scale = 10;

            auto fn = [&]() {
                pt* scale;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"3.) Division of two things that should be in bounds.");
        {
            const til::point pt{ 555, 510 };
            const int scale = 23;

            const til::point expected{ pt.x() / scale, pt.y() / scale };

            VERIFY_ARE_EQUAL(expected, pt / scale);
        }

        Log::Comment(L"4.) Division by zero");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::point pt{ 1, 1 };
            const int scale = 0;

            auto fn = [&]() {
                pt / scale;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"5.) Multiplication of floats that should be in bounds.");
        {
            const til::point pt{ 3, 10 };
            const float scale = 5.5f;

            // 3 * 5.5 = 15.5, which we'll round to 15
            const til::point expected{ 16, 55 };

            VERIFY_ARE_EQUAL(expected, pt * scale);
        }

        Log::Comment(L"6.) Multiplication of doubles that should be in bounds.");
        {
            const til::point pt{ 3, 10 };
            const double scale = 5.5f;

            // 3 * 5.5 = 15.5, which we'll round to 15
            const til::point expected{ 16, 55 };

            VERIFY_ARE_EQUAL(expected, pt * scale);
        }

        Log::Comment(L"5.) Division of floats that should be in bounds.");
        {
            const til::point pt{ 15, 10 };
            const float scale = 2.0f;

            // 15 / 2 = 7.5, which we'll floor to 7
            const til::point expected{ 7, 5 };

            VERIFY_ARE_EQUAL(expected, pt / scale);
        }

        Log::Comment(L"6.) Division of doubles that should be in bounds.");
        {
            const til::point pt{ 15, 10 };
            const double scale = 2.0;

            // 15 / 2 = 7.5, which we'll floor to 7
            const til::point expected{ 7, 5 };

            VERIFY_ARE_EQUAL(expected, pt / scale);
        }
    }

    template<typename T>
    struct PointTypeWith_xy
    {
        T x, y;
    };
    template<typename T>
    struct PointTypeWith_XY
    {
        T X, Y;
    };
    TEST_METHOD(CastFromFloatWithMathTypes)
    {
        PointTypeWith_xy<float> xyFloatIntegral{ 1.f, 2.f };
        PointTypeWith_xy<float> xyFloat{ 1.6f, 2.4f };
        PointTypeWith_XY<double> XYDoubleIntegral{ 3., 4. };
        PointTypeWith_XY<double> XYDouble{ 3.6, 4.4 };
        Log::Comment(L"0.) Ceiling");
        {
            {
                til::point converted{ til::math::ceiling, xyFloatIntegral };
                VERIFY_ARE_EQUAL((til::point{ 1, 2 }), converted);
            }
            {
                til::point converted{ til::math::ceiling, xyFloat };
                VERIFY_ARE_EQUAL((til::point{ 2, 3 }), converted);
            }
            {
                til::point converted{ til::math::ceiling, XYDoubleIntegral };
                VERIFY_ARE_EQUAL((til::point{ 3, 4 }), converted);
            }
            {
                til::point converted{ til::math::ceiling, XYDouble };
                VERIFY_ARE_EQUAL((til::point{ 4, 5 }), converted);
            }
        }

        Log::Comment(L"1.) Flooring");
        {
            {
                til::point converted{ til::math::flooring, xyFloatIntegral };
                VERIFY_ARE_EQUAL((til::point{ 1, 2 }), converted);
            }
            {
                til::point converted{ til::math::flooring, xyFloat };
                VERIFY_ARE_EQUAL((til::point{ 1, 2 }), converted);
            }
            {
                til::point converted{ til::math::flooring, XYDoubleIntegral };
                VERIFY_ARE_EQUAL((til::point{ 3, 4 }), converted);
            }
            {
                til::point converted{ til::math::flooring, XYDouble };
                VERIFY_ARE_EQUAL((til::point{ 3, 4 }), converted);
            }
        }

        Log::Comment(L"2.) Rounding");
        {
            {
                til::point converted{ til::math::rounding, xyFloatIntegral };
                VERIFY_ARE_EQUAL((til::point{ 1, 2 }), converted);
            }
            {
                til::point converted{ til::math::rounding, xyFloat };
                VERIFY_ARE_EQUAL((til::point{ 2, 2 }), converted);
            }
            {
                til::point converted{ til::math::rounding, XYDoubleIntegral };
                VERIFY_ARE_EQUAL((til::point{ 3, 4 }), converted);
            }
            {
                til::point converted{ til::math::rounding, XYDouble };
                VERIFY_ARE_EQUAL((til::point{ 4, 4 }), converted);
            }
        }

        Log::Comment(L"3.) Truncating");
        {
            {
                til::point converted{ til::math::truncating, xyFloatIntegral };
                VERIFY_ARE_EQUAL((til::point{ 1, 2 }), converted);
            }
            {
                til::point converted{ til::math::truncating, xyFloat };
                VERIFY_ARE_EQUAL((til::point{ 1, 2 }), converted);
            }
            {
                til::point converted{ til::math::truncating, XYDoubleIntegral };
                VERIFY_ARE_EQUAL((til::point{ 3, 4 }), converted);
            }
            {
                til::point converted{ til::math::truncating, XYDouble };
                VERIFY_ARE_EQUAL((til::point{ 3, 4 }), converted);
            }
        }
    }
};
