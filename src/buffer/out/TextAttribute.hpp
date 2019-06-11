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
        _isBold{ false }
    {
    }

    constexpr TextAttribute(const WORD wLegacyAttr) noexcept :
        _wAttrLegacy{ static_cast<WORD>(wLegacyAttr & META_ATTRS) },
        _foreground{ static_cast<BYTE>(wLegacyAttr & FG_ATTRS) },
        _background{ static_cast<BYTE>((wLegacyAttr & BG_ATTRS) >> 4) },
        _isBold{ false }
    {
        // If we're given lead/trailing byte information with the legacy color, strip it.
        WI_ClearAllFlags(_wAttrLegacy, COMMON_LVB_SBCSDBCS);
    }

    constexpr TextAttribute(const COLORREF rgbForeground,
                            const COLORREF rgbBackground) noexcept :
        _wAttrLegacy{ 0 },
        _foreground{ rgbForeground },
        _background{ rgbBackground },
        _isBold{ false }
    {
    }

    constexpr WORD GetLegacyAttributes() const noexcept
    {
        BYTE fg = (_foreground.GetIndex() & FG_ATTRS);
        BYTE bg = (_background.GetIndex() << 4) & BG_ATTRS;
        WORD meta = (_wAttrLegacy & META_ATTRS);
        return (fg | bg | meta) | (_isBold ? FOREGROUND_INTENSITY : 0);
    }

    // Method Description:
    // - Returns a WORD with legacy-style attributes for this textattribute.
    //      If either the foreground or background of this textattribute is not
    //      a legacy attribute, then instead use the provided default index as
    //      the value for that component.
    // Arguments:
    // - defaultFgIndex: the BYTE to use as the index for the foreground, should
    //      the foreground not be a legacy style attribute.
    // - defaultBgIndex: the BYTE to use as the index for the backgound, should
    //      the background not be a legacy style attribute.
    // Return Value:
    // - a WORD with legacy-style attributes for this textattribute.
    constexpr WORD GetLegacyAttributes(const BYTE defaultFgIndex,
                                       const BYTE defaultBgIndex) const noexcept
    {
        BYTE fgIndex = _foreground.IsLegacy() ? _foreground.GetIndex() : defaultFgIndex;
        BYTE bgIndex = _background.IsLegacy() ? _background.GetIndex() : defaultBgIndex;
        BYTE fg = (fgIndex & FG_ATTRS);
        BYTE bg = (bgIndex << 4) & BG_ATTRS;
        WORD meta = (_wAttrLegacy & META_ATTRS);
        return (fg | bg | meta) | (_isBold ? FOREGROUND_INTENSITY : 0);
    }

    COLORREF CalculateRgbForeground(std::basic_string_view<COLORREF> colorTable,
                                    COLORREF defaultFgColor,
                                    COLORREF defaultBgColor) const;
    COLORREF CalculateRgbBackground(std::basic_string_view<COLORREF> colorTable,
                                    COLORREF defaultFgColor,
                                    COLORREF defaultBgColor) const;

    bool IsLeadingByte() const noexcept;
    bool IsTrailingByte() const noexcept;
    bool IsTopHorizontalDisplayed() const noexcept;
    bool IsBottomHorizontalDisplayed() const noexcept;
    bool IsLeftVerticalDisplayed() const noexcept;
    bool IsRightVerticalDisplayed() const noexcept;

    void SetLeftVerticalDisplayed(const bool isDisplayed) noexcept;
    void SetRightVerticalDisplayed(const bool isDisplayed) noexcept;

    void SetFromLegacy(const WORD wLegacy) noexcept;

    void SetLegacyAttributes(const WORD attrs,
                             const bool setForeground,
                             const bool setBackground,
                             const bool setMeta);

    void SetIndexedAttributes(const std::optional<const BYTE> foreground,
                              const std::optional<const BYTE> background) noexcept;

    void SetMetaAttributes(const WORD wMeta) noexcept;
    WORD GetMetaAttributes() const noexcept;

    void Embolden() noexcept;
    void Debolden() noexcept;

    void Invert() noexcept;

    friend constexpr bool operator==(const TextAttribute& a, const TextAttribute& b) noexcept;
    friend constexpr bool operator!=(const TextAttribute& a, const TextAttribute& b) noexcept;
    friend constexpr bool operator==(const TextAttribute& attr, const WORD& legacyAttr) noexcept;
    friend constexpr bool operator!=(const TextAttribute& attr, const WORD& legacyAttr) noexcept;
    friend constexpr bool operator==(const WORD& legacyAttr, const TextAttribute& attr) noexcept;
    friend constexpr bool operator!=(const WORD& legacyAttr, const TextAttribute& attr) noexcept;

    bool IsLegacy() const noexcept;
    bool IsBold() const noexcept;

    void SetForeground(const COLORREF rgbForeground);
    void SetBackground(const COLORREF rgbBackground);
    void SetColor(const COLORREF rgbColor, const bool fIsForeground);

    void SetDefaultForeground() noexcept;
    void SetDefaultBackground() noexcept;

    bool ForegroundIsDefault() const noexcept;
    bool BackgroundIsDefault() const noexcept;

    constexpr bool IsRgb() const noexcept
    {
        return _foreground.IsRgb() || _background.IsRgb();
    }

private:
    COLORREF _GetRgbForeground(std::basic_string_view<COLORREF> colorTable,
                               COLORREF defaultColor) const;
    COLORREF _GetRgbBackground(std::basic_string_view<COLORREF> colorTable,
                               COLORREF defaultColor) const;
    bool _IsReverseVideo() const noexcept;
    void _SetBoldness(const bool isBold) noexcept;

    WORD _wAttrLegacy;
    TextColor _foreground;
    TextColor _background;
    bool _isBold;

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
           a._isBold == b._isBold;
}

constexpr bool operator!=(const TextAttribute& a, const TextAttribute& b) noexcept
{
    return !(a == b);
}

constexpr bool operator==(const TextAttribute& attr, const WORD& legacyAttr) noexcept
{
    return attr.GetLegacyAttributes() == legacyAttr;
}

constexpr bool operator!=(const TextAttribute& attr, const WORD& legacyAttr) noexcept
{
    return !(attr == legacyAttr);
}

constexpr bool operator==(const WORD& legacyAttr, const TextAttribute& attr) noexcept
{
    return attr == legacyAttr;
}

constexpr bool operator!=(const WORD& legacyAttr, const TextAttribute& attr) noexcept
{
    return !(attr == legacyAttr);
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
                    L"{FG:%s,BG:%s,bold:%d,wLegacy:(0x%04x)}",
                    VerifyOutputTraits<TextColor>::ToString(attr._foreground).GetBuffer(),
                    VerifyOutputTraits<TextColor>::ToString(attr._background).GetBuffer(),
                    attr.IsBold(),
                    attr._wAttrLegacy);
            }
        };
    }
}
#endif
