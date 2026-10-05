// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "RemotingInterop.h"

#include <cassert>

using TerminalApp::Native::CommandlineArgsInterop;
using TerminalApp::Native::CommandlineArgsRef;

void TestProjectedCommandlineBridge(const CommandlineArgsRef& value)
{
    assert(!CommandlineArgsInterop::ToProjected(nullptr));
    assert(!CommandlineArgsInterop::FromProjected(nullptr));
    {
        const auto first = CommandlineArgsInterop::ToProjected(value);
        const auto second = CommandlineArgsInterop::ToProjected(value);
        assert(first == second);
        assert(CommandlineArgsInterop::FromProjected(first).get() == value.get());
        first.CurrentDirectory(L"C:\\changed through adapter");
        assert(value->CurrentDirectory() == L"C:\\changed through adapter");
        assert(first.CurrentEnvironment() == value->CurrentEnvironment());
        assert(first.ShowWindowCommand() == value->ShowWindowCommand());

        winrt::Microsoft::Terminal::TerminalConnection::ConptyConnection connection;
        CommandlineArgsInterop::Connection(value, connection);
        assert(first.Connection() == connection);
        first.Connection(nullptr);
        assert(!CommandlineArgsInterop::Connection(value));

        winrt::TerminalApp::CommandlineArgs legacy;
        const auto native = CommandlineArgsInterop::FromProjected(legacy);
        assert(CommandlineArgsInterop::ToProjected(native) == legacy);
        native->CurrentDirectory(L"C:\\legacy owner");
        assert(legacy.CurrentDirectory() == L"C:\\legacy owner");
    }
    winrt::clear_factory_cache();
}
