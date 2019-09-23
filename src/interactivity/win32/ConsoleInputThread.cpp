// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ConsoleInputThread.hpp"

#include "WindowIo.hpp"

using namespace Microsoft::Console::Interactivity::Win32;

// Routine Description:
// - Starts the Win32-specific console input thread.
HANDLE ConsoleInputThread::Start()
{
    HANDLE hThread = nullptr;
    DWORD dwThreadId = (DWORD)-1;

    hThread = CreateThread(nullptr,
                           0,
                           ConsoleInputThreadProcWin32,
                           nullptr,
                           0,
                           &dwThreadId);

    if (hThread)
    {
        _hThread = hThread;
        _dwThreadId = dwThreadId;
    }

    return hThread;
}
