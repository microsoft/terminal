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
            // GH#18784: This may show a CoreWindow on the taskbar ("DesktopWindowXamlSource")
            // until WindowEmperor hides it after the first window was created. Park it right away instead.
            const wil::unique_hwineventhook hook{ SetWinEventHook(
                EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, wil::GetModuleInstanceHandle(), [](HWINEVENTHOOK, DWORD, HWND hwnd, LONG idObject, LONG, DWORD, DWORD) {
                    wchar_t name[32];
                    if (idObject == OBJID_WINDOW && GetClassNameW(hwnd, &name[0], ARRAYSIZE(name)) && wcscmp(&name[0], L"Windows.UI.Core.CoreWindow") == 0)
                    {
                        SetParent(hwnd, HWND_MESSAGE);
                    }
                },
                GetCurrentProcessId(),
                GetCurrentThreadId(),
                WINEVENT_INCONTEXT) };
            _windowsXamlManager = xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
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
