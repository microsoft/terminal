/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- conattrs.cpp

Abstract:
- Defines common operations on console attributes, especially in regards to
    finding the nearest color from a color table.

Author(s):
- Mike Griese (migrie) 01-Sept-2017
--*/

#include "precomp.h"
#include "..\inc\conattrs.hpp"
#include <math.h>

struct _HSL
{
    double h, s, l;

    // constructs an HSL color from a RGB Color.
    _HSL(const COLORREF rgb)
    {
        const double r = (double)GetRValue(rgb);
        const double g = (double)GetGValue(rgb);
        const double b = (double)GetBValue(rgb);

        const auto [min, max] = std::minmax({ r, g, b });

        const auto diff = max - min;
        const auto sum = max + min;
        // Luminance
        l = max / 255.0;

        // Saturation
        s = (max == 0) ? 0 : diff / max;

        //Hue
        double q = (diff == 0) ? 0 : 60.0 / diff;
        if (max == r)
        {
            h = (g < b) ? ((360.0 + q * (g - b)) / 360.0) : ((q * (g - b)) / 360.0);
        }
        else if (max == g)
        {
            h = (120.0 + q * (b - r)) / 360.0;
        }
        else if (max == b)
        {
            h = (240.0 + q * (r - g)) / 360.0;
        }
        else
        {
            h = 0;
        }
    }
};

//Routine Description:
// Finds the "distance" between a given HSL color and an RGB color, using the HSL color space.
//   This function is designed such that the caller would convert one RGB color to HSL ahead of time,
//   then compare many RGB colors to that first color.
//Arguments:
// - phslColorA - a pointer to the first color, as a HSL color.
// - rgbColorB - The second color to compare, in RGB color.
// Return value:
// The "distance" between the two.
static double _FindDifference(const _HSL* const phslColorA, const COLORREF rgbColorB)
{
    const _HSL hslColorB = _HSL(rgbColorB);
    return sqrt(pow((hslColorB.h - phslColorA->h), 2) +
                pow((hslColorB.s - phslColorA->s), 2) +
                pow((hslColorB.l - phslColorA->l), 2));
}

//Routine Description:
// For a given RGB color Color, finds the nearest color from the array ColorTable, and returns the index of that match.
//Arguments:
// - Color - The RGB color to fine the nearest color to.
// - ColorTable - The array of colors to find a nearest color from.
// - cColorTable - The number of elements in ColorTable
// Return value:
// The index in ColorTable of the nearest match to Color.
WORD FindNearestTableIndex(const COLORREF Color, _In_reads_(cColorTable) const COLORREF* const ColorTable, const WORD cColorTable)
{
    // Quick check for an exact match in the color table:
    for (WORD i = 0; i < cColorTable; i++)
    {
        if (Color == ColorTable[i])
        {
            return i;
        }
    }

    // Did not find an exact match - do an expensive comparison to the elements
    //      of the table to find the nearest color.
    const _HSL hslColor = _HSL(Color);
    WORD closest = 0;
    double minDiff = _FindDifference(&hslColor, ColorTable[0]);
    for (WORD i = 1; i < cColorTable; i++)
    {
        double diff = _FindDifference(&hslColor, ColorTable[i]);
        if (diff < minDiff)
        {
            minDiff = diff;
            closest = i;
        }
    }
    return closest;
}

// Function Description:
// - Converts the value of a xterm color table index to the windows color table equivalent.
// Arguments:
// - xtermTableEntry: the xterm color table index
// Return Value:
// - The windows color table equivalent.
WORD XtermToWindowsIndex(const size_t xtermTableEntry) noexcept
{
    const bool fRed = WI_IsFlagSet(xtermTableEntry, XTERM_RED_ATTR);
    const bool fGreen = WI_IsFlagSet(xtermTableEntry, XTERM_GREEN_ATTR);
    const bool fBlue = WI_IsFlagSet(xtermTableEntry, XTERM_BLUE_ATTR);
    const bool fBright = WI_IsFlagSet(xtermTableEntry, XTERM_BRIGHT_ATTR);

    return (fRed ? WINDOWS_RED_ATTR : 0x0) +
           (fGreen ? WINDOWS_GREEN_ATTR : 0x0) +
           (fBlue ? WINDOWS_BLUE_ATTR : 0x0) +
           (fBright ? WINDOWS_BRIGHT_ATTR : 0x0);
}

// Function Description:
// - Converts the value of a xterm color table index to the windows color table
//      equivalent. The range of values is [0, 255], where the lowest 16 are
//      mapped to the equivalent Windows index, and the rest of the values are
//      passed through.
// Arguments:
// - xtermTableEntry: the xterm color table index
// Return Value:
// - The windows color table equivalent.
WORD Xterm256ToWindowsIndex(const size_t xtermTableEntry) noexcept
{
    return xtermTableEntry < 16 ? XtermToWindowsIndex(xtermTableEntry) :
                                  static_cast<WORD>(xtermTableEntry);
}

// Function Description:
// - Converts the value of a pair of xterm color table indices to the legacy attr equivalent.
// Arguments:
// - xtermForeground: the xterm color table foreground index
// - xtermBackground: the xterm color table background index
// Return Value:
// - The legacy windows attribute equivalent.
WORD XtermToLegacy(const size_t xtermForeground, const size_t xtermBackground)
{
    const WORD fgAttr = XtermToWindowsIndex(xtermForeground);
    const WORD bgAttr = XtermToWindowsIndex(xtermBackground);

    return (bgAttr << 4) | fgAttr;
}

//Routine Description:
// Returns the exact entry from the color table, if it's in there.
//Arguments:
// - Color - The RGB color to fine the nearest color to.
// - ColorTable - The array of colors to find a nearest color from.
// - cColorTable - The number of elements in ColorTable
// Return value:
// The index in ColorTable of the nearest match to Color.
bool FindTableIndex(const COLORREF Color,
                    _In_reads_(cColorTable) const COLORREF* const ColorTable,
                    const WORD cColorTable,
                    _Out_ WORD* const pFoundIndex)
{
    *pFoundIndex = 0;
    for (WORD i = 0; i < cColorTable; i++)
    {
        if (ColorTable[i] == Color)
        {
            *pFoundIndex = i;
            return true;
        }
    }
    return false;
}

// Method Description:
// - Get a COLORREF for the foreground component of the given legacy attributes.
// Arguments:
// - wLegacyAttrs - The legacy attributes to get the foreground color from.
// - ColorTable - The array of colors to get the color from.
// - cColorTable - The number of elements in ColorTable
// Return Value:
// - the COLORREF for the foreground component
COLORREF ForegroundColor(const WORD wLegacyAttrs,
                         _In_reads_(cColorTable) const COLORREF* const ColorTable,
                         const size_t cColorTable)
{
    const byte iColorTableIndex = LOBYTE(wLegacyAttrs) & FG_ATTRS;

    return (iColorTableIndex < cColorTable && iColorTableIndex >= 0) ?
               ColorTable[iColorTableIndex] :
               INVALID_COLOR;
}

// Method Description:
// - Get a COLORREF for the background component of the given legacy attributes.
// Arguments:
// - wLegacyAttrs - The legacy attributes to get the background color from.
// - ColorTable - The array of colors to get the color from.
// - cColorTable - The number of elements in ColorTable
// Return Value:
// - the COLORREF for the background component
COLORREF BackgroundColor(const WORD wLegacyAttrs,
                         _In_reads_(cColorTable) const COLORREF* const ColorTable,
                         const size_t cColorTable)
{
    const byte iColorTableIndex = (LOBYTE(wLegacyAttrs) & BG_ATTRS) >> 4;

    return (iColorTableIndex < cColorTable && iColorTableIndex >= 0) ?
               ColorTable[iColorTableIndex] :
               INVALID_COLOR;
}
