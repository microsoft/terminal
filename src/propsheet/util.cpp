// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "shlwapi.h"

// Version detection code for ComCtl32 copied/adapter from
// https://msdn.microsoft.com/en-us/library/windows/desktop/hh298349(v=vs.85).aspx#DllGetVersion

#define PACKVERSION(major, minor) MAKELONG(minor, major)

static DWORD GetVersion(_In_ PCWSTR pwszDllName)
{
    DWORD dwVersion = 0;

    // We have to call for ComCtl32.dll without a path name so Fusion SxS will redirect us
    // if it thinks we are manifested properly and Fusion is enabled in this process space.
    HINSTANCE const hinstDll = LoadLibrary(pwszDllName);

    if (nullptr != hinstDll)
    {
        DLLGETVERSIONPROC const pDllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(hinstDll, "DllGetVersion");

        // Because some DLLs might not implement this function, you must test for
        // it explicitly. Depending on the particular DLL, the lack of a DllGetVersion
        // function can be a useful indicator of the version.

        if (nullptr != pDllGetVersion)
        {
            DLLVERSIONINFO dvi = { 0 };
            dvi.cbSize = sizeof(dvi);

            if (SUCCEEDED((*pDllGetVersion)(&dvi)))
            {
                dwVersion = PACKVERSION(dvi.dwMajorVersion, dvi.dwMinorVersion);
            }
        }

        FreeLibrary(hinstDll);
    }
    return dwVersion;
}

static bool IsComCtlV6Present()
{
    PCWSTR pwszDllName = L"ComCtl32.dll";
    DWORD const dwVer = GetVersion(pwszDllName);
    DWORD const dwTarget = PACKVERSION(6, 0);

    if (dwVer >= dwTarget)
    {
        // This version of ComCtl32.dll is version 6.0 or later.
        return true;
    }
    else
    {
        // Proceed knowing that version 6.0 or later additions are not available.
        // Use an alternate approach for older the DLL version.
        return false;
    }
}

BOOL InitializeConsoleState()
{
    RegisterClasses(ghInstance);
    OEMCP = GetOEMCP();
    g_fIsComCtlV6Present = IsComCtlV6Present();

    return NT_SUCCESS(InitializeDbcsMisc());
}

void UninitializeConsoleState()
{
    if (g_fHostedInFileProperties && gpStateInfo->LinkTitle != nullptr)
    {
        // If we're in the file props dialog and have an allocated title, we need to free it. Outside of the file props
        // dlg, the caller of ConsolePropertySheet() owns the lifetime.
        CoTaskMemFree(gpStateInfo->LinkTitle);
        gpStateInfo->LinkTitle = nullptr;
    }

    LOG_IF_NTSTATUS_FAILED(DestroyDbcsMisc());
    UnregisterClasses(ghInstance);
}

void UpdateApplyButton(const HWND hDlg)
{
    if (g_fHostedInFileProperties)
    {
        PropSheet_Changed(GetParent(hDlg), hDlg);
    }
}
