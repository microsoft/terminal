// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "TextAttribute.hpp"
#include "../../inc/conattrs.hpp"

// Keeping TextColor compact helps us keeping TextAttribute compact,
// which in turn ensures that our buffer memory usage is low.
static_assert(sizeof(TextAttribute) == 14);
static_assert(alignof(TextAttribute) == 2);
// Ensure that we can memcpy() and memmove() the struct for performance.
static_assert(std::is_trivially_copyable_v<TextAttribute>);

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
// Pursuant to GH#6807
// This routine replaces VT colors from the 16-color set with the "default"
// flag. It is intended to be used as part of the "VT Quirk" in
// WriteConsole[AW].
//
// There is going to be a very long tail of applications that will
// explicitly request VT SGR 40/37 when what they really want is to
// SetConsoleTextAttribute() with a black background/white foreground.
// Instead of making those applications look bad (and therefore making us
// look bad, because we're releasing this as an update to something that
// "looks good" already), we're introducing this compatibility hack. Before
// the color reckoning in GH#6698 + GH#6506, *every* color was subject to
// being spontaneously and erroneously turned into the default color. Now,
// only the 16-color palette value that matches the active console
// background color will be destroyed when the quirk is enabled.
//
// This is not intended to be a long-term solution. This comment will be
// discovered in forty years(*) time and people will laugh at our hubris.
//
// *it doesn't matter when you're reading this, it will always be 40 years
// from now.
TextAttribute TextAttribute::StripErroneousVT16VersionsOfLegacyDefaults(const TextAttribute& attribute) noexcept
{
    const auto fg{ attribute.GetForeground() };
    const auto bg{ attribute.GetBackground() };
    auto copy{ attribute };
    if (fg.IsIndex16() &&
        attribute.IsBold() == WI_IsFlagSet(s_legacyDefaultForeground, FOREGROUND_INTENSITY) &&
        fg.GetIndex() == (s_legacyDefaultForeground & ~FOREGROUND_INTENSITY))
    {
        // We don't want to turn 1;37m into 39m (or even 1;39m), as this was meant to mimic a legacy color.
        copy.SetDefaultForeground();
    }
    if (bg.IsIndex16() && bg.GetIndex() == s_legacyDefaultBackground)
    {
        copy.SetDefaultBackground();
    }
    return copy;
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

// Routine Description:
// - Calculates rgb colors based off of current color table and active modification attributes.
// Arguments:
// - colorTable: the current color table rgb values.
// - defaultFgColor: the default foreground color rgb value.
// - defaultBgColor: the default background color rgb value.
// - reverseScreenMode: true if the screen mode is reversed.
// - blinkingIsFaint: true if blinking should be interpreted as faint.
// Return Value:
// - the foreground and background colors that should be displayed.
std::pair<COLORREF, COLORREF> TextAttribute::CalculateRgbColors(const gsl::span<const COLORREF> colorTable,
                                                                const COLORREF defaultFgColor,
                                                                const COLORREF defaultBgColor,
                                                                const bool reverseScreenMode,
                                                                const bool blinkingIsFaint) const noexcept
{
    auto fg = _foreground.GetColor(colorTable, defaultFgColor, IsBold());
    auto bg = _background.GetColor(colorTable, defaultBgColor);
    if (IsFaint() || (IsBlinking() && blinkingIsFaint))
    {
        fg = (fg >> 1) & 0x7F7F7F; // Divide foreground color components by two.
    }
    if (IsReverseVideo() ^ reverseScreenMode)
    {
        std::swap(fg, bg);
    }
    if (IsInvisible())
    {
        fg = bg;
    }
    return { fg, bg };
}

// Method description:
// - Tells us whether the text is a hyperlink or not
// Return value:
// - True if it is a hyperlink, false otherwise
bool TextAttribute::IsHyperlink() const noexcept
{
    // All non-hyperlink text have a default hyperlinkId of 0 while
    // all hyperlink text have a non-zero hyperlinkId
    return _hyperlinkId != 0;
}

TextColor TextAttribute::GetForeground() const noexcept
{
    return _foreground;
}

TextColor TextAttribute::GetBackground() const noexcept
{
    return _background;
}

// Method description:
// - Retrieves the hyperlink ID of the text
// Return value:
// - The hyperlink ID
uint16_t TextAttribute::GetHyperlinkId() const noexcept
{
    return _hyperlinkId;
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

// Method description:
// - Sets the hyperlink ID of the text
// Arguments:
// - id - the id we wish to set
void TextAttribute::SetHyperlinkId(uint16_t id) noexcept
{
    _hyperlinkId = id;
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

bool TextAttribute::IsFaint() const noexcept
{
    return WI_IsFlagSet(_extendedAttrs, ExtendedAttributes::Faint);
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
    return WI_IsFlagSet(_extendedAttrs, ExtendedAttributes::Underlined);
}

bool TextAttribute::IsDoublyUnderlined() const noexcept
{
    return WI_IsFlagSet(_extendedAttrs, ExtendedAttributes::DoublyUnderlined);
}

bool TextAttribute::IsOverlined() const noexcept
{
    return WI_IsFlagSet(_wAttrLegacy, COMMON_LVB_GRID_HORIZONTAL);
}

bool TextAttribute::IsReverseVideo() const noexcept
{
    return WI_IsFlagSet(_wAttrLegacy, COMMON_LVB_REVERSE_VIDEO);
}

void TextAttribute::SetBold(bool isBold) noexcept
{
    WI_UpdateFlag(_extendedAttrs, ExtendedAttributes::Bold, isBold);
}

void TextAttribute::SetFaint(bool isFaint) noexcept
{
    WI_UpdateFlag(_extendedAttrs, ExtendedAttributes::Faint, isFaint);
}

void TextAttribute::SetItalic(bool isItalic) noexcept
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

void TextAttribute::SetUnderlined(bool isUnderlined) noexcept
{
    WI_UpdateFlag(_extendedAttrs, ExtendedAttributes::Underlined, isUnderlined);
}

void TextAttribute::SetDoublyUnderlined(bool isDoublyUnderlined) noexcept
{
    WI_UpdateFlag(_extendedAttrs, ExtendedAttributes::DoublyUnderlined, isDoublyUnderlined);
}

void TextAttribute::SetOverlined(bool isOverlined) noexcept
{
    WI_UpdateFlag(_wAttrLegacy, COMMON_LVB_GRID_HORIZONTAL, isOverlined);
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

// Method description:
// - Resets only the meta and extended attributes
void TextAttribute::SetDefaultMetaAttrs() noexcept
{
    _extendedAttrs = ExtendedAttributes::Normal;
    _wAttrLegacy = 0;
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
    SetDefaultMetaAttrs();
    _hyperlinkId = 0;
}
