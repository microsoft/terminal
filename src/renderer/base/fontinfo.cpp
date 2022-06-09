// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../inc/FontInfo.hpp"

FontInfo::FontInfo(const std::wstring_view& faceName,
                   const unsigned char family,
                   const unsigned int weight,
                   const til::size coordSize,
                   const unsigned int codePage,
                   const bool fSetDefaultRasterFont /* = false */) noexcept :
    FontInfoBase(faceName, family, weight, fSetDefaultRasterFont, codePage),
    _coordSize(coordSize),
    _coordSizeUnscaled(coordSize),
    _didFallback(false)
{
    ValidateFont();
}

bool FontInfo::operator==(const FontInfo& other) noexcept
{
    return FontInfoBase::operator==(other) &&
           _coordSize == other._coordSize &&
           _coordSizeUnscaled == other._coordSizeUnscaled;
}

til::size FontInfo::GetUnscaledSize() const noexcept
{
    return _coordSizeUnscaled;
}

til::size FontInfo::GetSize() const noexcept
{
    return _coordSize;
}

void FontInfo::SetFromEngine(const std::wstring_view& faceName,
                             const unsigned char family,
                             const unsigned int weight,
                             const bool fSetDefaultRasterFont,
                             const til::size coordSize,
                             const til::size coordSizeUnscaled) noexcept
{
    FontInfoBase::SetFromEngine(faceName,
                                family,
                                weight,
                                fSetDefaultRasterFont);
    _coordSize = coordSize;
    _coordSizeUnscaled = coordSizeUnscaled;
    _ValidateCoordSize();
}

bool FontInfo::GetFallback() const noexcept
{
    return _didFallback;
}

void FontInfo::SetFallback(const bool didFallback) noexcept
{
    _didFallback = didFallback;
}

void FontInfo::ValidateFont() noexcept
{
    _ValidateCoordSize();
}

void FontInfo::_ValidateCoordSize() noexcept
{
    // a (0,0) font is okay for the default raster font, as we will eventually set the dimensions based on the font GDI
    // passes back to us.
    if (!IsDefaultRasterFontNoSize())
    {
        // Initialize X to 1 so we don't divide by 0
        if (_coordSize.X == 0)
        {
            _coordSize.X = 1;
        }

        // If we have no font size, we want to use 8x12 by default
        if (_coordSize.Y == 0)
        {
            _coordSize.X = 8;
            _coordSize.Y = 12;

            _coordSizeUnscaled = _coordSize;
        }
    }
}
