#include <string>
#include <filesystem>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wil/stl.h>
#include <wil/resource.h>
#include <wil/win32_helpers.h>
#include <TraceLoggingProvider.h>

TRACELOGGING_DECLARE_PROVIDER(g_hShimProvider);
TRACELOGGING_DEFINE_PROVIDER(
    g_hShimProvider,
    "Microsoft.Windows.Terminal.Shim",
    // tl:{d295502a-ab39-5565-c342-6e6d7659a422}
    (0xd295502a, 0xab39, 0x5565, 0xc3, 0x42, 0x6e, 0x6d, 0x76, 0x59, 0xa4, 0x22));

#pragma warning(suppress : 26461) // we can't change the signature of wWinMain
int __stdcall wWinMain(HINSTANCE, HINSTANCE, LPWSTR pCmdLine, int)
{
    TraceLoggingRegister(g_hShimProvider);
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
    if (!CreateProcessW(module.c_str(), cmdline.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, nullptr, &si, &pi))
    {
        return 1;
    }

    if (!AllowSetForegroundWindow(pi.dwProcessId))
    {
        TraceLoggingWrite(g_hShimProvider,
                          "ShimAllowSetForegroundWindowFailed",
                          TraceLoggingPid(pi.dwProcessId, "processId"),
                          TraceLoggingWinError(GetLastError(), "error"));
    }

    ResumeThread(pi.hThread);
    return 0;
}
