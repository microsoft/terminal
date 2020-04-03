// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <windows.h>
#include <wil\Common.h>
#include <wil\result.h>
#include <wil\resource.h>
#include <wil\wistd_functional.h>
#include <wil\wistd_memory.h>
#include <stdlib.h> /* srand, rand */
#include <time.h> /* time */

#include <deque>
#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include <assert.h>

// This wmain exists for help in writing scratch programs while debugging.
int __cdecl wmain(int /*argc*/, WCHAR* /*argv[]*/)
{
    auto hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    {
        DWORD dwMode = 0;
        THROW_LAST_ERROR_IF(!GetConsoleMode(hOut, &dwMode));
        const auto originalMode = dwMode;
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        // dwMode |= DISABLE_NEWLINE_AUTO_RETURN;
        THROW_LAST_ERROR_IF(!SetConsoleMode(hOut, dwMode));
        wprintf(L"\x1b[2J");
        wprintf(L"\x1b[H");
        THROW_LAST_ERROR_IF(!SetConsoleMode(hOut, originalMode));
    }

    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    bool fSuccess = !!GetConsoleScreenBufferInfoEx(hOut, &csbiex);
    if (!fSuccess)
        return -1;

    SMALL_RECT srViewport = csbiex.srWindow;

    const unsigned short width = srViewport.Right - srViewport.Left + 1;
    const unsigned short height = srViewport.Bottom - srViewport.Top + 1;

    // CONSOLE_CURSOR_INFO cursorInfo{};
    // fSuccess = GetConsoleCursorInfo(hOut, &cursorInfo);
    // if (!fSuccess) return -1;

    auto restoreCursor = wil::scope_exit([&]() {
        const COORD adjusted{ 0, csbiex.dwCursorPosition.Y + 1 };
        SetConsoleCursorPosition(hOut, adjusted);
    });

    const auto originalBottom = srViewport.Bottom;

    // {

    //     COORD nearBottom{ 0, height - 3 };
    //     SetConsoleCursorPosition(hOut, nearBottom);
    //     wprintf(L"AAAAAAAAAAAAAAAAAAAA");
    //     wprintf(L"\n");
    //     wprintf(L"BBBBBBBBBBBBBBBBBBBB");
    //     wprintf(L"\n");
    //     wprintf(L"CCCCCCCCCCCCCCCCCCCC");
    //     Sleep(1000);

    //     CHAR_INFO clear;
    //     clear.Char.UnicodeChar = L' ';
    //     clear.Attributes = csbiex.wAttributes;

    // SMALL_RECT src;
    // src.Top = 1;
    // src.Left = 0;
    // src.Right = width;
    // src.Bottom = height - 3;
    // COORD tgt = { 0, 0 };
    // ScrollConsoleScreenBuffer(hOut, &src, nullptr, tgt, &clear);

    //     // Sleep(1000);

    //     COORD statusLine{ 0, height - 2 };
    //     SetConsoleCursorPosition(hOut, statusLine);

    //     wprintf(L"D---");
    //     wprintf(L"\n");
    //     wprintf(L"E---");

    //     Sleep(1000);

    // }

    {
        COORD nearBottom{ 0, originalBottom - 2 };
        SetConsoleCursorPosition(hOut, nearBottom);
        wprintf(L"AAAAAAAAAAAAAAAAAAAA");
        wprintf(L"\n");
        wprintf(L"BBBBBBBBBBBBBBBBBBBB");
        wprintf(L"\n");
        wprintf(L"CCCCCCCCCCCCCCCCCCCC");
        Sleep(1000);
        // COORD atBottom{ 0, height };
        // SetConsoleCursorPosition(hOut, nearBottom);
        wprintf(L"\n");
        const short newBottom = originalBottom + 1;
        Sleep(1000);

        CHAR_INFO clear;
        clear.Char.UnicodeChar = L' ';
        clear.Attributes = csbiex.wAttributes;

        SMALL_RECT src;
        src.Top = originalBottom - 1;
        src.Left = 0;
        src.Right = width;
        src.Bottom = originalBottom;
        COORD tgt = { 0, newBottom - 1 };
        ScrollConsoleScreenBuffer(hOut, &src, nullptr, tgt, &clear);

        Sleep(1000);
        COORD statusLine{ 0, newBottom - 1 };
        SetConsoleCursorPosition(hOut, statusLine);

        wprintf(L"D---");
        wprintf(L"\n");
        wprintf(L"E---");
        Sleep(1000);
    }

    return 0;
}
