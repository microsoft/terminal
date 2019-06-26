// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppHost.h"

#include <wil/result.h>

using namespace winrt;
using namespace Windows::UI;
using namespace Windows::UI::Composition;
using namespace Windows::UI::Xaml::Hosting;
using namespace Windows::Foundation::Numerics;

static constexpr std::wstring_view ImageArchitectureToString(USHORT imageArchitecture)
{
    // clang-format off
    return imageArchitecture == IMAGE_FILE_MACHINE_I386 ? L"i386" :
                                IMAGE_FILE_MACHINE_AMD64 ? L"AMD64" :
                                IMAGE_FILE_MACHINE_ARM64 ? L"ARM64" :
                                IMAGE_FILE_MACHINE_ARM ? L"ARM" :
                                L"Unknown";
    // clang-format on
}

static void EnsureNativeArchitecture()
{
    USHORT processMachine{};
    USHORT nativeMachine{};
    THROW_IF_WIN32_BOOL_FALSE(IsWow64Process2(GetCurrentProcess(), &processMachine, &nativeMachine));
    if (processMachine != IMAGE_FILE_MACHINE_UNKNOWN && processMachine != nativeMachine)
    {
        std::wstringstream messageBuilder;
        messageBuilder << L"Windows Terminal is designed to run on your system's native architecture (" << ImageArchitectureToString(nativeMachine) << L").\n";
        messageBuilder << L"You are currently using the " << ImageArchitectureToString(processMachine) << L" version.\n\n";
        messageBuilder << L"Please use the version of Windows Terminal that matches your system's native architecture.";
        auto message = messageBuilder.str();

        MessageBoxW(nullptr,
                    message.c_str(),
                    L"Error",
                    MB_OK | MB_ICONERROR);
        ExitProcess(0);
    }
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
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
