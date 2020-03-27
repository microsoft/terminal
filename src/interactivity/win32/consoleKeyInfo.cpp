// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "consoleKeyInfo.hpp"

// The following data structures are a hack to work around the fact that
// MapVirtualKey does not return the correct virtual key code in many cases.
// we store the correct info (from the keydown message) in the CONSOLE_KEY_INFO
// structure when a keydown message is translated. Then when we receive a
// wm_[sys][dead]char message, we retrieve it and clear out the record.

#define CONSOLE_FREE_KEY_INFO nullptr
#define CONSOLE_MAX_KEY_INFO 32

typedef struct _CONSOLE_KEY_INFO
{
    HWND hWnd;
    WORD wVirtualKeyCode;
    WORD wVirtualScanCode;
} CONSOLE_KEY_INFO, *PCONSOLE_KEY_INFO;

CONSOLE_KEY_INFO ConsoleKeyInfo[CONSOLE_MAX_KEY_INFO];

void StoreKeyInfo(_In_ PMSG msg)
{
    for (auto& consoleKeyInfo : ConsoleKeyInfo)
    {
        if (consoleKeyInfo.hWnd == CONSOLE_FREE_KEY_INFO || consoleKeyInfo.hWnd == msg->hwnd)
        {
            consoleKeyInfo.hWnd = msg->hwnd;
            consoleKeyInfo.wVirtualKeyCode = LOWORD(msg->wParam);
            consoleKeyInfo.wVirtualScanCode = (BYTE)(HIWORD(msg->lParam));
            return;
        }
    }

    RIPMSG0(RIP_WARNING, "ConsoleKeyInfo buffer is full");
}

void RetrieveKeyInfo(_In_ HWND hWnd, _Out_ PWORD pwVirtualKeyCode, _Inout_ PWORD pwVirtualScanCode, _In_ BOOL FreeKeyInfo)
{
    for (auto& consoleKeyInfo : ConsoleKeyInfo)
    {
        if (consoleKeyInfo.hWnd == hWnd)
        {
            *pwVirtualKeyCode = consoleKeyInfo.wVirtualKeyCode;
            *pwVirtualScanCode = consoleKeyInfo.wVirtualScanCode;
            if (FreeKeyInfo)
            {
                consoleKeyInfo.hWnd = CONSOLE_FREE_KEY_INFO;
            }
            return;
        }
    }
    *pwVirtualKeyCode = (WORD)MapVirtualKeyW(*pwVirtualScanCode, 3);
}

void ClearKeyInfo(const HWND hWnd)
{
    for (auto& consoleKeyInfo : ConsoleKeyInfo)
    {
        if (consoleKeyInfo.hWnd == hWnd)
        {
            consoleKeyInfo.hWnd = CONSOLE_FREE_KEY_INFO;
        }
    }
}
