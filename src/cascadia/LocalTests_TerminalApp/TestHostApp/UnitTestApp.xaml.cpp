//-----------------------------------------------------------
// Copyright (c) Microsoft Corporation. All Rights Reserved.
//-----------------------------------------------------------
#include "pch.h"

// NOTE: 03-Jan-2020
// This class is horrifyingly defined in CX, _NOT_ CppWinrt. It was largely
// taken straight from the TAEF sample code. However, it does _work_, and it's
// not about to be changed ever, so it's not worth the effort to try and port it
// to cppwinrt at this time.

namespace TestHostApp
{
    /// <summary>
    /// Initializes the singleton application object.  This is the first line of authored code
    /// executed, and as such is the logical equivalent of main() or WinMain().
    /// </summary>
    App::App()
    {
        InitializeComponent();
    }

    /// <summary>
    /// Invoked when the application is launched normally by the end user.    Other entry points
    /// will be used such as when the application is launched to open a specific file.
    /// </summary>
    /// <param name="e">Details about the launch request and process.</param>
    void App::OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs ^ e)
    {
        Windows::UI::Xaml::Window::Current->Activate();
        Microsoft::VisualStudio::TestPlatform::TestExecutor::WinRTCore::UnitTestClient::Run(e->Arguments);
    }
}
