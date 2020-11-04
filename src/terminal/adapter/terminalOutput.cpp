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

bool TerminalOutput::Designate94Charset(size_t gsetNumber, const VTID charset)
{
    switch (charset)
    {
    case VTID("B"): // US ASCII
    case VTID("1"): // Alternate Character ROM
        return _SetTranslationTable(gsetNumber, Ascii);
    case VTID("0"): // DEC Special Graphics
    case VTID("2"): // Alternate Character ROM Special Graphics
        return _SetTranslationTable(gsetNumber, DecSpecialGraphics);
    case VTID("<"): // DEC Supplemental
        return _SetTranslationTable(gsetNumber, DecSupplemental);
    case VTID("A"): // British NRCS
        return _SetTranslationTable(gsetNumber, BritishNrcs);
    case VTID("4"): // Dutch NRCS
        return _SetTranslationTable(gsetNumber, DutchNrcs);
    case VTID("5"): // Finnish NRCS
    case VTID("C"): // (fallback)
        return _SetTranslationTable(gsetNumber, FinnishNrcs);
    case VTID("R"): // French NRCS
        return _SetTranslationTable(gsetNumber, FrenchNrcs);
    case VTID("f"): // French NRCS (ISO update)
        return _SetTranslationTable(gsetNumber, FrenchNrcsIso);
    case VTID("9"): // French Canadian NRCS
    case VTID("Q"): // (fallback)
        return _SetTranslationTable(gsetNumber, FrenchCanadianNrcs);
    case VTID("K"): // German NRCS
        return _SetTranslationTable(gsetNumber, GermanNrcs);
    case VTID("Y"): // Italian NRCS
        return _SetTranslationTable(gsetNumber, ItalianNrcs);
    case VTID("6"): // Norwegian/Danish NRCS
    case VTID("E"): // (fallback)
        return _SetTranslationTable(gsetNumber, NorwegianDanishNrcs);
    case VTID("`"): // Norwegian/Danish NRCS (ISO standard)
        return _SetTranslationTable(gsetNumber, NorwegianDanishNrcsIso);
    case VTID("Z"): // Spanish NRCS
        return _SetTranslationTable(gsetNumber, SpanishNrcs);
    case VTID("7"): // Swedish NRCS
    case VTID("H"): // (fallback)
        return _SetTranslationTable(gsetNumber, SwedishNrcs);
    case VTID("="): // Swiss NRCS
        return _SetTranslationTable(gsetNumber, SwissNrcs);
    case VTID("&4"): // DEC Cyrillic
        return _SetTranslationTable(gsetNumber, DecCyrillic);
    case VTID("&5"): // Russian NRCS
        return _SetTranslationTable(gsetNumber, RussianNrcs);
    case VTID("\"?"): // DEC Greek
        return _SetTranslationTable(gsetNumber, DecGreek);
    case VTID("\">"): // Greek NRCS
        return _SetTranslationTable(gsetNumber, GreekNrcs);
    case VTID("\"4"): // DEC Hebrew
        return _SetTranslationTable(gsetNumber, DecHebrew);
    case VTID("%="): // Hebrew NRCS
        return _SetTranslationTable(gsetNumber, HebrewNrcs);
    case VTID("%0"): // DEC Turkish
        return _SetTranslationTable(gsetNumber, DecTurkish);
    case VTID("%2"): // Turkish NRCS
        return _SetTranslationTable(gsetNumber, TurkishNrcs);
    case VTID("%5"): // DEC Supplemental
        return _SetTranslationTable(gsetNumber, DecSupplemental);
    case VTID("%6"): // Portuguese NRCS
        return _SetTranslationTable(gsetNumber, PortugueseNrcs);
    default:
        return false;
    }
}

bool TerminalOutput::Designate96Charset(size_t gsetNumber, const VTID charset)
{
    switch (charset)
    {
    case VTID("A"): // ISO Latin-1 Supplemental
    case VTID("<"): // (UPSS when assigned to Latin-1)
        return _SetTranslationTable(gsetNumber, Latin1);
    case VTID("B"): // ISO Latin-2 Supplemental
        return _SetTranslationTable(gsetNumber, Latin2);
    case VTID("L"): // ISO Latin-Cyrillic Supplemental
        return _SetTranslationTable(gsetNumber, LatinCyrillic);
    case VTID("F"): // ISO Latin-Greek Supplemental
        return _SetTranslationTable(gsetNumber, LatinGreek);
    case VTID("H"): // ISO Latin-Hebrew Supplemental
        return _SetTranslationTable(gsetNumber, LatinHebrew);
    case VTID("M"): // ISO Latin-5 Supplemental
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
