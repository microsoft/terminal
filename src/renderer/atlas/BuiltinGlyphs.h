// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "common.h"

namespace Microsoft::Console::Render::Atlas::BuiltinGlyphs
{
    bool IsBuiltinGlyph(char32_t codepoint) noexcept;
    void DrawBuiltinGlyph(ID2D1Factory* factory, ID2D1DeviceContext* renderTarget, ID2D1SolidColorBrush* brush, const D2D1_COLOR_F (&shadeColorMap)[4], const D2D1_RECT_F& rect, char32_t codepoint);

    inline constexpr char32_t BoxDrawing_FirstChar = 0x2500;
    inline constexpr u32 BoxDrawing_CharCount = 0xA0;

    inline constexpr char32_t Powerline_FirstChar = 0xE0B0;
    inline constexpr u32 Powerline_CharCount = 0x10;

    inline constexpr u32 TotalCharCount = BoxDrawing_CharCount + Powerline_CharCount;

    i32 GetBitmapCellIndex(char32_t codepoint) noexcept;

    // This is just an extra. It's not actually implemented as part of BuiltinGlyphs.cpp.
    constexpr bool IsSoftFontChar(char32_t ch) noexcept
    {
        return ch >= 0xEF20 && ch < 0xEF80;
    }
}
