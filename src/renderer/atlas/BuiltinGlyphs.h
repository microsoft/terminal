// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "common.h"

namespace Microsoft::Console::Render::Atlas::BuiltinGlyphs
{
    bool IsBuiltinGlyph(char32_t codepoint) noexcept;
    void DrawBuiltinGlyph(ID2D1Factory* factory, ID2D1DeviceContext* renderTarget, ID2D1SolidColorBrush* brush, const D2D1_COLOR_F (&shadeColorMap)[5], const D2D1_RECT_F& rect, char32_t codepoint);

    inline constexpr char32_t BoxDrawing_FirstChar = 0x2500;
    inline constexpr u32 BoxDrawing_CharCount = 0xA0;

    inline constexpr char32_t Powerline_FirstChar = 0xE0B0;
    inline constexpr u32 Powerline_CharCount = 0x10;

    inline constexpr char32_t LegacyComputing_FirstChar = 0x1FB00;
    inline constexpr u32 LegacyComputing_CharCount = 0x95;

    inline constexpr u32 TotalCharCount = BoxDrawing_CharCount + Powerline_CharCount + LegacyComputing_CharCount;

    i32 GetBitmapCellIndex(char32_t codepoint) noexcept;

    // DECDLD soft fonts are mapped to U+EF20 and up. Only the code points the active
    // soft font actually defines belong to us; the rest go through regular font fallback.
    //
    // This is just an extra. It's not actually implemented as part of BuiltinGlyphs.cpp.
    inline constexpr char32_t SoftFont_FirstChar = 0xEF20;

    constexpr bool IsSoftFontChar(char32_t ch, u32 softFontCharCount) noexcept
    {
        return ch >= SoftFont_FirstChar && ch < (SoftFont_FirstChar + softFontCharCount);
    }

    static_assert(!IsSoftFontChar(SoftFont_FirstChar, 0));
    static_assert(IsSoftFontChar(SoftFont_FirstChar + 0x0F, 0x10) && !IsSoftFontChar(SoftFont_FirstChar + 0x10, 0x10));
}
