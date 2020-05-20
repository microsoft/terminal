#include "pch.h"
#include "MyShellExt.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;

static WCHAR const c_szVerbDisplayName[] = L"ExplorerCommand Verb Sample";
static WCHAR const c_szVerbName[] = L"Sample.ExplorerCommandVerb";

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

// Agressively copied from
// https://github.com/microsoft/Windows-classic-samples/blob/master/Samples/Win7Samples/winui/shell/appshellintegration/ExplorerCommandVerb/ExplorerCommandVerb.cpp

HRESULT MyShellExt::Invoke(IShellItemArray* /*psiItemArray*/,
                           IBindCtx* /*pbc*/)
{
    return S_OK;
}

HRESULT MyShellExt::GetToolTip(IShellItemArray* /*psiItemArray*/,
                               LPWSTR* ppszInfotip)
{
    // tooltip provided here, in this case none is provieded
    *ppszInfotip = NULL;
    return E_NOTIMPL;
}
HRESULT MyShellExt::GetTitle(IShellItemArray* /*psiItemArray*/,
                             LPWSTR* ppszName)
{
    // the verb name can be computed here, in this example it is static
    return SHStrDup(c_szVerbDisplayName, ppszName);
}
HRESULT MyShellExt::GetState(IShellItemArray* /*psiItemArray*/,
                             BOOL fOkToBeSlow,
                             EXPCMDSTATE* pCmdState)
{
    // compute the visibility of the verb here, respect "fOkToBeSlow" if this is
    // slow (does IO for example) when called with fOkToBeSlow == FALSE return
    // E_PENDING and this object will be called back on a background thread with
    // fOkToBeSlow == TRUE

    *pCmdState = ECS_ENABLED;
    return S_OK;

    // HRESULT hr;
    // if (fOkToBeSlow)
    // {
    //     Sleep(4 * 1000); // simulate expensive work
    //     *pCmdState = ECS_ENABLED;
    //     hr = S_OK;
    // }
    // else
    // {
    //     *pCmdState = ECS_DISABLED;
    //     // returning E_PENDING requests that a new instance of this object be called back
    //     // on a background thread so that it can do work that might be slow
    //     hr = E_PENDING;
    // }
    // return hr;
}
HRESULT MyShellExt::GetIcon(IShellItemArray* /*psiItemArray*/,
                            LPWSTR* ppszIcon)
{
    // the icon ref ("dll,-<resid>") is provied here, in this case none is provieded
    *ppszIcon = NULL;
    return E_NOTIMPL;
}
HRESULT MyShellExt::GetFlags(EXPCMDFLAGS* pFlags)
{
    *pFlags = ECF_DEFAULT;
    return S_OK;
}
HRESULT MyShellExt::GetCanonicalName(GUID* pguidCommandName)
{
    *pguidCommandName = __uuidof(this);
    return S_OK;
}
HRESULT MyShellExt::EnumSubCommands(IEnumExplorerCommand** ppEnum)
{
    *ppEnum = NULL;
    return E_NOTIMPL;
}
