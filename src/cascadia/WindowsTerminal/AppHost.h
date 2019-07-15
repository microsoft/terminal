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

    void AppTitleChanged(winrt::hstring newTitle);
    void LastTabClosed();
    void Initialize();

private:
    bool _useNonClientArea;

    std::unique_ptr<IslandWindow> _window;
    winrt::TerminalApp::App _app;

    void _HandleCreateWindow(const HWND hwnd, const RECT proposedRect);
    void _UpdateTitleBarContent(const winrt::TerminalApp::App& sender,
                                const winrt::Windows::UI::Xaml::UIElement& arg);
    void _UpdateTheme(const winrt::TerminalApp::App&,
                      const winrt::Windows::UI::Xaml::ElementTheme& arg);
};
