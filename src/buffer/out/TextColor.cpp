// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "TextColor.h"

#include <til/bit.h>

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
     0,  4,  2,  6,  1,  5,  3,  7,  8, 12, 10, 14,  9, 13, 11, 15,
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

// We should only need 4B for TextColor. Any more than that is just waste.
static_assert(sizeof(TextColor) == 4);
// Assert that the use of memcmp() for comparisons is safe.
static_assert(std::has_unique_object_representations_v<TextColor>);

bool TextColor::CanBeBrightened() const noexcept
{
    return IsIndex16() || IsDefault();
}

bool TextColor::IsLegacy() const noexcept
{
    return (IsIndex16() || IsIndex256()) && _index < 16;
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

bool TextColor::IsDefaultOrLegacy() const noexcept
{
    return _meta != ColorType::IsRgb && _index < 16;
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
    _green = 0;
    _blue = 0;
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
    _red = 0;
    _green = 0;
    _blue = 0;
}

// Method Description:
// - Retrieve the real color value for this TextColor.
//   * If we're an RGB color, we'll use that value.
//   * If we're an indexed color table value, we'll use that index to look up
//     our value in the provided color table.
//     - If brighten is true, and we've got a 16 color index in the "dark"
//       portion of the color table (indices [0,7]), then we'll look up the
//       bright version of this color (from indices [8,15]). This should be
//       true for TextAttributes that are "intense" and we're treating intense
//       as bright (which is the default behavior of most terminals.)
//   * If we're a default color, we'll return the default color provided.
// Arguments:
// - colorTable: The table of colors we should use to look up the value of
//      an indexed attribute from.
// - defaultIndex: The color table index to use if we're a default attribute.
// - brighten: if true, we'll brighten a dark color table index.
// Return Value:
// - a COLORREF containing the real value of this TextColor.
COLORREF TextColor::GetColor(const std::array<COLORREF, TextColor::TABLE_SIZE>& colorTable, const size_t defaultIndex, bool brighten) const noexcept
{
    if (IsDefault())
    {
        const auto defaultColor = til::at(colorTable, defaultIndex);

        if (brighten)
        {
            // See MSFT:20266024 for context on this fix.
            //      Additionally todo MSFT:20271956 to fix this better for 19H2+
            // If we're a default color, check to see if the defaultColor exists
            // in the dark section of the color table. If it does, then chances
            // are we're not a separate default color, instead we're an index
            //      color being used as the default color
            //      (Settings::_DefaultForeground==INVALID_COLOR, and the index
            //      from _wFillAttribute is being used instead.)
            // If we find a match, return instead the bright version of this color

            static_assert(sizeof(COLORREF) * 8 == 32, "The vectorized code broke. If you can't fix COLORREF, just remove the vectorized code.");

#pragma warning(push)
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26490) // Don't use reinterpret_cast (type.1).
#ifdef __AVX2__
            // I wrote this vectorized code one day, because the sun was shining so nicely.
            // There's no other reason for this to exist here, except for being pretty.
            // This code implements the exact same for loop you can find below, but is ~3x faster.
            //
            // A brief explanation for people unfamiliar with vectorized instructions:
            //   Vectorized instructions, like "SSE" or "AVX", allow you to run
            //   common operations like additions, multiplications, comparisons,
            //   or bitwise operations concurrently on multiple values at once.
            //
            // We want to find the given defaultColor in the first 8 values of colorTable.
            // Coincidentally a COLORREF is a DWORD and 8 of them are exactly 256 bits.
            // -- The size of a single AVX register.
            //
            // Thus, the code works like this:
            // 1. Load all 8 DWORDs at once into one register
            // 2. Set the same defaultColor 8 times in another register
            // 3. Compare all 8 values at once
            //    The result is either 0xffffffff or 0x00000000.
            // 4. Extract the most significant bit of each DWORD
            //    Assuming that no duplicate colors exist in colorTable,
            //    the result will be something like 0b00100000.
            // 5. Use BitScanForward (bsf) to find the index of the most significant 1 bit.
            const auto haystack = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(colorTable.data())); // 1.
            const auto needle = _mm256_set1_epi32(til::bit_cast<int>(defaultColor)); // 2.
            const auto result = _mm256_cmpeq_epi32(haystack, needle); // 3.
            const auto mask = _mm256_movemask_ps(_mm256_castsi256_ps(result)); // 4.
            unsigned long index;
            return _BitScanForward(&index, mask) ? til::at(colorTable, static_cast<size_t>(index) + 8) : defaultColor; // 5.
#elif _M_AMD64
            // If you look closely this SSE2 algorithm is the same as the AVX one.
            // The two differences are that we need to:
            // * do everything twice, because SSE is limited to 128 bits and not 256.
            // * use _mm_packs_epi32 to merge two 128 bits vectors into one in step 3.5.
            //   _mm_packs_epi32 takes two SSE registers and truncates all 8 DWORDs into 8 WORDs,
            //   the latter of which fits into a single register (which is then used in the identical step 4).
            // * since the result are now 8 WORDs, we need to use _mm_movemask_epi8 (there's no 16-bit variant),
            //   which unlike AVX's step 4 results in in something like 0b0000110000000000.
            //   --> the index returned by _BitScanForward must be divided by 2.
            const auto haystack1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(colorTable.data() + 0));
            const auto haystack2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(colorTable.data() + 4));
            const auto needle = _mm_set1_epi32(til::bit_cast<int>(defaultColor));
            const auto result1 = _mm_cmpeq_epi32(haystack1, needle);
            const auto result2 = _mm_cmpeq_epi32(haystack2, needle);
            const auto result = _mm_packs_epi32(result1, result2); // 3.5
            const auto mask = _mm_movemask_epi8(result);
            unsigned long index;
            return _BitScanForward(&index, mask) ? til::at(colorTable, static_cast<size_t>(index / 2) + 8) : defaultColor;
#else
            for (size_t i = 0; i < 8; i++)
            {
                if (til::at(colorTable, i) == defaultColor)
                {
                    return til::at(colorTable, i + 8);
                }
            }
#endif
#pragma warning(pop)
        }

        return defaultColor;
    }
    else if (IsRgb())
    {
        return GetRGB();
    }
    else if (IsIndex16() && brighten)
    {
        return til::at(colorTable, _index | 8);
    }
    else
    {
        return til::at(colorTable, _index);
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
    else if (IsIndex16() || IsIndex256())
    {
        return til::at(Index256ToIndex16, GetIndex());
    }
    else
    {
        // We compress the RGB down to an 8-bit value and use that to
        // lookup a representative 16-color index from a hard-coded table.
        const BYTE compressedRgb = (_red & 0b11100000) +
                                   ((_green >> 3) & 0b00011100) +
                                   ((_blue >> 6) & 0b00000011);
        return til::at(CompressedRgbToIndex16, compressedRgb);
    }
}

// Method Description:
// - Return a COLORREF containing our stored value. Will return garbage if this
//attribute is not a RGB attribute.
// Arguments:
// - <none>
// Return Value:
// - a COLORREF containing our stored value
COLORREF TextColor::GetRGB() const noexcept
{
    return RGB(_red, _green, _blue);
}
