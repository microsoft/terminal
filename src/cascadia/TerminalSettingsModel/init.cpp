// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>
#include <WilErrorReporting.h>

// Note: Generate GUID using TlgGuid.exe tool
TRACELOGGING_DEFINE_PROVIDER(
    g_hSettingsModelProvider,
    "Microsoft.Windows.Terminal.Setting.Model",
    // {be579944-4d33-5202-e5d6-a7a57f1935cb}
    (0xbe579944, 0x4d33, 0x5202, 0xe5, 0xd6, 0xa7, 0xa5, 0x7f, 0x19, 0x35, 0xcb),
    TraceLoggingOptionMicrosoftTelemetry());

BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD reason, LPVOID /*reserved*/)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInstDll);
        TraceLoggingRegister(g_hSettingsModelProvider);
        Microsoft::Console::ErrorReporting::EnableFallbackFailureReporting(g_hSettingsModelProvider);
        break;
    case DLL_PROCESS_DETACH:
        if (g_hSettingsModelProvider)
        {
            TraceLoggingUnregister(g_hSettingsModelProvider);
        }
        break;
    }

    return TRUE;
}

UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(L"Microsoft.Terminal.Settings.Model/Resources")
