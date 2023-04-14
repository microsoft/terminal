// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <windows.h>
#include <iostream>
#include <string>
#include <psapi.h>

BOOL QueryWindowFullProcessImageName(
    HWND hwnd,
    DWORD dwFlags,
    PWSTR lpExeName,
    DWORD dwSize)
{
    DWORD pid = 0;
    BOOL fRc = FALSE;
    if (GetWindowThreadProcessId(hwnd, &pid))
    {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess)
        {
            fRc = QueryFullProcessImageName(hProcess, dwFlags, lpExeName, &dwSize);
            CloseHandle(hProcess);
        }
    }
    return fRc;
}

void CALLBACK WinEventProc(
    HWINEVENTHOOK /*hWinEventHook*/,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD /*dwEventThread*/,
    DWORD /*dwmsEventTime*/)
{
    wprintf(L"got event %d!\n", event);

    if (event == EVENT_SYSTEM_FOREGROUND &&
        idObject == OBJID_WINDOW &&
        idChild == CHILDID_SELF)
    {
        PCWSTR pszMsg;
        WCHAR szBuf[MAX_PATH];
        if (hwnd)
        {
            DWORD cch = ARRAYSIZE(szBuf);
            if (QueryWindowFullProcessImageName(hwnd, 0, szBuf, cch))
            {
                pszMsg = szBuf;
            }
            else
            {
                pszMsg = L"<unknown>";
            }
        }
        else
        {
            pszMsg = L"<none>";
        }

        wprintf(pszMsg);
        wprintf(L"\n");
    }
}

int theOldNewThingWay()
{
    wprintf(L"Setting up event hook\n");

    HWINEVENTHOOK hWinEventHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND,
                                                  EVENT_SYSTEM_FOREGROUND,
                                                  NULL,
                                                  WinEventProc,
                                                  0,
                                                  0,
                                                  WINEVENT_OUTOFCONTEXT);

    if (!hWinEventHook)
        exit(GetLastError());
    HWINEVENTHOOK focusHook = SetWinEventHook(EVENT_OBJECT_FOCUS,
                                              EVENT_OBJECT_FOCUS,
                                              NULL,
                                              WinEventProc,
                                              0,
                                              0,
                                              WINEVENT_OUTOFCONTEXT);

    if (!focusHook)
        exit(GetLastError());

    wprintf(L"Press Ctrl+D to exit!\n");

    auto hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD events;
    INPUT_RECORD inputRecord;
    while (true)
    {
        if (!ReadConsoleInput(hIn, &inputRecord, 1, &events))
        {
            exit(GetLastError());
        }

        if (inputRecord.EventType == KEY_EVENT &&
            inputRecord.Event.KeyEvent.wVirtualKeyCode == 'D' &&
            inputRecord.Event.KeyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
        {
            // Ctrl+D pressed
            wprintf(L"Ctrl+D pressed, exiting!\n");
            break;
        }
    }

    if (hWinEventHook)
        UnhookWinEvent(hWinEventHook);
    if (focusHook)
        UnhookWinEvent(focusHook);
    return 0;
}

int theChatGptWay()
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
// This wmain exists for help in writing scratch programs while debugging.
int __cdecl wmain(int /*argc*/, WCHAR* /*argv[]*/)
{
    return theChatGptWay();
}
