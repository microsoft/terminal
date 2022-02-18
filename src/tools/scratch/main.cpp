// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <windows.h>
#include <iostream>

// This wmain exists for help in writing scratch programs while debugging.
int __cdecl wmain(int argc, WCHAR* /*argv[]*/)
{
    std::cout << "Args: " << argc << std::endl;
    DWORD verb = argc - 1;

    auto hwnd = GetConsoleWindow();
    ::SetLastError(S_OK);
    auto retVal = ShowWindow(hwnd, verb);
    auto lastError = ::GetLastError();
    std::cout << "Returned: " << retVal << " with error code " << lastError <<std::endl;
    return 0;
}
