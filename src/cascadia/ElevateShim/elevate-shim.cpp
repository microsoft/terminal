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
    // #1 Invoke the AppX package using shell:AppsFolder\app!id. The callers passes
    // the cmd as the first argument in the form of shell:AppsFolder\app!id. We parse
    // this out of the command line as the cmd and pass the remainder as the cmdline
    //      `shell:AppsFolder\WindowsTerminalDev_8wekyb3d8bbwe!App new-tab -p {guid}`
    //
    // #2 in this scenario we find and execute WindowsTerminal.exe and pass the
    // cmdline args as-is.
    //      `new-tab -p {guid}`
    //

    DebugBreak();

    // The cmdline argument in WinMain is stripping the first argument.
    // Using GetCommandLine() and global command line arguments instead.
    const int argc = __argc;
    LPWSTR* argv = __wargv;
    if (argv == nullptr)
    {
        OutputDebugString(L"elevate-shim: no arguments found, argv == nullptr");
        LOG_LAST_ERROR_IF_NULL(argv);
        return -1;
    }
    if (argc == 0)
    {
        OutputDebugString(L"elevate-shim: no arguments found, argc == 0");
        return -1;
    }

    std::wstring cmdLine = GetCommandLine();
    std::wstring cmd = {};
    std::wstring args = {};

    std::wstring arg0 = argv[0];
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
