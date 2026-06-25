// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>
#include <WilErrorReporting.h>

// Note: Generate GUID using TlgGuid.exe tool
TRACELOGGING_DEFINE_PROVIDER(
    g_hTerminalSettingsEditorProvider,
    "Microsoft.Windows.Terminal.Settings.Editor",
    // {1b16317d-b594-51f8-c552-5d50572b5efc}
    (0x1b16317d, 0xb594, 0x51f8, 0xc5, 0x52, 0x5d, 0x50, 0x57, 0x2b, 0x5e, 0xfc),
    TraceLoggingOptionMicrosoftTelemetry());

BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD reason, LPVOID /*reserved*/)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInstDll);
        TraceLoggingRegister(g_hTerminalSettingsEditorProvider);
        Microsoft::Console::ErrorReporting::EnableFallbackFailureReporting(g_hTerminalSettingsEditorProvider);
        break;
    case DLL_PROCESS_DETACH:
        if (g_hTerminalSettingsEditorProvider)
        {
            TraceLoggingUnregister(g_hTerminalSettingsEditorProvider);
        }
        break;
    }

    return TRUE;
}

UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(L"Microsoft.Terminal.Settings.Editor/Resources");
