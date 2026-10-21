// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Remoting.h"

#include "CommandlineArgs.g.cpp"
#include "RequestReceiveContentArgs.g.cpp"
#include "SummonWindowBehavior.g.cpp"
#include "WindowRequestedArgs.g.cpp"

using namespace winrt;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Windows::Foundation;

namespace winrt::TerminalApp::implementation
{
    ::TerminalApp::AppCommandlineArgs& CommandlineArgs::ParsedArgs() noexcept
    {
        return _parsed;
    }

    winrt::com_array<winrt::hstring>& CommandlineArgs::CommandlineRef() noexcept
    {
        return _args;
    }

    int32_t CommandlineArgs::ExitCode() const noexcept
    {
        return _parseResult;
    }

    winrt::hstring CommandlineArgs::ExitMessage() const
    {
        return winrt::to_hstring(_parsed.GetExitMessage());
    }

    winrt::hstring CommandlineArgs::TargetWindow() const
    {
        return winrt::to_hstring(_parsed.GetTargetWindow());
    }

    void CommandlineArgs::Commandline(const winrt::array_view<const winrt::hstring>& value)
    {
        _args = { value.begin(), value.end() };
        _parseResult = _parsed.ParseArgs(_args);
    }

    winrt::com_array<winrt::hstring> CommandlineArgs::Commandline()
    {
        return winrt::com_array<winrt::hstring>{ _args.begin(), _args.end() };
    }
}
