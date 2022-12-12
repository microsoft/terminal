// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "TextAttribute.hpp"
#include "../../inc/conattrs.hpp"

// Keeping TextColor compact helps us keeping TextAttribute compact,
// which in turn ensures that our buffer memory usage is low.
static_assert(sizeof(TextAttribute) == 12);
static_assert(alignof(TextAttribute) == 2);
// Ensure that we can memcpy() and memmove() the struct for performance.
static_assert(std::is_trivially_copyable_v<TextAttribute>);
// Assert that the use of memcmp() for comparisons is safe.
static_assert(std::has_unique_object_representations_v<TextAttribute>);

namespace
{
    constexpr std::array<TextColor, 16> s_initLegacyColorMap(const BYTE defaultIndex)
    {
        std::array<TextColor, 16> legacyColorMap;
        for (auto i = 0u; i < legacyColorMap.size(); i++)
        {
            const auto legacyIndex = TextColor::TransposeLegacyIndex(i);
            gsl::at(legacyColorMap, i) = i == defaultIndex ? TextColor{} : TextColor{ legacyIndex, true };
        }
        return legacyColorMap;
    }

    BYTE s_legacyDefaultForeground = 7;
    BYTE s_legacyDefaultBackground = 0;
    BYTE s_ansiDefaultForeground = 7;
    BYTE s_ansiDefaultBackground = 0;
}

// These maps allow for an efficient conversion from a legacy attribute index
// to a TextColor with the corresponding ANSI index, also taking into account
// the legacy index values that need to be converted to a default TextColor.
std::array<TextColor, 16> TextAttribute::s_legacyForegroundColorMap = s_initLegacyColorMap(7);
std::array<TextColor, 16> TextAttribute::s_legacyBackgroundColorMap = s_initLegacyColorMap(0);

// Routine Description:
// - Sets the legacy attributes which map to and from the default colors.
// Parameters:
// - defaultAttributes: the attribute values to be used for default colors.
// Return value:
// - None
void TextAttribute::SetLegacyDefaultAttributes(const WORD defaultAttributes) noexcept
{
    // First we reset the current default color map entries to what they should
    // be for a regular translation from a legacy index to an ANSI TextColor.
    gsl::at(s_legacyForegroundColorMap, s_legacyDefaultForeground) = TextColor{ s_ansiDefaultForeground, true };
    gsl::at(s_legacyBackgroundColorMap, s_legacyDefaultBackground) = TextColor{ s_ansiDefaultBackground, true };

    // Then we save the new default attribute values and their corresponding
    // ANSI translations. We use the latter values to more efficiently handle
    // the "VT Quirk" conversion below.
    s_legacyDefaultForeground = defaultAttributes & FG_ATTRS;
    s_legacyDefaultBackground = (defaultAttributes & BG_ATTRS) >> 4;
    s_ansiDefaultForeground = TextColor::TransposeLegacyIndex(s_legacyDefaultForeground);
    s_ansiDefaultBackground = TextColor::TransposeLegacyIndex(s_legacyDefaultBackground);

    // Finally we set the new default color map entries.
    gsl::at(s_legacyForegroundColorMap, s_legacyDefaultForeground) = TextColor{};
    gsl::at(s_legacyBackgroundColorMap, s_legacyDefaultBackground) = TextColor{};
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
        attribute.IsIntense() == WI_IsFlagSet(s_ansiDefaultForeground, FOREGROUND_INTENSITY) &&
        fg.GetIndex() == (s_ansiDefaultForeground & ~FOREGROUND_INTENSITY))
    {
        // We don't want to turn 1;37m into 39m (or even 1;39m), as this was meant to mimic a legacy color.
        copy.SetDefaultForeground();
    }
    if (bg.IsIndex16() && bg.GetIndex() == s_ansiDefaultBackground)
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
    const auto fgIndex = _foreground.GetLegacyIndex(s_legacyDefaultForeground);
    const auto bgIndex = _background.GetLegacyIndex(s_legacyDefaultBackground);
    const WORD metaAttrs = static_cast<WORD>(_attrs) & USED_META_ATTRS;
    const auto brighten = IsIntense() && _foreground.CanBeBrightened();
    return fgIndex | (bgIndex << 4) | metaAttrs | (brighten ? FOREGROUND_INTENSITY : 0);
}

bool TextAttribute::IsLegacy() const noexcept
{
    return _foreground.IsLegacy() && _background.IsLegacy();
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

bool TextAttribute::IsTopHorizontalDisplayed() const noexcept
{
    return WI_IsFlagSet(_attrs, CharacterAttributes::TopGridline);
}

bool TextAttribute::IsBottomHorizontalDisplayed() const noexcept
{
    return WI_IsFlagSet(_attrs, CharacterAttributes::BottomGridline);
}

bool TextAttribute::IsLeftVerticalDisplayed() const noexcept
{
    return WI_IsFlagSet(_attrs, CharacterAttributes::LeftGridline);
}

bool TextAttribute::IsRightVerticalDisplayed() const noexcept
{
    return WI_IsFlagSet(_attrs, CharacterAttributes::RightGridline);
}

void TextAttribute::SetLeftVerticalDisplayed(const bool isDisplayed) noexcept
{
    WI_UpdateFlag(_attrs, CharacterAttributes::LeftGridline, isDisplayed);
}

void TextAttribute::SetRightVerticalDisplayed(const bool isDisplayed) noexcept
{
    WI_UpdateFlag(_attrs, CharacterAttributes::RightGridline, isDisplayed);
}

bool TextAttribute::IsIntense() const noexcept
{
    return WI_IsFlagSet(_attrs, CharacterAttributes::Intense);
}

bool TextAttribute::IsFaint() const noexcept
{
    return WI_IsFlagSet(_attrs, CharacterAttributes::Faint);
}

bool TextAttribute::IsItalic() const noexcept
{
    return WI_IsFlagSet(_attrs, CharacterAttributes::Italics);
}

bool TextAttribute::IsBlinking() const noexcept
{
    return WI_IsFlagSet(_attrs, CharacterAttributes::Blinking);
}

bool TextAttribute::IsInvisible() const noexcept
{
    return WI_IsFlagSet(_attrs, CharacterAttributes::Invisible);
}

bool TextAttribute::IsCrossedOut() const noexcept
{
    return WI_IsFlagSet(_attrs, CharacterAttributes::CrossedOut);
}

bool TextAttribute::IsUnderlined() const noexcept
{
    return WI_IsFlagSet(_attrs, CharacterAttributes::Underlined);
}

bool TextAttribute::IsDoublyUnderlined() const noexcept
{
    return WI_IsFlagSet(_attrs, CharacterAttributes::DoublyUnderlined);
}

bool TextAttribute::IsOverlined() const noexcept
{
    return WI_IsFlagSet(_attrs, CharacterAttributes::TopGridline);
}

bool TextAttribute::IsReverseVideo() const noexcept
{
    return WI_IsFlagSet(_attrs, CharacterAttributes::ReverseVideo);
}

bool TextAttribute::IsProtected() const noexcept
{
    return WI_IsFlagSet(_attrs, CharacterAttributes::Protected);
}

void TextAttribute::SetIntense(bool isIntense) noexcept
{
    WI_UpdateFlag(_attrs, CharacterAttributes::Intense, isIntense);
}

void TextAttribute::SetFaint(bool isFaint) noexcept
{
    WI_UpdateFlag(_attrs, CharacterAttributes::Faint, isFaint);
}

void TextAttribute::SetItalic(bool isItalic) noexcept
{
    WI_UpdateFlag(_attrs, CharacterAttributes::Italics, isItalic);
}

void TextAttribute::SetBlinking(bool isBlinking) noexcept
{
    WI_UpdateFlag(_attrs, CharacterAttributes::Blinking, isBlinking);
}

void TextAttribute::SetInvisible(bool isInvisible) noexcept
{
    WI_UpdateFlag(_attrs, CharacterAttributes::Invisible, isInvisible);
}

void TextAttribute::SetCrossedOut(bool isCrossedOut) noexcept
{
    WI_UpdateFlag(_attrs, CharacterAttributes::CrossedOut, isCrossedOut);
}

void TextAttribute::SetUnderlined(bool isUnderlined) noexcept
{
    WI_UpdateFlag(_attrs, CharacterAttributes::Underlined, isUnderlined);
}

void TextAttribute::SetDoublyUnderlined(bool isDoublyUnderlined) noexcept
{
    WI_UpdateFlag(_attrs, CharacterAttributes::DoublyUnderlined, isDoublyUnderlined);
}

void TextAttribute::SetOverlined(bool isOverlined) noexcept
{
    WI_UpdateFlag(_attrs, CharacterAttributes::TopGridline, isOverlined);
}

void TextAttribute::SetReverseVideo(bool isReversed) noexcept
{
    WI_UpdateFlag(_attrs, CharacterAttributes::ReverseVideo, isReversed);
}

void TextAttribute::SetProtected(bool isProtected) noexcept
{
    WI_UpdateFlag(_attrs, CharacterAttributes::Protected, isProtected);
}

// Routine Description:
// - swaps foreground and background color
void TextAttribute::Invert() noexcept
{
    WI_ToggleFlag(_attrs, CharacterAttributes::ReverseVideo);
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
// - Resets only the rendition character attributes, which includes everything
//     except the Protected attribute.
void TextAttribute::SetDefaultRenditionAttributes() noexcept
{
    _attrs &= ~CharacterAttributes::Rendition;
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
// - Resets the character attributes, which is what the VT standard
//      requires for most erasing and filling operations. In modern
//      applications it is also expected that hyperlinks are erased.
void TextAttribute::SetStandardErase() noexcept
{
    _attrs = CharacterAttributes::Normal;
    _hyperlinkId = 0;
}
