// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <precomp.h>
#include <windows.h>
#include "terminalOutput.hpp"
#include "strsafe.h"

using namespace Microsoft::Console::VirtualTerminal;

TerminalOutput::TerminalOutput()
{
}

TerminalOutput::~TerminalOutput()
{
}

// We include a full table so all we have to do is the lookup.
// The tables only ever change the values x20 - x7f, hence why the table starts at \x20
// From http://vt100.net/docs/vt220-rm/table2-4.html
const wchar_t TerminalOutput::s_rgDECSpecialGraphicsTranslations[s_uiNumDisplayCharacters]{
    L'\x20',
    L'\x21',
    L'\x22',
    L'\x23',
    L'\x24',
    L'\x25',
    L'\x26',
    L'\x27',
    L'\x28',
    L'\x29',
    L'\x2a',
    L'\x2b',
    L'\x2c',
    L'\x2d',
    L'\x2e',
    L'\x2f',
    L'\x30',
    L'\x31',
    L'\x32',
    L'\x33',
    L'\x34',
    L'\x35',
    L'\x36',
    L'\x37',
    L'\x38',
    L'\x39',
    L'\x3a',
    L'\x3b',
    L'\x3c',
    L'\x3d',
    L'\x3e',
    L'\x3f',
    L'\x40',
    L'\x41',
    L'\x42',
    L'\x43',
    L'\x44',
    L'\x45',
    L'\x46',
    L'\x47',
    L'\x48',
    L'\x49',
    L'\x4a',
    L'\x4b',
    L'\x4c',
    L'\x4d',
    L'\x4e',
    L'\x4f',
    L'\x50',
    L'\x51',
    L'\x52',
    L'\x53',
    L'\x54',
    L'\x55',
    L'\x56',
    L'\x57',
    L'\x58',
    L'\x59',
    L'\x5a',
    L'\x5b',
    L'\x5c',
    L'\x5d',
    L'\x5e',
    L'\u0020', // L'\x5f',   -> Blank
    L'\u2666', // L'\x60',   -> Diamond (more commonly U+25C6, but U+2666 renders better for us)
    L'\u2592', // L'\x61',   -> Checkerboard
    L'\u2409', // L'\x62',   -> HT, SYMBOL FOR HORIZONTAL TABULATION
    L'\u240c', // L'\x63',   -> FF, SYMBOL FOR FORM FEED
    L'\u240d', // L'\x64',   -> CR, SYMBOL FOR CARRIAGE RETURN
    L'\u240a', // L'\x65',   -> LF, SYMBOL FOR LINE FEED
    L'\u00b0', // L'\x66',   -> Degree symbol
    L'\u00b1', // L'\x67',   -> Plus/minus
    L'\u2424', // L'\x68',   -> NL, SYMBOL FOR NEWLINE
    L'\u240b', // L'\x69',   -> VT, SYMBOL FOR VERTICAL TABULATION
    L'\u2518', // L'\x6a',   -> Lower-right corner
    L'\u2510', // L'\x6b',   -> Upper-right corner
    L'\u250c', // L'\x6c',   -> Upper-left corner
    L'\u2514', // L'\x6d',   -> Lower-left corner
    L'\u253c', // L'\x6e',   -> Crossing lines
    L'\u23ba', // L'\x6f',   -> Horizontal line - Scan 1
    L'\u23bb', // L'\x70',   -> Horizontal line - Scan 3
    L'\u2500', // L'\x71',   -> Horizontal line - Scan 5
    L'\u23bc', // L'\x72',   -> Horizontal line - Scan 7
    L'\u23bd', // L'\x73',   -> Horizontal line - Scan 9
    L'\u251c', // L'\x74',   -> Left "T"
    L'\u2524', // L'\x75',   -> Right "T"
    L'\u2534', // L'\x76',   -> Bottom "T"
    L'\u252c', // L'\x77',   -> Top "T"
    L'\u2502', // L'\x78',   -> | Vertical bar
    L'\u2264', // L'\x79',   -> Less than or equal to
    L'\u2265', // L'\x7a',   -> Greater than or equal to
    L'\u03c0', // L'\x7b',   -> Pi
    L'\u2260', // L'\x7c',   -> Not equal to
    L'\u00a3', // L'\x7d',   -> UK pound sign
    L'\u00b7', // L'\x7e',   -> Centered dot
    L'\x7f' // L'\x7f',   -> DEL
};

bool TerminalOutput::DesignateCharset(const wchar_t wchNewCharset)
{
    bool result = false;
    if (wchNewCharset == DispatchTypes::VTCharacterSets::DEC_LineDrawing ||
        wchNewCharset == DispatchTypes::VTCharacterSets::USASCII)
    {
        _wchCurrentCharset = wchNewCharset;
        result = true;
    }
    return result;
}

// Routine Description:
// - Returns true if the current charset isn't USASCII, indicating that text has to come through here
// Arguments:
// - <none>
// Return Value:
// - True if the current charset is not USASCII
bool TerminalOutput::NeedToTranslate() const
{
    return _wchCurrentCharset != DispatchTypes::VTCharacterSets::USASCII;
}

const wchar_t* TerminalOutput::_GetTranslationTable() const
{
    const wchar_t* pwchTranslation = nullptr;
    switch (_wchCurrentCharset)
    {
    case DispatchTypes::VTCharacterSets::DEC_LineDrawing:
        pwchTranslation = TerminalOutput::s_rgDECSpecialGraphicsTranslations;
        break;
    }
    return pwchTranslation;
}

wchar_t TerminalOutput::TranslateKey(const wchar_t wch) const
{
    wchar_t wchFound = wch;
    if (_wchCurrentCharset == DispatchTypes::VTCharacterSets::USASCII ||
        wch < '\x5f' || wch > '\x7f') // filter out the region we know is unchanged
    {
        ; // do nothing, these are the same as default.
    }
    else
    {
        const wchar_t* pwchTranslationTable = _GetTranslationTable();
        if (pwchTranslationTable != nullptr)
        {
            wchFound = (pwchTranslationTable[wch - '\x20']);
        }
    }
    return wchFound;
}
