/*++
Copyright (c) Microsoft Corporation

Licensed under the MIT license.
Module Name:
- stream.h

Abstract:
- This file implements the NT console server stream API

Author:
- Therese Stowell (ThereseS) 6-Nov-1990

Revision History:
--*/

#pragma once

#include "cmdline.h"

[[nodiscard]] NTSTATUS ReadCharacterInput(InputBuffer& inputBuffer,
                                          std::span<char> buffer,
                                          size_t& bytesRead,
                                          INPUT_READ_HANDLE_DATA& readHandleState,
                                          const bool unicode);

VOID UnblockWriteConsole(const DWORD dwReason);
