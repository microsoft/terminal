// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppHost.h"
#include "resource.h"

using namespace winrt;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Composition;
using namespace winrt::Windows::UI::Xaml::Hosting;
using namespace winrt::Windows::Foundation::Numerics;

// Note: Generate GUID using TlgGuid.exe tool - seriously, it won't work if you
// just generate an arbitrary GUID
TRACELOGGING_DEFINE_PROVIDER(
    g_hWindowsTerminalProvider,
    "Microsoft.Windows.Terminal.Win32Host",
    // {56c06166-2e2e-5f4d-7ff3-74f4b78c87d6}
    (0x56c06166, 0x2e2e, 0x5f4d, 0x7f, 0xf3, 0x74, 0xf4, 0xb7, 0x8c, 0x87, 0xd6),
    TraceLoggingOptionMicrosoftTelemetry());

// Routine Description:
// - Retrieves the string resource from the current module with the given ID
//   from the resources files. See resource.h and the .rc definitions for valid IDs.
// Arguments:
// - id - Resource ID
// Return Value:
// - String resource retrieved from that ID.
static std::wstring GetStringResource(const UINT id)
{
    // Calling LoadStringW with a pointer-sized storage and no length will return a read-only pointer
    // directly to the resource data instead of copying it immediately into a buffer.
    LPWSTR readOnlyResource = nullptr;
    const auto length = LoadStringW(wil::GetModuleInstanceHandle(),
                                    id,
                                    reinterpret_cast<LPWSTR>(&readOnlyResource),
                                    0);

    // However, the pointer and length given are NOT guaranteed to be zero-terminated
    // and most uses of this data will probably want a zero-terminated string.
    // So we're going to construct and return a std::wstring copy from the pointer/length
    // since those are certainly zero-terminated.
    return { readOnlyResource, gsl::narrow<size_t>(length) };
}

// Routine Description:
// - Takes an image architecture and locates a string resource that maps to that architecture.
// Arguments:
// - imageArchitecture - An IMAGE_FILE_MACHINE architecture enum value
//                     - See https://docs.microsoft.com/en-us/windows/win32/sysinfo/image-file-machine-constants
// Return Value:
// - A string value representing the human-readable name of this architecture.
static std::wstring ImageArchitectureToString(USHORT imageArchitecture)
{
    // clang-format off
    const auto id = imageArchitecture == IMAGE_FILE_MACHINE_I386 ? IDS_X86_ARCHITECTURE :
                    imageArchitecture == IMAGE_FILE_MACHINE_AMD64 ? IDS_AMD64_ARCHITECTURE :
                    imageArchitecture == IMAGE_FILE_MACHINE_ARM64 ? IDS_ARM64_ARCHITECTURE :
                    imageArchitecture == IMAGE_FILE_MACHINE_ARM ? IDS_ARM_ARCHITECTURE :
                    IDS_UNKNOWN_ARCHITECTURE;
    // clang-format on

    return GetStringResource(id);
}

// Routine Description:
// - Blocks the user from launching the application with a message box dialog and early exit
//   if the process architecture doesn't match the system platform native architecture.
// - This is because the conhost.exe must match the condrv.sys on the system and the PTY
//   infrastructure that powers everything won't work if we have a mismatch.
// Arguments:
// - <none>
// Return Value:
// - <none>
static void EnsureNativeArchitecture()
{
    USHORT processMachine{};
    USHORT nativeMachine{};
    THROW_IF_WIN32_BOOL_FALSE(IsWow64Process2(GetCurrentProcess(), &processMachine, &nativeMachine));
    if (processMachine != IMAGE_FILE_MACHINE_UNKNOWN && processMachine != nativeMachine)
    {
        const auto formatPattern = GetStringResource(IDS_ERROR_ARCHITECTURE_FORMAT);

        const auto nativeArchitecture = ImageArchitectureToString(nativeMachine);
        const auto processArchitecture = ImageArchitectureToString(processMachine);

        auto buffer{ wil::str_printf<std::wstring>(formatPattern.data(), nativeArchitecture.data(), processArchitecture.data()) };

        MessageBoxW(nullptr,
                    buffer.data(),
                    GetStringResource(IDS_ERROR_DIALOG_TITLE).data(),
                    MB_OK | MB_ICONERROR);

        ExitProcess(0);
    }
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    TraceLoggingRegister(g_hWindowsTerminalProvider);
    TraceLoggingWrite(
        g_hWindowsTerminalProvider,
        "ExecutableStarted",
        TraceLoggingDescription("Event emitted immediately on startup"),
        TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
        TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));

    // Block the user from starting if they launched the incorrect architecture version of the project.
    // This should only be applicable to developer versions. The package installation process
    // should choose and install the correct one from the bundle.
    EnsureNativeArchitecture();

    // Make sure to call this so we get WM_POINTER messages.
    EnableMouseInPointer(true);

    // Create the AppHost object, which will create both the window and the
    // Terminal App. This MUST BE constructed before the Xaml manager as TermApp
    // provides an implementation of Windows.UI.Xaml.Application.
    AppHost host;

    // !!! LOAD BEARING !!!
    // This is _magic_. Do the initial loading of our settings *BEFORE* we
    // initialize our COM apartment type. This is because the Windows.Storage
    // APIs require a MTA. However, other api's (notably the clipboard ones)
    // require that the main thread is an STA. During startup, we don't yet have
    // a dispatcher to background any async work, and we don't want to - we want
    // to load the settings synchronously. Fortunately, WinRT will assume we're
    // in a MTA until we explicitly call init_apartment. We can only call
    // init_apartment _once_, so we'll do the settings loading first, in the
    // implicit MTA, then set our apartment type to STA. The AppHost ctor will
    // load the settings for us, as it constructs the window.
    // This works because Kenny Kerr said it would, and he wrote cpp/winrt, so he knows.
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    // Initialize the xaml content. This must be called AFTER the
    // WindowsXamlManager is initalized.
    host.Initialize();

    MSG message;

    while (GetMessage(&message, nullptr, 0, 0))
    {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
    return 0;
}
