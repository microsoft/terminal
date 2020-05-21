#include "pch.h"
#include "MyShellExt.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;

static WCHAR const c_szVerbDisplayName[] = L"Open in Windows Terminal";
static WCHAR const c_szVerbName[] = L"WindowsTerminalOpenHere";

// This code is aggressively copied from
//   https://github.com/microsoft/Windows-classic-samples/blob/master/Samples/
//   Win7Samples/winui/shell/appshellintegration/ExplorerCommandVerb/ExplorerCommandVerb.cpp

HRESULT MyShellExt::Invoke(IShellItemArray* psiItemArray,
                           IBindCtx* /*pbc*/)
{
    // DebugBreak();
    DWORD count;
    psiItemArray->GetCount(&count);

    IShellItem* psi;
    // RETURN_IF_FAILED(psiItemArray->GetItemAt(0, IID_PPV_ARGS(&psi)));
    RETURN_IF_FAILED(psiItemArray->GetItemAt(0, &psi));

    // IShellItem2* psi2;

    PWSTR pszName;
    RETURN_IF_FAILED(psi->GetDisplayName(SIGDN_PARENTRELATIVEPARSING, &pszName));
    WCHAR szMsg[128];
    StringCchPrintf(szMsg, ARRAYSIZE(szMsg), L"%d item(s), first item is named %s", count, pszName);

    {
        wil::unique_process_information _piClient;

        STARTUPINFOEX siEx{ 0 };
        siEx.StartupInfo.cb = sizeof(STARTUPINFOEX);
        std::wstring mutableTitle{ pszName };

        siEx.StartupInfo.lpTitle = mutableTitle.data();

        // MessageBox((HWND)0x0, szMsg, L"ExplorerCommand Sample Verb", MB_OK);
        std::wstring cmdline = fmt::format(L"cmd.exe /k echo {}", pszName);
        RETURN_IF_WIN32_BOOL_FALSE(CreateProcessW(
            nullptr,
            cmdline.data(),
            nullptr, // lpProcessAttributes
            nullptr, // lpThreadAttributes
            false, // bInheritHandles
            EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT, // dwCreationFlags
            nullptr, // lpEnvironment
            nullptr,
            &siEx.StartupInfo, // lpStartupInfo
            &_piClient // lpProcessInformation
            ));
    }

    CoTaskMemFree(pszName);

    psi->Release();

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
                             BOOL /*fOkToBeSlow*/,
                             EXPCMDSTATE* pCmdState)
{
    // compute the visibility of the verb here, respect "fOkToBeSlow" if this is
    // slow (does IO for example) when called with fOkToBeSlow == FALSE return
    // E_PENDING and this object will be called back on a background thread with
    // fOkToBeSlow == TRUE

    // We however don't need to bother with any of that, so we'll just return ECS_ENABLED.

    *pCmdState = ECS_ENABLED;
    return S_OK;
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
