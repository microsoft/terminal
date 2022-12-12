// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "App.g.h"
#include "App.base.h"
#include <winrt/Windows.UI.Xaml.Hosting.h>

namespace winrt::TerminalApp::implementation
{
    struct App : AppT2<App>
    {
    public:
        App();
        void OnLaunched(const Windows::ApplicationModel::Activation::LaunchActivatedEventArgs&);
        void Initialize();

        TerminalApp::AppLogic Logic();

        void Close();

        bool IsDisposed() const
        {
            return _bIsClosed;
        }

    private:
        bool _isUwp = false;
        winrt::Windows::UI::Xaml::Hosting::WindowsXamlManager _windowsXamlManager = nullptr;
        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::UI::Xaml::Markup::IXamlMetadataProvider> _providers = winrt::single_threaded_vector<Windows::UI::Xaml::Markup::IXamlMetadataProvider>();
        bool _bIsClosed = false;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct App : AppT<App, implementation::App>
    {
    };
}
