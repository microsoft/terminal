// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    // color is a universal integral 8bpp RGBA (0-255) color type implicitly convertible to/from
    // a number of other color types.
#pragma warning(push)
    // we can't depend on GSL here, so we use static_cast for explicit narrowing
#pragma warning(disable : 26472) // Don't use a static_cast for arithmetic conversions. Use brace initialization, gsl::narrow_cast or gsl::narrow (type.1).
#pragma warning(disable : 26495) // Variable 'til::color::<unnamed-tag>::abgr' is uninitialized. Always initialize a member variable (type.6).
    struct color
    {
        // Clang (10) has no trouble optimizing the COLORREF conversion operator, below, to a
        // simple 32-bit load with mask (!) even though it's a series of bit shifts across
        // multiple struct members.
        // CL (19.24) doesn't make the same optimization decision, and it emits three 8-bit loads
        // and some shifting.
        // In any case, the optimization only applies at -O2 (clang) and above.
        // Here, we leverage the spec-legality of using unions for type conversions and the
        // overlap of four uint8_ts and a uint32_t to make the conversion very obvious to
        // both compilers.
#pragma warning(push)
        // CL will complain about the both nameless and anonymous struct.
#pragma warning(disable : 4201)
        union
        {
            struct
            {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ // Clang, GCC
                uint8_t a, b, g, r;
#else
                uint8_t r, g, b, a;
#endif
            };
            uint32_t abgr;
        };
#pragma warning(pop)

        constexpr color() noexcept :
            r{ 0 },
            g{ 0 },
            b{ 0 },
            a{ 0 }
        {
        }

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
            abgr{ static_cast<uint32_t>(c | 0xFF000000u) }
        {
        }

        constexpr operator COLORREF() const noexcept
        {
            return static_cast<COLORREF>(abgr & 0x00FFFFFFu);
        }
#endif

        // Method Description:
        // - Converting constructor for any other color structure type containing integral R, G, B, A (case-sensitive.)
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
        // - Converting constructor for any other color structure type containing integral r, g, b, a (case-sensitive.)
        template<typename TOther>
        constexpr color(const TOther& other, std::enable_if_t<std::is_integral_v<decltype(std::declval<TOther>().r)> && std::is_integral_v<decltype(std::declval<TOther>().a)>, int> /*sentinel*/ = 0) :
            r{ static_cast<uint8_t>(other.r) },
            g{ static_cast<uint8_t>(other.g) },
            b{ static_cast<uint8_t>(other.b) },
            a{ static_cast<uint8_t>(other.a) }
        {
        }

        // Method Description:
        // - Converting constructor for any other color structure type containing floating-point R, G, B, A (case-sensitive.)
        template<typename TOther>
        constexpr color(const TOther& other, std::enable_if_t<std::is_floating_point_v<decltype(std::declval<TOther>().R)> && std::is_floating_point_v<decltype(std::declval<TOther>().A)>, float> /*sentinel*/ = 1.0f) :
            r{ static_cast<uint8_t>(other.R * 255.0f) },
            g{ static_cast<uint8_t>(other.G * 255.0f) },
            b{ static_cast<uint8_t>(other.B * 255.0f) },
            a{ static_cast<uint8_t>(other.A * 255.0f) }
        {
        }

        // Method Description:
        // - Converting constructor for any other color structure type containing floating-point r, g, b, a (case-sensitive.)
        template<typename TOther>
        constexpr color(const TOther& other, std::enable_if_t<std::is_floating_point_v<decltype(std::declval<TOther>().r)> && std::is_floating_point_v<decltype(std::declval<TOther>().a)>, float> /*sentinel*/ = 1.0f) :
            r{ static_cast<uint8_t>(other.r * 255.0f) },
            g{ static_cast<uint8_t>(other.g * 255.0f) },
            b{ static_cast<uint8_t>(other.b * 255.0f) },
            a{ static_cast<uint8_t>(other.a * 255.0f) }
        {
        }

        constexpr color with_alpha(uint8_t alpha) const
        {
            return color{
                r,
                g,
                b,
                alpha
            };
        }

        // source-over alpha blending/composition.
        // `this` (source/top) will be blended "over" `destination` (bottom).
        // `this` and `destination` are expected to be in straight alpha.
        // See https://en.wikipedia.org/wiki/Alpha_compositing#Description
        constexpr color layer_over(const color& destination) const
        {
            const auto aInverse = (255 - a) / 255.0f;
            const auto resultA = a + destination.a * aInverse;
            const auto resultR = (r * a + destination.r * destination.a * aInverse) / resultA;
            const auto resultG = (g * a + destination.g * destination.a * aInverse) / resultA;
            const auto resultB = (b * a + destination.b * destination.a * aInverse) / resultA;

            return {
                static_cast<uint8_t>(resultR + 0.5f),
                static_cast<uint8_t>(resultG + 0.5f),
                static_cast<uint8_t>(resultB + 0.5f),
                static_cast<uint8_t>(resultA + 0.5f),
            };
        }

#ifdef D3DCOLORVALUE_DEFINED
        constexpr operator D3DCOLORVALUE() const
        {
            return D3DCOLORVALUE{ r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
        }
#endif

#ifdef WINRT_Windows_UI_H
        constexpr color(const winrt::Windows::UI::Color& winUIColor) :
            color(winUIColor.R, winUIColor.G, winUIColor.B, winUIColor.A)
        {
        }

        constexpr operator winrt::Windows::UI::Color() const
        {
            return { a, r, g, b };
        }
#endif

#ifdef WINRT_Microsoft_Terminal_Core_H
        constexpr color(const winrt::Microsoft::Terminal::Core::Color& coreColor) :
            color(coreColor.R, coreColor.G, coreColor.B, coreColor.A)
        {
        }

        constexpr operator winrt::Microsoft::Terminal::Core::Color() const noexcept
        {
            return { r, g, b, a };
        }
#endif

        // Helper for converting a hue [0, 1) to an RGB value.
        // Credit to https://www.chilliant.com/rgb2hsv.html
        static til::color from_hue(float hue)
        {
            const float R = abs(hue * 6 - 3) - 1;
            const float G = 2 - abs(hue * 6 - 2);
            const float B = 2 - abs(hue * 6 - 4);
            return color{
                base::saturated_cast<uint8_t>(255.f * std::clamp(R, 0.f, 1.f)),
                base::saturated_cast<uint8_t>(255.f * std::clamp(G, 0.f, 1.f)),
                base::saturated_cast<uint8_t>(255.f * std::clamp(B, 0.f, 1.f))
            };
        }

        constexpr bool operator==(const til::color& other) const
        {
            return abgr == other.abgr;
        }

        constexpr bool operator!=(const til::color& other) const
        {
            return !(*this == other);
        }

        std::wstring to_string() const
        {
            return ToHexString(false);
        }

        std::wstring ToHexString(const bool omitAlpha = false) const
        {
            auto str = fmt::format(FMT_COMPILE(L"#{:02X}{:02X}{:02X}{:02X}"), r, g, b, a);
            if (omitAlpha)
            {
                str.resize(7);
            }
            return str;
        }
    };
#pragma warning(pop)
}

static_assert(sizeof(til::color) == sizeof(uint32_t));

#ifdef __WEX_COMMON_H__
namespace WEX::TestExecution
{
    template<>
    class VerifyOutputTraits<::til::color>
    {
    public:
        static WEX::Common::NoThrowString ToString(const ::til::color& color)
        {
            return WEX::Common::NoThrowString(color.to_string().c_str());
        }
    };
};
#endif
