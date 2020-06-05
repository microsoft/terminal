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

using namespace std;
////////////////////////////////////////////////////////////////////////////////
// State
HANDLE hOut;
HANDLE hIn;

std::string csi(std::string seq)
{
    std::string fullSeq = "\x1b[";
    fullSeq += seq;
    return fullSeq;
}

void printCSI(std::string seq)
{
    printf("%s", csi(seq).c_str()); // save cursor
}

void printCUP(int x, int y)
{
    printf("\x1b[%d;%dH", y + 1, x + 1); // save cursor
}

void print256color(int bg)
{
    printf("\x1b[48;5;%dm", bg); // save cursor
}

// bin\x64\Debug\buffersize.exe
int __cdecl wmain(int /*argc*/, WCHAR* /*argv[]*/)
{
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    hIn = GetStdHandle(STD_INPUT_HANDLE);

    DWORD dwMode = 0;
    THROW_LAST_ERROR_IF(!GetConsoleMode(hOut, &dwMode));
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    dwMode |= DISABLE_NEWLINE_AUTO_RETURN;
    // THROW_LAST_ERROR_IF_FALSE(SetConsoleMode(hOut, dwMode));
    SetConsoleMode(hOut, dwMode);
    // the resize event doesn't actually have the info we want.
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    bool fSuccess = !!GetConsoleScreenBufferInfoEx(hOut, &csbiex);
    if (fSuccess)
    {
        SMALL_RECT srViewport = csbiex.srWindow;
        short width = srViewport.Right - srViewport.Left + 1;
        short height = srViewport.Bottom - srViewport.Top + 1;

        std::string topBorder = std::string(width, '-');
        std::string bottomBorder = std::string(width, '=');

        int color = 17;
        int const colorStep = 1;
        printf("Buffer size is wxh=%dx%d\n", width, height);
        printCSI("s"); // save cursor
        printCSI("H"); // Go Home
        print256color(color);
        color += colorStep;
        printf("%s", topBorder.c_str());
        printCUP(0, height - 1);
        print256color(color);
        color += colorStep;
        printf("%s", bottomBorder.c_str());

        for (int y = 1; y < height - 1; y++)
        {
            printCUP(0, y);
            print256color(color);
            color += colorStep;
            printf("L");

            printCUP(width - 1, y);
            print256color(color);
            color += colorStep;
            printf("R\n");
        }

        printCSI("u"); // restore cursor
        printCSI("m"); // restore color
    }

    return 0;
}
