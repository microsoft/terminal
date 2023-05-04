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
- Moved the colors into their own separate abstraction. (migrie Nov 2018)
--*/

#pragma once

#ifdef UNIT_TESTING
#include "WexTestClass.h"
#endif

// The enum values being in this particular order allows the compiler to do some useful optimizations,
// like simplifying `IsIndex16() || IsIndex256()` into a simple range check without branching.
enum class ColorType : BYTE
{
    IsDefault,
    IsIndex16,
    IsIndex256,
    IsRgb
};

enum class ColorAlias : size_t
{
    DefaultForeground,
    DefaultBackground,
    FrameForeground,
    FrameBackground,
    ENUM_COUNT // must be the last element in the enum class
};

struct TextColor
{
public:
    static constexpr BYTE DARK_BLACK = 0;
    static constexpr BYTE DARK_RED = 1;
    static constexpr BYTE DARK_GREEN = 2;
    static constexpr BYTE DARK_YELLOW = 3;
    static constexpr BYTE DARK_BLUE = 4;
    static constexpr BYTE DARK_MAGENTA = 5;
    static constexpr BYTE DARK_CYAN = 6;
    static constexpr BYTE DARK_WHITE = 7;
    static constexpr BYTE BRIGHT_BLACK = 8;
    static constexpr BYTE BRIGHT_RED = 9;
    static constexpr BYTE BRIGHT_GREEN = 10;
    static constexpr BYTE BRIGHT_YELLOW = 11;
    static constexpr BYTE BRIGHT_BLUE = 12;
    static constexpr BYTE BRIGHT_MAGENTA = 13;
    static constexpr BYTE BRIGHT_CYAN = 14;
    static constexpr BYTE BRIGHT_WHITE = 15;

    // Entries 256 to 260 are reserved for XTerm compatibility.
    static constexpr size_t DEFAULT_FOREGROUND = 261;
    static constexpr size_t DEFAULT_BACKGROUND = 262;
    static constexpr size_t FRAME_FOREGROUND = 263;
    static constexpr size_t FRAME_BACKGROUND = 264;
    static constexpr size_t CURSOR_COLOR = 265;
    static constexpr size_t TABLE_SIZE = 266;

    constexpr TextColor() noexcept :
        _meta{ ColorType::IsDefault },
        _red{ 0 },
        _green{ 0 },
        _blue{ 0 }
    {
    }

    constexpr TextColor(const BYTE index, const bool isIndex256) noexcept :
        _meta{ isIndex256 ? ColorType::IsIndex256 : ColorType::IsIndex16 },
        _index{ index },
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

    bool operator==(const TextColor& other) const noexcept
    {
        return memcmp(this, &other, sizeof(TextColor)) == 0;
    }

    bool operator!=(const TextColor& other) const noexcept
    {
        return memcmp(this, &other, sizeof(TextColor)) != 0;
    }

    bool CanBeBrightened() const noexcept;
    bool IsLegacy() const noexcept;
    bool IsIndex16() const noexcept;
    bool IsIndex256() const noexcept;
    bool IsDefault() const noexcept;
    bool IsDefaultOrLegacy() const noexcept;
    bool IsRgb() const noexcept;

    void SetColor(const COLORREF rgbColor) noexcept;
    void SetIndex(const BYTE index, const bool isIndex256) noexcept;
    void SetDefault() noexcept;

    COLORREF GetColor(const std::array<COLORREF, TABLE_SIZE>& colorTable, const size_t defaultIndex, bool brighten = false) const noexcept;
    BYTE GetLegacyIndex(const BYTE defaultIndex) const noexcept;

    constexpr BYTE GetIndex() const noexcept
    {
        return _index;
    }

    COLORREF GetRGB() const noexcept;

    static constexpr BYTE TransposeLegacyIndex(const size_t index)
    {
        // When converting a 16-color index in the legacy Windows order to or
        // from an ANSI-compatible order, we need to swap the bits in positions
        // 0 and 2. We do this by XORing the index with 00000101, but only if
        // one (but not both) of those bit positions is set.
        const auto oneBitSet = (index ^ (index >> 2)) & 1;
        return gsl::narrow_cast<BYTE>(index ^ oneBitSet ^ (oneBitSet << 2));
    }

private:
    union
    {
        BYTE _red, _index;
    };
    BYTE _green;
    BYTE _blue;
    ColorType _meta;

#ifdef UNIT_TESTING
    friend class TextBufferTests;
    template<typename TextColor>
    friend class WEX::TestExecution::VerifyOutputTraits;
#endif
};

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
                    return WEX::Common::NoThrowString().Format(L"{RGB:0x%06x}", color.GetRGB());
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
