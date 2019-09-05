// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/CodepointWidthDetector.hpp"
#include "inc/GlyphWidth.hpp"

#pragma warning(suppress : 26426)
// TODO GH 2676 - remove warning suppression and decide what to do re: singleton instance of CodepointWidthDetector
static CodepointWidthDetector widthDetector;

// Function Description:
// - determines if the glyph represented by the string of characters should be
//      wide or not. See CodepointWidthDetector::IsWide
bool IsGlyphFullWidth(const std::wstring_view glyph)
{
    return widthDetector.IsWide(glyph);
}

// Function Description:
// - determines if the glyph represented by the single character should be
//      wide or not. See CodepointWidthDetector::IsWide
bool IsGlyphFullWidth(const wchar_t wch) noexcept
{
    return widthDetector.IsWide(wch);
}

// Function Description:
// - Sets a function that should be used by the global CodepointWidthDetector
//      as the fallback mechanism for determining a particular glyph's width,
//      should the glyph be an ambiguous width.
//   A Terminal could hook in a Renderer's IsGlyphWideByFont method as the
//      fallback to ask the renderer for the glyph's width (for example).
// Arguments:
// - pfnFallback - the function to use as the fallback method.
// Return Value:
// - <none>
void SetGlyphWidthFallback(std::function<bool(const std::wstring_view)> pfnFallback)
{
    widthDetector.SetFallbackMethod(pfnFallback);
}

// Function Description:
// - Forwards notification about font changing to glyph width detector
// Arguments:
// - <none>
// Return Value:
// - <none>
void NotifyGlyphWidthFontChanged() noexcept
{
    widthDetector.NotifyFontChanged();
}
