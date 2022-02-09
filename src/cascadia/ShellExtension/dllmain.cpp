// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "OpenTerminalHere.h"

#include "../WinRTUtils/inc/LibraryResources.h"

using namespace Microsoft::WRL;

STDAPI DllCanUnloadNow()
{
    return Module<InProc>::GetModule().Terminate() ? S_OK : S_FALSE;
}

STDAPI DllGetActivationFactory(_In_ HSTRING activatableClassId, _COM_Outptr_ IActivationFactory** factory)
{
    return Module<InProc>::GetModule().GetActivationFactory(activatableClassId, factory);
}

STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _COM_Outptr_ void** ppv)
{
    return Module<InProc>::GetModule().GetClassObject(rclsid, riid, ppv);
}

STDAPI_(BOOL)
DllMain(_In_opt_ HINSTANCE hinst, DWORD reason, _In_opt_ void*)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hinst);
    }
    return TRUE;
}

// Usurp the TerminalApp's Context Menu specific resource group.
UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(L"TerminalApp/ContextMenu")
