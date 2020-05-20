// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>

// Note: Generate GUID using TlgGuid.exe tool
TRACELOGGING_DEFINE_PROVIDER(
    g_hTerminalSettingsControlProvider,
    "Microsoft.Windows.Terminal.Settings.Control",
    // {58272983-4ad8-5de8-adbc-2db4810c3b21}
    (0x58272983,0x4ad8,0x5de8,0xad,0xbc,0x2d,0xb4,0x81,0x0c,0x3b,0x21),
    TraceLoggingOptionMicrosoftTelemetry());

BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD reason, LPVOID /*reserved*/)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInstDll);
        TraceLoggingRegister(g_hTerminalSettingsControlProvider);
        break;
    case DLL_PROCESS_DETACH:
        if (g_hTerminalSettingsControlProvider)
        {
            TraceLoggingUnregister(g_hTerminalSettingsControlProvider);
        }
        break;
    }

    return TRUE;
}

UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(L"Microsoft.Terminal.Settings.Control/Resources");
