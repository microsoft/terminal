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

        MTApp::AppLogic Logic();

        void Close();

        bool IsDisposed() const
        {
            return _bIsClosed;
        }

    private:
        bool _isUwp = false;
        WUX::Hosting::WindowsXamlManager _windowsXamlManager = nullptr;
        WFC::IVector<WUXMarkup::IXamlMetadataProvider> _providers = winrt::single_threaded_vector<WUXMarkup::IXamlMetadataProvider>();
        bool _bIsClosed = false;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct App : AppT<App, implementation::App>
    {
    };
}
