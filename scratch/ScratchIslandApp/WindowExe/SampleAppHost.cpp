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

SampleAppHost::SampleAppHost(winrt::SampleApp::SampleAppLogic l) noexcept :
    _logic{ l }, 
    _window{ nullptr }
{
    _window = std::make_unique<SampleIslandWindow>();
    _window->MakeWindow();
}

SampleAppHost::~SampleAppHost()
{
    // destruction order is important for proper teardown here
    _window = nullptr;
    // _app.Close();
    // _app = nullptr;
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
    _logic = winrt::SampleApp::SampleAppLogic();
    _logic.Create();

    _window->UpdateTitle(_logic.Title());

    // Set up the content of the application. If the app has a custom titlebar,
    // set that content as well.
    _window->SetContent(_logic.GetRoot());

    _window->OnAppInitialized();

}
