#include "pch.h"
#include "OpenTerminalHere.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;

static std::wstring VerbDisplayName{ L"Open in Windows Terminal" };
static std::wstring VerbDevBuildDisplayName{ L"Open in Windows Terminal (Dev Build)" };
static std::wstring VerbName{ L"WindowsTerminalOpenHere" };

// This code is aggressively copied from
//   https://github.com/microsoft/Windows-classic-samples/blob/master/Samples/
//   Win7Samples/winui/shell/appshellintegration/ExplorerCommandVerb/ExplorerCommandVerb.cpp

bool IsDevBuild()
{
    // use C++11 magic statics to make sure we only do this once.
    static bool isDevBuild = []() -> bool {
        try
        {
            const auto package{ winrt::Windows::ApplicationModel::Package::Current() };
            const auto id = package.Id();
            const std::wstring name{ id.FullName() };
            // Does our PFN start with WindowsTerminalDev?
            return name.rfind(L"WindowsTerminalDev", 0) == 0;
        }
        CATCH_LOG();
        return true;
    }();

    return isDevBuild;
}

HRESULT OpenTerminalHere::Invoke(IShellItemArray* psiItemArray,
                                 IBindCtx* /*pbc*/)
{
    DWORD count;
    psiItemArray->GetCount(&count);

    winrt::com_ptr<IShellItem> psi;
    RETURN_IF_FAILED(psiItemArray->GetItemAt(0, psi.put()));

    PWSTR pszName;
    RETURN_IF_FAILED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszName));

    {
        wil::unique_process_information _piClient;
        STARTUPINFOEX siEx{ 0 };
        siEx.StartupInfo.cb = sizeof(STARTUPINFOEX);

        const bool isDevBuild = IsDevBuild();
        std::wstring cmdline = fmt::format(L"{}.exe -d \"{}\"", isDevBuild ? L"wtd" : L"wt", pszName);
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

    return S_OK;
}

HRESULT OpenTerminalHere::GetToolTip(IShellItemArray* /*psiItemArray*/,
                                     LPWSTR* ppszInfotip)
{
    // tooltip provided here, in this case none is provieded
    *ppszInfotip = NULL;
    return E_NOTIMPL;
}

HRESULT OpenTerminalHere::GetTitle(IShellItemArray* /*psiItemArray*/,
                                   LPWSTR* ppszName)
{
    // Change the string we return depending on if we're running from the dev
    // build package or not.
    const bool isDevBuild = IsDevBuild();
    return SHStrDup(isDevBuild ? VerbDevBuildDisplayName.c_str() : VerbDisplayName.c_str(), ppszName);
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
{
    // the icon ref ("dll,-<resid>") is provied here, in this case none is provieded
    *ppszIcon = NULL;
    return E_NOTIMPL;
}

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
    *ppEnum = NULL;
    return E_NOTIMPL;
}
