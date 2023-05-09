// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "App.g.h"
#include "App.base.h"

namespace winrt::SampleApp::implementation
{
    struct App : AppT2<App>
    {
    public:
        App();
        void Initialize();
        void Close();
        void OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs const&);

        bool IsDisposed() const
        {
            return _bIsClosed;
        }

        SampleApp::SampleAppLogic Logic();

    private:
        bool _isUwp = false;
        winrt::Windows::UI::Xaml::Hosting::WindowsXamlManager _windowsXamlManager = nullptr;
        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::UI::Xaml::Markup::IXamlMetadataProvider> _providers = winrt::single_threaded_vector<Windows::UI::Xaml::Markup::IXamlMetadataProvider>();
        bool _bIsClosed = false;
    };
}

namespace winrt::SampleApp::factory_implementation
{
    struct App : AppT<App, implementation::App>
    {
    };
}
