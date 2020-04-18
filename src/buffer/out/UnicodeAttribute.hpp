/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- DbcsAttribute.hpp

Abstract:
- helper class for storing unicode related information about a cell in the output buffer.

Author(s):
- Chester Liu (jialli) 17-April-2020

Revision History:
--*/

#pragma once

#include "../../types/inc/CodepointWidthDetector.hpp"

class UnicodeAttribute final
{
public:
    enum class Category : BYTE
    {
        Letter = 0x01,
        LetterOther = 0x04,
        MarkUnspacing = 0x07,
        SymbolModifier = 0x08,
        OtherFormat = 0x09
    };
    
    UnicodeAttribute() noexcept :
        _category{ Category::Letter },
        _zeroWidth{ false },
        _expandWidth{ false },
        _isJoiner { false },
        _backwordAdhesive{ false },
        _forwardAdhesive{ false }
    {
    }

    constexpr bool IsZeroWidth() const noexcept
    {
        return _category == Category::MarkUnspacing || _zeroWidth;
    }

    constexpr bool IsExpandWidth() const noexcept
    {
        return _expandWidth;
    }

    constexpr bool IsJoiner() const noexcept
    {
        return _isJoiner;
    }

    constexpr bool IsBackwordAdhesive() const noexcept
    {
        return _backwordAdhesive;
    }

    constexpr bool IsForwardAdhesive() const noexcept
    {
        return _forwardAdhesive;
    }

    void SetGlyph(wchar_t wch) noexcept
    {
        SetGlyph({ &wch, 1 });
    }

    void SetGlyph(std::wstring_view glyph) noexcept
    {
        const auto codepoint = CodepointWidthDetector::ExtractCodepoint(glyph);
        if (codepoint >= 0x300 && codepoint <= 0x36f)
        {
            // COMBINING GRAVE ACCENT..COMBINING LATIN SMALL LETTER X
            _category = Category::MarkUnspacing;
            _backwordAdhesive = true;
        }
        else if (codepoint >= 0x1100 && codepoint <= 0x115f)
        {
            // HANGUL CHOSEONG KIYEOK..HANGUL CHOSEONG FILLER
            _category = Category::LetterOther;
        }
        else if (codepoint >= 0x1160 && codepoint <= 0x11ff)
        {
            // HANGUL JUNGSEONG FILLER..HANGUL JONGSEONG SSANGNIEUN
            _category = Category::LetterOther;
        }
        else if (codepoint == 0x200b)
        {
            // ZERO WIDTH SPACE
            _category = Category::OtherFormat;
            _zeroWidth = true;
        }
        else if (codepoint == 0x200c)
        {
            // ZERO WIDTH NON-JOINER
            _category = Category::OtherFormat;
            _zeroWidth = true;
        }
        else if (codepoint == 0x200d)
        {
            // ZERO WIDTH JOINER
            _category = Category::OtherFormat;
            _zeroWidth = true;
            _isJoiner = true;
            _backwordAdhesive = true;
            _forwardAdhesive = true;
        }
        else if (codepoint == 0x2060)
        {
            // WORD JOINER
            _category = Category::OtherFormat;
            _zeroWidth = true;
            _isJoiner = true;
            _backwordAdhesive = true;
            _forwardAdhesive = true;
        }
        else if (codepoint >= 0x3099 && codepoint <= 0x309a)
        {
            // COMBINING KATAKANA-HIRAGANA VOICED SOUND MARK...
            // COMBINING KATAKANA-HIRAGANA SEMI-VOICED SOUND MARK
            _category = Category::MarkUnspacing;
            _backwordAdhesive = true;
        }
        else if (codepoint >= 0xfe00 && codepoint <= 0xfe0f)
        {
            // VARIATION SELECTOR-1..VARIATION SELECTOR-16
            _category = Category::MarkUnspacing;
            _backwordAdhesive = true;

            // VARIATION SELECTOR-16
            // EMOJI
            if (codepoint == 0xfe0f)
            {
                _expandWidth = true;
            }
        }
        else if (codepoint == 0xfeff)
        {
            // ZERO WIDTH NO-BREAK SPACE
            _category = Category::OtherFormat;
            _zeroWidth = true;
        }
        else if (codepoint >= 0x1f3fb && codepoint <= 0x1f3ff)
        {
            // EMOJI MODIFIER FITZPATRICK TYPE-1-2..EMOJI MODIFIER FITZPATRICK TYPE-6
            _category = Category::SymbolModifier;
            _zeroWidth = true;
            _backwordAdhesive = true;
        }
        if (codepoint >= 0xe0100 && codepoint <= 0xe01ef)
        {
            // VARIATION SELECTOR-17..VARIATION SELECTOR-256
            _category = Category::MarkUnspacing;
            _backwordAdhesive = true;
        }
    }

private:
    Category _category;
    bool _zeroWidth;
    bool _expandWidth;
    bool _isJoiner;
    bool _backwordAdhesive;
    bool _forwardAdhesive;
};
