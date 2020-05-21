#include "pch.h"
#include "MyShellExt.h"

std::atomic<DWORD> g_dwModuleRefCount = 0;
HINSTANCE g_hInst = 0;

extern "C" IMAGE_DOS_HEADER __ImageBase;
void ModuleAddRef()
{
    g_dwModuleRefCount++;
}

void ModuleRelease()
{
    g_dwModuleRefCount--;
}

struct MyClassFactory : winrt::implements<MyClassFactory, IClassFactory>
{
public:
    MyClassFactory(_In_ REFCLSID clsid) :
        m_clsid{ clsid } {};

    // IClassFactory methods
    IFACEMETHODIMP CreateInstance(_In_opt_ IUnknown* punkOuter, _In_ REFIID riid, _Outptr_ void** ppv)
    {
        *ppv = NULL;
        HRESULT hr;
        if (punkOuter)
        {
            hr = CLASS_E_NOAGGREGATION;
        }
        else if (m_clsid == __uuidof(MyShellExt))
        {
            // hr = CPowerRenameMenu::s_CreateInstance(punkOuter, riid, ppv);
            hr = winrt::make<MyShellExt>()->QueryInterface(riid, ppv);
        }
        else
        {
            hr = CLASS_E_CLASSNOTAVAILABLE;
        }
        return hr;
    }

    IFACEMETHODIMP LockServer(BOOL bLock)
    {
        if (bLock)
        {
            ModuleAddRef();
        }
        else
        {
            ModuleRelease();
        }
        return S_OK;
    }

private:
    CLSID m_clsid;
};

HRESULT __stdcall DllGetClassObject(GUID const& clsid, GUID const& iid, void** result)
{
    // DebugBreak();
    // try
    // {
    //     *result = nullptr;

    //     if (clsid == __uuidof(MyShellExt))
    //     {
    //         return winrt::make<MyShellExt>()->QueryInterface(iid, result);
    //     }

    //     return winrt::hresult_class_not_available().to_abi();
    // }
    // catch (...)
    // {
    //     return winrt::to_hresult();
    // }

    *result = NULL;
    auto pClassFactory = winrt::make<MyClassFactory>(clsid);
    HRESULT hr = pClassFactory->QueryInterface(iid, result);
    // pClassFactory->Release();
    return hr;
}
