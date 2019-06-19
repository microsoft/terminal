/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- output.h

Abstract:
- This module contains the internal structures and definitions used
  by the output (screen) component of the NT console subsystem.

Author:
- Therese Stowell (ThereseS) 12-Nov-1990

Revision History:
--*/

#pragma once

#include "screenInfo.hpp"
#include "server.h"
#include "../buffer/out/OutputCell.hpp"
#include "../buffer/out/OutputCellRect.hpp"

void ScreenBufferSizeChange(const COORD coordNewSize);

[[nodiscard]] NTSTATUS DoCreateScreenBuffer();

std::vector<WORD> ReadOutputAttributes(const SCREEN_INFORMATION& screenInfo,
                                       const COORD coordRead,
                                       const size_t amountToRead);

std::wstring ReadOutputStringW(const SCREEN_INFORMATION& screenInfo,
                               const COORD coordRead,
                               const size_t amountToRead);

std::string ReadOutputStringA(const SCREEN_INFORMATION& screenInfo,
                              const COORD coordRead,
                              const size_t amountToRead);

void ScrollRegion(SCREEN_INFORMATION& screenInfo,
                  const SMALL_RECT scrollRect,
                  const std::optional<SMALL_RECT> clipRect,
                  const COORD destinationOrigin,
                  const wchar_t fillCharGiven,
                  const TextAttribute fillAttrsGiven);

VOID SetConsoleWindowOwner(const HWND hwnd, _Inout_opt_ ConsoleProcessHandle* pProcessData);

bool StreamScrollRegion(SCREEN_INFORMATION& screenInfo);

// For handling process handle state, not the window state itself.
void CloseConsoleProcessState();
