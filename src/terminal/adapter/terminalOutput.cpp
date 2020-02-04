// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <precomp.h>
#include <windows.h>
#include "terminalOutput.hpp"
#include "strsafe.h"

using namespace Microsoft::Console::VirtualTerminal;

namespace
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
}

static constexpr auto Ascii = AsciiBasedCharSet{};
static constexpr auto Latin1 = Latin1BasedCharSet96{};

// https://www.vt100.net/docs/vt220-rm/table2-3b.html
#pragma warning(suppress : 26483) // Suppress spurious "value is outside the bounds" warning
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
#pragma warning(suppress : 26483) // Suppress spurious "value is outside the bounds" warning
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
#pragma warning(suppress : 26483) // Suppress spurious "value is outside the bounds" warning
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
#pragma warning(suppress : 26483) // Suppress spurious "value is outside the bounds" warning
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
#pragma warning(suppress : 26483) // Suppress spurious "value is outside the bounds" warning
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

// https://www.vt100.net/docs/vt220-rm/table2-9.html
#pragma warning(suppress : 26483) // Suppress spurious "value is outside the bounds" warning
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
#pragma warning(suppress : 26483) // Suppress spurious "value is outside the bounds" warning
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
#pragma warning(suppress : 26483) // Suppress spurious "value is outside the bounds" warning
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
#pragma warning(suppress : 26483) // Suppress spurious "value is outside the bounds" warning
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

// https://www.vt100.net/docs/vt220-rm/table2-13.html
#pragma warning(suppress : 26483) // Suppress spurious "value is outside the bounds" warning
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
#pragma warning(suppress : 26483) // Suppress spurious "value is outside the bounds" warning
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
#pragma warning(suppress : 26483) // Suppress spurious "value is outside the bounds" warning
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

TerminalOutput::TerminalOutput() noexcept
{
    _gsetTranslationTables.at(0) = Ascii;
    _gsetTranslationTables.at(1) = Ascii;
    _gsetTranslationTables.at(2) = Latin1;
    _gsetTranslationTables.at(3) = Latin1;
}

bool TerminalOutput::DesignateCharset(size_t gsetNumber, const wchar_t charset)
{
    switch (charset)
    {
    case L'B': // US ASCII
    case L'1': // Alternate Character ROM
        return _SetTranslationTable(gsetNumber, Ascii);
    case L'0': // DEC Special Graphics
    case L'2': // Alternate Character ROM Special Graphics
        return _SetTranslationTable(gsetNumber, DecSpecialGraphics);
    case L'<': // DEC Supplemental
        return _SetTranslationTable(gsetNumber, DecSupplemental);
    case L'A': // British NRCS
        return _SetTranslationTable(gsetNumber, BritishNrcs);
    case L'4': // Dutch NRCS
        return _SetTranslationTable(gsetNumber, DutchNrcs);
    case L'5': // Finnish NRCS
    case L'C': // (fallback)
        return _SetTranslationTable(gsetNumber, FinnishNrcs);
    case L'R': // French NRCS
        return _SetTranslationTable(gsetNumber, FrenchNrcs);
    case L'Q': // French Canadian NRCS
        return _SetTranslationTable(gsetNumber, FrenchCanadianNrcs);
    case L'K': // German NRCS
        return _SetTranslationTable(gsetNumber, GermanNrcs);
    case L'Y': // Italian NRCS
        return _SetTranslationTable(gsetNumber, ItalianNrcs);
    case L'6': // Norwegian/Danish NRCS
    case L'E': // (fallback)
        return _SetTranslationTable(gsetNumber, NorwegianDanishNrcs);
    case L'Z': // Spanish NRCS
        return _SetTranslationTable(gsetNumber, SpanishNrcs);
    case L'7': // Swedish NRCS
    case L'H': // (fallback)
        return _SetTranslationTable(gsetNumber, SwedishNrcs);
    case L'=': // Swiss NRCS
        return _SetTranslationTable(gsetNumber, SwissNrcs);
    default:
        return false;
    }
}

#pragma warning(suppress : 26440) // Suppress spurious "function can be declared noexcept" warning
bool TerminalOutput::LockingShift(const size_t gsetNumber)
{
    _glSetNumber = gsetNumber;
    _glTranslationTable = _gsetTranslationTables.at(_glSetNumber);
    // If GL is mapped to ASCII then we don't need to translate anything.
    if (_glTranslationTable == Ascii)
    {
        _glTranslationTable = {};
    }
    return true;
}

#pragma warning(suppress : 26440) // Suppress spurious "function can be declared noexcept" warning
bool TerminalOutput::LockingShiftRight(const size_t gsetNumber)
{
    _grSetNumber = gsetNumber;
    _grTranslationTable = _gsetTranslationTables.at(_grSetNumber);
    // If GR is mapped to Latin1 then we don't need to translate anything.
    if (_grTranslationTable == Latin1)
    {
        _grTranslationTable = {};
    }
    return true;
}

#pragma warning(suppress : 26440) // Suppress spurious "function can be declared noexcept" warning
bool TerminalOutput::SingleShift(const size_t gsetNumber)
{
    _ssTranslationTable = _gsetTranslationTables.at(gsetNumber);
    return true;
}

// Routine Description:
// - Returns true if there is an active translation table, indicating that text has to come through here
// Arguments:
// - <none>
// Return Value:
// - True if translation is required.
bool TerminalOutput::NeedToTranslate() const noexcept
{
    return !_glTranslationTable.empty() || !_grTranslationTable.empty() || !_ssTranslationTable.empty();
}

wchar_t TerminalOutput::TranslateKey(const wchar_t wch) const noexcept
{
    wchar_t wchFound = wch;
    if (!_ssTranslationTable.empty())
    {
        if (wch - 0x20u < _ssTranslationTable.size())
        {
            wchFound = _ssTranslationTable.at(wch - 0x20u);
        }
        else if (wch - 0xA0u < _ssTranslationTable.size())
        {
            wchFound = _ssTranslationTable.at(wch - 0xA0u);
        }
        _ssTranslationTable = {};
    }
    else
    {
        if (wch - 0x20u < _glTranslationTable.size())
        {
            wchFound = _glTranslationTable.at(wch - 0x20u);
        }
        else if (wch - 0xA0u < _grTranslationTable.size())
        {
            wchFound = _grTranslationTable.at(wch - 0xA0u);
        }
    }
    return wchFound;
}

bool TerminalOutput::_SetTranslationTable(const size_t gsetNumber, const std::wstring_view translationTable)
{
    _gsetTranslationTables.at(gsetNumber) = translationTable;
    // We need to reapply the locking shifts in case the underlying G-sets have changed.
    return LockingShift(_glSetNumber) && LockingShiftRight(_grSetNumber);
}
