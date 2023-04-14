// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <windows.h>
#include <iostream>
#include <string>
#include <psapi.h>

// This wmain exists for help in writing scratch programs while debugging.
int __cdecl wmain(int /*argc*/, WCHAR* /*argv[]*/)
{
    HWND hwnd = GetForegroundWindow();
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess == NULL)
    {
        std::cerr << "Failed to open process." << std::endl;
        return 1;
    }

    wchar_t processName[MAX_PATH];
    if (GetModuleFileNameExW(hProcess, NULL, processName, MAX_PATH) == 0)
    {
        std::cerr << "Failed to get module file name." << std::endl;
        CloseHandle(hProcess);
        return 1;
    }

    wprintf(L"Current process: %s\n", processName);
    while (true)
    {
        HWND hwndNew = GetForegroundWindow();
        if (hwndNew != hwnd)
        {
            hwnd = hwndNew;
            DWORD pidNew;
            GetWindowThreadProcessId(hwnd, &pidNew);

            if (pidNew != pid)
            {
                pid = pidNew;
                CloseHandle(hProcess);
                hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
                if (hProcess == NULL)
                {
                    std::cerr << "Failed to open process." << std::endl;
                    return 1;
                }

                if (GetModuleFileNameExW(hProcess, NULL, processName, MAX_PATH) == 0)
                {
                    std::cerr << "Failed to get module file name." << std::endl;
                    CloseHandle(hProcess);
                    return 1;
                }

                wprintf(L"Current process: %s\n", processName);
            }
        }
        Sleep(100);
    }

    CloseHandle(hProcess);
    return 0;
}
