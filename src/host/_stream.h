/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- _stream.h

Abstract:
- Process stream written content into the text buffer

Author:
- KazuM Jun.09.1997

Revision History:
- Remove FE/Non-FE separation in preparation for refactoring. (MiNiksa, 2014)
--*/

#pragma once

#include "..\server\IWaitRoutine.h"
#include "writeData.hpp"

/*++
Routine Description:
    This routine updates the cursor position.  Its input is the non-special
    cased new location of the cursor.  For example, if the cursor were being
    moved one space backwards from the left edge of the screen, the X
    coordinate would be -1.  This routine would set the X coordinate to
    the right edge of the screen and decrement the Y coordinate by one.

Arguments:
    pScreenInfo - Pointer to screen buffer information structure.
    coordCursor - New location of cursor.
    fKeepCursorVisible - TRUE if changing window origin desirable when hit right edge

Return Value:
--*/
[[nodiscard]] NTSTATUS AdjustCursorPosition(SCREEN_INFORMATION& screenInfo,
                                            _In_ COORD coordCursor,
                                            const BOOL fKeepCursorVisible,
                                            _Inout_opt_ PSHORT psScrollY);

/*++
Routine Description:
    This routine writes a string to the screen, processing any embedded
    unicode characters.  The string is also copied to the input buffer, if
    the output mode is line mode.

Arguments:
    ScreenInfo - Pointer to screen buffer information structure.
    lpBufferBackupLimit - Pointer to beginning of buffer.
    lpBuffer - Pointer to buffer to copy string to.  assumed to be at least
        as long as lpRealUnicodeString.  This pointer is updated to point to the
        next position in the buffer.
    lpRealUnicodeString - Pointer to string to write.
    NumBytes - On input, number of bytes to write.  On output, number of
        bytes written.
    NumSpaces - On output, the number of spaces consumed by the written characters.
    dwFlags -
      WC_DESTRUCTIVE_BACKSPACE backspace overwrites characters.
      WC_KEEP_CURSOR_VISIBLE   change window origin desirable when hit rt. edge
      WC_ECHO                  if called by Read (echoing characters)

Return Value:

Note:
    This routine does not process tabs and backspace properly.  That code
    will be implemented as part of the line editing services.
--*/
[[nodiscard]] NTSTATUS WriteCharsLegacy(SCREEN_INFORMATION& screenInfo,
                                        _In_range_(<=, pwchBuffer) const wchar_t* const pwchBufferBackupLimit,
                                        _In_ const wchar_t* pwchBuffer,
                                        _In_reads_bytes_(*pcb) const wchar_t* pwchRealUnicode,
                                        _Inout_ size_t* const pcb,
                                        _Out_opt_ size_t* const pcSpaces,
                                        const SHORT sOriginalXPosition,
                                        const DWORD dwFlags,
                                        _Inout_opt_ PSHORT const psScrollY);

// The new entry point for WriteChars to act as an intercept in case we place a Virtual Terminal processor in the way.
[[nodiscard]] NTSTATUS WriteChars(SCREEN_INFORMATION& screenInfo,
                                  _In_range_(<=, pwchBuffer) const wchar_t* const pwchBufferBackupLimit,
                                  _In_ const wchar_t* pwchBuffer,
                                  _In_reads_bytes_(*pcb) const wchar_t* pwchRealUnicode,
                                  _Inout_ size_t* const pcb,
                                  _Out_opt_ size_t* const pcSpaces,
                                  const SHORT sOriginalXPosition,
                                  const DWORD dwFlags,
                                  _Inout_opt_ PSHORT const psScrollY);

// NOTE: console lock must be held when calling this routine
// String has been translated to unicode at this point.
[[nodiscard]] NTSTATUS DoWriteConsole(_In_reads_bytes_(*pcbBuffer) PWCHAR pwchBuffer,
                                      _In_ size_t* const pcbBuffer,
                                      SCREEN_INFORMATION& screenInfo,
                                      std::unique_ptr<WriteData>& waiter);
