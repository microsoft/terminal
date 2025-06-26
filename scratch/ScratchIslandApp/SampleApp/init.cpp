// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"

#include <LibraryResources.h>
#include <WilErrorReporting.h> // Needed for OutputDebugString

BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
            OutputDebugString(L"DLL loaded (PROCESS_ATTACH)\n");
            break;
        case DLL_PROCESS_DETACH:
            OutputDebugString(L"DLL unloaded (PROCESS_DETACH)\n");
            break;
        case DLL_THREAD_ATTACH:
            OutputDebugString(L"Thread created (THREAD_ATTACH)\n");
            break;
        case DLL_THREAD_DETACH:
            OutputDebugString(L"Thread destroyed (THREAD_DETACH)\n");
            break;
    }
    return TRUE;
}


UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(L"SampleApp/Resources")
