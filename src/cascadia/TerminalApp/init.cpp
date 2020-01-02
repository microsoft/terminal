// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>

// Note: Generate GUID using TlgGuid.exe tool
TRACELOGGING_DEFINE_PROVIDER(
    g_hTerminalAppProvider,
    "Microsoft.Windows.Terminal.App",
    // {24a1622f-7da7-5c77-3303-d850bd1ab2ed}
    (0x24a1622f, 0x7da7, 0x5c77, 0x33, 0x03, 0xd8, 0x50, 0xbd, 0x1a, 0xb2, 0xed),
    TraceLoggingOptionMicrosoftTelemetry());

BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD reason, LPVOID /*reserved*/)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInstDll);
        TraceLoggingRegister(g_hTerminalAppProvider);
        break;
    case DLL_PROCESS_DETACH:
        if (g_hTerminalAppProvider)
        {
            TraceLoggingUnregister(g_hTerminalAppProvider);
        }
        break;
    }

    return TRUE;
}

UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(L"TerminalApp/Resources")
