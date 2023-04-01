// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SampleAppHost.h"
#include "../types/inc/Viewport.hpp"
#include "../types/inc/utils.hpp"
#include "../types/inc/User32Utils.hpp"
#include "resource.h"

using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Composition;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Hosting;
using namespace winrt::Windows::Foundation::Numerics;
using namespace ::Microsoft::Console;
using namespace ::Microsoft::Console::Types;

SampleAppHost::SampleAppHost() noexcept :
    _app{},
    _logic{ nullptr }, // don't make one, we're going to take a ref on app's
    _window{ nullptr }
{
    _logic = _app.Logic(); // get a ref to app's logic

    _window = std::make_unique<SampleIslandWindow>();
    _window->MakeWindow();
}

SampleAppHost::~SampleAppHost()
{
    // destruction order is important for proper teardown here
    _window = nullptr;
    _app.Close();
    _app = nullptr;
}
// Method Description:
// - Initializes the XAML island, creates the terminal app, and sets the
//   island's content to that of the terminal app's content. Also registers some
//   callbacks with TermApp.
// !!! IMPORTANT!!!
// This must be called *AFTER* WindowsXamlManager::InitializeForCurrentThread.
// If it isn't, then we won't be able to create the XAML island.
// Arguments:
// - <none>
// Return Value:
// - <none>
void SampleAppHost::Initialize()
{
    _window->Initialize();

    _logic.Create();

    _window->UpdateTitle(_logic.Title());

    // Set up the content of the application. If the app has a custom titlebar,
    // set that content as well.
    _window->SetContent(_logic.GetRoot());

    _window->OnAppInitialized();

    // THIS IS A HACK
    //
    // We've got a weird crash that happens terribly inconsistently, only in
    // Debug mode. Apparently, there's some weird ref-counting magic that goes
    // on during teardown, and our Application doesn't get closed quite right,
    // which can cause us to crash into the debugger. This of course, only
    // happens on exit, and happens somewhere in the XamlHost.dll code.
    //
    // Crazily, if we _manually leak the Application_ here, then the crash
    // doesn't happen. This doesn't matter, because we really want the
    // Application to live for _the entire lifetime of the process_, so the only
    // time when this object would actually need to get cleaned up is _during
    // exit_. So we can safely leak this Application object, and have it just
    // get cleaned up normally when our process exits.
    ::winrt::SampleApp::App a{ _app };
    ::winrt::detach_abi(a);
}
