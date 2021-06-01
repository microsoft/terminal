/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- TextAttribute.hpp

Abstract:
- contains data structure for run-length-encoding of text attribute data

Author(s):
- Michael Niksa (miniksa) 10-Apr-2014
- Paul Campbell (paulcam) 10-Apr-2014

Revision History:
- From components of output.h/.c
  by Therese Stowell (ThereseS) 1990-1991
- Pulled into its own file from textBuffer.hpp/cpp (AustDi, 2017)
- Pulled each of the fg/bg colors into their own abstraction (migrie, Nov 2018)
--*/

#pragma once
#include "TextColor.h"
#include "../../inc/conattrs.hpp"

#ifdef UNIT_TESTING
#include "WexTestClass.h"
#endif

class TextAttribute final
{
public:
    constexpr TextAttribute() noexcept :
        _wAttrLegacy{ 0 },
        _foreground{},
        _background{},
        _extendedAttrs{ ExtendedAttributes::Normal },
        _hyperlinkId{ 0 }
    {
    }

    explicit constexpr TextAttribute(const WORD wLegacyAttr) noexcept :
        _wAttrLegacy{ gsl::narrow_cast<WORD>(wLegacyAttr & META_ATTRS) },
        _foreground{ s_LegacyIndexOrDefault(wLegacyAttr & FG_ATTRS, s_legacyDefaultForeground) },
        _background{ s_LegacyIndexOrDefault((wLegacyAttr & BG_ATTRS) >> 4, s_legacyDefaultBackground) },
        _extendedAttrs{ ExtendedAttributes::Normal },
        _hyperlinkId{ 0 }
    {
        // If we're given lead/trailing byte information with the legacy color, strip it.
        WI_ClearAllFlags(_wAttrLegacy, COMMON_LVB_SBCSDBCS);
    }

    constexpr TextAttribute(const COLORREF rgbForeground,
                            const COLORREF rgbBackground) noexcept :
        _wAttrLegacy{ 0 },
        _foreground{ rgbForeground },
        _background{ rgbBackground },
        _extendedAttrs{ ExtendedAttributes::Normal },
        _hyperlinkId{ 0 }
    {
    }

    static void SetLegacyDefaultAttributes(const WORD defaultAttributes) noexcept;
    static TextAttribute StripErroneousVT16VersionsOfLegacyDefaults(const TextAttribute& attribute) noexcept;
    WORD GetLegacyAttributes() const noexcept;

    std::pair<COLORREF, COLORREF> CalculateRgbColors(const gsl::span<const COLORREF> colorTable,
                                                     const COLORREF defaultFgColor,
                                                     const COLORREF defaultBgColor,
                                                     const bool reverseScreenMode = false,
                                                     const bool blinkingIsFaint = false) const noexcept;

    bool IsLeadingByte() const noexcept;
    bool IsTrailingByte() const noexcept;
    bool IsTopHorizontalDisplayed() const noexcept;
    bool IsBottomHorizontalDisplayed() const noexcept;
    bool IsLeftVerticalDisplayed() const noexcept;
    bool IsRightVerticalDisplayed() const noexcept;

    void SetLeftVerticalDisplayed(const bool isDisplayed) noexcept;
    void SetRightVerticalDisplayed(const bool isDisplayed) noexcept;

    void Invert() noexcept;

    friend constexpr bool operator==(const TextAttribute& a, const TextAttribute& b) noexcept;
    friend constexpr bool operator!=(const TextAttribute& a, const TextAttribute& b) noexcept;
    friend constexpr bool operator==(const TextAttribute& attr, const WORD& legacyAttr) noexcept;
    friend constexpr bool operator!=(const TextAttribute& attr, const WORD& legacyAttr) noexcept;
    friend constexpr bool operator==(const WORD& legacyAttr, const TextAttribute& attr) noexcept;
    friend constexpr bool operator!=(const WORD& legacyAttr, const TextAttribute& attr) noexcept;

    bool IsLegacy() const noexcept;
    bool IsBold() const noexcept;
    bool IsFaint() const noexcept;
    bool IsItalic() const noexcept;
    bool IsBlinking() const noexcept;
    bool IsInvisible() const noexcept;
    bool IsCrossedOut() const noexcept;
    bool IsUnderlined() const noexcept;
    bool IsDoublyUnderlined() const noexcept;
    bool IsOverlined() const noexcept;
    bool IsReverseVideo() const noexcept;

    void SetBold(bool isBold) noexcept;
    void SetFaint(bool isFaint) noexcept;
    void SetItalic(bool isItalic) noexcept;
    void SetBlinking(bool isBlinking) noexcept;
    void SetInvisible(bool isInvisible) noexcept;
    void SetCrossedOut(bool isCrossedOut) noexcept;
    void SetUnderlined(bool isUnderlined) noexcept;
    void SetDoublyUnderlined(bool isDoublyUnderlined) noexcept;
    void SetOverlined(bool isOverlined) noexcept;
    void SetReverseVideo(bool isReversed) noexcept;

    ExtendedAttributes GetExtendedAttributes() const noexcept;

    bool IsHyperlink() const noexcept;

    TextColor GetForeground() const noexcept;
    TextColor GetBackground() const noexcept;
    uint16_t GetHyperlinkId() const noexcept;
    void SetForeground(const TextColor foreground) noexcept;
    void SetBackground(const TextColor background) noexcept;
    void SetForeground(const COLORREF rgbForeground) noexcept;
    void SetBackground(const COLORREF rgbBackground) noexcept;
    void SetIndexedForeground(const BYTE fgIndex) noexcept;
    void SetIndexedBackground(const BYTE bgIndex) noexcept;
    void SetIndexedForeground256(const BYTE fgIndex) noexcept;
    void SetIndexedBackground256(const BYTE bgIndex) noexcept;
    void SetColor(const COLORREF rgbColor, const bool fIsForeground) noexcept;
    void SetHyperlinkId(uint16_t id) noexcept;

    void SetDefaultForeground() noexcept;
    void SetDefaultBackground() noexcept;
    void SetDefaultMetaAttrs() noexcept;

    bool BackgroundIsDefault() const noexcept;

    void SetStandardErase() noexcept;

    // This returns whether this attribute, if printed directly next to another attribute, for the space
    // character, would look identical to the other one.
    bool HasIdenticalVisualRepresentationForBlankSpace(const TextAttribute& other, const bool inverted = false) const noexcept
    {
        // sneaky-sneaky: I'm using xor here
        // inverted is whether there's a global invert; Reverse is a local one.
        // global ^ local == true : the background attribute is actually the visible foreground, so we care about the foregrounds being identical
        // global ^ local == false: the foreground attribute is the visible foreground, so we care about the backgrounds being identical
        const auto checkForeground = (inverted != IsReverseVideo());
        return !IsAnyGridLineEnabled() && // grid lines have a visual representation
               // crossed out, doubly and singly underlined have a visual representation
               WI_AreAllFlagsClear(_extendedAttrs, ExtendedAttributes::CrossedOut | ExtendedAttributes::DoublyUnderlined | ExtendedAttributes::Underlined) &&
               // hyperlinks have a visual representation
               !IsHyperlink() &&
               // all other attributes do not have a visual representation
               (_wAttrLegacy & META_ATTRS) == (other._wAttrLegacy & META_ATTRS) &&
               ((checkForeground && _foreground == other._foreground) ||
                (!checkForeground && _background == other._background)) &&
               _extendedAttrs == other._extendedAttrs &&
               IsHyperlink() == other.IsHyperlink();
    }

    constexpr bool IsAnyGridLineEnabled() const noexcept
    {
        return WI_IsAnyFlagSet(_wAttrLegacy, COMMON_LVB_GRID_HORIZONTAL | COMMON_LVB_GRID_LVERTICAL | COMMON_LVB_GRID_RVERTICAL | COMMON_LVB_UNDERSCORE);
    }

private:
    static constexpr TextColor s_LegacyIndexOrDefault(const BYTE requestedIndex, const BYTE defaultIndex)
    {
        return requestedIndex == defaultIndex ? TextColor{} : TextColor{ requestedIndex, true };
    }

    static BYTE s_legacyDefaultForeground;
    static BYTE s_legacyDefaultBackground;

    uint16_t _wAttrLegacy; // sizeof: 2, alignof: 2
    uint16_t _hyperlinkId; // sizeof: 2, alignof: 2
    TextColor _foreground; // sizeof: 4, alignof: 1
    TextColor _background; // sizeof: 4, alignof: 1
    ExtendedAttributes _extendedAttrs; // sizeof: 1, alignof: 1

#ifdef UNIT_TESTING
    friend class TextBufferTests;
    friend class TextAttributeTests;
    template<typename TextAttribute>
    friend class WEX::TestExecution::VerifyOutputTraits;
#endif
};

enum class TextAttributeBehavior
{
    Stored, // use contained text attribute
    Current, // use text attribute of cell being written to
    StoredOnly, // only use the contained text attribute and skip the insertion of anything else
};

constexpr bool operator==(const TextAttribute& a, const TextAttribute& b) noexcept
{
    return a._wAttrLegacy == b._wAttrLegacy &&
           a._foreground == b._foreground &&
           a._background == b._background &&
           a._extendedAttrs == b._extendedAttrs &&
           a._hyperlinkId == b._hyperlinkId;
}

constexpr bool operator!=(const TextAttribute& a, const TextAttribute& b) noexcept
{
    return !(a == b);
}

#ifdef UNIT_TESTING

#define LOG_ATTR(attr) (Log::Comment(NoThrowString().Format( \
    L#attr L"=%s", VerifyOutputTraits<TextAttribute>::ToString(attr).GetBuffer())))

namespace WEX
{
    namespace TestExecution
    {
        template<>
        class VerifyOutputTraits<TextAttribute>
        {
        public:
            static WEX::Common::NoThrowString ToString(const TextAttribute& attr)
            {
                return WEX::Common::NoThrowString().Format(
                    L"{FG:%s,BG:%s,bold:%d,wLegacy:(0x%04x),ext:(0x%02x)}",
                    VerifyOutputTraits<TextColor>::ToString(attr._foreground).GetBuffer(),
                    VerifyOutputTraits<TextColor>::ToString(attr._background).GetBuffer(),
                    attr.IsBold(),
                    attr._wAttrLegacy,
                    static_cast<DWORD>(attr._extendedAttrs));
            }
        };
    }
}
#endif
