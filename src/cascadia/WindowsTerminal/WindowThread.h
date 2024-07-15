// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once
#include "pch.h"
#include "AppHost.h"

class WindowThread : public std::enable_shared_from_this<WindowThread>
{
public:
    WindowThread(winrt::TerminalApp::AppLogic logic,
                 winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs args,
                 winrt::Microsoft::Terminal::Remoting::WindowManager manager,
                 winrt::Microsoft::Terminal::Remoting::Peasant peasant);

    winrt::TerminalApp::TerminalWindow Logic();
    void CreateHost();
    int RunMessagePump();
    void RundownForExit();

    bool KeepWarm();
    void Refrigerate();
    void Microwave(
        winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs args,
        winrt::Microsoft::Terminal::Remoting::Peasant peasant);

    uint64_t PeasantID();

    til::event<winrt::delegate<void()>> UpdateSettingsRequested;

private:
    winrt::Microsoft::Terminal::Remoting::Peasant _peasant{ nullptr };

    winrt::TerminalApp::AppLogic _appLogic{ nullptr };
    winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs _args{ nullptr };
    winrt::Microsoft::Terminal::Remoting::WindowManager _manager{ nullptr };

    // This is a "shared_ptr", but it should be treated as a unique, owning ptr.
    // It's shared, because there are edge cases in refrigeration where internal
    // co_awaits inside AppHost might resume after we've dtor'd it, and there's
    // no other way for us to let the AppHost know it has passed on.
    std::shared_ptr<::AppHost> _host{ nullptr };
    winrt::event_token _UpdateSettingsRequestedToken;

    std::unique_ptr<::IslandWindow> _warmWindow{ nullptr };

    int _messagePump();
    void _pumpRemainingXamlMessages();
};
