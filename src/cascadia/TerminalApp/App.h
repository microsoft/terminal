// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "App.g.h"
#include "App.base.h"

namespace winrt::TerminalApp::implementation
{
    struct App : AppT2<App>
    {
    public:
        App();
        void OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs const&);

        TerminalApp::AppLogic Logic();

    private:
        bool _isUwp = false;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct App : AppT<App, implementation::App>
    {
    };
}
