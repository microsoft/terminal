// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "init.hpp"
#include "resource.h"

#pragma hdrstop

// Routine Description:
// - Ensures the SxS initialization for the process.
void InitSideBySide()
{
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

    ACTCTXW actctx{};
    actctx.cbSize = sizeof(actctx);
    // We set ACTCTX_FLAG_HMODULE_VALID, but leave hModule as nullptr.
    // A nullptr HMODULE refers to the current process/executable.
    actctx.lpResourceName = MAKEINTRESOURCE(IDR_SYSTEM_MANIFEST);
    actctx.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID | ACTCTX_FLAG_SET_PROCESS_DEFAULT | ACTCTX_FLAG_HMODULE_VALID;

    const auto hActCtx = CreateActCtxW(&actctx);

    // The error value is INVALID_HANDLE_VALUE.
    // ACTCTX_FLAG_SET_PROCESS_DEFAULT has nothing to return upon success, so it returns nullptr.
    // There is nothing to cleanup upon ACTCTX_FLAG_SET_PROCESS_DEFAULT success, the data
    // is referenced in the PEB, and lasts till process shutdown.
    if (hActCtx == INVALID_HANDLE_VALUE)
    {
        const auto error = GetLastError();

        // OpenConsole ships with a single manifest at ID 1, while conhost ships with 2 at ID 1
        // and IDR_SYSTEM_MANIFEST. If we call CreateActCtxW() with IDR_SYSTEM_MANIFEST inside
        // OpenConsole anyways, nothing happens and we get ERROR_SXS_PROCESS_DEFAULT_ALREADY_SET.
        if (ERROR_SXS_PROCESS_DEFAULT_ALREADY_SET != error)
        {
            RIPMSG1(RIP_WARNING, "InitSideBySide failed create an activation context. Error: %d\r\n", error);
        }
    }
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
    InitSideBySide();
}
