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
    ~WindowEmperor();
    bool ShouldExit();
    void WaitForWindows();

    bool HandleCommandlineArgs();
    void CreateNewWindowThread(winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs args);

private:
    winrt::TerminalApp::App _app;
    winrt::Microsoft::Terminal::Remoting::WindowManager2 _manager;

    std::vector<std::shared_ptr<WindowThread>> _windows;
    std::vector<std::thread> _threads;

    std::optional<til::throttled_func_trailing<>> _getWindowLayoutThrottler;

    winrt::event_token _WindowCreatedToken;
    winrt::event_token _WindowClosedToken;

    void _becomeMonarch();
    void _numberOfWindowsChanged(const winrt::Windows::Foundation::IInspectable&, const winrt::Windows::Foundation::IInspectable&);
};
