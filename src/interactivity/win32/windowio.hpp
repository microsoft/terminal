// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "precomp.h"

#include "..\..\server\ProcessHandle.h"

#pragma hdrstop

void HandleKeyEvent(const HWND hWnd,
                    const UINT Message,
                    const WPARAM wParam,
                    const LPARAM lParam,
                    _Inout_opt_ PBOOL pfUnlockConsole);
BOOL HandleSysKeyEvent(const HWND hWnd,
                       const UINT Message,
                       const WPARAM wParam,
                       const LPARAM lParam,
                       _Inout_opt_ PBOOL pfUnlockConsole);
BOOL HandleMouseEvent(const SCREEN_INFORMATION& ScreenInfo,
                      const UINT Message,
                      const WPARAM wParam,
                      const LPARAM lParam);

VOID SetConsoleWindowOwner(const HWND hwnd, _Inout_opt_ ConsoleProcessHandle* pProcessData);
DWORD WINAPI ConsoleInputThreadProcWin32(LPVOID lpParameter);
