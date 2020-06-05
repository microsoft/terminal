// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <precomp.h>
#include <windows.h>
#include "charsets.hpp"
#include "terminalOutput.hpp"
#include "strsafe.h"

using namespace Microsoft::Console::VirtualTerminal;

TerminalOutput::TerminalOutput() noexcept
{
    _gsetTranslationTables.at(0) = Ascii;
    _gsetTranslationTables.at(1) = Ascii;
    _gsetTranslationTables.at(2) = Latin1;
    _gsetTranslationTables.at(3) = Latin1;
}

bool TerminalOutput::Designate94Charset(size_t gsetNumber, const std::pair<wchar_t, wchar_t> charset)
{
    switch (charset.first)
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
    case L'f': // French NRCS (ISO update)
        return _SetTranslationTable(gsetNumber, FrenchNrcsIso);
    case L'9': // French Canadian NRCS
    case L'Q': // (fallback)
        return _SetTranslationTable(gsetNumber, FrenchCanadianNrcs);
    case L'K': // German NRCS
        return _SetTranslationTable(gsetNumber, GermanNrcs);
    case L'Y': // Italian NRCS
        return _SetTranslationTable(gsetNumber, ItalianNrcs);
    case L'6': // Norwegian/Danish NRCS
    case L'E': // (fallback)
        return _SetTranslationTable(gsetNumber, NorwegianDanishNrcs);
    case L'`': // Norwegian/Danish NRCS (ISO standard)
        return _SetTranslationTable(gsetNumber, NorwegianDanishNrcsIso);
    case L'Z': // Spanish NRCS
        return _SetTranslationTable(gsetNumber, SpanishNrcs);
    case L'7': // Swedish NRCS
    case L'H': // (fallback)
        return _SetTranslationTable(gsetNumber, SwedishNrcs);
    case L'=': // Swiss NRCS
        return _SetTranslationTable(gsetNumber, SwissNrcs);
    case L'&':
        switch (charset.second)
        {
        case L'4': // DEC Cyrillic
            return _SetTranslationTable(gsetNumber, DecCyrillic);
        case L'5': // Russian NRCS
            return _SetTranslationTable(gsetNumber, RussianNrcs);
        }
        return false;
    case L'"':
        switch (charset.second)
        {
        case L'?': // DEC Greek
            return _SetTranslationTable(gsetNumber, DecGreek);
        case L'>': // Greek NRCS
            return _SetTranslationTable(gsetNumber, GreekNrcs);
        case L'4': // DEC Hebrew
            return _SetTranslationTable(gsetNumber, DecHebrew);
        }
        return false;
    case L'%':
        switch (charset.second)
        {
        case L'=': // Hebrew NRCS
            return _SetTranslationTable(gsetNumber, HebrewNrcs);
        case L'0': // DEC Turkish
            return _SetTranslationTable(gsetNumber, DecTurkish);
        case L'2': // Turkish NRCS
            return _SetTranslationTable(gsetNumber, TurkishNrcs);
        case L'5': // DEC Supplemental
            return _SetTranslationTable(gsetNumber, DecSupplemental);
        case L'6': // Portuguese NRCS
            return _SetTranslationTable(gsetNumber, PortugueseNrcs);
        }
        return false;
    default:
        return false;
    }
}

bool TerminalOutput::Designate96Charset(size_t gsetNumber, const std::pair<wchar_t, wchar_t> charset)
{
    switch (charset.first)
    {
    case L'A': // ISO Latin-1 Supplemental
    case L'<': // (UPSS when assigned to Latin-1)
        return _SetTranslationTable(gsetNumber, Latin1);
    case L'B': // ISO Latin-2 Supplemental
        return _SetTranslationTable(gsetNumber, Latin2);
    case L'L': // ISO Latin-Cyrillic Supplemental
        return _SetTranslationTable(gsetNumber, LatinCyrillic);
    case L'F': // ISO Latin-Greek Supplemental
        return _SetTranslationTable(gsetNumber, LatinGreek);
    case L'H': // ISO Latin-Hebrew Supplemental
        return _SetTranslationTable(gsetNumber, LatinHebrew);
    case L'M': // ISO Latin-5 Supplemental
        return _SetTranslationTable(gsetNumber, Latin5);
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
    // If GR is mapped to Latin1, or GR translation is not allowed, we don't need to translate anything.
    if (_grTranslationTable == Latin1 || !_grTranslationEnabled)
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

void TerminalOutput::EnableGrTranslation(boolean enabled)
{
    _grTranslationEnabled = enabled;
    // We need to reapply the right locking shift to (de)activate the translation table.
    LockingShiftRight(_grSetNumber);
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
