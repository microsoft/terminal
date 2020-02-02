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
    typedef CharSet<L'\xa0', 96> Latin1BasedCharSet;
}

static constexpr auto Ascii = AsciiBasedCharSet{};
static constexpr auto Latin1 = Latin1BasedCharSet{};

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
    case L'A': // British NRCS
        return _SetTranslationTable(gsetNumber, BritishNrcs);
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
    return !_glTranslationTable.empty() || !_ssTranslationTable.empty();
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
        _ssTranslationTable = {};
    }
    else
    {
        if (wch - 0x20u < _glTranslationTable.size())
        {
            wchFound = _glTranslationTable.at(wch - 0x20u);
        }
    }
    return wchFound;
}

bool TerminalOutput::_SetTranslationTable(const size_t gsetNumber, const std::wstring_view translationTable)
{
    _gsetTranslationTables.at(gsetNumber) = translationTable;
    // We need to reapply the locking shift in case the underlying G-set has changed.
    return LockingShift(_glSetNumber);
}
