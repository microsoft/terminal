// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "../inc/FontInfoBase.hpp"

FontInfoBase::FontInfoBase(const std::wstring_view& faceName,
                           const unsigned char family,
                           const unsigned int weight,
                           const bool fSetDefaultRasterFont,
                           const unsigned int codePage) noexcept :
    _faceName(faceName),
    _family(family),
    _weight(weight),
    _fDefaultRasterSetFromEngine(fSetDefaultRasterFont),
    _codePage(codePage)
{
    ValidateFont();
}

bool FontInfoBase::operator==(const FontInfoBase& other) noexcept
{
    return _faceName == other._faceName &&
           _weight == other._weight &&
           _family == other._family &&
           _codePage == other._codePage &&
           _fDefaultRasterSetFromEngine == other._fDefaultRasterSetFromEngine;
}

unsigned char FontInfoBase::GetFamily() const noexcept
{
    return _family;
}

// When the default raster font is forced set from the engine, this is how we differentiate it from a simple apply.
// Default raster font is internally represented as a blank face name and zeros for weight, family, and size. This is
// the hint for the engine to use whatever comes back from GetStockObject(OEM_FIXED_FONT) (at least in the GDI world).
bool FontInfoBase::WasDefaultRasterSetFromEngine() const noexcept
{
    return _fDefaultRasterSetFromEngine;
}

unsigned int FontInfoBase::GetWeight() const noexcept
{
    return _weight;
}

const std::wstring& FontInfoBase::GetFaceName() const noexcept
{
    return _faceName;
}

unsigned int FontInfoBase::GetCodePage() const noexcept
{
    return _codePage;
}

// Method Description:
// - Populates a fixed-length **null-terminated** buffer with the name of this font, truncating it as necessary.
//   Positions within the buffer that are not filled by the font name are zeroed.
// Arguments:
// - buffer: the buffer into which to copy characters
// - size: the size of buffer
void FontInfoBase::FillLegacyNameBuffer(wchar_t (&buffer)[LF_FACESIZE]) const noexcept
{
    const auto toCopy = std::min(std::size(buffer) - 1, _faceName.size());
    const auto last = std::copy_n(_faceName.data(), toCopy, &buffer[0]);
    *last = L'\0';
}

// NOTE: this method is intended to only be used from the engine itself to respond what font it has chosen.
void FontInfoBase::SetFromEngine(const std::wstring_view& faceName,
                                 const unsigned char family,
                                 const unsigned int weight,
                                 const bool fSetDefaultRasterFont) noexcept
{
    _faceName = faceName;
    _family = family;
    _weight = weight;
    _fDefaultRasterSetFromEngine = fSetDefaultRasterFont;
}

// Internally, default raster font is represented by empty facename, and zeros for weight, family, and size. Since
// FontInfoBase doesn't have sizing information, this helper checks everything else.
bool FontInfoBase::IsDefaultRasterFontNoSize() const noexcept
{
    return (_weight == 0 && _family == 0 && _faceName.empty());
}

void FontInfoBase::ValidateFont() noexcept
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

bool FontInfoBase::IsTrueTypeFont() const noexcept
{
    return WI_IsFlagSet(_family, TMPF_TRUETYPE);
}

Microsoft::Console::Render::IFontDefaultList* FontInfoBase::s_pFontDefaultList;

void FontInfoBase::s_SetFontDefaultList(_In_ Microsoft::Console::Render::IFontDefaultList* const pFontDefaultList) noexcept
{
    s_pFontDefaultList = pFontDefaultList;
}
