// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "SampleIslandWindow.h"

class SampleAppHost
{
public:
    SampleAppHost(winrt::SampleApp::SampleAppLogic l) noexcept;
    virtual ~SampleAppHost();

    void Initialize();

    winrt::SampleApp::SampleAppLogic _logic{ nullptr };

private:
    std::unique_ptr<SampleIslandWindow> _window;
};
