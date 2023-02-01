// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once
#include "pch.h"
#include "AppHost.h"

class WindowThread
{
public:
    WindowThread(const winrt::TerminalApp::AppLogic& logic,
                 winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs args);
    int WindowProc();

private:
    ::AppHost _host;
};
