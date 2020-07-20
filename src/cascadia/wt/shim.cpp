#include <string>
#include <filesystem>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wil/stl.h>
#include <wil/resource.h>
#include <wil/win32_helpers.h>

#pragma warning(suppress : 26461) // we can't change the signature of wWinMain
int __stdcall wWinMain(HINSTANCE, HINSTANCE, LPWSTR pCmdLine, int)
{
    std::filesystem::path module{ wil::GetModuleFileNameW<std::wstring>(nullptr) };

    // Cache our name (wt, wtd)
    std::wstring ourFilename{ module.filename() };

    // Swap wt[d].exe for WindowsTerminal.exe
    module.replace_filename(L"WindowsTerminal.exe");

    // Append the rest of the commandline to the saved name
    std::wstring cmdline;
    if (FAILED(wil::str_printf_nothrow(cmdline, L"%s %s", ourFilename.c_str(), pCmdLine)))
    {
        return 1;
    }

    // Get our startup info so it can be forwarded
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    GetStartupInfoW(&si);

    // Go!
    wil::unique_process_information pi;
    return !CreateProcessW(module.c_str(), cmdline.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
}
