// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include <winrt/Microsoft.Terminal.TerminalControl.h>
#include <winrt/TerminalApp.h>

#include "NonClientIslandWindow.h"

class AppHost
{
public:
    AppHost() noexcept;
    virtual ~AppHost();

    void AppTitleChanged(const winrt::Windows::Foundation::IInspectable& sender, winrt::hstring newTitle);
    void LastTabClosed(const winrt::Windows::Foundation::IInspectable& sender, const winrt::TerminalApp::LastTabClosedEventArgs& args);
    void Initialize();

private:
    bool _useNonClientArea;

    std::unique_ptr<IslandWindow> _window;
    winrt::TerminalApp::App _app;
    winrt::TerminalApp::AppLogic _logic;

    void _HandleCreateWindow(const HWND hwnd, RECT proposedRect, winrt::TerminalApp::LaunchMode& launchMode);
    void _UpdateTitleBarContent(const winrt::Windows::Foundation::IInspectable& sender,
                                const winrt::Windows::UI::Xaml::UIElement& arg);
    void _UpdateTheme(const winrt::Windows::Foundation::IInspectable&,
                      const winrt::Windows::UI::Xaml::ElementTheme& arg);
    void _ToggleFullscreen(const winrt::Windows::Foundation::IInspectable& sender,
                           const winrt::TerminalApp::ToggleFullscreenEventArgs& arg);
};
