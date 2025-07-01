// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AppCommandlineArgs.h"
#include "CommandlineArgs.g.h"
#include "RequestReceiveContentArgs.g.h"
#include "SummonWindowBehavior.g.h"
#include "WindowRequestedArgs.g.h"

namespace winrt::TerminalApp::implementation
{
    struct CommandlineArgs : public CommandlineArgsT<CommandlineArgs>
    {
        ::TerminalApp::AppCommandlineArgs& ParsedArgs() noexcept;
        winrt::com_array<winrt::hstring>& CommandlineRef() noexcept;

        // These bits are exposed via WinRT:
        int32_t ExitCode() const noexcept;
        winrt::hstring ExitMessage() const;
        winrt::hstring TargetWindow() const;

        til::property<winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection> Connection;
        void Commandline(const winrt::array_view<const winrt::hstring>& value);
        winrt::com_array<winrt::hstring> Commandline();
        til::property<winrt::hstring> CurrentDirectory;
        til::property<winrt::hstring> CurrentEnvironment;
        til::property<uint32_t> ShowWindowCommand{ static_cast<uint32_t>(SW_NORMAL) }; // SW_NORMAL is 1, 0 is SW_HIDE

    private:
        ::TerminalApp::AppCommandlineArgs _parsed;
        int32_t _parseResult = 0;
        winrt::com_array<winrt::hstring> _args;
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
            _Command{ std::move(command) }
        {
        }

        WindowRequestedArgs(const winrt::hstring& window, const winrt::hstring& content, const Windows::Foundation::IReference<Windows::Foundation::Rect>& bounds) :
            _WindowName{ window },
            _Content{ content },
            _InitialBounds{ bounds }
        {
        }

        WINRT_PROPERTY(uint64_t, Id);
        WINRT_PROPERTY(winrt::hstring, WindowName);
        WINRT_PROPERTY(TerminalApp::CommandlineArgs, Command, nullptr);
        WINRT_PROPERTY(winrt::hstring, Content);
        WINRT_PROPERTY(Windows::Foundation::IReference<Windows::Foundation::Rect>, InitialBounds);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(SummonWindowBehavior);
    BASIC_FACTORY(CommandlineArgs);
    BASIC_FACTORY(RequestReceiveContentArgs);
    BASIC_FACTORY(WindowRequestedArgs);
}
