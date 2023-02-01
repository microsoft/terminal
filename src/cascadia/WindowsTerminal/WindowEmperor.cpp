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
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

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
    // _windowManager.ProposeCommandline(eventArgs);
    const auto result = _manager.ProposeCommandline2(eventArgs);

    // TODO! createWindow is false in cases like wt --help. Figure that out.

    if (result.ShouldCreateWindow())
    {
        CreateNewWindowThread(Remoting::WindowRequestedArgs{ result, eventArgs });

        _manager.RequestNewWindow([this](auto&&, const Remoting::WindowRequestedArgs& args) {
            CreateNewWindowThread(args);
        });
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
    // - Add an event to onarch to indicate that we should exit, because all the
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

void WindowEmperor::CreateNewWindowThread(Remoting::WindowRequestedArgs args)
{
    Remoting::Peasant peasant{ _manager.CreateAPeasant(args) };

    _threads.emplace_back([this, args, peasant]() {
        WindowThread foo{ _app.Logic(), args, _manager, peasant };
        return foo.WindowProc();
    });
}
