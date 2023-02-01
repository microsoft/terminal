// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once
#include "pch.h"
#include "AppHost.h"

class WindowThread
{
public:
    WindowThread(const winrt::TerminalApp::AppLogic& logic,
                 winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs args,
                 winrt::Microsoft::Terminal::Remoting::WindowManager2 manager,
                 winrt::Microsoft::Terminal::Remoting::Peasant peasant);
    int WindowProc();

private:
    ::AppHost _host;
    winrt::Microsoft::Terminal::Remoting::Peasant _peasant{ nullptr };
};
