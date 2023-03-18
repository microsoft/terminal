// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "charsets.hpp"
#include "terminalOutput.hpp"
#include "strsafe.h"

using namespace Microsoft::Console::VirtualTerminal;

TerminalOutput::TerminalOutput() noexcept
{
    // By default we set all of the G-sets to ASCII, so if someone accidentally
    // triggers a locking shift, they won't end up with Latin1 in the GL table,
    // making their system unreadable. If ISO-2022 encoding is selected, though,
    // we'll reset the G2 and G3 tables to Latin1, so that 8-bit apps will get a
    // more meaningful character mapping by default. This is triggered by a DOCS
    // sequence, which will call the EnableGrTranslation method below.
    _gsetTranslationTables.at(0) = Ascii;
    _gsetTranslationTables.at(1) = Ascii;
    _gsetTranslationTables.at(2) = Ascii;
    _gsetTranslationTables.at(3) = Ascii;
    _gsetIds.at(0) = VTID("B");
    _gsetIds.at(1) = VTID("B");
    _gsetIds.at(2) = VTID("B");
    _gsetIds.at(3) = VTID("B");
}

bool TerminalOutput::Designate94Charset(size_t gsetNumber, const VTID charset)
{
    const auto translationTable = _LookupTranslationTable94(charset);
    RETURN_BOOL_IF_FALSE(!translationTable.empty());
    _gsetIds.at(gsetNumber) = charset;
    return _SetTranslationTable(gsetNumber, translationTable);
}

bool TerminalOutput::Designate96Charset(size_t gsetNumber, const VTID charset)
{
    const auto translationTable = _LookupTranslationTable96(charset);
    RETURN_BOOL_IF_FALSE(!translationTable.empty());
    _gsetIds.at(gsetNumber) = charset;
    return _SetTranslationTable(gsetNumber, translationTable);
}

void TerminalOutput::SetDrcs94Designation(const VTID charset)
{
    _ReplaceDrcsTable(_LookupTranslationTable94(charset), Drcs94);
    _drcsId = charset;
    _drcsTranslationTable = Drcs94;
}

void TerminalOutput::SetDrcs96Designation(const VTID charset)
{
    _ReplaceDrcsTable(_LookupTranslationTable96(charset), Drcs96);
    _drcsId = charset;
    _drcsTranslationTable = Drcs96;
}

VTID TerminalOutput::GetCharsetId(const size_t gsetNumber) const
{
    return _gsetIds.at(gsetNumber);
}

size_t TerminalOutput::GetCharsetSize(const size_t gsetNumber) const
{
    return _gsetTranslationTables.at(gsetNumber).size() == 96 ? 96 : 94;
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

bool TerminalOutput::SingleShift(const size_t gsetNumber) noexcept
{
    _ssSetNumber = gsetNumber;
    return true;
}

size_t TerminalOutput::GetLeftSetNumber() const noexcept
{
    return _glSetNumber;
}

size_t TerminalOutput::GetRightSetNumber() const noexcept
{
    return _grSetNumber;
}

bool TerminalOutput::IsSingleShiftPending(const size_t gsetNumber) const noexcept
{
    return _ssSetNumber == gsetNumber;
}

// Routine Description:
// - Returns true if there is an active translation table, indicating that text has to come through here
// Arguments:
// - <none>
// Return Value:
// - True if translation is required.
bool TerminalOutput::NeedToTranslate() const noexcept
{
    return !_glTranslationTable.empty() || !_grTranslationTable.empty() || _ssSetNumber != 0;
}

void TerminalOutput::EnableGrTranslation(boolean enabled)
{
    _grTranslationEnabled = enabled;
    // The default table for G2 and G3 is Latin1 when GR translation is enabled,
    // and ASCII when disabled. The reason for this is explained in the constructor.
    const auto defaultTranslationTable = enabled ? std::wstring_view{ Latin1 } : std::wstring_view{ Ascii };
    _gsetTranslationTables.at(2) = defaultTranslationTable;
    _gsetTranslationTables.at(3) = defaultTranslationTable;
    // We need to reapply the locking shifts in case the underlying G-sets have changed.
    LockingShift(_glSetNumber);
    LockingShiftRight(_grSetNumber);
}

wchar_t TerminalOutput::TranslateKey(const wchar_t wch) const noexcept
{
    auto wchFound = wch;
    if (_ssSetNumber == 2 || _ssSetNumber == 3)
    {
        const auto ssTranslationTable = _gsetTranslationTables.at(_ssSetNumber);
        if (wch - 0x20u < ssTranslationTable.size())
        {
            wchFound = ssTranslationTable.at(wch - 0x20u);
        }
        else if (wch - 0xA0u < ssTranslationTable.size())
        {
            wchFound = ssTranslationTable.at(wch - 0xA0u);
        }
        _ssSetNumber = 0;
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

const std::wstring_view TerminalOutput::_LookupTranslationTable94(const VTID charset) const
{
    // Note that the DRCS set can be designated with either a 94 or 96 sequence,
    // regardless of the actual size of the set. This isn't strictly correct,
    // but there is existing software that depends on this behavior.
    if (charset == _drcsId)
    {
        return _drcsTranslationTable;
    }
    switch (charset)
    {
    case VTID("B"): // US ASCII
    case VTID("1"): // Alternate Character ROM
        return Ascii;
    case VTID("0"): // DEC Special Graphics
    case VTID("2"): // Alternate Character ROM Special Graphics
        return DecSpecialGraphics;
    case VTID("<"): // DEC Supplemental
        return DecSupplemental;
    case VTID("A"): // British NRCS
        return BritishNrcs;
    case VTID("4"): // Dutch NRCS
        return DutchNrcs;
    case VTID("5"): // Finnish NRCS
    case VTID("C"): // (fallback)
        return FinnishNrcs;
    case VTID("R"): // French NRCS
        return FrenchNrcs;
    case VTID("f"): // French NRCS (ISO update)
        return FrenchNrcsIso;
    case VTID("9"): // French Canadian NRCS
    case VTID("Q"): // (fallback)
        return FrenchCanadianNrcs;
    case VTID("K"): // German NRCS
        return GermanNrcs;
    case VTID("Y"): // Italian NRCS
        return ItalianNrcs;
    case VTID("6"): // Norwegian/Danish NRCS
    case VTID("E"): // (fallback)
        return NorwegianDanishNrcs;
    case VTID("`"): // Norwegian/Danish NRCS (ISO standard)
        return NorwegianDanishNrcsIso;
    case VTID("Z"): // Spanish NRCS
        return SpanishNrcs;
    case VTID("7"): // Swedish NRCS
    case VTID("H"): // (fallback)
        return SwedishNrcs;
    case VTID("="): // Swiss NRCS
        return SwissNrcs;
    case VTID("&4"): // DEC Cyrillic
        return DecCyrillic;
    case VTID("&5"): // Russian NRCS
        return RussianNrcs;
    case VTID("\"?"): // DEC Greek
        return DecGreek;
    case VTID("\">"): // Greek NRCS
        return GreekNrcs;
    case VTID("\"4"): // DEC Hebrew
        return DecHebrew;
    case VTID("%="): // Hebrew NRCS
        return HebrewNrcs;
    case VTID("%0"): // DEC Turkish
        return DecTurkish;
    case VTID("%2"): // Turkish NRCS
        return TurkishNrcs;
    case VTID("%5"): // DEC Supplemental
        return DecSupplemental;
    case VTID("%6"): // Portuguese NRCS
        return PortugueseNrcs;
    default:
        return {};
    }
}

const std::wstring_view TerminalOutput::_LookupTranslationTable96(const VTID charset) const
{
    // Note that the DRCS set can be designated with either a 94 or 96 sequence,
    // regardless of the actual size of the set. This isn't strictly correct,
    // but there is existing software that depends on this behavior.
    if (charset == _drcsId)
    {
        return _drcsTranslationTable;
    }
    switch (charset)
    {
    case VTID("A"): // ISO Latin-1 Supplemental
    case VTID("<"): // (UPSS when assigned to Latin-1)
        return Latin1;
    case VTID("B"): // ISO Latin-2 Supplemental
        return Latin2;
    case VTID("L"): // ISO Latin-Cyrillic Supplemental
        return LatinCyrillic;
    case VTID("F"): // ISO Latin-Greek Supplemental
        return LatinGreek;
    case VTID("H"): // ISO Latin-Hebrew Supplemental
        return LatinHebrew;
    case VTID("M"): // ISO Latin-5 Supplemental
        return Latin5;
    default:
        return {};
    }
}

bool TerminalOutput::_SetTranslationTable(const size_t gsetNumber, const std::wstring_view translationTable)
{
    _gsetTranslationTables.at(gsetNumber) = translationTable;
    // We need to reapply the locking shifts in case the underlying G-sets have changed.
    return LockingShift(_glSetNumber) && LockingShiftRight(_grSetNumber);
}

void TerminalOutput::_ReplaceDrcsTable(const std::wstring_view oldTable, const std::wstring_view newTable)
{
    if (newTable.data() != oldTable.data())
    {
        for (size_t gsetNumber = 0; gsetNumber < 4; gsetNumber++)
        {
            // Get the current translation table for this G-set.
            auto gsetTable = _gsetTranslationTables.at(gsetNumber);
            // If it's already a DRCS, replace it with a default charset.
            if (Drcs94 == gsetTable || Drcs96 == gsetTable)
            {
                gsetTable = gsetNumber < 2 ? (std::wstring_view)Ascii : (std::wstring_view)Latin1;
            }
            // If it matches the old table, replace it with the new table.
            if (gsetTable.data() == oldTable.data())
            {
                gsetTable = newTable;
            }
            // Update the G-set entry with the new translation table.
            _gsetTranslationTables.at(gsetNumber) = gsetTable;
        }
        // Reapply the locking shifts in case the underlying G-sets have changed.
        LockingShift(_glSetNumber);
        LockingShiftRight(_grSetNumber);
    }
}
