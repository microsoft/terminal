/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- FontInfo.hpp

Abstract:
- This serves as the structure defining font information.

- FontInfoDesired - derived from FontInfoBase.  It also contains
  a desired size { X, Y }, to be supplied to the GDI's LOGFONT
  structure.  Unlike FontInfo, both desired X and Y can be zero.

Author(s):
- Michael Niksa (MiNiksa) 17-Nov-2015
--*/

#pragma once

#include "FontInfoBase.hpp"
#include "CSSLengthPercentage.h"

struct FontInfoDesired : FontInfoBase
{
    const CSSLengthPercentage& GetCellWidth() const noexcept;
    const CSSLengthPercentage& GetCellHeight() const noexcept;
    float GetFontSizeInPt() const noexcept;
    bool GetEnableBuiltinGlyphs() const noexcept;
    bool GetEnableColorGlyphs() const noexcept;

    void SetCellWidth(CSSLengthPercentage cellWidth) noexcept;
    void SetCellHeight(CSSLengthPercentage cellHeight) noexcept;
    void SetFontSizeInPt(float fontSizeInPt) noexcept;
    void SetEnableBuiltinGlyphs(bool builtinGlyphs) noexcept;
    void SetEnableColorGlyphs(bool colorGlyphs) noexcept;

    til::size GetPixelCellSize() const noexcept;
    void SetPixelCellSize(til::size size) noexcept;
    bool IsTrueTypeFont() const noexcept;
    bool IsDefaultRasterFont() const noexcept;

private:
    CSSLengthPercentage _cellWidth;
    CSSLengthPercentage _cellHeight;
    float _fontSizeInPt = 0;
    bool _builtinGlyphs = false;
    bool _colorGlyphs = true;
};
