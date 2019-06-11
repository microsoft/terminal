// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "init.hpp"
#include "resource.h"

#pragma hdrstop

// Routine Description:
// - Ensures the SxS initialization for the process.
void InitSideBySide(_Out_writes_(ScratchBufferSize) PWSTR ScratchBuffer, __range(MAX_PATH, MAX_PATH) DWORD ScratchBufferSize)
{
    ACTCTXW actctx = { 0 };

    // Account for the fact that sidebyside stuff happens in CreateProcess
    // but conhost is run with RtlCreateUserProcess.

    // If conhost is at some future date
    // launched with CreateProcess or SideBySide support moved
    // into the kernel and SideBySide setup moved to textmode, at which
    // time this code block will not be needed.

    // Until then, this code block is needed when activated as the default console in the OS by the loader.
    // If the console is changed to be invoked a different way (for example if we add a main method that takes
    // a parameter to a client application instead), then this code would be unnecessary but not likely harmful.

    // Having SxS not initialized is a problem when 3rd party IMEs attempt to inject into the process and then
    // make references to DLLs in the system that are in the SxS cache (ex. a 3rd party IME is loaded and asks for
    // comctl32.dll. The load will fail if SxS wasn't initialized.) This was bug# WIN7:681280.

    // We look at the first few chars without being careful about a terminal nul, so init them.
    ScratchBuffer[0] = 0;
    ScratchBuffer[1] = 0;
    ScratchBuffer[2] = 0;
    ScratchBuffer[3] = 0;
    ScratchBuffer[4] = 0;
    ScratchBuffer[5] = 0;
    ScratchBuffer[6] = 0;

    // GetModuleFileNameW truncates its result to fit in the buffer, so to detect if we fit, we have to do this.
    ScratchBuffer[ScratchBufferSize - 2] = 0;
    DWORD const dwModuleFileNameLength = GetModuleFileNameW(nullptr, ScratchBuffer, ScratchBufferSize);
    if (dwModuleFileNameLength == 0)
    {
        RIPMSG1(RIP_ERROR, "GetModuleFileNameW failed %d.\n", GetLastError());
        goto Exit;
    }
    if (ScratchBuffer[ScratchBufferSize - 2] != 0)
    {
        RIPMSG1(RIP_ERROR, "GetModuleFileNameW requires more than ScratchBufferSize(%d) - 1.\n", ScratchBufferSize);
        goto Exit;
    }

    // We get an NT path from the Win32 api. Fix it to be Win32.
    UINT NtToWin32PathOffset = 0;
    if (ScratchBuffer[0] == '\\' && ScratchBuffer[1] == '?' && ScratchBuffer[2] == '?' && ScratchBuffer[3] == '\\'
        //&& ScratchBuffer[4] == a drive letter
        && ScratchBuffer[5] == ':' && ScratchBuffer[6] == '\\')
    {
        NtToWin32PathOffset = 4;
    }

    actctx.cbSize = sizeof(actctx);
    actctx.dwFlags = (ACTCTX_FLAG_RESOURCE_NAME_VALID | ACTCTX_FLAG_SET_PROCESS_DEFAULT);
    actctx.lpResourceName = MAKEINTRESOURCE(IDR_SYSTEM_MANIFEST);
    actctx.lpSource = ScratchBuffer + NtToWin32PathOffset;

    HANDLE const hActCtx = CreateActCtxW(&actctx);

    // The error value is INVALID_HANDLE_VALUE.
    // ACTCTX_FLAG_SET_PROCESS_DEFAULT has nothing to return upon success, so it returns nullptr.
    // There is nothing to cleanup upon ACTCTX_FLAG_SET_PROCESS_DEFAULT success, the data
    // is referenced in the PEB, and lasts till process shutdown.
    if (hActCtx == INVALID_HANDLE_VALUE)
    {
        auto const error = GetLastError();

        // Don't log if it's already set. This whole ordeal is to make sure one is set if there isn't one already.
        // If one is already set... good!
        if (ERROR_SXS_PROCESS_DEFAULT_ALREADY_SET != error)
        {
            RIPMSG1(RIP_WARNING, "InitSideBySide failed create an activation context. Error: %d\r\n", error);
        }
        goto Exit;
    }

Exit:
    ScratchBuffer[0] = 0;
}

// Routine Description:
// - Sets the program files environment variables for the process, if missing.
void InitEnvironmentVariables()
{
    struct
    {
        LPCWSTR szRegValue;
        LPCWSTR szVariable;
    } EnvProgFiles[] = {
        { L"ProgramFilesDir", L"ProgramFiles" },
        { L"CommonFilesDir", L"CommonProgramFiles" },
#if BUILD_WOW64_ENABLED
        { L"ProgramFilesDir (x86)", L"ProgramFiles(x86)" },
        { L"CommonFilesDir (x86)", L"CommonProgramFiles(x86)" },
        { L"ProgramW6432Dir", L"ProgramW6432" },
        { L"CommonW6432Dir", L"CommonProgramW6432" }
#endif
    };

    WCHAR wchValue[MAX_PATH];
    for (UINT i = 0; i < ARRAYSIZE(EnvProgFiles); i++)
    {
        if (!GetEnvironmentVariable(EnvProgFiles[i].szVariable, nullptr, 0))
        {
            DWORD dwMaxBufferSize = sizeof(wchValue);
            if (RegGetValue(HKEY_LOCAL_MACHINE,
                            L"Software\\Microsoft\\Windows\\CurrentVersion",
                            EnvProgFiles[i].szRegValue,
                            RRF_RT_REG_SZ,
                            nullptr,
                            (LPBYTE)wchValue,
                            &dwMaxBufferSize) == ERROR_SUCCESS)
            {
                wchValue[(dwMaxBufferSize / sizeof(wchValue[0])) - 1] = 0;
                SetEnvironmentVariable(EnvProgFiles[i].szVariable, wchValue);
            }
        }
    }

    // Initialize SxS for the process.
    InitSideBySide(wchValue, ARRAYSIZE(wchValue));
}
