// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <string>
#include <filesystem>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wil/stl.h>
#include <wil/resource.h>
#include <wil/win32_helpers.h>
#include <shellapi.h>

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
    // This supports being invoked in two possible ways. See GH#14501
    //
    // #1 Invoke the AppX package using shell:AppsFolder\package!appid if
    // the first argument starts with shell:AppsFolder. The first argument
    // is used as the cmd to ShellExecuteEx and the remainder is passed as
    // the parameters.  e.g.
    //  cmd:    shell:AppsFolder\WindowsTerminalDev_8wekyb3d8bbwe!App
    //  params: new-tab -p {guid}`
    //
    // #2 In this scenario we will find and execute WindowsTerminal.exe and pass
    // the entire cmdline as-is to the new process.  e.g.
    //  cmd:    {same path as this binary}\WindowsTerminal.exe
    //  params: new-tab -p {guid}`
    //

    // The cmdline argument passed to WinMain is stripping the first argument.
    // Using GetCommandLine() and global command line arguments instead.
    RETURN_HR_IF_MSG(E_UNEXPECTED, __argc <= 0, "elevate-shim: no arguments found, argc <= 0");
    RETURN_HR_IF_NULL_MSG(E_UNEXPECTED, __wargv, "elevate-shim: no arguments found, argv is null");

    std::wstring cmdLine = GetCommandLine();
    std::wstring cmd = {};
    std::wstring args = {};

    std::wstring arg0 = __wargv[0];
    if (arg0.starts_with(L"shell:AppsFolder"))
    {
        // scenario #1
        cmd = arg0;

        // We don't want to reconstruct the args with proper quoting and escaping.
        // Let's remove the first arg instead. This assumes it was not unescaped
        // when processed. We might leave a pair of empty quotes if it was quoted.
        args = cmdLine.replace(cmdLine.find(arg0), arg0.length(), L"");
    }
    else
    {
        // scenario #2
        // Get the path to WindowsTerminal.exe, which should live next to us.
        std::filesystem::path module{ wil::GetModuleFileNameW<std::wstring>(nullptr) };
        // Swap elevate-shim.exe for WindowsTerminal.exe
        module.replace_filename(L"WindowsTerminal.exe");
        cmd = module;
        args = GetCommandLine(); // args as-is
    }

    // Go!

    // disable warnings from SHELLEXECUTEINFOW struct. We can't fix that.
#pragma warning(push)
#pragma warning(disable : 26476) // Macro uses naked union over variant.
    SHELLEXECUTEINFOW seInfo{ 0 };
#pragma warning(pop)

    seInfo.cbSize = sizeof(seInfo);
    seInfo.fMask = SEE_MASK_DEFAULT;
    seInfo.lpVerb = L"runas"; // This asks the shell to elevate the process
    seInfo.lpFile = cmd.c_str(); // This is `shell:AppsFolder\...` or `...\WindowsTerminal.exe`
    seInfo.lpParameters = args.c_str(); // This is `new-tab -p {guid}`
    seInfo.nShow = SW_SHOWNORMAL;
    LOG_IF_WIN32_BOOL_FALSE(ShellExecuteExW(&seInfo));

    return 0;
}
