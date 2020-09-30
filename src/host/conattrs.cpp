/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- conattrs.cpp

Abstract:
- Defines common operations on console attributes, especially in regards to
    finding the nearest color from a color table.

Author(s):
- Mike Griese (migrie) 01-Sept-2017
--*/

#include "precomp.h"
#include "..\inc\conattrs.hpp"
#include <cmath>

// Function Description:
// - Converts the value of a xterm color table index to the windows color table equivalent.
// Arguments:
// - xtermTableEntry: the xterm color table index
// Return Value:
// - The windows color table equivalent.
WORD XtermToWindowsIndex(const size_t xtermTableEntry) noexcept
{
    const bool fRed = WI_IsFlagSet(xtermTableEntry, XTERM_RED_ATTR);
    const bool fGreen = WI_IsFlagSet(xtermTableEntry, XTERM_GREEN_ATTR);
    const bool fBlue = WI_IsFlagSet(xtermTableEntry, XTERM_BLUE_ATTR);
    const bool fBright = WI_IsFlagSet(xtermTableEntry, XTERM_BRIGHT_ATTR);

    return (fRed ? WINDOWS_RED_ATTR : 0x0) +
           (fGreen ? WINDOWS_GREEN_ATTR : 0x0) +
           (fBlue ? WINDOWS_BLUE_ATTR : 0x0) +
           (fBright ? WINDOWS_BRIGHT_ATTR : 0x0);
}

// Function Description:
// - Converts the value of a xterm color table index to the windows color table
//      equivalent. The range of values is [0, 255], where the lowest 16 are
//      mapped to the equivalent Windows index, and the rest of the values are
//      passed through.
// Arguments:
// - xtermTableEntry: the xterm color table index
// Return Value:
// - The windows color table equivalent.
WORD Xterm256ToWindowsIndex(const size_t xtermTableEntry) noexcept
{
    return xtermTableEntry < 16 ? XtermToWindowsIndex(xtermTableEntry) :
                                  static_cast<WORD>(xtermTableEntry);
}
