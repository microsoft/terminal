// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../inc/FontInfoDesired.hpp"

FontInfoDesired::FontInfoDesired(const std::wstring_view& faceName,
                                 const unsigned char family,
                                 const unsigned int weight,
                                 const til::size coordSizeDesired,
                                 const unsigned int codePage) noexcept :
    FontInfoBase(faceName, family, weight, false, codePage),
    _coordSizeDesired(coordSizeDesired)
{
}

FontInfoDesired::FontInfoDesired(const FontInfo& fiFont) noexcept :
    FontInfoBase(fiFont),
    _coordSizeDesired(fiFont.GetUnscaledSize())
{
}

bool FontInfoDesired::operator==(const FontInfoDesired& other) noexcept
{
    return FontInfoBase::operator==(other) &&
           _coordSizeDesired == other._coordSizeDesired;
}

til::size FontInfoDesired::GetEngineSize() const noexcept
{
    auto coordSize = _coordSizeDesired;
    if (IsTrueTypeFont())
    {
        coordSize.X = 0; // Don't tell the engine about the width for a TrueType font. It makes a mess.
    }

    return coordSize;
}

// This helper determines if this object represents the default raster font. This can either be because internally we're
// using the empty facename and zeros for size, weight, and family, or it can be because we were given explicit
// dimensions from the engine that were the result of loading the default raster font. See GdiEngine::_GetProposedFont().
bool FontInfoDesired::IsDefaultRasterFont() const noexcept
{
    // Either the raster was set from the engine...
    // OR the face name is empty with a size of 0x0 or 8x12.
    return WasDefaultRasterSetFromEngine() || (GetFaceName().empty() &&
                                               (_coordSizeDesired == til::size{ 0, 0 } ||
                                                _coordSizeDesired == til::size{ 8, 12 }));
}
