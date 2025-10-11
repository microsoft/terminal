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


#ifndef FONT_INFO_DESIRED_HPP
#define FONT_INFO_DESIRED_HPP

#pragma once

#include "FontInfoBase.hpp"
#include "FontInfo.hpp"
#include "CSSLengthPercentage.h"

class FontInfoDesired : public FontInfoBase
{
public:
    FontInfoDesired(const std::wstring_view& faceName,
                    const unsigned char family,
                    const unsigned int weight,
                    const float fontSize,
                    const unsigned int uiCodePage) noexcept;
    explicit FontInfoDesired(const FontInfo& fiFont) noexcept;

    // Deleted comparison operator to avoid accidental equality checks
    bool operator==(const FontInfoDesired& other) const = delete;


    void SetCellSize(const CSSLengthPercentage& cellWidth, const CSSLengthPercentage& cellHeight) noexcept;
    void SetEnableBuiltinGlyphs(bool builtinGlyphs) noexcept;
    void SetEnableColorGlyphs(bool colorGlyphs) noexcept;

    // Getters (nodiscard prevents ignored important values)
    [[nodiscard]] const CSSLengthPercentage& GetCellWidth() const noexcept;
    [[nodiscard]] const CSSLengthPercentage& GetCellHeight() const noexcept;
    [[nodiscard]] bool GetEnableBuiltinGlyphs() const noexcept;
    [[nodiscard]] bool GetEnableColorGlyphs() const noexcept;
    [[nodiscard]] float GetFontSize() const noexcept;
    [[nodiscard]] til::size GetEngineSize() const noexcept;
    [[nodiscard]] bool IsDefaultRasterFont() const noexcept;

    // Destructor (only needed if polymorphism is expected)
    ~FontInfoDesired() override = default;

private:
    til::size _coordSizeDesired{};    // initialized to default (0,0)
    float _fontSize{ 0.0f };   // safe default
    CSSLengthPercentage _cellWidth;
    CSSLengthPercentage _cellHeight;
    bool _builtinGlyphs{ false };
    bool _colorGlyphs{ true };
};

