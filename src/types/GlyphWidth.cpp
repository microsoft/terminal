// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/CodepointWidthDetector.hpp"
#include "inc/GlyphWidth.hpp"

// Function Description:
// - determines if the glyph represented by the string of characters should be
//      wide or not. See CodepointWidthDetector::IsWide
bool IsGlyphFullWidth(const std::wstring_view& glyph) noexcept
{
    GraphemeState state;
    CodepointWidthDetector::Singleton().GraphemeNext(state, glyph);
    return state.width == 2;
}

// Function Description:
// - determines if the glyph represented by the single character should be
//      wide or not. See CodepointWidthDetector::IsWide
bool IsGlyphFullWidth(const wchar_t wch) noexcept
{
    return wch < 0x80 ? false : IsGlyphFullWidth({ &wch, 1 });
}
