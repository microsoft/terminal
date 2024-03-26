// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    using CoordType = int32_t;
    inline constexpr CoordType CoordTypeMin = INT32_MIN;
    inline constexpr CoordType CoordTypeMax = INT32_MAX;

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

        constexpr explicit operator bool() const noexcept
        {
            return (x > 0) & (y > 0);
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
            auto copy = *this;
            copy += other;
            return copy;
        }

        constexpr point& operator+=(const point other)
        {
            x = details::extract(::base::CheckAdd(x, other.x));
            y = details::extract(::base::CheckAdd(y, other.y));
            return *this;
        }

        constexpr point operator-(const point other) const
        {
            auto copy = *this;
            copy -= other;
            return copy;
        }

        constexpr point& operator-=(const point other)
        {
            x = details::extract(::base::CheckSub(x, other.x));
            y = details::extract(::base::CheckSub(y, other.y));
            return *this;
        }

        constexpr point operator*(const point other) const
        {
            auto copy = *this;
            copy *= other;
            return copy;
        }

        constexpr point& operator*=(const point other)
        {
            x = details::extract(::base::CheckMul(x, other.x));
            y = details::extract(::base::CheckMul(y, other.y));
            return *this;
        }

        constexpr point operator/(const point other) const
        {
            auto copy = *this;
            copy /= other;
            return copy;
        }

        constexpr point& operator/=(const point other)
        {
            x = details::extract(::base::CheckDiv(x, other.x));
            y = details::extract(::base::CheckDiv(y, other.y));
            return *this;
        }

        constexpr point operator*(const til::CoordType scale) const
        {
            return point{
                details::extract(::base::CheckMul(x, scale)),
                details::extract(::base::CheckMul(y, scale)),
            };
        }

        constexpr point operator/(const til::CoordType scale) const
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

#ifdef _WINDEF_
        explicit constexpr point(const POINT other) noexcept :
            x{ other.x }, y{ other.y }
        {
        }

        constexpr POINT to_win32_point() const noexcept
        {
            return { x, y };
        }

        // til::point and POINT have the exact same layout at the time of writing,
        // so this function lets you unsafely "view" this point as a POINT
        // if you need to pass it to a Win32 function.
        //
        // Use as_win32_point() as sparingly as possible because it'll be a pain to hack
        // it out of this code base once til::point and POINT aren't the same anymore.
        // Prefer casting to POINT and back to til::point instead if possible.
        POINT* as_win32_point() noexcept
        {
#pragma warning(suppress : 26490) // Don't use reinterpret_cast (type.1).
            return std::launder(reinterpret_cast<POINT*>(this));
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

    constexpr point wrap_coord(const COORD pt) noexcept
    {
        return { pt.X, pt.Y };
    }

    constexpr COORD unwrap_coord(const point pt)
    {
        return {
            gsl::narrow<short>(pt.x),
            gsl::narrow<short>(pt.y),
        };
    }

    constexpr HRESULT unwrap_coord_hr(const point pt, COORD& out) noexcept
    {
        short x = 0;
        short y = 0;
        if (narrow_maybe(pt.x, x) && narrow_maybe(pt.y, y))
        {
            out.X = x;
            out.Y = y;
            return S_OK;
        }
        RETURN_WIN32(ERROR_UNHANDLED_EXCEPTION);
    }

    // point_span can be pictured as a "selection" range inside our text buffer. So given
    // a text buffer of 10x4, a start of 4,1 and end of 7,3 the span might look like this:
    //   +----------+
    //   |          |
    //   |    xxxxxx|
    //   |xxxxxxxxxx|
    //   |xxxxxxxx  |
    //   +----------+
    // At the time of writing there's a push to make selections have an exclusive end coordinate,
    // so the interpretation of end might change soon (making this comment potentially outdated).
    struct point_span
    {
        til::point start;
        til::point end;

        constexpr bool operator==(const point_span& rhs) const noexcept
        {
            // `__builtin_memcmp` isn't an official standard, but it's the
            // only way at the time of writing to get a constexpr `memcmp`.
            return __builtin_memcmp(this, &rhs, sizeof(rhs)) == 0;
        }

        constexpr bool operator!=(const point_span& rhs) const noexcept
        {
            return __builtin_memcmp(this, &rhs, sizeof(rhs)) != 0;
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
