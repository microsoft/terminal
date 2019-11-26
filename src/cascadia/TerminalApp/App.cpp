// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "App.h"
#include "App.g.cpp"

using namespace winrt;
using namespace winrt::Windows::ApplicationModel;
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::TerminalApp::implementation
{
    App::App()
    {
        // This is the same trick that Initialize() is about to use to figure out whether we're coming
        // from a UWP context or from a Win32 context
        // See https://github.com/windows-toolkit/Microsoft.Toolkit.Win32/blob/52611c57d89554f357f281d0c79036426a7d9257/Microsoft.Toolkit.Win32.UI.XamlApplication/XamlApplication.cpp#L42
        const auto dispatcherQueue = ::winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
        if (dispatcherQueue)
        {
            _isUwp = true;
        }

        Initialize();
    }

    AppLogic App::Logic()
    {
        static AppLogic logic;
        return logic;
    }

    /// <summary>
    /// Invoked when the application is launched normally by the end user.  Other entry points
    /// will be used such as when the application is launched to open a specific file.
    /// </summary>
    /// <param name="e">Details about the launch request and process.</param>
    void App::OnLaunched(LaunchActivatedEventArgs const& /*e*/)
    {
        // if this is a UWP... it means its our problem to hook up the content to the window here.
        if (_isUwp)
        {
            auto content = Window::Current().Content();
            if (content == nullptr)
            {
                auto logic = Logic();
                logic.LoadSettings();
                logic.Create();

                auto page = logic.GetRoot().as<TerminalPage>();

                Window::Current().Content(page);
                Window::Current().Activate();
            }
        }
    }
}
