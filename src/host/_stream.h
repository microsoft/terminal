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

#include "writeData.hpp"

void WriteCharsLegacy(SCREEN_INFORMATION& screenInfo, const std::wstring_view& str, til::CoordType* psScrollY);
void WriteCharsVT(SCREEN_INFORMATION& screenInfo, const std::wstring_view& str);
void WriteClearScreen(SCREEN_INFORMATION& screenInfo);

// NOTE: console lock must be held when calling this routine
// String has been translated to unicode at this point.
[[nodiscard]] NTSTATUS DoWriteConsole(_In_reads_bytes_(pcbBuffer) const wchar_t* pwchBuffer,
                                      _Inout_ size_t* const pcbBuffer,
                                      SCREEN_INFORMATION& screenInfo,
                                      std::unique_ptr<WriteData>& waiter);
