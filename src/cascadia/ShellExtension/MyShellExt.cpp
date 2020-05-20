#include "pch.h"
#include "MyShellExt.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;

struct MyCoclass : winrt::implements<MyCoclass, IPersist, IStringable, IMyShellExt>
{
    HRESULT __stdcall Call() noexcept override
    {
        return S_OK;
    }

    HRESULT __stdcall GetClassID(CLSID* id) noexcept override
    {
        *id = IID_IPersist; // Doesn't matter what we return, for this example.
        return S_OK;
    }

    winrt::hstring ToString()
    {
        return L"MyCoclass as a string";
    }
};
