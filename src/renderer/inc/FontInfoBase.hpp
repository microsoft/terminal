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

#define DEFAULT_TT_FONT_FACENAME L"__DefaultTTFont__"
#define DEFAULT_RASTER_FONT_FACENAME L"Terminal"

class FontInfoBase
{
public:
    FontInfoBase(_In_ PCWSTR const pwszFaceName,
                 const BYTE bFamily,
                 const LONG lWeight,
                 const bool fSetDefaultRasterFont,
                 const UINT uiCodePage);

    FontInfoBase(const FontInfoBase& fibFont);

    ~FontInfoBase();

    BYTE GetFamily() const;
    LONG GetWeight() const;
    PCWCHAR GetFaceName() const;
    UINT GetCodePage() const;

    bool IsTrueTypeFont() const;

    void SetFromEngine(_In_ PCWSTR const pwszFaceName,
                       const BYTE bFamily,
                       const LONG lWeight,
                       const bool fSetDefaultRasterFont);

    bool WasDefaultRasterSetFromEngine() const;
    void ValidateFont();

    static Microsoft::Console::Render::IFontDefaultList* s_pFontDefaultList;
    static void s_SetFontDefaultList(_In_ Microsoft::Console::Render::IFontDefaultList* const pFontDefaultList);

    friend bool operator==(const FontInfoBase& a, const FontInfoBase& b);

protected:
    bool IsDefaultRasterFontNoSize() const;

private:
    WCHAR _wszFaceName[LF_FACESIZE];
    LONG _lWeight;
    BYTE _bFamily;
    UINT _uiCodePage;
    bool _fDefaultRasterSetFromEngine;
};

bool operator==(const FontInfoBase& a, const FontInfoBase& b);
