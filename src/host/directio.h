/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- directio.h

Abstract:
- This file implements the NT console direct I/O API (read/write STDIO streams)

Author:
- KazuM Apr.19.1996

Revision History:
--*/

#pragma once

#include "conapi.h"
#include "inputBuffer.hpp"

class SCREEN_INFORMATION;

[[nodiscard]] NTSTATUS ConsoleCreateScreenBuffer(std::unique_ptr<ConsoleHandleData>& handle,
                                                 _In_ PCONSOLE_API_MSG Message,
                                                 _In_ PCD_CREATE_OBJECT_INFORMATION Information,
                                                 _In_ PCONSOLE_CREATESCREENBUFFER_MSG a);
