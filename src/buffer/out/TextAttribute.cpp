// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "TextAttribute.hpp"
#include "../../inc/conattrs.hpp"

BYTE TextAttribute::s_legacyDefaultForeground = 7;
BYTE TextAttribute::s_legacyDefaultBackground = 0;

// Routine Description:
// - Sets the legacy attributes which map to and from the default colors.
// Parameters:
// - defaultAttributes: the attribute values to be used for default colors.
// Return value:
// - None
void TextAttribute::SetLegacyDefaultAttributes(const WORD defaultAttributes) noexcept
{
    s_legacyDefaultForeground = defaultAttributes & FG_ATTRS;
    s_legacyDefaultBackground = (defaultAttributes & BG_ATTRS) >> 4;
}

// Routine Description:
// - Returns a WORD with legacy-style attributes for this textattribute.
// Parameters:
// - None
// Return value:
// - a WORD with legacy-style attributes for this textattribute.
WORD TextAttribute::GetLegacyAttributes() const noexcept
{
    const BYTE fgIndex = _foreground.GetLegacyIndex(s_legacyDefaultForeground);
    const BYTE bgIndex = _background.GetLegacyIndex(s_legacyDefaultBackground);
    const WORD metaAttrs = _wAttrLegacy & META_ATTRS;
    const bool brighten = IsBold() && _foreground.CanBeBrightened();
    return fgIndex | (bgIndex << 4) | metaAttrs | (brighten ? FOREGROUND_INTENSITY : 0);
}

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
    return IsReverseVideo() ? _GetRgbBackground(colorTable, defaultBgColor) : _GetRgbForeground(colorTable, defaultFgColor);
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
    return IsReverseVideo() ? _GetRgbForeground(colorTable, defaultFgColor) : _GetRgbBackground(colorTable, defaultBgColor);
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

TextColor TextAttribute::GetForeground() const noexcept
{
    return _foreground;
}

TextColor TextAttribute::GetBackground() const noexcept
{
    return _background;
}

void TextAttribute::SetForeground(const TextColor foreground) noexcept
{
    _foreground = foreground;
}

void TextAttribute::SetBackground(const TextColor background) noexcept
{
    _background = background;
}

void TextAttribute::SetForeground(const COLORREF rgbForeground) noexcept
{
    _foreground = TextColor(rgbForeground);
}

void TextAttribute::SetBackground(const COLORREF rgbBackground) noexcept
{
    _background = TextColor(rgbBackground);
}

void TextAttribute::SetIndexedForeground(const BYTE fgIndex) noexcept
{
    _foreground = TextColor(fgIndex, false);
}

void TextAttribute::SetIndexedBackground(const BYTE bgIndex) noexcept
{
    _background = TextColor(bgIndex, false);
}

void TextAttribute::SetIndexedForeground256(const BYTE fgIndex) noexcept
{
    _foreground = TextColor(fgIndex, true);
}

void TextAttribute::SetIndexedBackground256(const BYTE bgIndex) noexcept
{
    _background = TextColor(bgIndex, true);
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

bool TextAttribute::IsBold() const noexcept
{
    return WI_IsFlagSet(_extendedAttrs, ExtendedAttributes::Bold);
}

bool TextAttribute::IsItalic() const noexcept
{
    return WI_IsFlagSet(_extendedAttrs, ExtendedAttributes::Italics);
}

bool TextAttribute::IsBlinking() const noexcept
{
    return WI_IsFlagSet(_extendedAttrs, ExtendedAttributes::Blinking);
}

bool TextAttribute::IsInvisible() const noexcept
{
    return WI_IsFlagSet(_extendedAttrs, ExtendedAttributes::Invisible);
}

bool TextAttribute::IsCrossedOut() const noexcept
{
    return WI_IsFlagSet(_extendedAttrs, ExtendedAttributes::CrossedOut);
}

bool TextAttribute::IsUnderlined() const noexcept
{
    // TODO:GH#2915 Treat underline separately from LVB_UNDERSCORE
    return WI_IsFlagSet(_wAttrLegacy, COMMON_LVB_UNDERSCORE);
}

bool TextAttribute::IsReverseVideo() const noexcept
{
    return WI_IsFlagSet(_wAttrLegacy, COMMON_LVB_REVERSE_VIDEO);
}

void TextAttribute::SetBold(bool isBold) noexcept
{
    WI_UpdateFlag(_extendedAttrs, ExtendedAttributes::Bold, isBold);
}

void TextAttribute::SetItalics(bool isItalic) noexcept
{
    WI_UpdateFlag(_extendedAttrs, ExtendedAttributes::Italics, isItalic);
}

void TextAttribute::SetBlinking(bool isBlinking) noexcept
{
    WI_UpdateFlag(_extendedAttrs, ExtendedAttributes::Blinking, isBlinking);
}

void TextAttribute::SetInvisible(bool isInvisible) noexcept
{
    WI_UpdateFlag(_extendedAttrs, ExtendedAttributes::Invisible, isInvisible);
}

void TextAttribute::SetCrossedOut(bool isCrossedOut) noexcept
{
    WI_UpdateFlag(_extendedAttrs, ExtendedAttributes::CrossedOut, isCrossedOut);
}

void TextAttribute::SetUnderline(bool isUnderlined) noexcept
{
    // TODO:GH#2915 Treat underline separately from LVB_UNDERSCORE
    WI_UpdateFlag(_wAttrLegacy, COMMON_LVB_UNDERSCORE, isUnderlined);
}

void TextAttribute::SetReverseVideo(bool isReversed) noexcept
{
    WI_UpdateFlag(_wAttrLegacy, COMMON_LVB_REVERSE_VIDEO, isReversed);
}

ExtendedAttributes TextAttribute::GetExtendedAttributes() const noexcept
{
    return _extendedAttrs;
}

// Routine Description:
// - swaps foreground and background color
void TextAttribute::Invert() noexcept
{
    WI_ToggleFlag(_wAttrLegacy, COMMON_LVB_REVERSE_VIDEO);
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

// Routine Description:
// - Resets the meta and extended attributes, which is what the VT standard
//      requires for most erasing and filling operations.
void TextAttribute::SetStandardErase() noexcept
{
    _extendedAttrs = ExtendedAttributes::Normal;
    _wAttrLegacy = 0;
}
