// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "OpenTerminalHere.h"

// For reference, see:
// * https://docs.microsoft.com/en-us/cpp/cppcx/wrl/how-to-create-a-classic-com-component-using-wrl?view=vs-2019
// * https://docs.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/move-to-winrt-from-wrl#porting-a-wrl-module-microsoftwrlmodule
//
// We don't need to implement DllGetActivationFactory or DllCanUnloadNow
// manually, since the generated module.g.cpp will handle it for us, and will
// handle our WRL types appropriately.
//
// We DO need to implement DllGetClassObject, because that's what explorer.exe
// will call to attempt to create a class factory for our shell extension. The
// CoCreatableClass macro in OpenTerminalHere.h will create the factory for us,
// so that the GetClassObject call will work like magic.

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, _COM_Outptr_ void** ppv)
{
    return Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule().GetClassObject(rclsid, riid, ppv);
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
