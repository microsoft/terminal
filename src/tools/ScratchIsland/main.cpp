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

int __stdcall wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    // If Terminal is spawned by a shortcut that requests that it run in a new process group
    // while attached to a console session, that request is nonsense. That request will, however,
    // cause WT to start with Ctrl-C disabled. This wouldn't matter, because it's a Windows-subsystem
    // application. Unfortunately, that state is heritable. In short, if you start WT using cmd in
    // a weird way, ^C stops working _inside_ the terminal. Mad.
    SetConsoleCtrlHandler(NULL, FALSE);

    // Make sure to call this so we get WM_POINTER messages.
    EnableMouseInPointer(true);

    // !!! LOAD BEARING !!!
    // We must initialize the main thread as a single-threaded apartment before
    // constructing any Xaml objects. Failing to do so will cause some issues
    // in accessibility somewhere down the line when a UIAutomation object will
    // be queried on the wrong thread at the wrong time.
    // We used to initialize as STA only _after_ initializing the application
    // host, which loaded the settings. The settings needed to be loaded in MTA
    // because we were using the Windows.Storage APIs. Since we're no longer
    // doing that, we can safely init as STA before any WinRT dispatches.
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    // Create the AppHost object, which will create both the window and the
    // Terminal App. This MUST BE constructed before the Xaml manager as TermApp
    // provides an implementation of Windows.UI.Xaml.Application.
    AppHost host;

    // Initialize the xaml content. This must be called AFTER the
    // WindowsXamlManager is initialized.
    host.Initialize();

    MSG message;

    while (GetMessage(&message, nullptr, 0, 0))
    {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
    return 0;
}
