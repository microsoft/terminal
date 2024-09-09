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

enum class UnderlineStyle
{
    NoUnderline = 0U,
    SinglyUnderlined = 1U,
    DoublyUnderlined = 2U,
    CurlyUnderlined = 3U,
    DottedUnderlined = 4U,
    DashedUnderlined = 5U,
    Max = DashedUnderlined
};

// We only need a few bits, but uint8_t apparently doesn't satisfy std::has_unique_object_representations_v
enum class MarkKind : uint16_t
{
    None = 0,
    Prompt = 1,
    Command = 2,
    Output = 3,
};

class TextAttribute final
{
public:
    constexpr TextAttribute() noexcept :
        _attrs{ CharacterAttributes::Normal },
        _foreground{},
        _background{},
        _hyperlinkId{ 0 },
        _underlineColor{},
        _markKind{ MarkKind::None }
    {
    }

    explicit constexpr TextAttribute(const WORD wLegacyAttr) noexcept :
        _attrs{ gsl::narrow_cast<WORD>(wLegacyAttr & USED_META_ATTRS) },
        _foreground{ gsl::at(s_legacyForegroundColorMap, wLegacyAttr & FG_ATTRS) },
        _background{ gsl::at(s_legacyBackgroundColorMap, (wLegacyAttr & BG_ATTRS) >> 4) },
        _hyperlinkId{ 0 },
        _underlineColor{},
        _markKind{ MarkKind::None }
    {
    }

    constexpr TextAttribute(const COLORREF rgbForeground,
                            const COLORREF rgbBackground,
                            const COLORREF rgbUnderline = INVALID_COLOR) noexcept :
        _attrs{ CharacterAttributes::Normal },
        _foreground{ rgbForeground },
        _background{ rgbBackground },
        _hyperlinkId{ 0 },
        _underlineColor{ rgbUnderline },
        _markKind{ MarkKind::None }
    {
    }

    constexpr TextAttribute(const CharacterAttributes attrs, const TextColor foreground, const TextColor background, const uint16_t hyperlinkId, const TextColor underlineColor) noexcept :
        _attrs{ attrs },
        _foreground{ foreground },
        _background{ background },
        _hyperlinkId{ hyperlinkId },
        _underlineColor{ underlineColor },
        _markKind{ MarkKind::None }
    {
    }

    static void SetLegacyDefaultAttributes(const WORD defaultAttributes) noexcept;
    WORD GetLegacyAttributes() const noexcept;

    bool IsTopHorizontalDisplayed() const noexcept;
    bool IsBottomHorizontalDisplayed() const noexcept;
    bool IsLeftVerticalDisplayed() const noexcept;
    bool IsRightVerticalDisplayed() const noexcept;

    void SetLeftVerticalDisplayed(const bool isDisplayed) noexcept;
    void SetRightVerticalDisplayed(const bool isDisplayed) noexcept;

    void Invert() noexcept;

    inline bool operator==(const TextAttribute& other) const noexcept
    {
        return memcmp(this, &other, sizeof(TextAttribute)) == 0;
    }

    inline bool operator!=(const TextAttribute& other) const noexcept
    {
        return memcmp(this, &other, sizeof(TextAttribute)) != 0;
    }

    bool IsLegacy() const noexcept;
    bool IsIntense() const noexcept;
    bool IsFaint() const noexcept;
    bool IsItalic() const noexcept;
    bool IsBlinking() const noexcept;
    bool IsInvisible() const noexcept;
    bool IsCrossedOut() const noexcept;
    bool IsUnderlined() const noexcept;
    bool IsOverlined() const noexcept;
    bool IsReverseVideo() const noexcept;
    bool IsProtected() const noexcept;

    void SetIntense(bool isIntense) noexcept;
    void SetFaint(bool isFaint) noexcept;
    void SetItalic(bool isItalic) noexcept;
    void SetBlinking(bool isBlinking) noexcept;
    void SetInvisible(bool isInvisible) noexcept;
    void SetCrossedOut(bool isCrossedOut) noexcept;
    void SetUnderlineStyle(const UnderlineStyle underlineStyle) noexcept;
    void SetOverlined(bool isOverlined) noexcept;
    void SetReverseVideo(bool isReversed) noexcept;
    void SetProtected(bool isProtected) noexcept;

    constexpr void SetCharacterAttributes(const CharacterAttributes attrs) noexcept
    {
        _attrs = attrs;
    }
    constexpr CharacterAttributes GetCharacterAttributes() const noexcept
    {
        return _attrs;
    }

    constexpr void SetMarkAttributes(const MarkKind attrs) noexcept
    {
        _markKind = attrs;
    }
    constexpr MarkKind GetMarkAttributes() const noexcept
    {
        return _markKind;
    }

    bool IsHyperlink() const noexcept;

    TextColor GetForeground() const noexcept;
    TextColor GetBackground() const noexcept;
    uint16_t GetHyperlinkId() const noexcept;
    TextColor GetUnderlineColor() const noexcept;
    UnderlineStyle GetUnderlineStyle() const noexcept;
    void SetForeground(const TextColor foreground) noexcept;
    void SetBackground(const TextColor background) noexcept;
    void SetUnderlineColor(const TextColor color) noexcept;
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
    void SetDefaultUnderlineColor() noexcept;
    void SetDefaultRenditionAttributes() noexcept;

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
               // styled underline and crossed out have a visual representation
               !IsUnderlined() && WI_IsFlagClear(_attrs, CharacterAttributes::CrossedOut) &&
               // hyperlinks have a visual representation
               !IsHyperlink() &&
               // all other attributes do not have a visual representation
               _attrs == other._attrs &&
               ((checkForeground && _foreground == other._foreground) ||
                (!checkForeground && _background == other._background)) &&
               IsHyperlink() == other.IsHyperlink();
    }

    constexpr bool IsAnyGridLineEnabled() const noexcept
    {
        return WI_IsAnyFlagSet(_attrs, CharacterAttributes::TopGridline | CharacterAttributes::LeftGridline | CharacterAttributes::RightGridline | CharacterAttributes::BottomGridline);
    }
    constexpr bool HasAnyVisualAttributes() const noexcept
    {
        return GetCharacterAttributes() != CharacterAttributes::Normal || GetHyperlinkId() != 0;
    }

private:
    static std::array<TextColor, 16> s_legacyForegroundColorMap;
    static std::array<TextColor, 16> s_legacyBackgroundColorMap;

    CharacterAttributes _attrs; // sizeof: 2, alignof: 2
    uint16_t _hyperlinkId; // sizeof: 2, alignof: 2
    TextColor _foreground; // sizeof: 4, alignof: 1
    TextColor _background; // sizeof: 4, alignof: 1
    TextColor _underlineColor; // sizeof: 4, alignof: 1
    MarkKind _markKind; // sizeof: 2, alignof: 1

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

#ifdef UNIT_TESTING

#define LOG_ATTR(attr) (Log::Comment(NoThrowString().Format( \
    L## #attr L"=%s", VerifyOutputTraits<TextAttribute>::ToString(attr).GetBuffer())))

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
                    L"{FG:%s,BG:%s,intense:%d,attrs:(0x%02x)}",
                    VerifyOutputTraits<TextColor>::ToString(attr._foreground).GetBuffer(),
                    VerifyOutputTraits<TextColor>::ToString(attr._background).GetBuffer(),
                    attr.IsIntense(),
                    static_cast<DWORD>(attr._attrs));
            }
        };
    }
}
#endif
