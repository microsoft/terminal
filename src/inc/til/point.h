// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    using CoordType = int32_t;

    namespace details
    {
        template<typename T, typename U = T>
        constexpr U extract(const ::base::CheckedNumeric<T>& num)
        {
            U val;
            if (!num.AssignIfValid(&val))
            {
                throw gsl::narrowing_error{};
            }
            return val;
        }
    }

    struct point
    {
        CoordType x = 0;
        CoordType y = 0;

        constexpr point() noexcept = default;

        constexpr point(CoordType x, CoordType y) noexcept :
            x{ x }, y{ y }
        {
        }

        // This template will convert to point from floating-point args;
        // a math type is required. If you _don't_ provide one, you're going to
        // get a compile-time error about "cannot convert from initializer-list to til::point"
        template<typename TilMath, typename T>
        constexpr point(TilMath, const T x, const T y) :
            x{ TilMath::template cast<CoordType>(x) }, y{ TilMath::template cast<CoordType>(y) }
        {
        }

        constexpr bool operator==(const point rhs) const noexcept
        {
            // `__builtin_memcmp` isn't an official standard, but it's the
            // only way at the time of writing to get a constexpr `memcmp`.
            return __builtin_memcmp(this, &rhs, sizeof(rhs)) == 0;
        }

        constexpr bool operator!=(const point rhs) const noexcept
        {
            return __builtin_memcmp(this, &rhs, sizeof(rhs)) != 0;
        }

        constexpr bool operator<(const point other) const noexcept
        {
            return y < other.y || (y == other.y && x < other.x);
        }

        constexpr bool operator<=(const point other) const noexcept
        {
            return y < other.y || (y == other.y && x <= other.x);
        }

        constexpr bool operator>(const point other) const noexcept
        {
            return y > other.y || (y == other.y && x > other.x);
        }

        constexpr bool operator>=(const point other) const noexcept
        {
            return y > other.y || (y == other.y && x >= other.x);
        }

        constexpr point operator+(const point other) const
        {
            return point{
                details::extract(::base::CheckAdd(x, other.x)),
                details::extract(::base::CheckAdd(y, other.y)),
            };
        }

        constexpr point& operator+=(const point other)
        {
            *this = *this + other;
            return *this;
        }

        constexpr point operator-(const point other) const
        {
            return point{
                details::extract(::base::CheckSub(x, other.x)),
                details::extract(::base::CheckSub(y, other.y)),
            };
        }

        constexpr point& operator-=(const point other)
        {
            *this = *this - other;
            return *this;
        }

        constexpr point operator*(const point other) const
        {
            return point{
                details::extract(::base::CheckMul(x, other.x)),
                details::extract(::base::CheckMul(y, other.y)),
            };
        }

        constexpr point& operator*=(const point other)
        {
            *this = *this * other;
            return *this;
        }

        constexpr point operator/(const point other) const
        {
            return point{
                details::extract(::base::CheckDiv(x, other.x)),
                details::extract(::base::CheckDiv(y, other.y)),
            };
        }

        constexpr point& operator/=(const point other)
        {
            *this = *this / other;
            return *this;
        }

        template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
        constexpr point operator*(const T scale) const
        {
            return point{
                details::extract(::base::CheckMul(x, scale)),
                details::extract(::base::CheckMul(y, scale)),
            };
        }

        template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
        constexpr point operator/(const T scale) const
        {
            return point{
                details::extract(::base::CheckDiv(x, scale)),
                details::extract(::base::CheckDiv(y, scale)),
            };
        }

        template<typename T>
        constexpr T narrow_x() const
        {
            return gsl::narrow<T>(x);
        }

        template<typename T>
        constexpr T narrow_y() const
        {
            return gsl::narrow<T>(y);
        }

#ifdef _WINCONTYPES_
        explicit constexpr point(const COORD other) noexcept :
            x{ other.X }, y{ other.Y }
        {
        }

        constexpr COORD to_win32_coord() const
        {
            return { narrow_x<short>(), narrow_y<short>() };
        }
#endif

#ifdef _WINDEF_
        explicit constexpr point(const POINT other) noexcept :
            x{ other.x }, y{ other.y }
        {
        }

        constexpr POINT to_win32_point() const noexcept
        {
            return { x, y };
        }
#endif

#ifdef DCOMMON_H_INCLUDED
        template<typename TilMath>
        constexpr point(TilMath, const D2D1_POINT_2F other) :
            x{ TilMath::template cast<CoordType>(other.x) },
            y{ TilMath::template cast<CoordType>(other.y) }
        {
        }

        constexpr D2D1_POINT_2F to_d2d_point() const noexcept
        {
            return { static_cast<float>(x), static_cast<float>(y) };
        }
#endif

#ifdef WINRT_Windows_Foundation_H
        template<typename TilMath>
        constexpr point(TilMath, const winrt::Windows::Foundation::Point other) :
            x{ TilMath::template cast<CoordType>(other.X) },
            y{ TilMath::template cast<CoordType>(other.Y) }
        {
        }

        winrt::Windows::Foundation::Point to_winrt_point() const noexcept
        {
            return { static_cast<float>(x), static_cast<float>(y) };
        }
#endif

#ifdef WINRT_Microsoft_Terminal_Core_H
        explicit constexpr point(const winrt::Microsoft::Terminal::Core::Point other) :
            x{ other.X }, y{ other.Y }
        {
        }

        winrt::Microsoft::Terminal::Core::Point to_core_point() const noexcept
        {
            return { x, y };
        }
#endif

        std::wstring to_string() const
        {
            return wil::str_printf<std::wstring>(L"(X:%d, Y:%d)", x, y);
        }
    };
}

#ifdef __WEX_COMMON_H__
namespace WEX::TestExecution
{
    template<>
    class VerifyOutputTraits<til::point>
    {
    public:
        static WEX::Common::NoThrowString ToString(const til::point point)
        {
            return WEX::Common::NoThrowString(point.to_string().c_str());
        }
    };

    template<>
    class VerifyCompareTraits<til::point, til::point>
    {
    public:
        static constexpr bool AreEqual(const til::point expected, const til::point actual) noexcept
        {
            return expected == actual;
        }

        static constexpr bool AreSame(const til::point expected, const til::point actual) noexcept
        {
            return &expected == &actual;
        }

        static constexpr bool IsLessThan(const til::point expectedLess, const til::point expectedGreater) = delete;

        static constexpr bool IsGreaterThan(const til::point expectedGreater, const til::point expectedLess) = delete;

        static constexpr bool IsNull(const til::point object) noexcept
        {
            return object == til::point{};
        }
    };
};
#endif
