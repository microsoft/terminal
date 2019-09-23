/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- TextColor.h

Abstract:
- contains data for a single color of the text. Text Attributes are composed of
  two of these - one for the foreground and one for the background.
  The color can be in one of three states:
    * Default Colors - The terminal should use the terminal's notion of whatever
      the default color should be for this component.
      It's up to the terminal that's consuming this buffer to control the
      behavior of default attributes.
      Terminals typically have a pair of Default colors that are separate from
      their color table. This component should use that value.
      Consoles also can have a legacy table index as their default colors.
    * Indexed Color - The terminal should use our value as an index into the
      color table to retrieve the real value of the color.
      This is the type of color that "legacy" 16-color attributes have.
    * RGB color - We'll store a real color value in this attribute

Author(s):
- Mike Griese (migrie) Nov 2018

Revision History:
- From components of output.h/.c
  by Therese Stowell (ThereseS) 1990-1991
- Pulled into its own file from textBuffer.hpp/cpp (AustDi, 2017)
- Moved the colors into their own seperate abstraction. (migrie Nov 2018)
--*/

#pragma once

#ifdef UNIT_TESTING
#include "WexTestClass.h"
#endif

#pragma pack(push, 1)

enum class ColorType : BYTE
{
    IsIndex = 0x0,
    IsDefault = 0x1,
    IsRgb = 0x2
};

struct TextColor
{
public:
    constexpr TextColor() noexcept :
        _meta{ ColorType::IsDefault },
        _red{ 0 },
        _green{ 0 },
        _blue{ 0 }
    {
    }

    constexpr TextColor(const BYTE wLegacyAttr) noexcept :
        _meta{ ColorType::IsIndex },
        _index{ wLegacyAttr },
        _green{ 0 },
        _blue{ 0 }
    {
    }

    constexpr TextColor(const COLORREF rgb) noexcept :
        _meta{ ColorType::IsRgb },
        _red{ GetRValue(rgb) },
        _green{ GetGValue(rgb) },
        _blue{ GetBValue(rgb) }
    {
    }

    friend constexpr bool operator==(const TextColor& a, const TextColor& b) noexcept;
    friend constexpr bool operator!=(const TextColor& a, const TextColor& b) noexcept;

    constexpr bool IsLegacy() const noexcept
    {
        return !(IsDefault() || IsRgb());
    }

    constexpr bool IsDefault() const noexcept
    {
        return _meta == ColorType::IsDefault;
    }

    constexpr bool IsRgb() const noexcept
    {
        return _meta == ColorType::IsRgb;
    }

    void SetColor(const COLORREF rgbColor) noexcept;
    void SetIndex(const BYTE index) noexcept;
    void SetDefault() noexcept;

    COLORREF GetColor(std::basic_string_view<COLORREF> colorTable,
                      const COLORREF defaultColor,
                      const bool brighten) const noexcept;

    constexpr BYTE GetIndex() const noexcept
    {
        return _index;
    }

private:
    ColorType _meta : 2;
    union
    {
        BYTE _red, _index;
    };
    BYTE _green;
    BYTE _blue;

    COLORREF _GetRGB() const noexcept;

#ifdef UNIT_TESTING
    friend class TextBufferTests;
    template<typename TextColor>
    friend class WEX::TestExecution::VerifyOutputTraits;
#endif
};

#pragma pack(pop)

bool constexpr operator==(const TextColor& a, const TextColor& b) noexcept
{
    return a._meta == b._meta &&
           a._red == b._red &&
           a._green == b._green &&
           a._blue == b._blue;
}

bool constexpr operator!=(const TextColor& a, const TextColor& b) noexcept
{
    return !(a == b);
}

#ifdef UNIT_TESTING

namespace WEX
{
    namespace TestExecution
    {
        template<>
        class VerifyOutputTraits<TextColor>
        {
        public:
            static WEX::Common::NoThrowString ToString(const TextColor& color)
            {
                if (color.IsDefault())
                {
                    return L"{default}";
                }
                else if (color.IsRgb())
                {
                    return WEX::Common::NoThrowString().Format(L"{RGB:0x%06x}", color._GetRGB());
                }
                else
                {
                    return WEX::Common::NoThrowString().Format(L"{index:0x%04x}", color._red);
                }
            }
        };
    }
}
#endif

static_assert(sizeof(TextColor) <= 4 * sizeof(BYTE), "We should only need 4B for an entire TextColor. Any more than that is just waste");
