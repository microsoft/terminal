// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#define WIN32_LEAN_AND_MEAN

#include <filesystem>

#include <wil/stl.h>
#include <wil/win32_helpers.h>

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

// we can't change the signature of wWinMain
#pragma warning(suppress : 26461) // The pointer argument 'cmdline' for function 'wWinMain' can be marked as a pointer to const (con.3).
int __stdcall wWinMain(HINSTANCE, HINSTANCE, LPWSTR cmdline, int)
{
    // This will invoke an elevated terminal in two possible ways. We need to do this,
    // because ShellExecuteExW() fails to work if we're running elevated and
    // the given executable path is a packaged application. See GH#14501.
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

    std::wstring cmd;

    // scenario #1
    {
#pragma warning(suppress : 26494) // Variable 'buffer' is uninitialized. Always initialize an object (type.5).
        wchar_t buffer[APPLICATION_USER_MODEL_ID_MAX_LENGTH];
        // "On input, the size of the applicationUserModelId buffer, in wide characters."
        UINT32 length = APPLICATION_USER_MODEL_ID_MAX_LENGTH;

        const auto result = GetCurrentApplicationUserModelId(&length, &buffer[0]);
        if (result == ERROR_SUCCESS)
        {
            cmd.append(L"shell:AppsFolder\\");
            // "On success, the size of the buffer used, including the null terminator."
            // --> Remove the null terminator
            cmd.append(&buffer[0], length - 1);
        }
    }

    if (cmd.empty())
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

    // disable warnings from SHELLEXECUTEINFOW struct. We can't fix that.
#pragma warning(suppress : 26476) // Expression/symbol '{seInfo.<unnamed-tag>.hIcon = 0}' uses a naked union 'union ' with multiple type pointers: Use variant instead (type.7).
    SHELLEXECUTEINFOW seInfo{};
    seInfo.cbSize = sizeof(seInfo);
    seInfo.fMask = SEE_MASK_DEFAULT;
    seInfo.lpVerb = L"runas"; // This asks the shell to elevate the process
    seInfo.lpFile = cmd.c_str(); // This is `shell:AppsFolder\...` or `...\WindowsTerminal.exe`
    seInfo.lpParameters = cmdline; // This is `new-tab -p {guid}`
    seInfo.nShow = SW_SHOWNORMAL;
    LOG_IF_WIN32_BOOL_FALSE(ShellExecuteExW(&seInfo));

    return 0;
}
