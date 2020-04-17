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
        OtherFormat = 0x08
    };
    
    UnicodeAttribute() noexcept :
        _category{ Category::Letter },
        _zeroWidth{ false }
    {
    }

    constexpr bool IsZeroWidth() const noexcept
    {
        return _zeroWidth || _category == Category::MarkUnspacing;
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
        }
        else if (codepoint == 0x200d)
        {
            // ZERO WIDTH JOINER
            _category = Category::OtherFormat;
            _zeroWidth = true;
        }
        else if (codepoint >= 0x3099 && codepoint <= 0x309a)
        {
            // COMBINING KATAKANA-HIRAGANA VOICED SOUND MARK...
            // COMBINING KATAKANA-HIRAGANA SEMI-VOICED SOUND MARK
            _category = Category::MarkUnspacing;
        }
        else if (codepoint >= 0xfe00 && codepoint <= 0xfe0f)
        {
            // VARIATION SELECTOR-1..VARIATION SELECTOR-16
            _category = Category::MarkUnspacing;
        }
        else if (codepoint == 0xfeff)
        {
            // ZERO WIDTH NO-BREAK SPACE
            _category = Category::OtherFormat;
            _zeroWidth = true;
        }
        if (codepoint >= 0xe0100 && codepoint <= 0xe01ef)
        {
            // VARIATION SELECTOR-17..VARIATION SELECTOR-256
            _category = Category::MarkUnspacing;
        }
    }

private:
    Category _category;
    bool _zeroWidth;
};
