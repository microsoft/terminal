// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "..\inc\FontInfo.hpp"

bool operator==(const FontInfo& a, const FontInfo& b)
{
    return (static_cast<FontInfoBase>(a) == static_cast<FontInfoBase>(b) &&
            a._coordSize == b._coordSize &&
            a._coordSizeUnscaled == b._coordSizeUnscaled);
}

FontInfo::FontInfo(_In_ PCWSTR const pwszFaceName,
                   const BYTE bFamily,
                   const LONG lWeight,
                   const COORD coordSize,
                   const UINT uiCodePage,
                   const bool fSetDefaultRasterFont /*= false*/) :
    FontInfoBase(pwszFaceName, bFamily, lWeight, fSetDefaultRasterFont, uiCodePage),
    _coordSize(coordSize),
    _coordSizeUnscaled(coordSize)
{
    ValidateFont();
}

FontInfo::FontInfo(const FontInfo& fiFont) :
    FontInfoBase(fiFont),
    _coordSize(fiFont.GetSize()),
    _coordSizeUnscaled(fiFont.GetUnscaledSize())
{
}

COORD FontInfo::GetUnscaledSize() const
{
    return _coordSizeUnscaled;
}

COORD FontInfo::GetSize() const
{
    return _coordSize;
}

void FontInfo::SetFromEngine(_In_ PCWSTR const pwszFaceName,
                             const BYTE bFamily,
                             const LONG lWeight,
                             const bool fSetDefaultRasterFont,
                             const COORD coordSize,
                             const COORD coordSizeUnscaled)
{
    FontInfoBase::SetFromEngine(pwszFaceName,
                                bFamily,
                                lWeight,
                                fSetDefaultRasterFont);

    _coordSize = coordSize;
    _coordSizeUnscaled = coordSizeUnscaled;

    _ValidateCoordSize();
}

void FontInfo::ValidateFont()
{
    _ValidateCoordSize();
}

void FontInfo::_ValidateCoordSize()
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

#pragma warning(push)
#pragma warning(suppress : 4356)
Microsoft::Console::Render::IFontDefaultList* FontInfo::s_pFontDefaultList;
#pragma warning(pop)

void FontInfo::s_SetFontDefaultList(_In_ Microsoft::Console::Render::IFontDefaultList* const pFontDefaultList)
{
    FontInfoBase::s_SetFontDefaultList(pFontDefaultList);
}
