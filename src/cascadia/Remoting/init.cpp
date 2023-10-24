// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>
#include <WilErrorReporting.h>

// Note: Generate GUID using TlgGuid.exe tool
#pragma warning(suppress : 26477) // One of the macros uses 0/NULL. We don't have control to make it nullptr.
TRACELOGGING_DEFINE_PROVIDER(
    g_hRemotingProvider,
    "Microsoft.Windows.Terminal.Remoting",
    // {d6f04aad-629f-539a-77c1-73f5c3e4aa7b}
    (0xd6f04aad, 0x629f, 0x539a, 0x77, 0xc1, 0x73, 0xf5, 0xc3, 0xe4, 0xaa, 0x7b),
    TraceLoggingOptionMicrosoftTelemetry());

BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD reason, LPVOID /*reserved*/)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInstDll);
        TraceLoggingRegister(g_hRemotingProvider);
        Microsoft::Console::ErrorReporting::EnableFallbackFailureReporting(g_hRemotingProvider);
        break;
    case DLL_PROCESS_DETACH:
        if (g_hRemotingProvider)
        {
            TraceLoggingUnregister(g_hRemotingProvider);
        }
        break;
    }

    return TRUE;
}

UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(L"Microsoft.Terminal.Remoting/Resources");
