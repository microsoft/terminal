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

[[nodiscard]] HRESULT DoSrvPrivateWriteConsoleInputW(_Inout_ InputBuffer* const pInputBuffer,
                                                     _Inout_ std::deque<std::unique_ptr<IInputEvent>>& events,
                                                     _Out_ size_t& eventsWritten,
                                                     const bool append) noexcept;

[[nodiscard]] NTSTATUS ConsoleCreateScreenBuffer(std::unique_ptr<ConsoleHandleData>& handle,
                                                 _In_ PCONSOLE_API_MSG Message,
                                                 _In_ PCD_CREATE_OBJECT_INFORMATION Information,
                                                 _In_ PCONSOLE_CREATESCREENBUFFER_MSG a);

[[nodiscard]] NTSTATUS DoSrvPrivatePrependConsoleInput(_Inout_ InputBuffer* const pInputBuffer,
                                                       _Inout_ std::deque<std::unique_ptr<IInputEvent>>& events,
                                                       _Out_ size_t& eventsWritten);

[[nodiscard]] NTSTATUS DoSrvPrivateWriteConsoleControlInput(_Inout_ InputBuffer* const pInputBuffer,
                                                            _In_ KeyEvent key);
