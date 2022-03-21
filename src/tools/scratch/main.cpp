// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <windows.h>
#include <stdio.h>

#define GS L"\x1D"
#define US L"\x1F"
#define ST L"\x07"

// This wmain exists for help in writing scratch programs while debugging.
int __cdecl wmain(int /*argc*/, WCHAR* /*argv[]*/)
{
    wprintf(L"\x1b]9001;1;"
            L"Foo" GS L"A comment for foo" GS L"echo Foo\r" US L"Bar" GS L"the bar comment" GS L"echo Bar" US
                ST);
    return 0;
}
