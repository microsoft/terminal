// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

class WindowEmperor
{
public:
    WindowEmperor() noexcept;
    bool ShouldExit();
    void WaitForWindows();

private:
    winrt::TerminalApp::App _app;
};
