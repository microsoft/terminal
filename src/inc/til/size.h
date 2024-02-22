// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "point.h"

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    struct size
    {
        constexpr size() noexcept = default;

        constexpr size(CoordType width, CoordType height) noexcept :
            width{ width }, height{ height }
        {
        }

        // This template will convert to size from floating-point args;
        // a math type is required. If you _don't_ provide one, you're going to
        // get a compile-time error about "cannot convert from initializer-list to til::size"
        template<typename TilMath, typename T>
        constexpr size(TilMath, const T width, const T height) :
            width{ TilMath::template cast<CoordType>(width) }, height{ TilMath::template cast<CoordType>(height) }
        {
        }

        constexpr bool operator==(const size rhs) const noexcept
        {
            // `__builtin_memcmp` isn't an official standard, but it's the
            // only way at the time of writing to get a constexpr `memcmp`.
            return __builtin_memcmp(this, &rhs, sizeof(rhs)) == 0;
        }

        constexpr bool operator!=(const size rhs) const noexcept
        {
            return __builtin_memcmp(this, &rhs, sizeof(rhs)) != 0;
        }

        constexpr explicit operator bool() const noexcept
        {
            return (width > 0) & (height > 0);
        }

        constexpr size operator+(const size other) const
        {
            return size{
                details::extract(::base::CheckAdd(width, other.width)),
                details::extract(::base::CheckAdd(height, other.height)),
            };
        }

        constexpr size operator-(const size other) const
        {
            return size{
                details::extract(::base::CheckSub(width, other.width)),
                details::extract(::base::CheckSub(height, other.height)),
            };
        }

        constexpr size operator*(const size other) const
        {
            return size{
                details::extract(::base::CheckMul(width, other.width)),
                details::extract(::base::CheckMul(height, other.height)),
            };
        }

        constexpr size operator/(const size other) const
        {
            return size{
                details::extract(::base::CheckDiv(width, other.width)),
                details::extract(::base::CheckDiv(height, other.height)),
            };
        }

        template<typename TilMath, typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>>
        [[nodiscard]] constexpr size scale(TilMath math, const T scale) const
        {
            return {
                math,
                width * scale,
                height * scale,
            };
        }

        [[nodiscard]] constexpr size divide_ceil(const size other) const
        {
            // The integer ceil division `((a - 1) / b) + 1` only works for numbers >0.
            // Support for negative numbers wasn't deemed useful at this point.
            if ((width < 0) | (height < 0) | (other.width <= 0) | (other.height <= 0))
            {
                throw std::invalid_argument{ "invalid til::size::divide_ceil" };
            }

            return {
                width != 0 ? (width - 1) / other.width + 1 : 0,
                height != 0 ? (height - 1) / other.height + 1 : 0,
            };
        }

        template<typename T = CoordType>
        constexpr T narrow_width() const
        {
            return gsl::narrow<T>(width);
        }

        template<typename T = CoordType>
        constexpr T narrow_height() const
        {
            return gsl::narrow<T>(height);
        }

        template<typename T = CoordType>
        constexpr T area() const
        {
            return gsl::narrow<T>(static_cast<int64_t>(width) * static_cast<int64_t>(height));
        }

#ifdef _WINDEF_
        explicit constexpr size(const SIZE other) noexcept :
            width{ other.cx }, height{ other.cy }
        {
        }

        constexpr SIZE to_win32_size() const noexcept
        {
            return { width, height };
        }

        // til::size and SIZE have the exact same layout at the time of writing,
        // so this function lets you unsafely "view" this size as a SIZE
        // if you need to pass it to a Win32 function.
        //
        // Use as_win32_size() as sparingly as possible because it'll be a pain to hack
        // it out of this code base once til::size and SIZE aren't the same anymore.
        // Prefer casting to SIZE and back to til::size instead if possible.
        SIZE* as_win32_size() noexcept
        {
#pragma warning(suppress : 26490) // Don't use reinterpret_cast (type.1).
            return std::launder(reinterpret_cast<SIZE*>(this));
        }
#endif

#ifdef DCOMMON_H_INCLUDED
        template<typename TilMath>
        constexpr size(TilMath, const D2D1_SIZE_F other) :
            width{ TilMath::template cast<CoordType>(other.width) },
            height{ TilMath::template cast<CoordType>(other.height) }
        {
        }

        constexpr D2D1_SIZE_F to_d2d_size() const noexcept
        {
            return { static_cast<float>(width), static_cast<float>(height) };
        }
#endif

#ifdef WINRT_Windows_Foundation_H
        template<typename TilMath>
        constexpr size(TilMath, const winrt::Windows::Foundation::Size other) :
            width{ TilMath::template cast<CoordType>(other.Width) },
            height{ TilMath::template cast<CoordType>(other.Height) }
        {
        }

        winrt::Windows::Foundation::Size to_winrt_size() const noexcept
        {
            return { static_cast<float>(width), static_cast<float>(height) };
        }
#endif

        std::wstring to_string() const
        {
            return wil::str_printf<std::wstring>(L"[W:%d, H:%d]", width, height);
        }

        CoordType width = 0;
        CoordType height = 0;
    };

    constexpr size wrap_coord_size(const COORD sz) noexcept
    {
        return { sz.X, sz.Y };
    }

    constexpr COORD unwrap_coord_size(const size sz)
    {
        return {
            gsl::narrow<short>(sz.width),
            gsl::narrow<short>(sz.height),
        };
    }

    constexpr HRESULT unwrap_coord_size_hr(const size sz, COORD& out) noexcept
    {
        short x = 0;
        short y = 0;
        if (narrow_maybe(sz.width, x) && narrow_maybe(sz.height, y))
        {
            out.X = x;
            out.Y = y;
            return S_OK;
        }
        RETURN_WIN32(ERROR_UNHANDLED_EXCEPTION);
    }
};

#ifdef __WEX_COMMON_H__
namespace WEX::TestExecution
{
    template<>
    class VerifyOutputTraits<til::size>
    {
    public:
        static WEX::Common::NoThrowString ToString(const til::size size)
        {
            return WEX::Common::NoThrowString(size.to_string().c_str());
        }
    };

    template<>
    class VerifyCompareTraits<til::size, til::size>
    {
    public:
        static bool AreEqual(const til::size expected, const til::size actual) noexcept
        {
            return expected == actual;
        }

        static bool AreSame(const til::size expected, const til::size actual) noexcept
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const til::size expectedLess, const til::size expectedGreater) = delete;

        static bool IsGreaterThan(const til::size expectedGreater, const til::size expectedLess) = delete;

        static bool IsNull(const til::size object) noexcept
        {
            return object == til::size{};
        }
    };
};
#endif
