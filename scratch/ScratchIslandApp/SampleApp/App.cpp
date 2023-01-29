// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "App.h"
#include "App.g.cpp"

using namespace winrt;
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Navigation;

namespace xaml = ::winrt::Windows::UI::Xaml;

namespace winrt::SampleApp::implementation
{
    App::App()
    {
        Initialize();

        // Disable XAML's automatic backplating of text when in High Contrast
        // mode: we want full control of and responsibility for the foreground
        // and background colors that we draw in XAML.
        HighContrastAdjustment(::winrt::Windows::UI::Xaml::ApplicationHighContrastAdjustment::None);
    }

    void App::Initialize()
    {
        const auto dispatcherQueue = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
        if (!dispatcherQueue)
        {
            _windowsXamlManager = xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
        }
        else
        {
            _isUwp = true;
        }
    }

    void App::Close()
    {
        if (_bIsClosed)
        {
            return;
        }

        _bIsClosed = true;

        if (_windowsXamlManager)
        {
            _windowsXamlManager.Close();
        }
        _windowsXamlManager = nullptr;

        Exit();
        {
            MSG msg = {};
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                ::DispatchMessageW(&msg);
            }
        }
    }

    SampleAppLogic App::Logic()
    {
        static SampleAppLogic logic;
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
                logic.Create();

                auto page = logic.GetRoot().as<MyPage>();

                Window::Current().Content(page);
                Window::Current().Activate();
            }
        }
    }
}
