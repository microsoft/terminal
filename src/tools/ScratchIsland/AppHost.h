// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "IslandWindow.h"
#include <winrt/ScratchWinRTServer.h>
#include "HostManager.h"

class AppHost
{
public:
    AppHost() noexcept;
    virtual ~AppHost();

    void Initialize();

private:
    bool _useNonClientArea{ false };

    std::unique_ptr<IslandWindow> _window;
    winrt::ScratchIsland::HostManager _manager;

    void _HandleCreateWindow(const HWND hwnd, RECT proposedRect);
    void _UpdateTheme(const winrt::Windows::Foundation::IInspectable&,
                      const winrt::Windows::UI::Xaml::ElementTheme& arg);

    winrt::Windows::UI::Xaml::Controls::Grid _rootGrid{ nullptr };
    winrt::Windows::UI::Xaml::Controls::Grid _swapchainsGrid{ nullptr };

    winrt::Windows::UI::Xaml::Controls::SwapChainPanel _swp0{ nullptr };
    winrt::Windows::UI::Xaml::Controls::SwapChainPanel _swp1{ nullptr };
    winrt::Windows::UI::Xaml::Controls::SwapChainPanel _swp2{ nullptr };
    winrt::Windows::UI::Xaml::Controls::SwapChainPanel _swp3{ nullptr };
};
