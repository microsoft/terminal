// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <precomp.h>
#include <windows.h>
#include <wincon.h>

int TestSetViewport(HANDLE hIn, HANDLE hOut);
int TestGetchar(HANDLE hIn, HANDLE hOut);

int __cdecl wmain(int /*argc*/, WCHAR* /*argv[]*/)
{
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn == INVALID_HANDLE_VALUE)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    TestGetchar(hIn, hOut);

    return 0;
}

int TestSetViewport(HANDLE /*hIn*/, HANDLE hOut)
{
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    bool fSuccess = GetConsoleScreenBufferInfoEx(hOut, &csbiex);
    if (fSuccess)
    {
        const SMALL_RECT Screen = csbiex.srWindow;
        const short sWidth = Screen.Right - Screen.Left;
        const short sHeight = Screen.Bottom - Screen.Top;

        csbiex.srWindow.Top = 50;
        csbiex.srWindow.Bottom = sHeight + 50;
        csbiex.srWindow.Left = 0;
        csbiex.srWindow.Right = sWidth;

        SetConsoleScreenBufferInfoEx(hOut, &csbiex);
    }
    return 0;
}

int TestGetchar(HANDLE hIn, HANDLE /*hOut*/)
{
    DWORD dwInputModes;
    if (!GetConsoleMode(hIn, &dwInputModes))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    DWORD const dwEnableVirtualTerminalInput = 0x200; // Until the new wincon.h is published
    if (!SetConsoleMode(hIn, dwInputModes | dwEnableVirtualTerminalInput))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    while (hIn != nullptr)
    {
        int ch = getchar();
        printf("0x%x\r\n", ch);
    }

    return 0;
}
