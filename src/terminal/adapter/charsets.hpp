/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- charsets.hpp

Abstract:
- Defines translation tables for the various VT character sets used in the TerminalOutput class.
--*/

#pragma once

namespace Microsoft::Console::VirtualTerminal
{
    template<wchar_t BaseChar, size_t Size>
    class CharSet
    {
    public:
        constexpr CharSet(const std::initializer_list<std::pair<wchar_t, wchar_t>> replacements)
        {
            for (auto i = L'\0'; i < _translationTable.size(); i++)
                _translationTable.at(i) = BaseChar + i;
            for (auto replacement : replacements)
                _translationTable.at(replacement.first - BaseChar) = replacement.second;
        }
        constexpr operator const std::wstring_view() const
        {
            return { _translationTable.data(), _translationTable.size() };
        }
        constexpr bool operator==(const std::wstring_view rhs) const
        {
            return _translationTable.data() == rhs.data();
        }

    private:
        std::array<wchar_t, Size> _translationTable = {};
    };

    template<wchar_t BaseChar, size_t Size>
    constexpr bool operator==(const std::wstring_view lhs, const CharSet<BaseChar, Size>& rhs)
    {
        return rhs == lhs;
    }

    typedef CharSet<L'\x20', 95> AsciiBasedCharSet;
    typedef CharSet<L'\xa0', 95> Latin1BasedCharSet94;
    typedef CharSet<L'\xa0', 96> Latin1BasedCharSet96;

    static constexpr auto Ascii = AsciiBasedCharSet{};
    static constexpr auto Latin1 = Latin1BasedCharSet96{};

#pragma warning(push)
#pragma warning(disable : 26483) // Suppress spurious "value is outside the bounds" warnings

    // https://www.vt100.net/docs/vt220-rm/table2-3b.html
    static constexpr auto DecSupplemental = Latin1BasedCharSet94{
        { L'\xa4', L'\u2426' }, // Undefined
        { L'\xa6', L'\u2426' }, // Undefined
        { L'\xa8', L'\u00a4' }, // Currency Sign
        { L'\xac', L'\u2426' }, // Undefined
        { L'\xad', L'\u2426' }, // Undefined
        { L'\xae', L'\u2426' }, // Undefined
        { L'\xaf', L'\u2426' }, // Undefined
        { L'\xb4', L'\u2426' }, // Undefined
        { L'\xb8', L'\u2426' }, // Undefined
        { L'\xbe', L'\u2426' }, // Undefined
        { L'\xd0', L'\u2426' }, // Undefined
        { L'\xd7', L'\u0152' }, // Latin Capital Ligature Oe
        { L'\xdd', L'\u0178' }, // Latin Capital Letter Y With Diaeresis
        { L'\xde', L'\u2426' }, // Undefined
        { L'\xf0', L'\u2426' }, // Undefined
        { L'\xf7', L'\u0153' }, // Latin Small Ligature Oe
        { L'\xfd', L'\u00ff' }, // Latin Small Letter Y With Diaeresis
        { L'\xfe', L'\u2426' }, // Undefined
    };

    // https://www.vt100.net/docs/vt220-rm/table2-4.html
    static constexpr auto DecSpecialGraphics = AsciiBasedCharSet{
        { L'\x5f', L'\u0020' }, // Blank
        { L'\x60', L'\u2666' }, // Diamond (more commonly U+25C6, but U+2666 renders better for us)
        { L'\x61', L'\u2592' }, // Checkerboard
        { L'\x62', L'\u2409' }, // HT, SYMBOL FOR HORIZONTAL TABULATION
        { L'\x63', L'\u240c' }, // FF, SYMBOL FOR FORM FEED
        { L'\x64', L'\u240d' }, // CR, SYMBOL FOR CARRIAGE RETURN
        { L'\x65', L'\u240a' }, // LF, SYMBOL FOR LINE FEED
        { L'\x66', L'\u00b0' }, // Degree symbol
        { L'\x67', L'\u00b1' }, // Plus/minus
        { L'\x68', L'\u2424' }, // NL, SYMBOL FOR NEWLINE
        { L'\x69', L'\u240b' }, // VT, SYMBOL FOR VERTICAL TABULATION
        { L'\x6a', L'\u2518' }, // Lower-right corner
        { L'\x6b', L'\u2510' }, // Upper-right corner
        { L'\x6c', L'\u250c' }, // Upper-left corner
        { L'\x6d', L'\u2514' }, // Lower-left corner
        { L'\x6e', L'\u253c' }, // Crossing lines
        { L'\x6f', L'\u23ba' }, // Horizontal line - Scan 1
        { L'\x70', L'\u23bb' }, // Horizontal line - Scan 3
        { L'\x71', L'\u2500' }, // Horizontal line - Scan 5
        { L'\x72', L'\u23bc' }, // Horizontal line - Scan 7
        { L'\x73', L'\u23bd' }, // Horizontal line - Scan 9
        { L'\x74', L'\u251c' }, // Left "T"
        { L'\x75', L'\u2524' }, // Right "T"
        { L'\x76', L'\u2534' }, // Bottom "T"
        { L'\x77', L'\u252c' }, // Top "T"
        { L'\x78', L'\u2502' }, // | Vertical bar
        { L'\x79', L'\u2264' }, // Less than or equal to
        { L'\x7a', L'\u2265' }, // Greater than or equal to
        { L'\x7b', L'\u03c0' }, // Pi
        { L'\x7c', L'\u2260' }, // Not equal to
        { L'\x7d', L'\u00a3' }, // UK pound sign
        { L'\x7e', L'\u00b7' }, // Centered dot
    };

    // https://www.vt100.net/docs/vt220-rm/table2-5.html
    static constexpr auto BritishNrcs = AsciiBasedCharSet{
        { L'\x23', L'\u00a3' }, // Pound Sign
    };

    // https://www.vt100.net/docs/vt220-rm/table2-6.html
    static constexpr auto DutchNrcs = AsciiBasedCharSet{
        { L'\x23', L'\u00a3' }, // Pound Sign
        { L'\x40', L'\u00be' }, // Vulgar Fraction Three Quarters
        { L'\x5b', L'\u0133' }, // Latin Small Ligature Ij (sometimes approximated as y with diaeresis)
        { L'\x5c', L'\u00bd' }, // Vulgar Fraction One Half
        { L'\x5d', L'\u007c' }, // Vertical Line
        { L'\x7b', L'\u00a8' }, // Diaeresis
        { L'\x7c', L'\u0192' }, // Latin Small Letter F With Hook (sometimes approximated as f)
        { L'\x7d', L'\u00bc' }, // Vulgar Fraction One Quarter
        { L'\x7e', L'\u00b4' }, // Acute Accent
    };

    // https://www.vt100.net/docs/vt220-rm/table2-7.html
    static constexpr auto FinnishNrcs = AsciiBasedCharSet{
        { L'\x5b', L'\u00c4' }, // Latin Capital Letter A With Diaeresis
        { L'\x5c', L'\u00d6' }, // Latin Capital Letter O With Diaeresis
        { L'\x5d', L'\u00c5' }, // Latin Capital Letter A With Ring Above
        { L'\x5e', L'\u00dc' }, // Latin Capital Letter U With Diaeresis
        { L'\x60', L'\u00e9' }, // Latin Small Letter E With Acute
        { L'\x7b', L'\u00e4' }, // Latin Small Letter A With Diaeresis
        { L'\x7c', L'\u00f6' }, // Latin Small Letter O With Diaeresis
        { L'\x7d', L'\u00e5' }, // Latin Small Letter A With Ring Above
        { L'\x7e', L'\u00fc' }, // Latin Small Letter U With Diaeresis
    };

    // https://www.vt100.net/docs/vt220-rm/table2-8.html
    static constexpr auto FrenchNrcs = AsciiBasedCharSet{
        { L'\x23', L'\u00a3' }, // Pound Sign
        { L'\x40', L'\u00e0' }, // Latin Small Letter A With Grave
        { L'\x5b', L'\u00b0' }, // Degree Sign
        { L'\x5c', L'\u00e7' }, // Latin Small Letter C With Cedilla
        { L'\x5d', L'\u00a7' }, // Section Sign
        { L'\x7b', L'\u00e9' }, // Latin Small Letter E With Acute
        { L'\x7c', L'\u00f9' }, // Latin Small Letter U With Grave
        { L'\x7d', L'\u00e8' }, // Latin Small Letter E With Grave
        { L'\x7e', L'\u00a8' }, // Diaeresis
    };

    // https://www.itscj.ipsj.or.jp/iso-ir/069.pdf
    // Some terminal emulators consider all the French character sets as equivalent,
    // but the 6/6 designator is actually an updated ISO standard, which adds the
    // Micro Sign character, which is not included in the DEC version.
    static constexpr auto FrenchNrcsIso = AsciiBasedCharSet{
        { L'\x23', L'\u00a3' }, // Pound Sign
        { L'\x40', L'\u00e0' }, // Latin Small Letter A With Grave
        { L'\x5b', L'\u00b0' }, // Degree Sign
        { L'\x5c', L'\u00e7' }, // Latin Small Letter C With Cedilla
        { L'\x5d', L'\u00a7' }, // Section Sign
        { L'\x60', L'\u00b5' }, // Micro Sign
        { L'\x7b', L'\u00e9' }, // Latin Small Letter E With Acute
        { L'\x7c', L'\u00f9' }, // Latin Small Letter U With Grave
        { L'\x7d', L'\u00e8' }, // Latin Small Letter E With Grave
        { L'\x7e', L'\u00a8' }, // Diaeresis
    };

    // https://www.vt100.net/docs/vt220-rm/table2-9.html
    static constexpr auto FrenchCanadianNrcs = AsciiBasedCharSet{
        { L'\x40', L'\u00e0' }, // Latin Small Letter A With Grave
        { L'\x5b', L'\u00e2' }, // Latin Small Letter A With Circumflex
        { L'\x5c', L'\u00e7' }, // Latin Small Letter C With Cedilla
        { L'\x5d', L'\u00ea' }, // Latin Small Letter E With Circumflex
        { L'\x5e', L'\u00ee' }, // Latin Small Letter I With Circumflex
        { L'\x60', L'\u00f4' }, // Latin Small Letter O With Circumflex
        { L'\x7b', L'\u00e9' }, // Latin Small Letter E With Acute
        { L'\x7c', L'\u00f9' }, // Latin Small Letter U With Grave
        { L'\x7d', L'\u00e8' }, // Latin Small Letter E With Grave
        { L'\x7e', L'\u00fb' }, // Latin Small Letter U With Circumflex
    };

    // https://www.vt100.net/docs/vt220-rm/table2-10.html
    static constexpr auto GermanNrcs = AsciiBasedCharSet{
        { L'\x40', L'\u00a7' }, // Section Sign
        { L'\x5b', L'\u00c4' }, // Latin Capital Letter A With Diaeresis
        { L'\x5c', L'\u00d6' }, // Latin Capital Letter O With Diaeresis
        { L'\x5d', L'\u00dc' }, // Latin Capital Letter U With Diaeresis
        { L'\x7b', L'\u00e4' }, // Latin Small Letter A With Diaeresis
        { L'\x7c', L'\u00f6' }, // Latin Small Letter O With Diaeresis
        { L'\x7d', L'\u00fc' }, // Latin Small Letter U With Diaeresis (VT320 manual incorrectly has this as U+00A8)
        { L'\x7e', L'\u00df' }, // Latin Small Letter Sharp S
    };

    // https://www.vt100.net/docs/vt220-rm/table2-11.html
    static constexpr auto ItalianNrcs = AsciiBasedCharSet{
        { L'\x23', L'\u00a3' }, // Pound Sign
        { L'\x40', L'\u00a7' }, // Section Sign
        { L'\x5b', L'\u00b0' }, // Degree Sign
        { L'\x5c', L'\u00e7' }, // Latin Small Letter C With Cedilla
        { L'\x5d', L'\u00e9' }, // Latin Small Letter E With Acute
        { L'\x60', L'\u00f9' }, // Latin Small Letter U With Grave
        { L'\x7b', L'\u00e0' }, // Latin Small Letter A With Grave
        { L'\x7c', L'\u00f2' }, // Latin Small Letter O With Grave
        { L'\x7d', L'\u00e8' }, // Latin Small Letter E With Grave
        { L'\x7e', L'\u00ec' }, // Latin Small Letter I With Grave
    };

    // https://www.vt100.net/docs/vt220-rm/table2-12.html
    static constexpr auto NorwegianDanishNrcs = AsciiBasedCharSet{
        { L'\x40', L'\u00c4' }, // Latin Capital Letter A With Diaeresis
        { L'\x5b', L'\u00c6' }, // Latin Capital Letter Ae
        { L'\x5c', L'\u00d8' }, // Latin Capital Letter O With Stroke
        { L'\x5d', L'\u00c5' }, // Latin Capital Letter A With Ring Above
        { L'\x5e', L'\u00dc' }, // Latin Capital Letter U With Diaeresis
        { L'\x60', L'\u00e4' }, // Latin Small Letter A With Diaeresis
        { L'\x7b', L'\u00e6' }, // Latin Small Letter Ae
        { L'\x7c', L'\u00f8' }, // Latin Small Letter O With Stroke
        { L'\x7d', L'\u00e5' }, // Latin Small Letter A With Ring Above
        { L'\x7e', L'\u00fc' }, // Latin Small Letter U With Diaeresis
    };

    // https://www.itscj.ipsj.or.jp/iso-ir/060.pdf
    // Some terminal emulators consider all the Nordic character sets as equivalent,
    // but the 6/0 designator is a separate ISO-registered standard, which only maps
    // a subset of the characters included in the DEC version.
    static constexpr auto NorwegianDanishNrcsIso = AsciiBasedCharSet{
        { L'\x5b', L'\u00c6' }, // Latin Capital Letter Ae
        { L'\x5c', L'\u00d8' }, // Latin Capital Letter O With Stroke
        { L'\x5d', L'\u00c5' }, // Latin Capital Letter A With Ring Above
        { L'\x7b', L'\u00e6' }, // Latin Small Letter Ae
        { L'\x7c', L'\u00f8' }, // Latin Small Letter O With Stroke
        { L'\x7d', L'\u00e5' }, // Latin Small Letter A With Ring Above
    };

    // https://www.vt100.net/docs/vt320-uu/appendixe.html#SE.2.3
    static constexpr auto PortugueseNrcs = AsciiBasedCharSet{
        { L'\x5b', L'\u00c3' }, // Latin Capital Letter A With Tilde
        { L'\x5c', L'\u00c7' }, // Latin Capital Letter C With Cedilla
        { L'\x5d', L'\u00d5' }, // Latin Capital Letter O With Tilde
        { L'\x7b', L'\u00e3' }, // Latin Small Letter A With Tilde
        { L'\x7c', L'\u00e7' }, // Latin Small Letter C With Cedilla
        { L'\x7d', L'\u00f5' }, // Latin Small Letter O With Tilde
    };

    // https://www.vt100.net/docs/vt220-rm/table2-13.html
    static constexpr auto SpanishNrcs = AsciiBasedCharSet{
        { L'\x23', L'\u00a3' }, // Pound Sign
        { L'\x40', L'\u00a7' }, // Section Sign
        { L'\x5b', L'\u00a1' }, // Inverted Exclamation Mark
        { L'\x5c', L'\u00d1' }, // Latin Capital Letter N With Tilde
        { L'\x5d', L'\u00bf' }, // Inverted Question Mark
        { L'\x7b', L'\u00b0' }, // Degree Sign (VT320 manual has these last 3 off by 1)
        { L'\x7c', L'\u00f1' }, // Latin Small Letter N With Tilde
        { L'\x7d', L'\u00e7' }, // Latin Small Letter C With Cedilla
    };

    // https://www.vt100.net/docs/vt220-rm/table2-14.html
    static constexpr auto SwedishNrcs = AsciiBasedCharSet{
        { L'\x40', L'\u00c9' }, // Latin Capital Letter E With Acute
        { L'\x5b', L'\u00c4' }, // Latin Capital Letter A With Diaeresis
        { L'\x5c', L'\u00d6' }, // Latin Capital Letter O With Diaeresis
        { L'\x5d', L'\u00c5' }, // Latin Capital Letter A With Ring Above
        { L'\x5e', L'\u00dc' }, // Latin Capital Letter U With Diaeresis
        { L'\x60', L'\u00e9' }, // Latin Small Letter E With Acute
        { L'\x7b', L'\u00e4' }, // Latin Small Letter A With Diaeresis
        { L'\x7c', L'\u00f6' }, // Latin Small Letter O With Diaeresis
        { L'\x7d', L'\u00e5' }, // Latin Small Letter A With Ring Above
        { L'\x7e', L'\u00fc' }, // Latin Small Letter U With Diaeresis
    };

    // https://www.vt100.net/docs/vt220-rm/table2-15.html
    static constexpr auto SwissNrcs = AsciiBasedCharSet{
        { L'\x23', L'\u00f9' }, // Latin Small Letter U With Grave
        { L'\x40', L'\u00e0' }, // Latin Small Letter A With Grave
        { L'\x5b', L'\u00e9' }, // Latin Small Letter E With Acute
        { L'\x5c', L'\u00e7' }, // Latin Small Letter C With Cedilla
        { L'\x5d', L'\u00ea' }, // Latin Small Letter E With Circumflex
        { L'\x5e', L'\u00ee' }, // Latin Small Letter I With Circumflex
        { L'\x5f', L'\u00e8' }, // Latin Small Letter E With Grave
        { L'\x60', L'\u00f4' }, // Latin Small Letter O With Circumflex
        { L'\x7b', L'\u00e4' }, // Latin Small Letter A With Diaeresis
        { L'\x7c', L'\u00f6' }, // Latin Small Letter O With Diaeresis
        { L'\x7d', L'\u00fc' }, // Latin Small Letter U With Diaeresis
        { L'\x7e', L'\u00fb' }, // Latin Small Letter U With Circumflex
    };

#pragma warning(pop)
}
