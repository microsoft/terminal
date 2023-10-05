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

enum class WriteCharsLegacyFlags : int
{
    None = 0,
    Interactive = 0x1,
    SuppressMSAA = 0x2,
};
DEFINE_ENUM_FLAG_OPERATORS(WriteCharsLegacyFlags)

void WriteCharsLegacy(SCREEN_INFORMATION& screenInfo, const std::wstring_view& str, WriteCharsLegacyFlags flags, til::CoordType* psScrollY);

// NOTE: console lock must be held when calling this routine
// String has been translated to unicode at this point.
[[nodiscard]] NTSTATUS DoWriteConsole(_In_reads_bytes_(pcbBuffer) const wchar_t* pwchBuffer,
                                      _Inout_ size_t* const pcbBuffer,
                                      SCREEN_INFORMATION& screenInfo,
                                      bool requiresVtQuirk,
                                      std::unique_ptr<WriteData>& waiter);
