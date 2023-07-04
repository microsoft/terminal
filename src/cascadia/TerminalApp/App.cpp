// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "App.h"
#include "App.g.cpp"
#include <CoreWindow.h>

using namespace winrt;
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Navigation;

namespace xaml = ::winrt::Windows::UI::Xaml;

namespace winrt::TerminalApp::implementation
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
        // LOAD BEARING
        AddOtherProvider(winrt::Microsoft::Terminal::Control::XamlMetaDataProvider{});
        AddOtherProvider(winrt::Microsoft::UI::Xaml::XamlTypeInfo::XamlControlsXamlMetaDataProvider{});

        const auto dispatcherQueue = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
        if (!dispatcherQueue)
        {
            _windowsXamlManager = xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();

            // As of Process Model v3, terminal windows are all created on their
            // own threads, but we still initiate XAML for the App on the main
            // thread. Thing is, just initializing XAML creates a CoreWindow for
            // us. On Windows 10, that CoreWindow will show up as a visible
            // window on the taskbar, unless we hide it manually. So, go get it
            // and do the SW_HIDE thing on it.
            if (const auto& coreWindow{ winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread() })
            {
                if (const auto& interop{ coreWindow.try_as<ICoreWindowInterop>() })
                {
                    HWND coreHandle{ 0 };
                    interop->get_WindowHandle(&coreHandle);
                    if (coreHandle)
                    {
                        // This prevents an empty "DesktopWindowXamlSource" from
                        // appearing on the taskbar
                        ShowWindow(coreHandle, SW_HIDE);
                    }
                }
            }
        }
        else
        {
            FAIL_FAST_MSG("Terminal is not intended to run as a Universal Windows Application");
        }
    }

    AppLogic App::Logic()
    {
        static AppLogic logic;
        return logic;
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

    /// <summary>
    /// Invoked when the application is launched normally by the end user.  Other entry points
    /// will be used such as when the application is launched to open a specific file.
    /// </summary>
    /// <param name="e">Details about the launch request and process.</param>
    void App::OnLaunched(const LaunchActivatedEventArgs& /*e*/)
    {
        // We used to support a pure UWP version of the Terminal. This method
        // was only ever used to do UWP-specific setup of our App.
    }

    void App::PrepareForSettingsUI()
    {
        if (!std::exchange(_preparedForSettingsUI, true))
        {
            AddOtherProvider(winrt::Microsoft::Terminal::Settings::Editor::XamlMetaDataProvider{});
        }
    }
}
