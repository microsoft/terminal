// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../inc/FontInfoDesired.hpp"

const CSSLengthPercentage& FontInfoDesired::GetCellWidth() const noexcept
{
    return _cellWidth;
}

const CSSLengthPercentage& FontInfoDesired::GetCellHeight() const noexcept
{
    return _cellHeight;
}

float FontInfoDesired::GetFontSizeInPt() const noexcept
{
    return _fontSizeInPt;
}

bool FontInfoDesired::GetEnableBuiltinGlyphs() const noexcept
{
    return _builtinGlyphs;
}

bool FontInfoDesired::GetEnableColorGlyphs() const noexcept
{
    return _colorGlyphs;
}

void FontInfoDesired::SetCellWidth(CSSLengthPercentage cellWidth) noexcept
{
    _cellWidth = cellWidth;
}

void FontInfoDesired::SetCellHeight(CSSLengthPercentage cellHeight) noexcept
{
    _cellHeight = cellHeight;
}

void FontInfoDesired::SetFontSizeInPt(float fontSizeInPt) noexcept
{
    _fontSizeInPt = fontSizeInPt;
}

void FontInfoDesired::SetEnableBuiltinGlyphs(bool builtinGlyphs) noexcept
{
    _builtinGlyphs = builtinGlyphs;
}

void FontInfoDesired::SetEnableColorGlyphs(bool colorGlyphs) noexcept
{
    _colorGlyphs = colorGlyphs;
}

til::size FontInfoDesired::GetPixelCellSize() const noexcept
{
    return {
        lrintf(_cellWidth.Resolve(0, 96.0f, 12.0f, 0)),
        lrintf(_cellHeight.Resolve(0, 96.0f, 12.0f, 0)),
    };
}

void FontInfoDesired::SetPixelCellSize(til::size size) noexcept
{
    _cellWidth = CSSLengthPercentage::FromPixel(static_cast<float>(size.width));
    _cellHeight = CSSLengthPercentage::FromPixel(static_cast<float>(size.height));
}

bool FontInfoDesired::IsTrueTypeFont() const noexcept
{
    return WI_IsFlagSet(_family, TMPF_TRUETYPE);
}

// This helper determines if this object represents the default raster font. This can either be because internally we're
// using the empty facename and zeros for size, weight, and family, or it can be because we were given explicit
// dimensions from the engine that were the result of loading the default raster font. See GdiEngine::_GetProposedFont().
bool FontInfoDesired::IsDefaultRasterFont() const noexcept
{
    static constexpr CSSLengthPercentage s_0 = CSSLengthPercentage::FromPixel(0);
    static constexpr CSSLengthPercentage s_8 = CSSLengthPercentage::FromPixel(8);
    static constexpr CSSLengthPercentage s_12 = CSSLengthPercentage::FromPixel(12);
    return _faceName.empty() &&
           ((_cellWidth == s_0 && _cellHeight == s_0) ||
            (_cellWidth == s_8 && _cellHeight == s_12));
}
