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
#include <conio.h>
#include <deque>
#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include <assert.h>

#define ENABLE_PASSTHROUGH_MODE 0x0020

std::string csi(std::string seq)
{
    std::string fullSeq = "\x1b[";
    fullSeq += seq;
    return fullSeq;
}

std::string osc(std::string seq)
{
    std::string fullSeq = "\x1b]";
    fullSeq += seq;
    fullSeq += "\x7";
    return fullSeq;
}

// This wmain exists for help in writing scratch programs while debugging.
int __cdecl wmain(int /*argc*/, WCHAR* /*argv[]*/)
{
    wprintf(L"Attempting to start passthrough mode...\n");
    auto hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD dwMode = 0;
    THROW_LAST_ERROR_IF(!GetConsoleMode(hOut, &dwMode));

    wprintf(L"Original Mode: 0x%x\n", dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    dwMode |= DISABLE_NEWLINE_AUTO_RETURN;
    dwMode |= ENABLE_PASSTHROUGH_MODE;

    wprintf(L"Requested Mode: 0x%x\n", dwMode);
    THROW_LAST_ERROR_IF(!SetConsoleMode(hOut, dwMode));

    DWORD roundtripMode = 0;
    THROW_LAST_ERROR_IF(!GetConsoleMode(hOut, &roundtripMode));
    wprintf(L"Rountripped Mode: 0x%x\n", dwMode);

    if (roundtripMode != dwMode)
    {
        wprintf(L"Mode did not rountrip\n");
    }
    else
    {
        wprintf(L"Mode rountripped successfully\n");
    }

    wprintf(L"Press a key to continue\n");
    _getch();

    wprintf(L"We're going to write some VT straight to the terminal\n");

    printf(csi("31m").c_str());
    printf(osc("0;Title:foo").c_str());
    wprintf(L"Press a key to continue\n");
    _getch();

    printf(csi("0m").c_str());
    wprintf(L"Time for something more complicated...\n");
    Sleep(500);
    printf(csi("2;1H").c_str());
    printf(csi("44m").c_str());
    printf(csi("K").c_str());
    Sleep(500);

    printf(csi("9;1H").c_str());
    printf(csi("46m").c_str());
    printf(csi("K").c_str());
    Sleep(500);

    printf(csi("3;8r").c_str());
    printf(csi("3;1H").c_str());
    printf(csi("0m").c_str());
    Sleep(500);

    for (int i = 0; i < 10; i++)
    {
        wprintf(L"Print in the margins %d\n", i);
        Sleep(500);
    }

    printf(csi("r").c_str());
    wprintf(L"Press a key to continue\n");
    _getch();

    return 0;
}
