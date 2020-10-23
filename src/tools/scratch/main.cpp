// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <windows.h>
#include <stdio.h>

// This wmain exists for help in writing scratch programs while debugging.
int __cdecl wmain(int /*argc*/, WCHAR* /*argv[]*/)
{
    unsigned char buf[256];
    DWORD len;
    int i;
    auto in = GetStdHandle(STD_INPUT_HANDLE);
    auto out = GetStdHandle(STD_OUTPUT_HANDLE);

    const wchar_t alpha = L'\u03b1';

    DWORD mode;
    GetConsoleMode(in, &mode);
    //mode &= ~ENABLE_LINE_INPUT;
    SetConsoleMode(in, mode);

    SetConsoleCP(932);
    SetConsoleOutputCP(437);

    printf("Input CP %d\n", GetConsoleCP());
    printf("Output CP %d\n", GetConsoleOutputCP());

    GetConsoleMode(in, &mode);
    printf("Input Mode %02x\n", mode);
    GetConsoleMode(out, &mode);
    printf("Output Mode %02x\n", mode);

    CONSOLE_FONT_INFOEX font = { 0 };
    font.cbSize = sizeof(font);
    BOOL retval = GetCurrentConsoleFontEx(out, FALSE, &font);
    DWORD err = GetLastError();

    retval;
    err;

    //mode &= ~ENABLE_LINE_INPUT;
    //SetConsoleMode(in, mode);

    while (true)
    {
        ReadFile(in, buf, sizeof(buf), &len, NULL);
        for (i = 0; i < (int)len; i++)
            printf("%02x ", buf[i]);
        printf("\n");
    }
    return 0;
}
