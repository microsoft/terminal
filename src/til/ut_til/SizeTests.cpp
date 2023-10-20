// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "til/size.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

// Ensure the "safety" of til::point::as_win32_size
static_assert(
    sizeof(til::size) == sizeof(SIZE) &&
    alignof(til::size) == alignof(SIZE) &&
    offsetof(til::size, width) == offsetof(SIZE, cx) &&
    offsetof(til::size, height) == offsetof(SIZE, cy));

class SizeTests
{
    TEST_CLASS(SizeTests);

    TEST_METHOD(DefaultConstruct)
    {
        const til::size sz;
        VERIFY_ARE_EQUAL(0, sz.width);
        VERIFY_ARE_EQUAL(0, sz.height);
    }

    TEST_METHOD(RawConstruct)
    {
        const til::size sz{ 5, 10 };
        VERIFY_ARE_EQUAL(5, sz.width);
        VERIFY_ARE_EQUAL(10, sz.height);
    }

    TEST_METHOD(RawFloatingConstruct)
    {
        const til::size sz{ til::math::rounding, 3.2f, 7.8f };
        VERIFY_ARE_EQUAL(3, sz.width);
        VERIFY_ARE_EQUAL(8, sz.height);
    }

    TEST_METHOD(SignedConstruct)
    {
        const auto width = -5;
        const auto height = -10;

        const til::size sz{ width, height };
        VERIFY_ARE_EQUAL(width, sz.width);
        VERIFY_ARE_EQUAL(height, sz.height);
    }

    TEST_METHOD(CoordConstruct)
    {
        COORD coord{ -5, 10 };

        const auto sz = til::wrap_coord_size(coord);
        VERIFY_ARE_EQUAL(coord.X, sz.width);
        VERIFY_ARE_EQUAL(coord.Y, sz.height);
    }

    TEST_METHOD(SizeConstruct)
    {
        SIZE size{ 5, -10 };

        const til::size sz{ size };
        VERIFY_ARE_EQUAL(size.cx, sz.width);
        VERIFY_ARE_EQUAL(size.cy, sz.height);
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

            const til::size expected{ sz.width + sz2.width, sz.height + sz2.height };

            VERIFY_ARE_EQUAL(expected, sz + sz2);
        }

        Log::Comment(L"1.) Addition results in value that is too large (width).");
        {
            constexpr auto bigSize = std::numeric_limits<til::CoordType>().max();
            const til::size sz{ bigSize, static_cast<til::CoordType>(0) };
            const til::size sz2{ 1, 1 };

            auto fn = [&]() {
                sz + sz2;
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
        }

        Log::Comment(L"2.) Addition results in value that is too large (height).");
        {
            constexpr auto bigSize = std::numeric_limits<til::CoordType>().max();
            const til::size sz{ static_cast<til::CoordType>(0), bigSize };
            const til::size sz2{ 1, 1 };

            auto fn = [&]() {
                sz + sz2;
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
        }
    }

    TEST_METHOD(Subtraction)
    {
        Log::Comment(L"0.) Subtraction of two things that should be in bounds.");
        {
            const til::size sz{ 5, 10 };
            const til::size sz2{ 23, 47 };

            const til::size expected{ sz.width - sz2.width, sz.height - sz2.height };

            VERIFY_ARE_EQUAL(expected, sz - sz2);
        }

        Log::Comment(L"1.) Subtraction results in value that is too small (width).");
        {
            constexpr auto bigSize = std::numeric_limits<til::CoordType>().max();
            const til::size sz{ bigSize, static_cast<til::CoordType>(0) };
            const til::size sz2{ -2, -2 };

            auto fn = [&]() {
                sz2 - sz;
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
        }

        Log::Comment(L"2.) Subtraction results in value that is too small (height).");
        {
            constexpr auto bigSize = std::numeric_limits<til::CoordType>().max();
            const til::size sz{ static_cast<til::CoordType>(0), bigSize };
            const til::size sz2{ -2, -2 };

            auto fn = [&]() {
                sz2 - sz;
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
        }
    }

    TEST_METHOD(Multiplication)
    {
        Log::Comment(L"0.) Multiplication of two things that should be in bounds.");
        {
            const til::size sz{ 5, 10 };
            const til::size sz2{ 23, 47 };

            const til::size expected{ sz.width * sz2.width, sz.height * sz2.height };

            VERIFY_ARE_EQUAL(expected, sz * sz2);
        }

        Log::Comment(L"1.) Multiplication results in value that is too large (width).");
        {
            constexpr auto bigSize = std::numeric_limits<til::CoordType>().max();
            const til::size sz{ bigSize, static_cast<til::CoordType>(0) };
            const til::size sz2{ 10, 10 };

            auto fn = [&]() {
                sz* sz2;
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
        }

        Log::Comment(L"2.) Multiplication results in value that is too large (height).");
        {
            constexpr auto bigSize = std::numeric_limits<til::CoordType>().max();
            const til::size sz{ static_cast<til::CoordType>(0), bigSize };
            const til::size sz2{ 10, 10 };

            auto fn = [&]() {
                sz* sz2;
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
        }
    }

    TEST_METHOD(ScaleByFloat)
    {
        Log::Comment(L"0.) Scale that should be in bounds.");
        {
            const til::size sz{ 5, 10 };
            const auto scale = 1.783f;

            const til::size expected{ static_cast<til::CoordType>(ceil(5 * scale)), static_cast<til::CoordType>(ceil(10 * scale)) };

            const auto actual = sz.scale(til::math::ceiling, scale);

            VERIFY_ARE_EQUAL(expected, actual);
        }

        Log::Comment(L"1.) Scale results in value that is too large.");
        {
            const til::size sz{ 5, 10 };
            constexpr auto scale = 1e12f;

            auto fn = [&]() {
                std::ignore = sz.scale(til::math::ceiling, scale);
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
        }
    }

    TEST_METHOD(Division)
    {
        Log::Comment(L"0.) Division of two things that should be in bounds.");
        {
            const til::size sz{ 555, 510 };
            const til::size sz2{ 23, 47 };

            const til::size expected{ sz.width / sz2.width, sz.height / sz2.height };

            VERIFY_ARE_EQUAL(expected, sz / sz2);
        }

        Log::Comment(L"1.) Division by zero");
        {
            constexpr auto bigSize = std::numeric_limits<til::CoordType>().max();
            const til::size sz{ bigSize, static_cast<til::CoordType>(0) };
            const til::size sz2{ 1, 1 };

            auto fn = [&]() {
                sz2 / sz;
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
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

            auto fn = [&]() {
                std::ignore = sz.divide_ceil(divisor);
            };

            VERIFY_THROWS(fn(), std::invalid_argument);
        }
    }

    TEST_METHOD(WidthCast)
    {
        const til::size sz{ 5, 10 };
        VERIFY_ARE_EQUAL(static_cast<SHORT>(sz.width), sz.narrow_width<SHORT>());
    }

    TEST_METHOD(HeightCast)
    {
        const til::size sz{ 5, 10 };
        VERIFY_ARE_EQUAL(static_cast<SHORT>(sz.height), sz.narrow_height<SHORT>());
    }

    TEST_METHOD(Area)
    {
        Log::Comment(L"0.) Area of two things that should be in bounds.");
        {
            const til::size sz{ 5, 10 };
            VERIFY_ARE_EQUAL(sz.width * sz.height, sz.area());
        }

        Log::Comment(L"1.) Area is out of bounds on multiplication.");
        {
            constexpr auto bigSize = std::numeric_limits<til::CoordType>().max();
            const til::size sz{ bigSize, bigSize };

            auto fn = [&]() {
                sz.area();
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
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
            constexpr til::CoordType bigSize = std::numeric_limits<SHORT>().max();
            const til::size sz{ bigSize, bigSize };

            auto fn = [&]() {
                sz.area<SHORT>();
            };

            VERIFY_THROWS(fn(), gsl::narrowing_error);
        }
    }

    TEST_METHOD(CastToSize)
    {
        Log::Comment(L"0.) Typical situation.");
        {
            const til::size sz{ 5, 10 };
            auto val = sz.to_win32_size();
            VERIFY_ARE_EQUAL(5, val.cx);
            VERIFY_ARE_EQUAL(10, val.cy);
        }

        Log::Comment(L"1.) Fit max width into SIZE (may overflow).");
        {
            constexpr auto width = std::numeric_limits<til::CoordType>().max();
            const auto height = 10;
            const til::size sz{ width, height };

            // On some platforms, til::CoordType will fit inside cx/cy
            const auto overflowExpected = width > std::numeric_limits<decltype(SIZE::cx)>().max();

            if (overflowExpected)
            {
                auto fn = [&]() {
                    auto val = sz.to_win32_size();
                };

                VERIFY_THROWS(fn(), gsl::narrowing_error);
            }
            else
            {
                auto val = sz.to_win32_size();
                VERIFY_ARE_EQUAL(width, val.cx);
            }
        }

        Log::Comment(L"2.) Fit max height into SIZE (may overflow).");
        {
            constexpr auto height = std::numeric_limits<til::CoordType>().max();
            const auto width = 10;
            const til::size sz{ width, height };

            // On some platforms, til::CoordType will fit inside cx/cy
            const auto overflowExpected = height > std::numeric_limits<decltype(SIZE::cy)>().max();

            if (overflowExpected)
            {
                auto fn = [&]() {
                    auto val = sz.to_win32_size();
                };

                VERIFY_THROWS(fn(), gsl::narrowing_error);
            }
            else
            {
                auto val = sz.to_win32_size();
                VERIFY_ARE_EQUAL(height, val.cy);
            }
        }
    }

    TEST_METHOD(CastToD2D1SizeF)
    {
        Log::Comment(L"0.) Typical situation.");
        {
            const til::size sz{ 5, 10 };
            auto val = sz.to_d2d_size();
            VERIFY_ARE_EQUAL(5, val.width);
            VERIFY_ARE_EQUAL(10, val.height);
        }

        // All til::CoordTypes fit into a float, so there's no exception tests.
    }

    TEST_METHOD(CastFromFloatWithMathTypes)
    {
        Log::Comment(L"0.) Ceiling");
        {
            {
                til::size converted{ til::math::ceiling, 1.f, 2.f };
                VERIFY_ARE_EQUAL((til::size{ 1, 2 }), converted);
            }
            {
                til::size converted{ til::math::ceiling, 1.6f, 2.4f };
                VERIFY_ARE_EQUAL((til::size{ 2, 3 }), converted);
            }
            {
                til::size converted{ til::math::ceiling, 3., 4. };
                VERIFY_ARE_EQUAL((til::size{ 3, 4 }), converted);
            }
            {
                til::size converted{ til::math::ceiling, 3.6, 4.4 };
                VERIFY_ARE_EQUAL((til::size{ 4, 5 }), converted);
            }
            {
                til::size converted{ til::math::ceiling, 5., 6. };
                VERIFY_ARE_EQUAL((til::size{ 5, 6 }), converted);
            }
            {
                til::size converted{ til::math::ceiling, 5.6, 6.4 };
                VERIFY_ARE_EQUAL((til::size{ 6, 7 }), converted);
            }
        }

        Log::Comment(L"1.) Flooring");
        {
            {
                til::size converted{ til::math::flooring, 1.f, 2.f };
                VERIFY_ARE_EQUAL((til::size{ 1, 2 }), converted);
            }
            {
                til::size converted{ til::math::flooring, 1.6f, 2.4f };
                VERIFY_ARE_EQUAL((til::size{ 1, 2 }), converted);
            }
            {
                til::size converted{ til::math::flooring, 3., 4. };
                VERIFY_ARE_EQUAL((til::size{ 3, 4 }), converted);
            }
            {
                til::size converted{ til::math::flooring, 3.6, 4.4 };
                VERIFY_ARE_EQUAL((til::size{ 3, 4 }), converted);
            }
            {
                til::size converted{ til::math::flooring, 5., 6. };
                VERIFY_ARE_EQUAL((til::size{ 5, 6 }), converted);
            }
            {
                til::size converted{ til::math::flooring, 5.6, 6.4 };
                VERIFY_ARE_EQUAL((til::size{ 5, 6 }), converted);
            }
        }

        Log::Comment(L"2.) Rounding");
        {
            {
                til::size converted{ til::math::rounding, 1.f, 2.f };
                VERIFY_ARE_EQUAL((til::size{ 1, 2 }), converted);
            }
            {
                til::size converted{ til::math::rounding, 1.6f, 2.4f };
                VERIFY_ARE_EQUAL((til::size{ 2, 2 }), converted);
            }
            {
                til::size converted{ til::math::rounding, 3., 4. };
                VERIFY_ARE_EQUAL((til::size{ 3, 4 }), converted);
            }
            {
                til::size converted{ til::math::rounding, 3.6, 4.4 };
                VERIFY_ARE_EQUAL((til::size{ 4, 4 }), converted);
            }
            {
                til::size converted{ til::math::rounding, 5., 6. };
                VERIFY_ARE_EQUAL((til::size{ 5, 6 }), converted);
            }
            {
                til::size converted{ til::math::rounding, 5.6, 6.4 };
                VERIFY_ARE_EQUAL((til::size{ 6, 6 }), converted);
            }
        }
    }
};
