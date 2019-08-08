// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"

// Note: Generate GUID using TlgGuid.exe tool
TRACELOGGING_DEFINE_PROVIDER(
    g_hTerminalControlProvider,
    "Microsoft.Windows.Terminal.Control",
    // {28c82e50-57af-5a86-c25b-e39cd990032b}
    (0x28c82e50, 0x57af, 0x5a86, 0xc2, 0x5b, 0xe3, 0x9c, 0xd9, 0x90, 0x03, 0x2b),
    TraceLoggingOptionMicrosoftTelemetry());

BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD reason, LPVOID /*reserved*/)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInstDll);
        TraceLoggingRegister(g_hTerminalControlProvider);
        break;
    case DLL_PROCESS_DETACH:
        if (g_hTerminalControlProvider)
        {
            TraceLoggingUnregister(g_hTerminalControlProvider);
        }
        break;
    }

    return TRUE;
}
