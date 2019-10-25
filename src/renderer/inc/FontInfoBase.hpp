/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- FontInfoBase.hpp

Abstract:
- This serves as the structure defining font information.

- FontInfoBase - the base class that holds the font's GDI's LOGFONT
  lfFaceName, lfWeight and lfPitchAndFamily, as well as the code page
  to use for WideCharToMultiByte and font name.

Author(s):
- Michael Niksa (MiNiksa) 17-Nov-2015
--*/

#pragma once

#include "IFontDefaultList.hpp"

static constexpr wchar_t DEFAULT_TT_FONT_FACENAME[]{ L"__DefaultTTFont__" };
static constexpr wchar_t DEFAULT_RASTER_FONT_FACENAME[]{ L"Terminal" };

class FontInfoBase
{
public:
    FontInfoBase(const std::wstring_view faceName,
                 const unsigned char family,
                 const unsigned int weight,
                 const bool fSetDefaultRasterFont,
                 const unsigned int uiCodePage);

    FontInfoBase(const FontInfoBase& fibFont);

    ~FontInfoBase();

    unsigned char GetFamily() const;
    unsigned int GetWeight() const;
    const std::wstring_view GetFaceName() const;
    unsigned int GetCodePage() const;

    HRESULT FillLegacyNameBuffer(gsl::span<wchar_t> buffer) const;

    bool IsTrueTypeFont() const;

    void SetFromEngine(const std::wstring_view faceName,
                       const unsigned char family,
                       const unsigned int weight,
                       const bool fSetDefaultRasterFont);

    bool WasDefaultRasterSetFromEngine() const;
    void ValidateFont();

    static Microsoft::Console::Render::IFontDefaultList* s_pFontDefaultList;
    static void s_SetFontDefaultList(_In_ Microsoft::Console::Render::IFontDefaultList* const pFontDefaultList);

    friend bool operator==(const FontInfoBase& a, const FontInfoBase& b);

protected:
    bool IsDefaultRasterFontNoSize() const;

private:
    std::wstring _faceName;
    unsigned int _weight;
    unsigned char _family;
    unsigned int _codePage;
    bool _fDefaultRasterSetFromEngine;
};

bool operator==(const FontInfoBase& a, const FontInfoBase& b);
