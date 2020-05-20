#include "pch.h"
#include "MyShellExt.h"

HRESULT __stdcall DllGetClassObject(GUID const& clsid, GUID const& iid, void** result)
{
    try
    {
        *result = nullptr;

        if (clsid == __uuidof(MyShellExt))
        {
            return winrt::make<MyShellExt>()->QueryInterface(iid, result);
        }

        return winrt::hresult_class_not_available().to_abi();
    }
    catch (...)
    {
        return winrt::to_hresult();
    }
}
