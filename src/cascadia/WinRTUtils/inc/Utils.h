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

// Method Description:
// - Scales a Rect based on a scale factor
// Arguments:
// - rect: Rect to scale by scale
// - scale: amount to scale rect by
// Return Value:
// - Rect scaled by scale
inline winrt::Windows::Foundation::Rect ScaleRect(winrt::Windows::Foundation::Rect rect, double scale)
{
    const float scaleLocal = gsl::narrow<float>(scale);
    rect.X *= scaleLocal;
    rect.Y *= scaleLocal;
    rect.Width *= scaleLocal;
    rect.Height *= scaleLocal;
    return rect;
}
