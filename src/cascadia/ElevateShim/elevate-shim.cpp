// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <string>
#include <filesystem>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wil/stl.h>
#include <wil/resource.h>
#include <wil/win32_helpers.h>
#include <gsl/gsl_util>
#include <gsl/pointers>
#include <shellapi.h>
#include <appmodel.h>

// BODGY
//
// If we try to do this in the Terminal itself, then there's a bunch of weird
// things that can go wrong and prevent the elevated Terminal window from
// getting created. Specifically, if the origin Terminal exits right away after
// spawning the elevated WT, then ShellExecute might not successfully complete
// the elevation. What's even more, the Terminal will mysteriously crash
// somewhere in XAML land.
//
// To mitigate this, the Terminal will call into us with the commandline it
// wants elevated. We'll hang around until ShellExecute is finished, so that the
// process can successfully elevate.

#pragma warning(suppress : 26461) // we can't change the signature of wWinMain
int __stdcall wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    // This will invoke an elevated terminal in two possible ways. See GH#14501
    // In both scenarios, it passes the entire cmdline as-is to the new process.
    //
    // #1 discover and invoke the app using the GetCurrentApplicationUserModelId
    // api using shell:AppsFolder\package!appid
    //    cmd:    shell:AppsFolder\WindowsTerminalDev_8wekyb3d8bbwe!App
    //    params: new-tab -p {guid}
    //
    // #2 find and execute WindowsTerminal.exe
    //    cmd:    {same path as this binary}\WindowsTerminal.exe
    //    params: new-tab -p {guid}

    // see if we're a store app we can invoke with shell:AppsFolder
    std::wstring appUserModelId;
    const auto result = wil::AdaptFixedSizeToAllocatedResult<std::wstring, APPLICATION_USER_MODEL_ID_MAX_LENGTH>(
        appUserModelId, [&](PWSTR value, size_t valueLength, gsl::not_null<size_t*> valueLengthNeededWithNull) noexcept -> HRESULT {
            UINT32 length = gsl::narrow_cast<UINT32>(valueLength);
            const LONG rc = GetCurrentApplicationUserModelId(&length, value);
            switch (rc)
            {
            case ERROR_SUCCESS:
                *valueLengthNeededWithNull = length;
                return S_OK;

            case ERROR_INSUFFICIENT_BUFFER:
                *valueLengthNeededWithNull = length;
                return S_FALSE; // trigger allocation loop

            case APPMODEL_ERROR_NO_APPLICATION:
                return E_FAIL; // we are not running as a store app

            default:
                return E_UNEXPECTED;
            }
        });
    LOG_IF_FAILED(result);

    std::wstring cmd = {};
    if (result == S_OK && appUserModelId.length() > 0)
    {
        // scenario #1
        cmd = L"shell:AppsFolder\\" + appUserModelId;
    }
    else
    {
        // scenario #2
        // Get the path to WindowsTerminal.exe, which should live next to us.
        std::filesystem::path module{
            wil::GetModuleFileNameW<std::wstring>(nullptr)
        };
        // Swap elevate-shim.exe for WindowsTerminal.exe
        module.replace_filename(L"WindowsTerminal.exe");
        cmd = module;
    }

    // Go!

    // The cmdline argument passed to WinMain is stripping the first argument.
    // Using GetCommandLine() instead for lParameters

    // disable warnings from SHELLEXECUTEINFOW struct. We can't fix that.
#pragma warning(push)
#pragma warning(disable : 26476) // Macro uses naked union over variant.
    SHELLEXECUTEINFOW seInfo{ 0 };
#pragma warning(pop)

    seInfo.cbSize = sizeof(seInfo);
    seInfo.fMask = SEE_MASK_DEFAULT;
    seInfo.lpVerb = L"runas"; // This asks the shell to elevate the process
    seInfo.lpFile = cmd.c_str(); // This is `shell:AppsFolder\...` or `...\WindowsTerminal.exe`
    seInfo.lpParameters = GetCommandLine(); // This is `new-tab -p {guid}`
    seInfo.nShow = SW_SHOWNORMAL;
    LOG_IF_WIN32_BOOL_FALSE(ShellExecuteExW(&seInfo));

    return 0;
}
