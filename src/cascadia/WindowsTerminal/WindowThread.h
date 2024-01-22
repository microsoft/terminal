// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once
#include "pch.h"
#include "AppHost.h"

class WindowThread : public std::enable_shared_from_this<WindowThread>
{
public:
    WindowThread(winrt::TerminalApp::AppLogic logic, winrt::Microsoft::Terminal::Remoting::WindowManager manager);

    winrt::TerminalApp::TerminalWindow Logic();
    void CreateHost(winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs args);
    int RunMessagePump();
    void RundownForExit();

    void Refrigerate();
    void Microwave(winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs args);

    uint64_t PeasantID();

    WINRT_CALLBACK(UpdateSettingsRequested, winrt::delegate<void()>);

private:
    winrt::TerminalApp::AppLogic _appLogic{ nullptr };
    winrt::Microsoft::Terminal::Remoting::WindowManager _manager{ nullptr };
    winrt::Microsoft::Terminal::Remoting::Peasant _peasant{ nullptr };
    winrt::Windows::System::DispatcherQueue  _dispatcher{ nullptr };

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
