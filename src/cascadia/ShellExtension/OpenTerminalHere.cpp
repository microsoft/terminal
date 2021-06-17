// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "OpenTerminalHere.h"
#include "../WinRTUtils/inc/WtExeUtils.h"
#include <ShlObj.h>

// TODO GH#6112: Localize these strings
static constexpr std::wstring_view VerbDisplayName{ L"Open in Windows Terminal" };
static constexpr std::wstring_view VerbDevBuildDisplayName{ L"Open in Windows Terminal (Dev Build)" };
static constexpr std::wstring_view VerbName{ L"WindowsTerminalOpenHere" };

// This code is aggressively copied from
//   https://github.com/microsoft/Windows-classic-samples/blob/master/Samples/
//   Win7Samples/winui/shell/appshellintegration/ExplorerCommandVerb/ExplorerCommandVerb.cpp

// Method Description:
// - This method is called when the user activates the context menu item. We'll
//   launch the Terminal using the current working directory.
// Arguments:
// - psiItemArray: a IShellItemArray which contains the item that's selected.
// Return Value:
// - S_OK if we successfully attempted to launch the Terminal, otherwise a
//   failure from an earlier HRESULT.
HRESULT OpenTerminalHere::Invoke(IShellItemArray* psiItemArray,
                                 IBindCtx* /*pBindContext*/)
{
    wil::unique_cotaskmem_string pszName;

    if (psiItemArray == nullptr)
    {
        // get the current path from explorer.exe
        const auto path = this->_GetPathFromExplorer();

        // no go, unable to get a reasonable path
        if (path.empty())
        {
            return S_FALSE;
        }
        pszName = wil::make_cotaskmem_string(path.c_str(), path.length());
    }
    else
    {
        DWORD count;
        psiItemArray->GetCount(&count);

        winrt::com_ptr<IShellItem> psi;
        RETURN_IF_FAILED(psiItemArray->GetItemAt(0, psi.put()));
        RETURN_IF_FAILED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszName));
    }

    {
        wil::unique_process_information _piClient;
        STARTUPINFOEX siEx{ 0 };
        siEx.StartupInfo.cb = sizeof(STARTUPINFOEX);

        // Append a "\." to the given path, so that this will work in "C:\"
        auto cmdline{ wil::str_printf<std::wstring>(LR"-("%s" -d "%s\.")-", GetWtExePath().c_str(), pszName.get()) };
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

    return S_OK;
}

HRESULT OpenTerminalHere::GetToolTip(IShellItemArray* /*psiItemArray*/,
                                     LPWSTR* ppszInfoTip)
{
    // tooltip provided here, in this case none is provided
    *ppszInfoTip = nullptr;
    return E_NOTIMPL;
}

HRESULT OpenTerminalHere::GetTitle(IShellItemArray* /*psiItemArray*/,
                                   LPWSTR* ppszName)
{
    // Change the string we return depending on if we're running from the dev
    // build package or not.
    const bool isDevBuild = IsDevBuild();
    return SHStrDup(isDevBuild ? VerbDevBuildDisplayName.data() : VerbDisplayName.data(), ppszName);
}

HRESULT OpenTerminalHere::GetState(IShellItemArray* /*psiItemArray*/,
                                   BOOL /*fOkToBeSlow*/,
                                   EXPCMDSTATE* pCmdState)
{
    // compute the visibility of the verb here, respect "fOkToBeSlow" if this is
    // slow (does IO for example) when called with fOkToBeSlow == FALSE return
    // E_PENDING and this object will be called back on a background thread with
    // fOkToBeSlow == TRUE

    // We however don't need to bother with any of that, so we'll just return
    // ECS_ENABLED.

    *pCmdState = ECS_ENABLED;
    return S_OK;
}

HRESULT OpenTerminalHere::GetIcon(IShellItemArray* /*psiItemArray*/,
                                  LPWSTR* ppszIcon)
try
{
    std::filesystem::path modulePath{ wil::GetModuleFileNameW<std::wstring>(wil::GetModuleInstanceHandle()) };
    modulePath.replace_filename(WindowsTerminalExe);
    // WindowsTerminal.exe,-101 will be the first icon group in WT
    // We're using WindowsTerminal here explicitly, and not wt (from GetWtExePath), because
    // WindowsTerminal is the only one built with the right icons.
    const auto resource{ modulePath.wstring() + L",-101" };
    return SHStrDupW(resource.c_str(), ppszIcon);
}
CATCH_RETURN();

HRESULT OpenTerminalHere::GetFlags(EXPCMDFLAGS* pFlags)
{
    *pFlags = ECF_DEFAULT;
    return S_OK;
}

HRESULT OpenTerminalHere::GetCanonicalName(GUID* pguidCommandName)
{
    *pguidCommandName = __uuidof(this);
    return S_OK;
}

HRESULT OpenTerminalHere::EnumSubCommands(IEnumExplorerCommand** ppEnum)
{
    *ppEnum = nullptr;
    return E_NOTIMPL;
}

std::wstring OpenTerminalHere::_GetPathFromExplorer() const
{
    using namespace std;
    using namespace winrt;

    wstring path;
    HRESULT hr = NOERROR;

    auto hwnd = ::GetForegroundWindow();
    if (hwnd == nullptr)
    {
        return path;
    }

    TCHAR szName[MAX_PATH] = { 0 };
    ::GetClassName(hwnd, szName, MAX_PATH);
    if (0 == StrCmp(szName, L"WorkerW") ||
        0 == StrCmp(szName, L"Progman"))
    {
        //special folder: desktop
        hr = ::SHGetFolderPath(NULL, CSIDL_DESKTOP, NULL, SHGFP_TYPE_CURRENT, szName);
        if (FAILED(hr))
        {
            return path;
        }

        path = szName;
        return path;
    }

    if (0 != StrCmp(szName, L"CabinetWClass"))
    {
        return path;
    }

    com_ptr<IShellWindows> shell;
    try
    {
        shell = create_instance<IShellWindows>(CLSID_ShellWindows, CLSCTX_ALL);
    }
    catch (...)
    {
        //look like try_create_instance is not available no more
    }

    if (shell == nullptr)
    {
        return path;
    }

    com_ptr<IDispatch> disp;
    wil::unique_variant variant;
    variant.vt = VT_I4;

    com_ptr<IWebBrowserApp> browser;
    // look for correct explorer window
    for (variant.intVal = 0;
         shell->Item(variant, disp.put()) == S_OK;
         variant.intVal++)
    {
        com_ptr<IWebBrowserApp> tmp;
        if (FAILED(disp->QueryInterface(tmp.put())))
        {
            disp = nullptr; // get rid of DEBUG non-nullptr warning
            continue;
        }

        HWND tmpHWND = NULL;
        hr = tmp->get_HWND(reinterpret_cast<SHANDLE_PTR*>(&tmpHWND));
        if (hwnd == tmpHWND)
        {
            browser = tmp;
            disp = nullptr; // get rid of DEBUG non-nullptr warning
            break; //found
        }

        disp = nullptr; // get rid of DEBUG non-nullptr warning
    }

    if (browser != nullptr)
    {
        wil::unique_bstr url;
        hr = browser->get_LocationURL(&url);
        if (FAILED(hr))
        {
            return path;
        }

        wstring sUrl(url.get(), SysStringLen(url.get()));
        DWORD size = MAX_PATH;
        hr = ::PathCreateFromUrl(sUrl.c_str(), szName, &size, NULL);
        if (SUCCEEDED(hr))
        {
            path = szName;
        }
    }

    return path;
}
