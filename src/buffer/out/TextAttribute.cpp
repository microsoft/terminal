// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "TextAttribute.hpp"
#include "../../inc/conattrs.hpp"

bool TextAttribute::IsLegacy() const noexcept
{
    return _foreground.IsLegacy() && _background.IsLegacy();
}

// Arguments:
// - None
// Return Value:
// - color that should be displayed as the foreground color
COLORREF TextAttribute::CalculateRgbForeground(std::basic_string_view<COLORREF> colorTable,
                                               COLORREF defaultFgColor,
                                               COLORREF defaultBgColor) const noexcept
{
    return _IsReverseVideo() ? _GetRgbBackground(colorTable, defaultBgColor) : _GetRgbForeground(colorTable, defaultFgColor);
}

// Routine Description:
// - Calculates rgb background color based off of current color table and active modification attributes
// Arguments:
// - None
// Return Value:
// - color that should be displayed as the background color
COLORREF TextAttribute::CalculateRgbBackground(std::basic_string_view<COLORREF> colorTable,
                                               COLORREF defaultFgColor,
                                               COLORREF defaultBgColor) const noexcept
{
    return _IsReverseVideo() ? _GetRgbForeground(colorTable, defaultFgColor) : _GetRgbBackground(colorTable, defaultBgColor);
}

// Routine Description:
// - gets rgb foreground color, possibly based off of current color table. Does not take active modification
// attributes into account
// Arguments:
// - None
// Return Value:
// - color that is stored as the foreground color
COLORREF TextAttribute::_GetRgbForeground(std::basic_string_view<COLORREF> colorTable,
                                          COLORREF defaultColor) const noexcept
{
    return _foreground.GetColor(colorTable, defaultColor, IsBold());
}

// Routine Description:
// - gets rgb background color, possibly based off of current color table. Does not take active modification
// attributes into account
// Arguments:
// - None
// Return Value:
// - color that is stored as the background color
COLORREF TextAttribute::_GetRgbBackground(std::basic_string_view<COLORREF> colorTable,
                                          COLORREF defaultColor) const noexcept
{
    return _background.GetColor(colorTable, defaultColor, false);
}

void TextAttribute::SetMetaAttributes(const WORD wMeta) noexcept
{
    WI_UpdateFlagsInMask(_wAttrLegacy, META_ATTRS, wMeta);
    WI_ClearAllFlags(_wAttrLegacy, COMMON_LVB_SBCSDBCS);
}

WORD TextAttribute::GetMetaAttributes() const noexcept
{
    WORD wMeta = _wAttrLegacy;
    WI_ClearAllFlags(wMeta, FG_ATTRS);
    WI_ClearAllFlags(wMeta, BG_ATTRS);
    WI_ClearAllFlags(wMeta, COMMON_LVB_SBCSDBCS);
    return wMeta;
}

void TextAttribute::SetForeground(const COLORREF rgbForeground) noexcept
{
    _foreground = TextColor(rgbForeground);
}

void TextAttribute::SetBackground(const COLORREF rgbBackground) noexcept
{
    _background = TextColor(rgbBackground);
}

void TextAttribute::SetFromLegacy(const WORD wLegacy) noexcept
{
    _wAttrLegacy = gsl::narrow_cast<WORD>(wLegacy & META_ATTRS);
    WI_ClearAllFlags(_wAttrLegacy, COMMON_LVB_SBCSDBCS);
    const BYTE fgIndex = gsl::narrow_cast<BYTE>(wLegacy & FG_ATTRS);
    const BYTE bgIndex = gsl::narrow_cast<BYTE>(wLegacy & BG_ATTRS) >> 4;
    _foreground = TextColor(fgIndex);
    _background = TextColor(bgIndex);
}

void TextAttribute::SetLegacyAttributes(const WORD attrs,
                                        const bool setForeground,
                                        const bool setBackground,
                                        const bool setMeta) noexcept
{
    if (setForeground)
    {
        const BYTE fgIndex = gsl::narrow_cast<BYTE>(attrs & FG_ATTRS);
        _foreground = TextColor(fgIndex);
    }
    if (setBackground)
    {
        const BYTE bgIndex = gsl::narrow_cast<BYTE>(attrs & BG_ATTRS) >> 4;
        _background = TextColor(bgIndex);
    }
    if (setMeta)
    {
        SetMetaAttributes(attrs);
    }
}

// Method Description:
// - Sets the foreground and/or background to a particular index in the 256color
//      table. If either parameter is nullptr, it's ignored.
//   This method can be used to set the colors to indexes in the range [0, 255],
//      as opposed to SetLegacyAttributes, which clamps them to [0,15]
// Arguments:
// - foreground: nullptr if we should ignore this attr, else a pointer to a byte
//      value to use as an index into the 256-color table.
// - background: nullptr if we should ignore this attr, else a pointer to a byte
//      value to use as an index into the 256-color table.
// Return Value:
// - <none>
void TextAttribute::SetIndexedAttributes(const std::optional<const BYTE> foreground,
                                         const std::optional<const BYTE> background) noexcept
{
    if (foreground)
    {
        const BYTE fgIndex = (*foreground) & 0xFF;
        _foreground = TextColor(fgIndex);
    }
    if (background)
    {
        const BYTE bgIndex = (*background) & 0xFF;
        _background = TextColor(bgIndex);
    }
}

void TextAttribute::SetColor(const COLORREF rgbColor, const bool fIsForeground) noexcept
{
    if (fIsForeground)
    {
        SetForeground(rgbColor);
    }
    else
    {
        SetBackground(rgbColor);
    }
}

bool TextAttribute::_IsReverseVideo() const noexcept
{
    return WI_IsFlagSet(_wAttrLegacy, COMMON_LVB_REVERSE_VIDEO);
}

bool TextAttribute::IsLeadingByte() const noexcept
{
    return WI_IsFlagSet(_wAttrLegacy, COMMON_LVB_LEADING_BYTE);
}

bool TextAttribute::IsTrailingByte() const noexcept
{
    return WI_IsFlagSet(_wAttrLegacy, COMMON_LVB_LEADING_BYTE);
}

bool TextAttribute::IsTopHorizontalDisplayed() const noexcept
{
    return WI_IsFlagSet(_wAttrLegacy, COMMON_LVB_GRID_HORIZONTAL);
}

bool TextAttribute::IsBottomHorizontalDisplayed() const noexcept
{
    return WI_IsFlagSet(_wAttrLegacy, COMMON_LVB_UNDERSCORE);
}

bool TextAttribute::IsLeftVerticalDisplayed() const noexcept
{
    return WI_IsFlagSet(_wAttrLegacy, COMMON_LVB_GRID_LVERTICAL);
}

bool TextAttribute::IsRightVerticalDisplayed() const noexcept
{
    return WI_IsFlagSet(_wAttrLegacy, COMMON_LVB_GRID_RVERTICAL);
}

void TextAttribute::SetLeftVerticalDisplayed(const bool isDisplayed) noexcept
{
    WI_UpdateFlag(_wAttrLegacy, COMMON_LVB_GRID_LVERTICAL, isDisplayed);
}

void TextAttribute::SetRightVerticalDisplayed(const bool isDisplayed) noexcept
{
    WI_UpdateFlag(_wAttrLegacy, COMMON_LVB_GRID_RVERTICAL, isDisplayed);
}

void TextAttribute::Embolden() noexcept
{
    _SetBoldness(true);
}

void TextAttribute::Debolden() noexcept
{
    _SetBoldness(false);
}

void TextAttribute::SetExtendedAttributes(const ExtendedAttributes attrs) noexcept
{
    _extendedAttrs = attrs;
}

// Routine Description:
// - swaps foreground and background color
void TextAttribute::Invert() noexcept
{
    WI_ToggleFlag(_wAttrLegacy, COMMON_LVB_REVERSE_VIDEO);
}

void TextAttribute::_SetBoldness(const bool isBold) noexcept
{
    WI_UpdateFlag(_extendedAttrs, ExtendedAttributes::Bold, isBold);
}

void TextAttribute::SetDefaultForeground() noexcept
{
    _foreground = TextColor();
}

void TextAttribute::SetDefaultBackground() noexcept
{
    _background = TextColor();
}

// Method Description:
// - Returns true if this attribute indicates its foreground is the "default"
//      foreground. Its _rgbForeground will contain the actual value of the
//      default foreground. If the default colors are ever changed, this method
//      should be used to identify attributes with the default fg value, and
//      update them accordingly.
// Arguments:
// - <none>
// Return Value:
// - true iff this attribute indicates it's the "default" foreground color.
bool TextAttribute::ForegroundIsDefault() const noexcept
{
    return _foreground.IsDefault();
}

// Method Description:
// - Returns true if this attribute indicates its background is the "default"
//      background. Its _rgbBackground will contain the actual value of the
//      default background. If the default colors are ever changed, this method
//      should be used to identify attributes with the default bg value, and
//      update them accordingly.
// Arguments:
// - <none>
// Return Value:
// - true iff this attribute indicates it's the "default" background color.
bool TextAttribute::BackgroundIsDefault() const noexcept
{
    return _background.IsDefault();
}
