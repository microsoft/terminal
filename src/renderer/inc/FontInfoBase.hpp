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

inline constexpr wchar_t DEFAULT_TT_FONT_FACENAME[]{ L"__DefaultTTFont__" };
inline constexpr wchar_t DEFAULT_RASTER_FONT_FACENAME[]{ L"Terminal" };

class FontInfoBase
{
public:
    FontInfoBase(const std::wstring_view& faceName,
                 const unsigned char family,
                 const unsigned int weight,
                 const bool fSetDefaultRasterFont,
                 const unsigned int uiCodePage) noexcept;

    bool operator==(const FontInfoBase& other) noexcept;

    unsigned char GetFamily() const noexcept;
    unsigned int GetWeight() const noexcept;
    const std::wstring& GetFaceName() const noexcept;
    unsigned int GetCodePage() const noexcept;
    void FillLegacyNameBuffer(wchar_t (&buffer)[LF_FACESIZE]) const noexcept;
    bool IsTrueTypeFont() const noexcept;
    void SetFromEngine(const std::wstring_view& faceName,
                       const unsigned char family,
                       const unsigned int weight,
                       const bool fSetDefaultRasterFont) noexcept;
    bool WasDefaultRasterSetFromEngine() const noexcept;
    void ValidateFont() noexcept;

    static Microsoft::Console::Render::IFontDefaultList* s_pFontDefaultList;
    static void s_SetFontDefaultList(_In_ Microsoft::Console::Render::IFontDefaultList* const pFontDefaultList) noexcept;

protected:
    bool IsDefaultRasterFontNoSize() const noexcept;

private:
    std::wstring _faceName;
    unsigned int _weight;
    unsigned char _family;
    unsigned int _codePage;
    bool _fDefaultRasterSetFromEngine;
};
