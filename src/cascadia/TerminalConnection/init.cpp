// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>
#include <WilErrorReporting.h>

// Note: Generate GUID using TlgGuid.exe tool
#pragma warning(suppress : 26477) // One of the macros uses 0/NULL. We don't have control to make it nullptr.
TRACELOGGING_DEFINE_PROVIDER(
    g_hTerminalConnectionProvider,
    "Microsoft.Windows.Terminal.Connection",
    // {e912fe7b-eeb6-52a5-c628-abe388e5f792}
    (0xe912fe7b, 0xeeb6, 0x52a5, 0xc6, 0x28, 0xab, 0xe3, 0x88, 0xe5, 0xf7, 0x92),
    TraceLoggingOptionMicrosoftTelemetry());

#pragma warning(suppress : 26440) // Not interested in changing the specification of DllMain to make it noexcept given it's an interface to the OS.
BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD reason, LPVOID /*reserved*/)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInstDll);
        TraceLoggingRegister(g_hTerminalConnectionProvider);
        Microsoft::Console::ErrorReporting::EnableFallbackFailureReporting(g_hTerminalConnectionProvider);
        break;
    case DLL_PROCESS_DETACH:
        if (g_hTerminalConnectionProvider)
        {
            TraceLoggingUnregister(g_hTerminalConnectionProvider);
        }
        break;
    default:
        break;
    }

    return TRUE;
}

UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(L"Microsoft.Terminal.TerminalConnection/Resources");
