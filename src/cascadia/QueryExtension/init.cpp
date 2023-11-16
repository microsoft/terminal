// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>
#include <WilErrorReporting.h>

// Note: Generate GUID using TlgGuid.exe tool
#pragma warning(suppress : 26477) // One of the macros uses 0/NULL. We don't have control to make it nullptr.
TRACELOGGING_DEFINE_PROVIDER(
    g_hQueryExtensionProvider,
    "Microsoft.Windows.Terminal.Query.Extension",
    // {44b43e25-7420-56e8-12bd-a9fb33b77df7}
    (0x44b43e25, 0x7420, 0x56e8, 0x12, 0xbd, 0xa9, 0xfb, 0x33, 0xb7, 0x7d, 0xf7),
    TraceLoggingOptionMicrosoftTelemetry());

#pragma warning(suppress : 26440) // Not interested in changing the specification of DllMain to make it noexcept given it's an interface to the OS.
BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD reason, LPVOID /*reserved*/)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInstDll);
        TraceLoggingRegister(g_hQueryExtensionProvider);
        Microsoft::Console::ErrorReporting::EnableFallbackFailureReporting(g_hQueryExtensionProvider);
        break;
    case DLL_PROCESS_DETACH:
        if (g_hQueryExtensionProvider)
        {
            TraceLoggingUnregister(g_hQueryExtensionProvider);
        }
        break;
    default:
        break;
    }

    return TRUE;
}

UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(L"Microsoft.Terminal.Query.Extension/Resources");
