// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "..\inc\FontInfoDesired.hpp"

bool operator==(const FontInfoDesired& a, const FontInfoDesired& b)
{
    return (static_cast<FontInfoBase>(a) == static_cast<FontInfoBase>(b) &&
            a._coordSizeDesired == b._coordSizeDesired);
}

COORD FontInfoDesired::GetEngineSize() const
{
    COORD coordSize = _coordSizeDesired;
    if (IsTrueTypeFont())
    {
        coordSize.X = 0; // Don't tell the engine about the width for a TrueType font. It makes a mess.
    }

    return coordSize;
}

FontInfoDesired::FontInfoDesired(_In_ PCWSTR const pwszFaceName,
                                 const BYTE bFamily,
                                 const LONG lWeight,
                                 const COORD coordSizeDesired,
                                 const UINT uiCodePage) :
    FontInfoBase(pwszFaceName, bFamily, lWeight, false, uiCodePage),
    _coordSizeDesired(coordSizeDesired)
{
}

FontInfoDesired::FontInfoDesired(const FontInfo& fiFont) :
    FontInfoBase(fiFont),
    _coordSizeDesired(fiFont.GetUnscaledSize())
{
}

// This helper determines if this object represents the default raster font. This can either be because internally we're
// using the empty facename and zeros for size, weight, and family, or it can be because we were given explicit
// dimensions from the engine that were the result of loading the default raster font. See GdiEngine::_GetProposedFont().
bool FontInfoDesired::IsDefaultRasterFont() const
{
    // Either the raster was set from the engine...
    // OR the face name is empty with a size of 0x0 or 8x12.
    return WasDefaultRasterSetFromEngine() || (wcsnlen_s(GetFaceName(), LF_FACESIZE) == 0 &&
                                               ((_coordSizeDesired.X == 0 && _coordSizeDesired.Y == 0) ||
                                                (_coordSizeDesired.X == 8 && _coordSizeDesired.Y == 12)));
}
