/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- OneCoreDelay.hpp

Abstract:
- This module contains API definitions that aren't available on OneCore.
  It will help us to call them on a full Desktop machine and fail on OneCore.

Author:
- Michael Niksa (MiNiksa)          2017

Revision History:
--*/

#pragma once

#include "..\..\inc\consoletaeftemplates.hpp"

namespace OneCoreDelay
{
    BOOLEAN
    __stdcall IsIsWindowPresent();

    BOOLEAN
    __stdcall IsGetSystemMetricsPresent();

    BOOLEAN
    __stdcall IsPostMessageWPresent();

    BOOLEAN
    __stdcall IsSendMessageWPresent();

    BOOL
        APIENTRY
        AddConsoleAliasA(
            _In_ LPSTR Source,
            _In_ LPSTR Target,
            _In_ LPSTR ExeName);

    BOOL
        APIENTRY
        AddConsoleAliasW(
            _In_ LPWSTR Source,
            _In_ LPWSTR Target,
            _In_ LPWSTR ExeName);

    DWORD
    APIENTRY
    GetConsoleAliasA(
        _In_ LPSTR Source,
        _Out_writes_(TargetBufferLength) LPSTR TargetBuffer,
        _In_ DWORD TargetBufferLength,
        _In_ LPSTR ExeName);

    DWORD
    APIENTRY
    GetConsoleAliasW(
        _In_ LPWSTR Source,
        _Out_writes_(TargetBufferLength) LPWSTR TargetBuffer,
        _In_ DWORD TargetBufferLength,
        _In_ LPWSTR ExeName);

    BOOL
        WINAPI
        GetCurrentConsoleFont(
            _In_ HANDLE hConsoleOutput,
            _In_ BOOL bMaximumWindow,
            _Out_ PCONSOLE_FONT_INFO lpConsoleCurrentFont);

    BOOL
        WINAPI
        GetCurrentConsoleFontEx(
            _In_ HANDLE hConsoleOutput,
            _In_ BOOL bMaximumWindow,
            _Out_ PCONSOLE_FONT_INFOEX lpConsoleCurrentFontEx);

    BOOL
        WINAPI
        SetCurrentConsoleFontEx(
            _In_ HANDLE hConsoleOutput,
            _In_ BOOL bMaximumWindow,
            _In_ PCONSOLE_FONT_INFOEX lpConsoleCurrentFontEx);

    COORD
    WINAPI
    GetConsoleFontSize(
        _In_ HANDLE hConsoleOutput,
        _In_ DWORD nFont);

    BOOL
        WINAPI
        GetNumberOfConsoleMouseButtons(
            _Out_ LPDWORD lpNumberOfMouseButtons);

    HMENU
    WINAPI
    GetMenu(
        _In_ HWND hWnd);

};
