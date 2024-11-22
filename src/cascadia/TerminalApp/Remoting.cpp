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
    CommandlineArgs::CommandlineArgs(winrt::array_view<const winrt::hstring> args, winrt::hstring currentDirectory, uint32_t showWindowCommand, winrt::hstring envString) :
        _args{ args.begin(), args.end() },
        _cwd{ std::move(currentDirectory) },
        ShowWindowCommand{ showWindowCommand },
        CurrentEnvironment{ std::move(envString) }
    {
        _parseResult = _parsed.ParseArgs(_args);
        if (_parseResult == 0)
        {
            _parsed.ValidateStartupCommands();
        }
    }

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
    }

    winrt::com_array<winrt::hstring> CommandlineArgs::Commandline()
    {
        return winrt::com_array<winrt::hstring>{ _args.begin(), _args.end() };
    }
}
