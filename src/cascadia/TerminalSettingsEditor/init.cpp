// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>
#include <WilErrorReporting.h>

// Note: Generate GUID using TlgGuid.exe tool
#pragma warning(suppress : 26477) // One of the macros uses 0/NULL. We don't have control to make it nullptr.
TRACELOGGING_DEFINE_PROVIDER(
    g_hSettingsEditorProvider,
    "Microsoft.Windows.Terminal.Settings.Editor",
    // {1b16317d-b594-51f8-c552-5d50572b5efc}
    (0x1b16317d, 0xb594, 0x51f8, 0xc5, 0x52, 0x5d, 0x50, 0x57, 0x2b, 0x5e, 0xfc),
    TraceLoggingOptionMicrosoftTelemetry());

#pragma warning(suppress : 26440) // Not interested in changing the specification of DllMain to make it noexcept given it's an interface to the OS.
BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD reason, LPVOID /*reserved*/)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInstDll);
        TraceLoggingRegister(g_hSettingsEditorProvider);
        Microsoft::Console::ErrorReporting::EnableFallbackFailureReporting(g_hSettingsEditorProvider);
        break;
    case DLL_PROCESS_DETACH:
        if (g_hSettingsEditorProvider)
        {
            TraceLoggingUnregister(g_hSettingsEditorProvider);
        }
        break;
    default:
        break;
    }

    return TRUE;
}
