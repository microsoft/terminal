// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppHost.h"

class WindowThread
{
public:
    WindowThread(const winrt::TerminalApp::AppLogic& logic);
    int WindowProc();

private:
    ::AppHost _host;
};
