// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <cwchar>

#include "..\inc\FontInfoBase.hpp"

bool operator==(const FontInfoBase& a, const FontInfoBase& b)
{
    return (wcscmp(a._wszFaceName, b._wszFaceName) == 0 &&
            a._lWeight == b._lWeight &&
            a._bFamily == b._bFamily &&
            a._uiCodePage == b._uiCodePage &&
            a._fDefaultRasterSetFromEngine == b._fDefaultRasterSetFromEngine);
}

FontInfoBase::FontInfoBase(_In_ PCWSTR const pwszFaceName,
                           const BYTE bFamily,
                           const LONG lWeight,
                           const bool fSetDefaultRasterFont,
                           const UINT uiCodePage) :
    _bFamily(bFamily),
    _lWeight(lWeight),
    _fDefaultRasterSetFromEngine(fSetDefaultRasterFont),
    _uiCodePage(uiCodePage)
{
    if (nullptr != pwszFaceName)
    {
        wcscpy_s(_wszFaceName, ARRAYSIZE(_wszFaceName), pwszFaceName);
    }

    ValidateFont();
}

FontInfoBase::FontInfoBase(const FontInfoBase& fibFont) :
    FontInfoBase(fibFont.GetFaceName(),
                 fibFont.GetFamily(),
                 fibFont.GetWeight(),
                 fibFont.WasDefaultRasterSetFromEngine(),
                 fibFont.GetCodePage())
{
}

FontInfoBase::~FontInfoBase()
{
}

BYTE FontInfoBase::GetFamily() const
{
    return _bFamily;
}

// When the default raster font is forced set from the engine, this is how we differentiate it from a simple apply.
// Default raster font is internally represented as a blank face name and zeros for weight, family, and size. This is
// the hint for the engine to use whatever comes back from GetStockObject(OEM_FIXED_FONT) (at least in the GDI world).
bool FontInfoBase::WasDefaultRasterSetFromEngine() const
{
    return _fDefaultRasterSetFromEngine;
}

LONG FontInfoBase::GetWeight() const
{
    return _lWeight;
}

PCWCHAR FontInfoBase::GetFaceName() const
{
    return (PCWCHAR)_wszFaceName;
}

UINT FontInfoBase::GetCodePage() const
{
    return _uiCodePage;
}

// NOTE: this method is intended to only be used from the engine itself to respond what font it has chosen.
void FontInfoBase::SetFromEngine(_In_ PCWSTR const pwszFaceName,
                                 const BYTE bFamily,
                                 const LONG lWeight,
                                 const bool fSetDefaultRasterFont)
{
    wcscpy_s(_wszFaceName, ARRAYSIZE(_wszFaceName), pwszFaceName);
    _bFamily = bFamily;
    _lWeight = lWeight;
    _fDefaultRasterSetFromEngine = fSetDefaultRasterFont;
}

// Internally, default raster font is represented by empty facename, and zeros for weight, family, and size. Since
// FontInfoBase doesn't have sizing information, this helper checks everything else.
bool FontInfoBase::IsDefaultRasterFontNoSize() const
{
    return (_lWeight == 0 && _bFamily == 0 && wcsnlen_s(_wszFaceName, ARRAYSIZE(_wszFaceName)) == 0);
}

void FontInfoBase::ValidateFont()
{
    // If we were given a blank name, it meant raster fonts, which to us is always Terminal.
    if (!IsDefaultRasterFontNoSize() && s_pFontDefaultList != nullptr)
    {
        // If we have a list of default fonts and our current font is the placeholder for the defaults, substitute here.
        if (0 == wcsncmp(_wszFaceName, DEFAULT_TT_FONT_FACENAME, ARRAYSIZE(DEFAULT_TT_FONT_FACENAME)))
        {
            WCHAR pwszDefaultFontFace[LF_FACESIZE];
            if (SUCCEEDED(s_pFontDefaultList->RetrieveDefaultFontNameForCodepage(GetCodePage(),
                                                                                 pwszDefaultFontFace,
                                                                                 ARRAYSIZE(pwszDefaultFontFace))))
            {
                wcscpy_s(_wszFaceName, ARRAYSIZE(_wszFaceName), pwszDefaultFontFace);

                // If we're assigning a default true type font name, make sure the family is also set to TrueType
                // to help GDI select the appropriate font when we actually create it.
                _bFamily = TMPF_TRUETYPE;
            }
        }
    }
}

bool FontInfoBase::IsTrueTypeFont() const
{
    return (_bFamily & TMPF_TRUETYPE) != 0;
}

void FontInfoBase::s_SetFontDefaultList(_In_ Microsoft::Console::Render::IFontDefaultList* const pFontDefaultList)
{
    s_pFontDefaultList = pFontDefaultList;
}
