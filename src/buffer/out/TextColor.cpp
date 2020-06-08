// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "TextColor.h"

// clang-format off

// A table mapping 8-bit RGB colors, in the form RRRGGGBB,
// down to one of the 16 colors in the legacy palette.
constexpr std::array<BYTE, 256> CompressedRgbToIndex16 = {
     0,  1,  1,  9,  0,  0,  1,  1,  2,  1,  1,  1,  2,  8,  1,  9,
     2,  2,  3,  3,  2,  2, 11,  3, 10, 10, 11, 11, 10, 10, 10, 11,
     0,  5,  1,  1,  0,  0,  1,  1,  8,  1,  1,  1,  2,  8,  1,  9,
     2,  2,  3,  3,  2,  2, 11,  3, 10, 10, 10, 11, 10, 10, 10, 11,
     5,  5,  5,  1,  4,  5,  1,  1,  8,  8,  1,  9,  2,  8,  9,  9,
     2,  2,  3,  3,  2,  2, 11,  3, 10, 10, 11, 11, 10, 10, 10, 11,
     4,  5,  5,  1,  4,  5,  5,  1,  8,  5,  5,  1,  8,  8,  9,  9,
     2,  2,  8,  9, 10,  2, 11,  3, 10, 10, 11, 11, 10, 10, 10, 11,
     4, 13,  5,  5,  4, 13,  5,  5,  4, 13, 13, 13,  6,  8, 13,  9,
     6,  8,  8,  9, 10, 10, 11,  3, 10, 10, 11, 11, 10, 10, 10, 11,
     4, 13, 13, 13,  4, 13, 13, 13,  4, 12, 13, 13,  6, 12, 13, 13,
     6,  6,  8,  9,  6,  6,  7,  7, 10, 14, 14,  7, 10, 10, 14, 11,
     4, 12, 13, 13,  4, 12, 13, 13,  4, 12, 13, 13,  6, 12, 12, 13,
     6,  6, 12,  7,  6,  6,  7,  7,  6, 14, 14,  7, 14, 14, 14, 15,
    12, 12, 13, 13, 12, 12, 13, 13, 12, 12, 12, 13, 12, 12, 12, 13,
     6, 12, 12,  7,  6,  6,  7,  7,  6, 14, 14,  7, 14, 14, 14, 15
};

// A table mapping indexed colors from the 256-color palette,
// down to one of the 16 colors in the legacy palette.
constexpr std::array<BYTE, 256> Index256ToIndex16 = {
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
     0,  1,  1,  1,  9,  9,  2,  1,  1,  1,  1,  1,  2,  2,  3,  3,
     3,  3,  2,  2, 11, 11,  3,  3, 10, 10, 11, 11, 11, 11, 10, 10,
    10, 10, 11, 11,  5,  5,  5,  5,  1,  1,  8,  8,  1,  1,  9,  9,
     2,  2,  3,  3,  3,  3,  2,  2, 11, 11,  3,  3, 10, 10, 11, 11,
    11, 11, 10, 10, 10, 10, 11, 11,  4, 13,  5,  5,  5,  5,  4, 13,
    13, 13, 13, 13,  6,  8,  8,  8,  9,  9, 10, 10, 11, 11,  3,  3,
    10, 10, 11, 11, 11, 11, 10, 10, 10, 10, 11, 11,  4, 13, 13, 13,
    13, 13,  4, 12, 13, 13, 13, 13,  6,  6,  8,  8,  9,  9,  6,  6,
     7,  7,  7,  7, 10, 14, 14, 14,  7,  7, 10, 10, 14, 14, 11, 11,
     4, 12, 13, 13, 13, 13,  4, 12, 13, 13, 13, 13,  6,  6, 12, 12,
     7,  7,  6,  6,  7,  7,  7,  7,  6, 14, 14, 14,  7,  7, 14, 14,
    14, 14, 15, 15, 12, 12, 13, 13, 13, 13, 12, 12, 12, 12, 13, 13,
     6, 12, 12, 12,  7,  7,  6,  6,  7,  7,  7,  7,  6, 14, 14, 14,
     7,  7, 14, 14, 14, 14, 15, 15,  0,  0,  0,  0,  0,  0,  8,  8,
     8,  8,  8,  8,  8,  8,  8,  8,  7,  7,  7,  7,  7,  7, 15, 15
};

// clang-format on

bool TextColor::IsLegacy() const noexcept
{
    return IsIndex16() || (IsIndex256() && _index < 16);
}

bool TextColor::IsIndex16() const noexcept
{
    return _meta == ColorType::IsIndex16;
}

bool TextColor::IsIndex256() const noexcept
{
    return _meta == ColorType::IsIndex256;
}

bool TextColor::IsDefault() const noexcept
{
    return _meta == ColorType::IsDefault;
}

bool TextColor::IsRgb() const noexcept
{
    return _meta == ColorType::IsRgb;
}

// Method Description:
// - Sets the color value of this attribute, and sets this color to be an RGB
//      attribute.
// Arguments:
// - rgbColor: the COLORREF containing the color information for this TextColor
// Return Value:
// - <none>
void TextColor::SetColor(const COLORREF rgbColor) noexcept
{
    _meta = ColorType::IsRgb;
    _red = GetRValue(rgbColor);
    _green = GetGValue(rgbColor);
    _blue = GetBValue(rgbColor);
}

// Method Description:
// - Sets this TextColor to be a legacy-style index into the color table.
// Arguments:
// - index: the index of the colortable we should use for this TextColor.
// - isIndex256: is this a 256 color index (true) or a 16 color index (false).
// Return Value:
// - <none>
void TextColor::SetIndex(const BYTE index, const bool isIndex256) noexcept
{
    _meta = isIndex256 ? ColorType::IsIndex256 : ColorType::IsIndex16;
    _index = index;
}

// Method Description:
// - Sets this TextColor to be a default text color, who's appearance is
//      controlled by the terminal's implementation of what a default color is.
// Arguments:
// - <none>
// Return Value:
// - <none>
void TextColor::SetDefault() noexcept
{
    _meta = ColorType::IsDefault;
}

// Method Description:
// - Retrieve the real color value for this TextColor.
//   * If we're an RGB color, we'll use that value.
//   * If we're an indexed color table value, we'll use that index to look up
//     our value in the provided color table.
//     - If brighten is true, and we've got a 16 color index in the "dark"
//       portion of the color table (indices [0,7]), then we'll look up the
//       bright version of this color (from indices [8,15]). This should be
//       true for TextAttributes that are "Bold" and we're treating bold as
//       bright (which is the default behavior of most terminals.)
//   * If we're a default color, we'll return the default color provided.
// Arguments:
// - colorTable: The table of colors we should use to look up the value of
//      an indexed attribute from.
// - defaultColor: The color value to use if we're a default attribute.
// - brighten: if true, we'll brighten a dark color table index.
// Return Value:
// - a COLORREF containing the real value of this TextColor.
COLORREF TextColor::GetColor(std::basic_string_view<COLORREF> colorTable,
                             const COLORREF defaultColor,
                             bool brighten) const noexcept
{
    if (IsDefault())
    {
        if (brighten)
        {
            FAIL_FAST_IF(colorTable.size() < 16);
            // See MSFT:20266024 for context on this fix.
            //      Additionally todo MSFT:20271956 to fix this better for 19H2+
            // If we're a default color, check to see if the defaultColor exists
            // in the dark section of the color table. If it does, then chances
            // are we're not a separate default color, instead we're an index
            //      color being used as the default color
            //      (Settings::_DefaultForeground==INVALID_COLOR, and the index
            //      from _wFillAttribute is being used instead.)
            // If we find a match, return instead the bright version of this color
            for (size_t i = 0; i < 8; i++)
            {
                if (colorTable.at(i) == defaultColor)
                {
                    return colorTable.at(i + 8);
                }
            }
        }

        return defaultColor;
    }
    else if (IsRgb())
    {
        return _GetRGB();
    }
    else if (IsIndex16() && brighten)
    {
        return colorTable.at(_index | 8);
    }
    else
    {
        return colorTable.at(_index);
    }
}

// Method Description:
// - Return a legacy index value that best approximates this color.
// Arguments:
// - defaultIndex: The index to use for a default color.
// Return Value:
// - an index into the 16-color table
BYTE TextColor::GetLegacyIndex(const BYTE defaultIndex) const noexcept
{
    if (IsDefault())
    {
        return defaultIndex;
    }
    else if (IsIndex16())
    {
        return GetIndex();
    }
    else if (IsIndex256())
    {
        return Index256ToIndex16.at(GetIndex());
    }
    else
    {
        // We compress the RGB down to an 8-bit value and use that to
        // lookup a representative 16-color index from a hard-coded table.
        const BYTE compressedRgb = (_red & 0b11100000) +
                                   ((_green >> 3) & 0b00011100) +
                                   ((_blue >> 6) & 0b00000011);
        return CompressedRgbToIndex16.at(compressedRgb);
    }
}

// Method Description:
// - Return a COLORREF containing our stored value. Will return garbage if this
//attribute is not a RGB attribute.
// Arguments:
// - <none>
// Return Value:
// - a COLORREF containing our stored value
COLORREF TextColor::_GetRGB() const noexcept
{
    return RGB(_red, _green, _blue);
}
