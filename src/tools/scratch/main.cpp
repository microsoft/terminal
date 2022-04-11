// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <windows.h>
#include <iostream>

// {fmt}, a C++20-compatible formatting library
#include <fmt/format.h>
#include <fmt/compile.h>

// This wmain exists for help in writing scratch programs while debugging.
int __cdecl wmain(int argc, WCHAR* /*argv[]*/)
{
    std::cout << "Args: " << argc << std::endl;

    auto consoleHwnd = GetConsoleWindow();

    const auto consoleAncestor_PARENT{ GetAncestor(consoleHwnd, GA_PARENT) };
    const auto consoleAncestor_ROOT{ GetAncestor(consoleHwnd, GA_ROOT) };
    const auto consoleAncestor_ROOTOWNER{ GetAncestor(consoleHwnd, GA_ROOTOWNER) };
    wprintf(fmt::format(L"Ancestor_PARENT: {0:#010x}\n", reinterpret_cast<unsigned long long>(consoleAncestor_PARENT)).c_str());
    wprintf(fmt::format(L"Ancestor_ROOT: {0:#010x}\n", reinterpret_cast<unsigned long long>(consoleAncestor_ROOT)).c_str());
    wprintf(fmt::format(L"Ancestor_ROOTOWNER: {0:#010x}\n", reinterpret_cast<unsigned long long>(consoleAncestor_ROOTOWNER)).c_str());

    const auto terminalHwnd = consoleAncestor_ROOT;
    wprintf(fmt::format(L"consoleHwnd: {0:#010x}\n", reinterpret_cast<unsigned long long>(consoleHwnd)).c_str());
    wprintf(fmt::format(L"terminalHwnd: {0:#010x}\n", reinterpret_cast<unsigned long long>(terminalHwnd)).c_str());

    ::SetLastError(S_OK);

    auto logStyles = [&]() {
        wprintf(fmt::format(L"console:\t{0:#010x}\n", GetWindowLong(consoleHwnd, GWL_STYLE)).c_str());
    };
    logStyles();
    printf("\x1b[39;1mHide window...\x1b[m\n");
    Sleep(200);
    ::ShowWindow(consoleHwnd, SW_HIDE);
    Sleep(1000);

    logStyles();
    printf("\x1b[39;1mNormal window...\x1b[m\n");
    Sleep(200);
    ::ShowWindow(consoleHwnd, SW_NORMAL);
    Sleep(1000);

    logStyles();
    printf("\x1b[39;1mHide window...\x1b[m\n");
    Sleep(200);
    ::ShowWindow(consoleHwnd, SW_HIDE);
    Sleep(1000);

    logStyles();
    printf("\x1b[39;1mShow window...\x1b[m\n");
    Sleep(200);
    ::ShowWindow(consoleHwnd, SW_SHOW);
    Sleep(1000);

    logStyles();
    printf("\x1b[39;1mBack to normal window...\x1b[m\n");
    Sleep(200);
    ::ShowWindow(consoleHwnd, SW_NORMAL);
    Sleep(1000);

    logStyles();

    return 0;
}
