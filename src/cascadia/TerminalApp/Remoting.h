// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "CommandlineArgs.g.h"
#include "RequestReceiveContentArgs.g.h"
#include "SummonWindowBehavior.g.h"
#include "WindowRequestedArgs.g.h"

namespace winrt::TerminalApp::implementation
{
    struct CommandlineArgs : public CommandlineArgsT<CommandlineArgs>
    {
    public:
        CommandlineArgs() :
            _args{},
            _cwd{ L"" }
        {
        }

        CommandlineArgs(const winrt::array_view<const winrt::hstring>& args,
                        winrt::hstring currentDirectory,
                        const uint32_t showWindowCommand,
                        winrt::hstring envString) :
            _args{ args.begin(), args.end() },
            _cwd{ currentDirectory },
            _ShowWindowCommand{ showWindowCommand },
            CurrentEnvironment{ envString }
        {
        }

        winrt::hstring CurrentDirectory() { return _cwd; };

        void Commandline(const winrt::array_view<const winrt::hstring>& value);
        winrt::com_array<winrt::hstring> Commandline();

        til::property<winrt::hstring> CurrentEnvironment;

        WINRT_PROPERTY(uint32_t, ShowWindowCommand, SW_NORMAL); // SW_NORMAL is 1, 0 is SW_HIDE

    private:
        winrt::com_array<winrt::hstring> _args;
        winrt::hstring _cwd;
    };

    struct RequestReceiveContentArgs : RequestReceiveContentArgsT<RequestReceiveContentArgs>
    {
        WINRT_PROPERTY(uint64_t, SourceWindow);
        WINRT_PROPERTY(uint64_t, TargetWindow);
        WINRT_PROPERTY(uint32_t, TabIndex);

    public:
        RequestReceiveContentArgs(const uint64_t src, const uint64_t tgt, const uint32_t tabIndex) :
            _SourceWindow{ src },
            _TargetWindow{ tgt },
            _TabIndex{ tabIndex } {};
    };

    struct SummonWindowBehavior : public SummonWindowBehaviorT<SummonWindowBehavior>
    {
    public:
        SummonWindowBehavior() = default;
        WINRT_PROPERTY(bool, MoveToCurrentDesktop, true);
        WINRT_PROPERTY(bool, ToggleVisibility, true);
        WINRT_PROPERTY(uint32_t, DropdownDuration, 0);
        WINRT_PROPERTY(MonitorBehavior, ToMonitor, MonitorBehavior::ToCurrent);

    public:
        SummonWindowBehavior(const SummonWindowBehavior& other) :
            _MoveToCurrentDesktop{ other.MoveToCurrentDesktop() },
            _ToMonitor{ other.ToMonitor() },
            _DropdownDuration{ other.DropdownDuration() },
            _ToggleVisibility{ other.ToggleVisibility() } {};
    };

    struct WindowRequestedArgs : public WindowRequestedArgsT<WindowRequestedArgs>
    {
    public:
        WindowRequestedArgs(uint64_t id, const winrt::TerminalApp::CommandlineArgs& command) :
            _Id{ id },
            _args{ command.Commandline() },
            _CurrentDirectory{ command.CurrentDirectory() },
            _ShowWindowCommand{ command.ShowWindowCommand() },
            _CurrentEnvironment{ command.CurrentEnvironment() } {};

        WindowRequestedArgs(const winrt::hstring& window, const winrt::hstring& content, const Windows::Foundation::IReference<Windows::Foundation::Rect>& bounds) :
            _Id{ 0u },
            _WindowName{ window },
            _args{},
            _CurrentDirectory{},
            _Content{ content },
            _InitialBounds{ bounds } {};

        void Commandline(const winrt::array_view<const winrt::hstring>& value) { _args = { value.begin(), value.end() }; };
        winrt::com_array<winrt::hstring> Commandline() { return winrt::com_array<winrt::hstring>{ _args.begin(), _args.end() }; }

        WINRT_PROPERTY(uint64_t, Id);
        WINRT_PROPERTY(winrt::hstring, WindowName);
        WINRT_PROPERTY(winrt::hstring, CurrentDirectory);
        WINRT_PROPERTY(winrt::hstring, Content);
        WINRT_PROPERTY(uint32_t, ShowWindowCommand, SW_NORMAL);
        WINRT_PROPERTY(winrt::hstring, CurrentEnvironment);
        WINRT_PROPERTY(Windows::Foundation::IReference<Windows::Foundation::Rect>, InitialBounds);

    private:
        winrt::com_array<winrt::hstring> _args;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(SummonWindowBehavior);
    BASIC_FACTORY(CommandlineArgs);
    BASIC_FACTORY(RequestReceiveContentArgs);
    BASIC_FACTORY(WindowRequestedArgs);
}
