// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "OpenTerminalHere.h"
#include "../WinRTUtils/inc/WtExeUtils.h"
#include "../WinRTUtils/inc/LibraryResources.h"

#include <winrt/Windows.ApplicationModel.Resources.Core.h>
#include <ShlObj.h>

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
try
{
    const auto runElevated = IsControlAndShiftPressed();

    wil::com_ptr_nothrow<IShellItem> psi;
    RETURN_IF_FAILED(GetBestLocationFromSelectionOrSite(psiItemArray, psi.put()));
    if (!psi)
    {
        return S_FALSE;
    }

    wil::unique_cotaskmem_string pszName;
    RETURN_IF_FAILED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszName));

    {
        wil::unique_process_information _piClient;
        STARTUPINFOEX siEx{ 0 };
        siEx.StartupInfo.cb = sizeof(STARTUPINFOEX);

        // Explicitly create the terminal window visible.
        siEx.StartupInfo.dwFlags |= STARTF_USESHOWWINDOW;
        siEx.StartupInfo.wShowWindow = SW_SHOWNORMAL;

        std::filesystem::path modulePath{ wil::GetModuleFileNameW<std::wstring>(wil::GetModuleInstanceHandle()) };
        std::wstring cmdline;
        if (runElevated)
        {
            RETURN_IF_FAILED(wil::str_printf_nothrow(cmdline, LR"-(-d %s)-", QuoteAndEscapeCommandlineArg(pszName.get()).c_str()));
        }
        else
        {
            RETURN_IF_FAILED(wil::str_printf_nothrow(cmdline, LR"-("%s" -d %s)-", GetWtExePath().c_str(), QuoteAndEscapeCommandlineArg(pszName.get()).c_str()));
        }
        RETURN_IF_WIN32_BOOL_FALSE(CreateProcessW(
            runElevated ? modulePath.replace_filename(ElevateShimExe).c_str() : nullptr, // if elevation requested pass the elevate-shim.exe as the application name
            cmdline.data(),
            nullptr, // lpProcessAttributes
            nullptr, // lpThreadAttributes
            false, // bInheritHandles
            EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT, // dwCreationFlags
            nullptr, // lpEnvironment
            pszName.get(),
            &siEx.StartupInfo, // lpStartupInfo
            &_piClient // lpProcessInformation
            ));
    }

    return S_OK;
}
CATCH_RETURN()

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
    const auto resource =
#if defined(WT_BRANDING_RELEASE)
        RS_(L"ShellExtension_OpenInTerminalMenuItem");
#elif defined(WT_BRANDING_PREVIEW)
        RS_(L"ShellExtension_OpenInTerminalMenuItem_Preview");
#else
        RS_(L"ShellExtension_OpenInTerminalMenuItem_Dev");
#endif
    return SHStrDup(resource.data(), ppszName);
}

HRESULT OpenTerminalHere::GetState(IShellItemArray* psiItemArray,
                                   BOOL /*fOkToBeSlow*/,
                                   EXPCMDSTATE* pCmdState)
{
    // compute the visibility of the verb here, respect "fOkToBeSlow" if this is
    // slow (does IO for example) when called with fOkToBeSlow == FALSE return
    // E_PENDING and this object will be called back on a background thread with
    // fOkToBeSlow == TRUE

    // We however don't need to bother with any of that.

    // If no item was selected when the context menu was opened and Explorer
    // is not at a valid location (e.g. This PC or Quick Access), we should hide
    // the verb from the context menu.
    wil::com_ptr_nothrow<IShellItem> psi;
    RETURN_IF_FAILED(GetBestLocationFromSelectionOrSite(psiItemArray, psi.put()));

    SFGAOF attributes;
    const bool isFileSystemItem = psi && (psi->GetAttributes(SFGAO_FILESYSTEM, &attributes) == S_OK);
    const bool isCompressed = psi && (psi->GetAttributes(SFGAO_FOLDER | SFGAO_STREAM, &attributes) == S_OK);
    *pCmdState = isFileSystemItem && !isCompressed ? ECS_ENABLED : ECS_HIDDEN;

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

IFACEMETHODIMP OpenTerminalHere::SetSite(IUnknown* site) noexcept
{
    site_ = site;
    return S_OK;
}

IFACEMETHODIMP OpenTerminalHere::GetSite(REFIID riid, void** site) noexcept
{
    RETURN_IF_FAILED(site_.query_to(riid, site));
    return S_OK;
}

HRESULT OpenTerminalHere::GetLocationFromSite(IShellItem** location) const noexcept
{
    wil::assign_null_to_opt_param(location);

    if (!site_)
    {
        return S_FALSE;
    }

    wil::com_ptr_nothrow<IServiceProvider> serviceProvider;
    RETURN_IF_FAILED(site_.query_to(serviceProvider.put()));
    wil::com_ptr_nothrow<IFolderView> folderView;
    RETURN_IF_FAILED(serviceProvider->QueryService(SID_SFolderView, IID_PPV_ARGS(folderView.put())));
    RETURN_IF_FAILED(folderView->GetFolder(IID_PPV_ARGS(location)));
    return S_OK;
}

HRESULT OpenTerminalHere::GetBestLocationFromSelectionOrSite(IShellItemArray* psiArray, IShellItem** location) const noexcept
{
    wil::com_ptr_nothrow<IShellItem> psi;
    if (psiArray)
    {
        DWORD count{};
        RETURN_IF_FAILED(psiArray->GetCount(&count));
        if (count) // Sometimes we get an array with a count of 0. Fall back to the site chain.
        {
            RETURN_IF_FAILED(psiArray->GetItemAt(0, psi.put()));
        }
    }

    if (!psi)
    {
        RETURN_IF_FAILED(GetLocationFromSite(psi.put()));
    }

    RETURN_HR_IF(S_FALSE, !psi);
    RETURN_IF_FAILED(psi.copy_to(location));
    return S_OK;
}

// Check is both ctrl and shift keys are pressed during activation of the shell extension
bool OpenTerminalHere::IsControlAndShiftPressed()
{
    short control = 0;
    short shift = 0;
    control = GetAsyncKeyState(VK_CONTROL);
    shift = GetAsyncKeyState(VK_SHIFT);

    // GetAsyncKeyState returns a value with the most significant bit set to 1 if the key is pressed. This is the sign bit.
    return control < 0 && shift < 0;
}
