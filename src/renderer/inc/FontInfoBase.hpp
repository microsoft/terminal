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

inline constexpr wchar_t DEFAULT_TT_FONT_FACENAME[]{ L"__DefaultTTFont__" };
inline constexpr wchar_t DEFAULT_RASTER_FONT_FACENAME[]{ L"Terminal" };

struct FontInfoBase
{
    const std::wstring& GetFaceName() const noexcept;
    unsigned char GetFamily() const noexcept;
    unsigned int GetWeight() const noexcept;
    unsigned int GetCodePage() const noexcept;

    void SetFaceName(std::wstring faceName) noexcept;
    void SetFamily(unsigned char family) noexcept;
    void SetWeight(unsigned int weight) noexcept;
    void SetCodePage(unsigned int codePage) noexcept;

    void FillLegacyNameBuffer(wchar_t (&buffer)[LF_FACESIZE]) const noexcept;

protected:
    std::wstring _faceName;
    unsigned char _family = 0;
    unsigned int _weight = 0;
    unsigned int _codePage = 0;
};
