// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "SampleIslandWindow.h"

class SampleAppHost
{
public:
    SampleAppHost() noexcept;
    virtual ~SampleAppHost();

    void Initialize();

private:
    std::unique_ptr<SampleIslandWindow> _window;
    winrt::SampleApp::App _app;
    winrt::SampleApp::SampleAppLogic _logic;
};
