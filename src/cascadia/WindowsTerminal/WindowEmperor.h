/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Class Name:
- WindowEmperor.h

Abstract:
- TODO!

--*/

#pragma once
#include "pch.h"

#include "WindowThread.h"

class WindowEmperor
{
public:
    WindowEmperor() noexcept;
    bool ShouldExit();
    void WaitForWindows();

    bool HandleCommandlineArgs();
    void CreateNewWindowThread(winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs args);

private:
    winrt::TerminalApp::App _app;
    winrt::Microsoft::Terminal::Remoting::WindowManager2 _manager;

    std::vector<std::thread> _threads;
};
