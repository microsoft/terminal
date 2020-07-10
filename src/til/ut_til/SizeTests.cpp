// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "til/size.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class SizeTests
{
    TEST_CLASS(SizeTests);

    TEST_METHOD(DefaultConstruct)
    {
        const til::size sz;
        VERIFY_ARE_EQUAL(0, sz._width);
        VERIFY_ARE_EQUAL(0, sz._height);
    }

    TEST_METHOD(RawConstruct)
    {
        const til::size sz{ 5, 10 };
        VERIFY_ARE_EQUAL(5, sz._width);
        VERIFY_ARE_EQUAL(10, sz._height);
    }

    TEST_METHOD(RawFloatingConstruct)
    {
        const til::size sz{ til::math::rounding, 3.2f, 7.8f };
        VERIFY_ARE_EQUAL(3, sz._width);
        VERIFY_ARE_EQUAL(8, sz._height);
    }

    TEST_METHOD(UnsignedConstruct)
    {
        Log::Comment(L"0.) Normal unsigned construct.");
        {
            const size_t width = 5;
            const size_t height = 10;

            const til::size sz{ width, height };
            VERIFY_ARE_EQUAL(5, sz._width);
            VERIFY_ARE_EQUAL(10, sz._height);
        }

        Log::Comment(L"1.) Unsigned construct overflow on width.");
        {
            constexpr size_t width = std::numeric_limits<size_t>().max();
            const size_t height = 10;

            auto fn = [&]() {
                til::size sz{ width, height };
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Unsigned construct overflow on height.");
        {
            constexpr size_t height = std::numeric_limits<size_t>().max();
            const size_t width = 10;

            auto fn = [&]() {
                til::size sz{ width, height };
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(SignedConstruct)
    {
        const ptrdiff_t width = -5;
        const ptrdiff_t height = -10;

        const til::size sz{ width, height };
        VERIFY_ARE_EQUAL(width, sz._width);
        VERIFY_ARE_EQUAL(height, sz._height);
    }

    TEST_METHOD(MixedRawTypeConstruct)
    {
        const ptrdiff_t a = -5;
        const int b = -10;

        Log::Comment(L"Case 1: ptrdiff_t/int");
        {
            const til::size sz{ a, b };
            VERIFY_ARE_EQUAL(a, sz._width);
            VERIFY_ARE_EQUAL(b, sz._height);
        }

        Log::Comment(L"Case 2: int/ptrdiff_t");
        {
            const til::size sz{ b, a };
            VERIFY_ARE_EQUAL(b, sz._width);
            VERIFY_ARE_EQUAL(a, sz._height);
        }
    }

    TEST_METHOD(CoordConstruct)
    {
        COORD coord{ -5, 10 };

        const til::size sz{ coord };
        VERIFY_ARE_EQUAL(coord.X, sz._width);
        VERIFY_ARE_EQUAL(coord.Y, sz._height);
    }

    TEST_METHOD(SizeConstruct)
    {
        SIZE size{ 5, -10 };

        const til::size sz{ size };
        VERIFY_ARE_EQUAL(size.cx, sz._width);
        VERIFY_ARE_EQUAL(size.cy, sz._height);
    }

    TEST_METHOD(Equality)
    {
        Log::Comment(L"0.) Equal.");
        {
            const til::size s1{ 5, 10 };
            const til::size s2{ 5, 10 };
            VERIFY_IS_TRUE(s1 == s2);
        }

        Log::Comment(L"1.) Left Width changed.");
        {
            const til::size s1{ 4, 10 };
            const til::size s2{ 5, 10 };
            VERIFY_IS_FALSE(s1 == s2);
        }

        Log::Comment(L"2.) Right Width changed.");
        {
            const til::size s1{ 5, 10 };
            const til::size s2{ 6, 10 };
            VERIFY_IS_FALSE(s1 == s2);
        }

        Log::Comment(L"3.) Left Height changed.");
        {
            const til::size s1{ 5, 9 };
            const til::size s2{ 5, 10 };
            VERIFY_IS_FALSE(s1 == s2);
        }

        Log::Comment(L"4.) Right Height changed.");
        {
            const til::size s1{ 5, 10 };
            const til::size s2{ 5, 11 };
            VERIFY_IS_FALSE(s1 == s2);
        }
    }

    TEST_METHOD(Inequality)
    {
        Log::Comment(L"0.) Equal.");
        {
            const til::size s1{ 5, 10 };
            const til::size s2{ 5, 10 };
            VERIFY_IS_FALSE(s1 != s2);
        }

        Log::Comment(L"1.) Left Width changed.");
        {
            const til::size s1{ 4, 10 };
            const til::size s2{ 5, 10 };
            VERIFY_IS_TRUE(s1 != s2);
        }

        Log::Comment(L"2.) Right Width changed.");
        {
            const til::size s1{ 5, 10 };
            const til::size s2{ 6, 10 };
            VERIFY_IS_TRUE(s1 != s2);
        }

        Log::Comment(L"3.) Left Height changed.");
        {
            const til::size s1{ 5, 9 };
            const til::size s2{ 5, 10 };
            VERIFY_IS_TRUE(s1 != s2);
        }

        Log::Comment(L"4.) Right Height changed.");
        {
            const til::size s1{ 5, 10 };
            const til::size s2{ 5, 11 };
            VERIFY_IS_TRUE(s1 != s2);
        }
    }

    TEST_METHOD(Boolean)
    {
        const til::size empty;
        VERIFY_IS_FALSE(!!empty);

        const til::size yOnly{ 0, 10 };
        VERIFY_IS_FALSE(!!yOnly);

        const til::size xOnly{ 10, 0 };
        VERIFY_IS_FALSE(!!xOnly);

        const til::size both{ 10, 10 };
        VERIFY_IS_TRUE(!!both);

        const til::size yNegative{ 10, -10 };
        VERIFY_IS_FALSE(!!yNegative);

        const til::size xNegative{ -10, 10 };
        VERIFY_IS_FALSE(!!xNegative);

        const til::size bothNegative{ -10, -10 };
        VERIFY_IS_FALSE(!!bothNegative);
    }

    TEST_METHOD(Addition)
    {
        Log::Comment(L"0.) Addition of two things that should be in bounds.");
        {
            const til::size sz{ 5, 10 };
            const til::size sz2{ 23, 47 };

            const til::size expected{ sz.width() + sz2.width(), sz.height() + sz2.height() };

            VERIFY_ARE_EQUAL(expected, sz + sz2);
        }

        Log::Comment(L"1.) Addition results in value that is too large (width).");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::size sz{ bigSize, static_cast<ptrdiff_t>(0) };
            const til::size sz2{ 1, 1 };

            auto fn = [&]() {
                sz + sz2;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Addition results in value that is too large (height).");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::size sz{ static_cast<ptrdiff_t>(0), bigSize };
            const til::size sz2{ 1, 1 };

            auto fn = [&]() {
                sz + sz2;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(Subtraction)
    {
        Log::Comment(L"0.) Subtraction of two things that should be in bounds.");
        {
            const til::size sz{ 5, 10 };
            const til::size sz2{ 23, 47 };

            const til::size expected{ sz.width() - sz2.width(), sz.height() - sz2.height() };

            VERIFY_ARE_EQUAL(expected, sz - sz2);
        }

        Log::Comment(L"1.) Subtraction results in value that is too small (width).");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::size sz{ bigSize, static_cast<ptrdiff_t>(0) };
            const til::size sz2{ -2, -2 };

            auto fn = [&]() {
                sz2 - sz;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Subtraction results in value that is too small (height).");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::size sz{ static_cast<ptrdiff_t>(0), bigSize };
            const til::size sz2{ -2, -2 };

            auto fn = [&]() {
                sz2 - sz;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(Multiplication)
    {
        Log::Comment(L"0.) Multiplication of two things that should be in bounds.");
        {
            const til::size sz{ 5, 10 };
            const til::size sz2{ 23, 47 };

            const til::size expected{ sz.width() * sz2.width(), sz.height() * sz2.height() };

            VERIFY_ARE_EQUAL(expected, sz * sz2);
        }

        Log::Comment(L"1.) Multiplication results in value that is too large (width).");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::size sz{ bigSize, static_cast<ptrdiff_t>(0) };
            const til::size sz2{ 10, 10 };

            auto fn = [&]() {
                sz* sz2;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Multiplication results in value that is too large (height).");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::size sz{ static_cast<ptrdiff_t>(0), bigSize };
            const til::size sz2{ 10, 10 };

            auto fn = [&]() {
                sz* sz2;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(ScaleByFloat)
    {
        Log::Comment(L"0.) Scale that should be in bounds.");
        {
            const til::size sz{ 5, 10 };
            const float scale = 1.783f;

            const til::size expected{ static_cast<ptrdiff_t>(ceil(5 * scale)), static_cast<ptrdiff_t>(ceil(10 * scale)) };

            const auto actual = sz.scale(til::math::ceiling, scale);

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"1.) Scale results in value that is too large.");
        {
            const til::size sz{ 5, 10 };
            constexpr float scale = std::numeric_limits<float>().max();

            auto fn = [&]() {
                sz.scale(til::math::ceiling, scale);
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(Division)
    {
        Log::Comment(L"0.) Division of two things that should be in bounds.");
        {
            const til::size sz{ 555, 510 };
            const til::size sz2{ 23, 47 };

            const til::size expected{ sz.width() / sz2.width(), sz.height() / sz2.height() };

            VERIFY_ARE_EQUAL(expected, sz / sz2);
        }

        Log::Comment(L"1.) Division by zero");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::size sz{ bigSize, static_cast<ptrdiff_t>(0) };
            const til::size sz2{ 1, 1 };

            auto fn = [&]() {
                sz2 / sz;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(DivisionRoundingUp)
    {
        Log::Comment(L"1.) Division rounding up with positive result.");
        {
            const til::size sz{ 10, 5 };
            const til::size divisor{ 3, 2 };

            // 10 / 3 is 3.333, rounded up is 4.
            // 5 / 2 is 2.5, rounded up is 3.
            const til::size expected{ 4, 3 };

            VERIFY_ARE_EQUAL(expected, sz.divide_ceil(divisor));
        }

        Log::Comment(L"2.) Division rounding larger(up) with negative result.");
        {
            const til::size sz{ -10, -5 };
            const til::size divisor{ 3, 2 };

            // -10 / 3 is -3.333, rounded up is -4.
            // -5 / 2 is -2.5, rounded up is -3.
            const til::size expected{ -4, -3 };

            VERIFY_ARE_EQUAL(expected, sz.divide_ceil(divisor));
        }
    }

    TEST_METHOD(Width)
    {
        const til::size sz{ 5, 10 };
        VERIFY_ARE_EQUAL(sz._width, sz.width());
    }

    TEST_METHOD(WidthCast)
    {
        const til::size sz{ 5, 10 };
        VERIFY_ARE_EQUAL(static_cast<SHORT>(sz._width), sz.width<SHORT>());
    }

    TEST_METHOD(Height)
    {
        const til::size sz{ 5, 10 };
        VERIFY_ARE_EQUAL(sz._height, sz.height());
    }

    TEST_METHOD(HeightCast)
    {
        const til::size sz{ 5, 10 };
        VERIFY_ARE_EQUAL(static_cast<SHORT>(sz._height), sz.height<SHORT>());
    }

    TEST_METHOD(Area)
    {
        Log::Comment(L"0.) Area of two things that should be in bounds.");
        {
            const til::size sz{ 5, 10 };
            VERIFY_ARE_EQUAL(sz._width * sz._height, sz.area());
        }

        Log::Comment(L"1.) Area is out of bounds on multiplication.");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::size sz{ bigSize, bigSize };

            auto fn = [&]() {
                sz.area();
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }
    TEST_METHOD(AreaCast)
    {
        Log::Comment(L"0.) Area of two things that should be in bounds.");
        {
            const til::size sz{ 5, 10 };
            VERIFY_ARE_EQUAL(static_cast<SHORT>(sz.area()), sz.area<SHORT>());
        }

        Log::Comment(L"1.) Area is out of bounds on multiplication.");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<SHORT>().max();
            const til::size sz{ bigSize, bigSize };

            auto fn = [&]() {
                sz.area<SHORT>();
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(CastToCoord)
    {
        Log::Comment(L"0.) Typical situation.");
        {
            const til::size sz{ 5, 10 };
            COORD val = sz;
            VERIFY_ARE_EQUAL(5, val.X);
            VERIFY_ARE_EQUAL(10, val.Y);
        }

        Log::Comment(L"1.) Overflow on width.");
        {
            constexpr ptrdiff_t width = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t height = 10;
            const til::size sz{ width, height };

            auto fn = [&]() {
                COORD val = sz;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Overflow on height.");
        {
            constexpr ptrdiff_t height = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t width = 10;
            const til::size sz{ width, height };

            auto fn = [&]() {
                COORD val = sz;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(CastToSize)
    {
        Log::Comment(L"0.) Typical situation.");
        {
            const til::size sz{ 5, 10 };
            SIZE val = sz;
            VERIFY_ARE_EQUAL(5, val.cx);
            VERIFY_ARE_EQUAL(10, val.cy);
        }

        Log::Comment(L"1.) Fit max width into SIZE (may overflow).");
        {
            constexpr ptrdiff_t width = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t height = 10;
            const til::size sz{ width, height };

            // On some platforms, ptrdiff_t will fit inside cx/cy
            const bool overflowExpected = width > std::numeric_limits<decltype(SIZE::cx)>().max();

            if (overflowExpected)
            {
                auto fn = [&]() {
                    SIZE val = sz;
                };

                VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
            }
            else
            {
                SIZE val = sz;
                VERIFY_ARE_EQUAL(width, val.cx);
            }
        }

        Log::Comment(L"2.) Fit max height into SIZE (may overflow).");
        {
            constexpr ptrdiff_t height = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t width = 10;
            const til::size sz{ width, height };

            // On some platforms, ptrdiff_t will fit inside cx/cy
            const bool overflowExpected = height > std::numeric_limits<decltype(SIZE::cy)>().max();

            if (overflowExpected)
            {
                auto fn = [&]() {
                    SIZE val = sz;
                };

                VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
            }
            else
            {
                SIZE val = sz;
                VERIFY_ARE_EQUAL(height, val.cy);
            }
        }
    }

    TEST_METHOD(CastToD2D1SizeF)
    {
        Log::Comment(L"0.) Typical situation.");
        {
            const til::size sz{ 5, 10 };
            D2D1_SIZE_F val = sz;
            VERIFY_ARE_EQUAL(5, val.width);
            VERIFY_ARE_EQUAL(10, val.height);
        }

        // All ptrdiff_ts fit into a float, so there's no exception tests.
    }

    template<typename T>
    struct SizeTypeWith_XY
    {
        T X, Y;
    };
    template<typename T>
    struct SizeTypeWith_cxcy
    {
        T cx, cy;
    };
    template<typename T>
    struct SizeTypeWith_WidthHeight
    {
        T Width, Height;
    };
    TEST_METHOD(CastFromFloatWithMathTypes)
    {
        SizeTypeWith_XY<float> XYFloatIntegral{ 1.f, 2.f };
        SizeTypeWith_XY<float> XYFloat{ 1.6f, 2.4f };
        SizeTypeWith_cxcy<double> cxcyDoubleIntegral{ 3., 4. };
        SizeTypeWith_cxcy<double> cxcyDouble{ 3.6, 4.4 };
        SizeTypeWith_WidthHeight<double> WHDoubleIntegral{ 5., 6. };
        SizeTypeWith_WidthHeight<double> WHDouble{ 5.6, 6.4 };
        Log::Comment(L"0.) Ceiling");
        {
            {
                til::size converted{ til::math::ceiling, XYFloatIntegral };
                VERIFY_ARE_EQUAL((til::size{ 1, 2 }), converted);
            }
            {
                til::size converted{ til::math::ceiling, XYFloat };
                VERIFY_ARE_EQUAL((til::size{ 2, 3 }), converted);
            }
            {
                til::size converted{ til::math::ceiling, cxcyDoubleIntegral };
                VERIFY_ARE_EQUAL((til::size{ 3, 4 }), converted);
            }
            {
                til::size converted{ til::math::ceiling, cxcyDouble };
                VERIFY_ARE_EQUAL((til::size{ 4, 5 }), converted);
            }
            {
                til::size converted{ til::math::ceiling, WHDoubleIntegral };
                VERIFY_ARE_EQUAL((til::size{ 5, 6 }), converted);
            }
            {
                til::size converted{ til::math::ceiling, WHDouble };
                VERIFY_ARE_EQUAL((til::size{ 6, 7 }), converted);
            }
        }

        Log::Comment(L"1.) Flooring");
        {
            {
                til::size converted{ til::math::flooring, XYFloatIntegral };
                VERIFY_ARE_EQUAL((til::size{ 1, 2 }), converted);
            }
            {
                til::size converted{ til::math::flooring, XYFloat };
                VERIFY_ARE_EQUAL((til::size{ 1, 2 }), converted);
            }
            {
                til::size converted{ til::math::flooring, cxcyDoubleIntegral };
                VERIFY_ARE_EQUAL((til::size{ 3, 4 }), converted);
            }
            {
                til::size converted{ til::math::flooring, cxcyDouble };
                VERIFY_ARE_EQUAL((til::size{ 3, 4 }), converted);
            }
            {
                til::size converted{ til::math::flooring, WHDoubleIntegral };
                VERIFY_ARE_EQUAL((til::size{ 5, 6 }), converted);
            }
            {
                til::size converted{ til::math::flooring, WHDouble };
                VERIFY_ARE_EQUAL((til::size{ 5, 6 }), converted);
            }
        }

        Log::Comment(L"2.) Rounding");
        {
            {
                til::size converted{ til::math::rounding, XYFloatIntegral };
                VERIFY_ARE_EQUAL((til::size{ 1, 2 }), converted);
            }
            {
                til::size converted{ til::math::rounding, XYFloat };
                VERIFY_ARE_EQUAL((til::size{ 2, 2 }), converted);
            }
            {
                til::size converted{ til::math::rounding, cxcyDoubleIntegral };
                VERIFY_ARE_EQUAL((til::size{ 3, 4 }), converted);
            }
            {
                til::size converted{ til::math::rounding, cxcyDouble };
                VERIFY_ARE_EQUAL((til::size{ 4, 4 }), converted);
            }
            {
                til::size converted{ til::math::rounding, WHDoubleIntegral };
                VERIFY_ARE_EQUAL((til::size{ 5, 6 }), converted);
            }
            {
                til::size converted{ til::math::rounding, WHDouble };
                VERIFY_ARE_EQUAL((til::size{ 6, 6 }), converted);
            }
        }

        Log::Comment(L"3.) Truncating");
        {
            {
                til::size converted{ til::math::truncating, XYFloatIntegral };
                VERIFY_ARE_EQUAL((til::size{ 1, 2 }), converted);
            }
            {
                til::size converted{ til::math::truncating, XYFloat };
                VERIFY_ARE_EQUAL((til::size{ 1, 2 }), converted);
            }
            {
                til::size converted{ til::math::truncating, cxcyDoubleIntegral };
                VERIFY_ARE_EQUAL((til::size{ 3, 4 }), converted);
            }
            {
                til::size converted{ til::math::truncating, cxcyDouble };
                VERIFY_ARE_EQUAL((til::size{ 3, 4 }), converted);
            }
            {
                til::size converted{ til::math::truncating, WHDoubleIntegral };
                VERIFY_ARE_EQUAL((til::size{ 5, 6 }), converted);
            }
            {
                til::size converted{ til::math::truncating, WHDouble };
                VERIFY_ARE_EQUAL((til::size{ 5, 6 }), converted);
            }
        }
    }
};
