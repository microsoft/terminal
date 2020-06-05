// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "OneCoreDelay.hpp"

BOOLEAN __stdcall OneCoreDelay::IsIsWindowPresent()
{
#ifdef __INSIDE_WINDOWS
    return ::IsIsWindowPresent();
#else
    return true;
#endif
}

BOOLEAN __stdcall OneCoreDelay::IsGetSystemMetricsPresent()
{
#ifdef __INSIDE_WINDOWS
    return ::IsGetSystemMetricsPresent();
#else
    return true;
#endif
}

BOOLEAN __stdcall OneCoreDelay::IsPostMessageWPresent()
{
#ifdef __INSIDE_WINDOWS
    return ::IsPostMessageWPresent();
#else
    return true;
#endif
}

BOOLEAN __stdcall OneCoreDelay::IsSendMessageWPresent()
{
#ifdef __INSIDE_WINDOWS
    return ::IsSendMessageWPresent();
#else
    return true;
#endif
}

HMODULE GetUser32()
{
    static HMODULE _hUser32 = nullptr;
    if (_hUser32 == nullptr)
    {
        _hUser32 = LoadLibraryExW(L"user32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    }

    return _hUser32;
}

HMODULE GetKernel32()
{
    static HMODULE _hKernel32 = nullptr;
    if (_hKernel32 == nullptr)
    {
        _hKernel32 = LoadLibraryExW(L"kernel32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    }

    return _hKernel32;
}

BOOL APIENTRY OneCoreDelay::AddConsoleAliasA(
    _In_ LPSTR Source,
    _In_ LPSTR Target,
    _In_ LPSTR ExeName)
{
    HMODULE h = GetKernel32();

    if (h != nullptr)
    {
        typedef BOOL(WINAPI * PfnAddConsoleAliasA)(LPSTR Source, LPSTR Target, LPSTR ExeName);

        static PfnAddConsoleAliasA pfn = nullptr;

        if (pfn == nullptr)
        {
            pfn = (PfnAddConsoleAliasA)GetProcAddress(h, "AddConsoleAliasA");
        }

        if (pfn != nullptr)
        {
            return pfn(Source, Target, ExeName);
        }
    }

    return FALSE;
}

BOOL APIENTRY OneCoreDelay::AddConsoleAliasW(
    _In_ LPWSTR Source,
    _In_ LPWSTR Target,
    _In_ LPWSTR ExeName)
{
    HMODULE h = GetKernel32();

    if (h != nullptr)
    {
        typedef BOOL(WINAPI * PfnAddConsoleAliasW)(LPWSTR Source, LPWSTR Target, LPWSTR ExeName);

        static PfnAddConsoleAliasW pfn = nullptr;

        if (pfn == nullptr)
        {
            pfn = (PfnAddConsoleAliasW)GetProcAddress(h, "AddConsoleAliasW");
        }

        if (pfn != nullptr)
        {
            return pfn(Source, Target, ExeName);
        }
    }

    return FALSE;
}

DWORD APIENTRY OneCoreDelay::GetConsoleAliasA(
    _In_ LPSTR Source,
    _Out_writes_(TargetBufferLength) LPSTR TargetBuffer,
    _In_ DWORD TargetBufferLength,
    _In_ LPSTR ExeName)
{
    HMODULE h = GetKernel32();

    if (h != nullptr)
    {
        typedef BOOL(WINAPI * PfnGetConsoleAliasA)(LPSTR Source, LPSTR TargetBuffer, DWORD TargetBufferLength, LPSTR ExeName);

        static PfnGetConsoleAliasA pfn = nullptr;

        if (pfn == nullptr)
        {
            pfn = (PfnGetConsoleAliasA)GetProcAddress(h, "GetConsoleAliasA");
        }

        if (pfn != nullptr)
        {
            return pfn(Source, TargetBuffer, TargetBufferLength, ExeName);
        }
    }

    return FALSE;
}

DWORD APIENTRY OneCoreDelay::GetConsoleAliasW(
    _In_ LPWSTR Source,
    _Out_writes_(TargetBufferLength) LPWSTR TargetBuffer,
    _In_ DWORD TargetBufferLength,
    _In_ LPWSTR ExeName)
{
    HMODULE h = GetKernel32();

    if (h != nullptr)
    {
        typedef BOOL(WINAPI * PfnGetConsoleAliasW)(LPWSTR Source, LPWSTR TargetBuffer, DWORD TargetBufferLength, LPWSTR ExeName);

        static PfnGetConsoleAliasW pfn = nullptr;

        if (pfn == nullptr)
        {
            pfn = (PfnGetConsoleAliasW)GetProcAddress(h, "GetConsoleAliasW");
        }

        if (pfn != nullptr)
        {
            return pfn(Source, TargetBuffer, TargetBufferLength, ExeName);
        }
    }

    return FALSE;
}

BOOL WINAPI OneCoreDelay::GetCurrentConsoleFont(
    _In_ HANDLE hConsoleOutput,
    _In_ BOOL bMaximumWindow,
    _Out_ PCONSOLE_FONT_INFO lpConsoleCurrentFont)
{
    HMODULE h = GetKernel32();

    if (h != nullptr)
    {
        typedef BOOL(WINAPI * PfnGetCurrentConsoleFont)(HANDLE hConsoleOutput, BOOL bMaximumWindow, PCONSOLE_FONT_INFO lpConsoleCurrentFont);

        static PfnGetCurrentConsoleFont pfn = nullptr;

        if (pfn == nullptr)
        {
            pfn = (PfnGetCurrentConsoleFont)GetProcAddress(h, "GetCurrentConsoleFont");
        }

        if (pfn != nullptr)
        {
            return pfn(hConsoleOutput, bMaximumWindow, lpConsoleCurrentFont);
        }
    }

    return FALSE;
}

BOOL WINAPI OneCoreDelay::GetCurrentConsoleFontEx(
    _In_ HANDLE hConsoleOutput,
    _In_ BOOL bMaximumWindow,
    _Out_ PCONSOLE_FONT_INFOEX lpConsoleCurrentFontEx)
{
    HMODULE h = GetKernel32();

    if (h != nullptr)
    {
        typedef BOOL(WINAPI * PfnGetCurrentConsoleFontEx)(HANDLE hConsoleOutput, BOOL bMaximumWindow, PCONSOLE_FONT_INFOEX lpConsoleCurrentFontEx);

        static PfnGetCurrentConsoleFontEx pfn = nullptr;

        if (pfn == nullptr)
        {
            pfn = (PfnGetCurrentConsoleFontEx)GetProcAddress(h, "GetCurrentConsoleFontEx");
        }

        if (pfn != nullptr)
        {
            return pfn(hConsoleOutput, bMaximumWindow, lpConsoleCurrentFontEx);
        }
    }

    return FALSE;
}

BOOL WINAPI OneCoreDelay::SetCurrentConsoleFontEx(
    _In_ HANDLE hConsoleOutput,
    _In_ BOOL bMaximumWindow,
    _In_ PCONSOLE_FONT_INFOEX lpConsoleCurrentFontEx)
{
    HMODULE h = GetKernel32();

    if (h != nullptr)
    {
        typedef BOOL(WINAPI * PfnSetCurrentConsoleFontEx)(HANDLE hConsoleOutput, BOOL bMaximumWindow, PCONSOLE_FONT_INFOEX lpConsoleCurrentFontEx);

        static PfnSetCurrentConsoleFontEx pfn = nullptr;

        if (pfn == nullptr)
        {
            pfn = (PfnSetCurrentConsoleFontEx)GetProcAddress(h, "SetCurrentConsoleFontEx");
        }

        if (pfn != nullptr)
        {
            return pfn(hConsoleOutput, bMaximumWindow, lpConsoleCurrentFontEx);
        }
    }

    return FALSE;
}

COORD WINAPI OneCoreDelay::GetConsoleFontSize(
    _In_ HANDLE hConsoleOutput,
    _In_ DWORD nFont)
{
    HMODULE h = GetKernel32();

    if (h != nullptr)
    {
        typedef COORD(WINAPI * PfnGetConsoleFontSize)(HANDLE hConsoleOutput, DWORD nFont);

        static PfnGetConsoleFontSize pfn = nullptr;

        if (pfn == nullptr)
        {
            pfn = (PfnGetConsoleFontSize)GetProcAddress(h, "GetConsoleFontSize");
        }

        if (pfn != nullptr)
        {
            return pfn(hConsoleOutput, nFont);
        }
    }

    return { 0 };
}

BOOL WINAPI OneCoreDelay::GetNumberOfConsoleMouseButtons(
    _Out_ LPDWORD lpNumberOfMouseButtons)
{
    HMODULE h = GetKernel32();

    if (h != nullptr)
    {
        typedef BOOL(WINAPI * PfnGetNumberOfConsoleMouseButtons)(LPDWORD lpNumberOfMouseButtons);

        static PfnGetNumberOfConsoleMouseButtons pfn = nullptr;

        if (pfn == nullptr)
        {
            pfn = (PfnGetNumberOfConsoleMouseButtons)GetProcAddress(h, "GetNumberOfConsoleMouseButtons");
        }

        if (pfn != nullptr)
        {
            return pfn(lpNumberOfMouseButtons);
        }
    }

    return FALSE;
}

HMENU WINAPI OneCoreDelay::GetMenu(
    _In_ HWND hWnd)
{
    HMODULE h = GetUser32();

    if (h != nullptr)
    {
        typedef HMENU(WINAPI * PfnGetMenu)(HWND hWnd);

        static PfnGetMenu pfn = nullptr;

        if (pfn == nullptr)
        {
            pfn = (PfnGetMenu)GetProcAddress(h, "GetMenu");
        }

        if (pfn != nullptr)
        {
            return pfn(hWnd);
        }
    }

    return nullptr;
}
