// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "IslandWindow.h"

class AppHost
{
public:
    AppHost() noexcept;
    virtual ~AppHost();

    void Initialize();

private:
    bool _useNonClientArea{ false };

    std::unique_ptr<IslandWindow> _window;

    void _HandleCreateWindow(const HWND hwnd, RECT proposedRect);
    void _UpdateTheme(const winrt::Windows::Foundation::IInspectable&,
                      const winrt::Windows::UI::Xaml::ElementTheme& arg);
};
