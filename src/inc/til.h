// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "til/some.h"

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    // The at function declares that you've already sufficiently checked that your array access
    // is in range before retrieving an item inside it at an offset.
    // This is to save double/triple/quadruple testing in circumstances where you are already
    // pivoting on the length of a set and now want to pull elements out of it by offset
    // without checking again.
    // gsl::at will do the check again. As will .at(). And using [] will have a warning in audit.
    template<class T>
    constexpr auto at(T& cont, const size_t i) -> decltype(cont[cont.size()])
    {
#pragma warning(suppress : 26482) // Suppress bounds.2 check for indexing with constant expressions
#pragma warning(suppress : 26446) // Suppress bounds.4 check for subscript operator.
        return cont[i];
    }

    // color is a universal integral 8bpp RGBA (0-255) color type implicitly convertible to/from
    // a number of other color types.
#pragma warning(push)
    // we can't depend on GSL here (some libraries use BLOCK_GSL), so we use static_cast for explicit narrowing
#pragma warning(disable : 26472)
    struct color
    {
        uint8_t r, g, b, a;

        constexpr color() noexcept :
            r{ 0 },
            g{ 0 },
            b{ 0 },
            a{ 0 } {}

        constexpr color(uint8_t _r, uint8_t _g, uint8_t _b) noexcept :
            r{ _r },
            g{ _g },
            b{ _b },
            a{ 255 } {}

        constexpr color(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _a) noexcept :
            r{ _r },
            g{ _g },
            b{ _b },
            a{ _a } {}

        constexpr color(const color&) = default;
        constexpr color(color&&) = default;
        color& operator=(const color&) = default;
        color& operator=(color&&) = default;
        ~color() = default;

#ifdef _WINDEF_
        constexpr color(COLORREF c) :
            r{ static_cast<uint8_t>(c & 0xFF) },
            g{ static_cast<uint8_t>((c & 0xFF00) >> 8) },
            b{ static_cast<uint8_t>((c & 0xFF0000) >> 16) },
            a{ 255 }
        {
        }

        operator COLORREF() const noexcept
        {
            return static_cast<COLORREF>(r) | (static_cast<COLORREF>(g) << 8) | (static_cast<COLORREF>(b) << 16);
        }
#endif

        // Method Description:
        // - Converting constructor for any other color structure type containing integral R, G, B, A (case sensitive.)
        // Notes:
        // - This and all below conversions make use of std::enable_if and a default parameter to disambiguate themselves.
        //   enable_if will result in an <error-type> if the constraint within it is not met, which will make this
        //   template ill-formed. Because SFINAE, ill-formed templated members "disappear" instead of causing an error.
        template<typename TOther>
        constexpr color(const TOther& other, std::enable_if_t<std::is_integral_v<decltype(std::declval<TOther>().R)> && std::is_integral_v<decltype(std::declval<TOther>().A)>, int> /*sentinel*/ = 0) :
            r{ static_cast<uint8_t>(other.R) },
            g{ static_cast<uint8_t>(other.G) },
            b{ static_cast<uint8_t>(other.B) },
            a{ static_cast<uint8_t>(other.A) }
        {
        }

        // Method Description:
        // - Converting constructor for any other color structure type containing integral r, g, b, a (case sensitive.)
        template<typename TOther>
        constexpr color(const TOther& other, std::enable_if_t<std::is_integral_v<decltype(std::declval<TOther>().r)> && std::is_integral_v<decltype(std::declval<TOther>().a)>, int> /*sentinel*/ = 0) :
            r{ static_cast<uint8_t>(other.r) },
            g{ static_cast<uint8_t>(other.g) },
            b{ static_cast<uint8_t>(other.b) },
            a{ static_cast<uint8_t>(other.a) }
        {
        }

        // Method Description:
        // - Converting constructor for any other color structure type containing floating-point R, G, B, A (case sensitive.)
        template<typename TOther>
        constexpr color(const TOther& other, std::enable_if_t<std::is_floating_point_v<decltype(std::declval<TOther>().R)> && std::is_floating_point_v<decltype(std::declval<TOther>().A)>, float> /*sentinel*/ = 1.0f) :
            r{ static_cast<uint8_t>(other.R * 255.0f) },
            g{ static_cast<uint8_t>(other.G * 255.0f) },
            b{ static_cast<uint8_t>(other.B * 255.0f) },
            a{ static_cast<uint8_t>(other.A * 255.0f) }
        {
        }

        // Method Description:
        // - Converting constructor for any other color structure type containing floating-point r, g, b, a (case sensitive.)
        template<typename TOther>
        constexpr color(const TOther& other, std::enable_if_t<std::is_floating_point_v<decltype(std::declval<TOther>().r)> && std::is_floating_point_v<decltype(std::declval<TOther>().a)>, float> /*sentinel*/ = 1.0f) :
            r{ static_cast<uint8_t>(other.r * 255.0f) },
            g{ static_cast<uint8_t>(other.g * 255.0f) },
            b{ static_cast<uint8_t>(other.b * 255.0f) },
            a{ static_cast<uint8_t>(other.a * 255.0f) }
        {
        }

#ifdef D3DCOLORVALUE_DEFINED
        constexpr operator D3DCOLORVALUE() const
        {
            return D3DCOLORVALUE{ r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
        }
#endif
    };
#pragma warning(pop)
}

// These sit outside the namespace because they sit outside for WIL too.

// Inspired from RETURN_IF_WIN32_BOOL_FALSE
// WIL doesn't include a RETURN_BOOL_IF_FALSE, and RETURN_IF_WIN32_BOOL_FALSE
//  will actually return the value of GLE.
#define RETURN_BOOL_IF_FALSE(b)                     \
    do                                              \
    {                                               \
        const bool __boolRet = wil::verify_bool(b); \
        if (!__boolRet)                             \
        {                                           \
            return __boolRet;                       \
        }                                           \
    } while (0, 0)

// Due to a bug (DevDiv 441931), Warning 4297 (function marked noexcept throws exception) is detected even when the throwing code is unreachable, such as the end of scope after a return, in function-level catch.
#define CATCH_LOG_RETURN_FALSE()            \
    catch (...)                             \
    {                                       \
        __pragma(warning(suppress : 4297)); \
        LOG_CAUGHT_EXCEPTION();             \
        return false;                       \
    }
