// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "WindowEmperor.h"

// #include "MonarchFactory.h"
// #include "CommandlineArgs.h"
#include "../inc/WindowingBehavior.h"
// #include "FindTargetWindowArgs.h"
// #include "ProposeCommandlineResult.h"

#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;
using namespace std::chrono_literals;

WindowEmperor::WindowEmperor() noexcept :
    _app{}
{
    _manager.FindTargetWindowRequested([this](const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                              const winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs& findWindowArgs) {
        {
            const auto targetWindow = _app.Logic().FindTargetWindow(findWindowArgs.Args().Commandline());
            findWindowArgs.ResultTargetWindow(targetWindow.WindowId());
            findWindowArgs.ResultTargetWindowName(targetWindow.WindowName());
        }
    });
}

WindowEmperor::~WindowEmperor()
{
    _app.Close();
    _app = nullptr;
}

void _buildArgsFromCommandline(std::vector<winrt::hstring>& args)
{
    if (auto commandline{ GetCommandLineW() })
    {
        auto argc = 0;

        // Get the argv, and turn them into a hstring array to pass to the app.
        wil::unique_any<LPWSTR*, decltype(&::LocalFree), ::LocalFree> argv{ CommandLineToArgvW(commandline, &argc) };
        if (argv)
        {
            for (auto& elem : wil::make_range(argv.get(), argc))
            {
                args.emplace_back(elem);
            }
        }
    }
    if (args.empty())
    {
        args.emplace_back(L"wt.exe");
    }
}

bool WindowEmperor::HandleCommandlineArgs()
{
    std::vector<winrt::hstring> args;
    _buildArgsFromCommandline(args);
    auto cwd{ wil::GetCurrentDirectoryW<std::wstring>() };

    Remoting::CommandlineArgs eventArgs{ { args }, { cwd } };

    const auto result = _manager.ProposeCommandline2(eventArgs);

    // TODO! createWindow is false in cases like wt --help. Figure that out.

    if (result.ShouldCreateWindow())
    {
        CreateNewWindowThread(Remoting::WindowRequestedArgs{ result, eventArgs }, true);

        _manager.RequestNewWindow([this](auto&&, const Remoting::WindowRequestedArgs& args) {
            CreateNewWindowThread(args, false);
        });

        _becomeMonarch();
    }

    return result.ShouldCreateWindow();
}

bool WindowEmperor::ShouldExit()
{
    // TODO!
    return false;
}

void WindowEmperor::WaitForWindows()
{
    // std::thread one{ [this]() {
    //     WindowThread foo{ _app.Logic() };
    //     return foo.WindowProc();
    // } };

    // Sleep(2000);

    // std::thread two{ [this]() {
    //     WindowThread foo{ _app.Logic() };
    //     return foo.WindowProc();
    // } };

    // one.join();
    // two.join();

    // Sleep(30000); //30s

    // TODO! This creates a loop that never actually exits right now. It seems
    // to get a message wehn another window is activated, but never a WM_CLOSE
    // (that makes sense). It keeps running even when the threads all exit,
    // which is INTERESTING for sure.
    //
    // what we should do:
    // - Add an event to Monarch to indicate that we should exit, because all the
    //   peasants have exited.
    // - We very well may need an HWND_MESSAGE that's connected to the main
    //   thread, for processing global hotkeys. Consider that in the future too.

    MSG message;
    while (GetMessage(&message, nullptr, 0, 0))
    {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
}

void WindowEmperor::CreateNewWindowThread(Remoting::WindowRequestedArgs args, const bool firstWindow)
{
    Remoting::Peasant peasant{ _manager.CreateAPeasant(args) };

    auto func = [this, args, peasant, firstWindow]() {
        auto window{ std::make_shared<WindowThread>(_app.Logic(), args, _manager, peasant) };
        _windows.push_back(window);
        return window->WindowProc();
    };

    _threads.emplace_back(func);
}

void WindowEmperor::_becomeMonarch()
{
    ////////////////////////////////////////////////////////////////////////////
    //     _setupGlobalHotkeys();

    ////////////////////////////////////////////////////////////////////////////

    //     if (_windowManager2.DoesQuakeWindowExist() ||
    //         _window->IsQuakeWindow() ||
    //         (_windowLogic.GetAlwaysShowNotificationIcon() || _windowLogic.GetMinimizeToNotificationArea()))
    //     {
    //         _CreateNotificationIcon();
    //     }

    //     // These events are coming from peasants that become or un-become quake windows.
    //     _revokers.ShowNotificationIconRequested = _windowManager2.ShowNotificationIconRequested(winrt::auto_revoke, { this, &AppHost::_ShowNotificationIconRequested });
    //     _revokers.HideNotificationIconRequested = _windowManager2.HideNotificationIconRequested(winrt::auto_revoke, { this, &AppHost::_HideNotificationIconRequested });

    ////////////////////////////////////////////////////////////////////////////

    //     // Set the number of open windows (so we know if we are the last window)
    //     // and subscribe for updates if there are any changes to that number.
    //     _windowLogic.SetNumberOfOpenWindows(_windowManager2.GetNumberOfPeasants());

    _WindowCreatedToken = _manager.WindowCreated({ this, &WindowEmperor::_numberOfWindowsChanged });
    _WindowClosedToken = _manager.WindowClosed({ this, &WindowEmperor::_numberOfWindowsChanged });

    ////////////////////////////////////////////////////////////////////////////

    //     // If the monarch receives a QuitAll event it will signal this event to be
    //     // ran before each peasant is closed.
    //     _revokers.QuitAllRequested = _windowManager2.QuitAllRequested(winrt::auto_revoke, { this, &AppHost::_QuitAllRequested });

    ////////////////////////////////////////////////////////////////////////////

    // The monarch should be monitoring if it should save the window layout.
    // We want at least some delay to prevent the first save from overwriting
    _getWindowLayoutThrottler.emplace(std::move(std::chrono::seconds(10)), std::move([this]() { _SaveWindowLayoutsRepeat(); }));
    _getWindowLayoutThrottler.value()();
}

// sender and args are always nullptr
void WindowEmperor::_numberOfWindowsChanged(const winrt::Windows::Foundation::IInspectable&,
                                            const winrt::Windows::Foundation::IInspectable&)
{
    if (_getWindowLayoutThrottler)
    {
        _getWindowLayoutThrottler.value()();
    }

    const auto& numWindows{ _manager.GetNumberOfPeasants() };
    for (const auto& _windowThread : _windows)
    {
        _windowThread->Logic().SetNumberOfOpenWindows(numWindows);
    }
}

winrt::Windows::Foundation::IAsyncAction WindowEmperor::_SaveWindowLayouts()
{
    // Make sure we run on a background thread to not block anything.
    co_await winrt::resume_background();

    if (_app.Logic().ShouldUsePersistedLayout())
    {
        try
        {
            TraceLoggingWrite(g_hWindowsTerminalProvider,
                              "AppHost_SaveWindowLayouts_Collect",
                              TraceLoggingDescription("Logged when collecting window state"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));

            const auto layoutJsons = _manager.GetAllWindowLayouts();

            TraceLoggingWrite(g_hWindowsTerminalProvider,
                              "AppHost_SaveWindowLayouts_Save",
                              TraceLoggingDescription("Logged when writing window state"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));

            _app.Logic().SaveWindowLayoutJsons(layoutJsons);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            TraceLoggingWrite(g_hWindowsTerminalProvider,
                              "AppHost_SaveWindowLayouts_Failed",
                              TraceLoggingDescription("An error occurred when collecting or writing window state"),
                              TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                              TraceLoggingKeyword(TIL_KEYWORD_TRACE));
        }
    }

    co_return;
}

winrt::fire_and_forget WindowEmperor::_SaveWindowLayoutsRepeat()
{
    // Make sure we run on a background thread to not block anything.
    co_await winrt::resume_background();

    co_await _SaveWindowLayouts();

    // Don't need to save too frequently.
    co_await 30s;

    // As long as we are supposed to keep saving, request another save.
    // This will be delayed by the throttler so that at most one save happens
    // per 10 seconds, if a save is requested by another source simultaneously.
    if (_getWindowLayoutThrottler.has_value())
    {
        TraceLoggingWrite(g_hWindowsTerminalProvider,
                          "AppHost_requestGetLayout",
                          TraceLoggingDescription("Logged when triggering a throttled write of the window state"),
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));

        _getWindowLayoutThrottler.value()();
    }
}
