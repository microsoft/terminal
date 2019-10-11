// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <cwchar>

#include "..\inc\FontInfoBase.hpp"

bool operator==(const FontInfoBase& a, const FontInfoBase& b)
{
    return a._faceName == b._faceName &&
           a._weight == b._weight &&
           a._family == b._family &&
           a._codePage == b._codePage &&
           a._fDefaultRasterSetFromEngine == b._fDefaultRasterSetFromEngine;
}

FontInfoBase::FontInfoBase(const std::wstring_view faceName,
                           const unsigned char family,
                           const unsigned int weight,
                           const bool fSetDefaultRasterFont,
                           const unsigned int codePage) :
    _faceName(faceName),
    _family(family),
    _weight(weight),
    _fDefaultRasterSetFromEngine(fSetDefaultRasterFont),
    _codePage(codePage)
{
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

unsigned char FontInfoBase::GetFamily() const
{
    return _family;
}

// When the default raster font is forced set from the engine, this is how we differentiate it from a simple apply.
// Default raster font is internally represented as a blank face name and zeros for weight, family, and size. This is
// the hint for the engine to use whatever comes back from GetStockObject(OEM_FIXED_FONT) (at least in the GDI world).
bool FontInfoBase::WasDefaultRasterSetFromEngine() const
{
    return _fDefaultRasterSetFromEngine;
}

unsigned int FontInfoBase::GetWeight() const
{
    return _weight;
}

const std::wstring_view FontInfoBase::GetFaceName() const
{
    return _faceName;
}

unsigned int FontInfoBase::GetCodePage() const
{
    return _codePage;
}

// Method Description:
// - Populates a fixed-length **null-terminated** buffer with the name of this font, truncating it as necessary.
//   Positions within the buffer that are not filled by the font name are zeroed.
// Arguments:
// - buffer: the buffer into which to copy characters
// - size: the size of buffer
HRESULT FontInfoBase::FillLegacyNameBuffer(gsl::span<wchar_t> buffer) const
try
{
    auto toCopy = std::min<size_t>(buffer.size() - 1, _faceName.size());
    auto last = std::copy(_faceName.cbegin(), _faceName.cbegin() + toCopy, buffer.begin());
    std::fill(last, buffer.end(), L'\0');
    return S_OK;
}
CATCH_RETURN();

// NOTE: this method is intended to only be used from the engine itself to respond what font it has chosen.
void FontInfoBase::SetFromEngine(const std::wstring_view faceName,
                                 const unsigned char family,
                                 const unsigned int weight,
                                 const bool fSetDefaultRasterFont)
{
    _faceName = faceName;
    _family = family;
    _weight = weight;
    _fDefaultRasterSetFromEngine = fSetDefaultRasterFont;
}

// Internally, default raster font is represented by empty facename, and zeros for weight, family, and size. Since
// FontInfoBase doesn't have sizing information, this helper checks everything else.
bool FontInfoBase::IsDefaultRasterFontNoSize() const
{
    return (_weight == 0 && _family == 0 && _faceName.empty());
}

void FontInfoBase::ValidateFont()
{
    // If we were given a blank name, it meant raster fonts, which to us is always Terminal.
    if (!IsDefaultRasterFontNoSize() && s_pFontDefaultList != nullptr)
    {
        // If we have a list of default fonts and our current font is the placeholder for the defaults, substitute here.
        if (_faceName == DEFAULT_TT_FONT_FACENAME)
        {
            std::wstring defaultFontFace;
            if (SUCCEEDED(s_pFontDefaultList->RetrieveDefaultFontNameForCodepage(GetCodePage(),
                                                                                 defaultFontFace)))
            {
                _faceName = defaultFontFace;

                // If we're assigning a default true type font name, make sure the family is also set to TrueType
                // to help GDI select the appropriate font when we actually create it.
                _family = TMPF_TRUETYPE;
            }
        }
    }
}

bool FontInfoBase::IsTrueTypeFont() const
{
    return WI_IsFlagSet(_family, TMPF_TRUETYPE);
}

Microsoft::Console::Render::IFontDefaultList* FontInfoBase::s_pFontDefaultList;

void FontInfoBase::s_SetFontDefaultList(_In_ Microsoft::Console::Render::IFontDefaultList* const pFontDefaultList)
{
    s_pFontDefaultList = pFontDefaultList;
}
