// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once
#include "pch.h"
#include "AppHost.h"

class WindowThread
{
public:
    WindowThread(winrt::TerminalApp::AppLogic logic,
                 winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs args,
                 winrt::Microsoft::Terminal::Remoting::WindowManager manager,
                 winrt::Microsoft::Terminal::Remoting::Peasant peasant);

    winrt::TerminalApp::TerminalWindow Logic();
    void Start();
    winrt::Microsoft::Terminal::Remoting::Peasant Peasant();

    WINRT_CALLBACK(Started, winrt::delegate<>);
    WINRT_CALLBACK(Exited, winrt::delegate<uint64_t>);
    WINRT_CALLBACK(UpdateSettingsRequested, winrt::delegate<void()>);

private:
    winrt::Microsoft::Terminal::Remoting::Peasant _peasant{ nullptr };

    winrt::TerminalApp::AppLogic _appLogic{ nullptr };
    winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs _args{ nullptr };
    winrt::Microsoft::Terminal::Remoting::WindowManager _manager{ nullptr };

    std::unique_ptr<::AppHost> _host{ nullptr };

    int _messagePump();
};
