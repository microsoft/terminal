// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// Method Description:
// - Converts a COLORREF to Color
// Arguments:
// - colorref: COLORREF to convert to Color
// Return Value:
// - Color containing the RGB values from colorref
inline winrt::Windows::UI::Color ColorRefToColor(const COLORREF& colorref)
{
    winrt::Windows::UI::Color color;
    color.R = GetRValue(colorref);
    color.G = GetGValue(colorref);
    color.B = GetBValue(colorref);
    return color;
}
