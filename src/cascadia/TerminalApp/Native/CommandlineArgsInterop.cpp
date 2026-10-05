// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../RemotingInterop.h"
#include "../NativeCommandlineArgsState.h"
#include "../Remoting.h"

namespace TerminalApp::Native
{
    CommandlineArgsRef CommandlineArgsInterop::FromProjected(const winrt::TerminalApp::CommandlineArgs& value)
    {
        if (!value) { return nullptr; }
        auto native = winrt::get_self<winrt::TerminalApp::implementation::CommandlineArgs>(value)->NativeArgs();
        const std::lock_guard lock{ native->_state->adapterMutex };
        if (!native->_state->adapter.get())
        {
            native->_state->adapter = winrt::make_weak(value.as<winrt::Windows::Foundation::IInspectable>());
        }
        return native;
    }

    winrt::TerminalApp::CommandlineArgs CommandlineArgsInterop::ToProjected(const CommandlineArgsRef& value)
    {
        if (!value) { return nullptr; }
        const std::lock_guard lock{ value->_state->adapterMutex };
        if (auto existing = value->_state->adapter.get())
        {
            return existing.as<winrt::TerminalApp::CommandlineArgs>();
        }
        auto adapter = winrt::make<winrt::TerminalApp::implementation::CommandlineArgs>(value);
        value->_state->adapter = winrt::make_weak(adapter.as<winrt::Windows::Foundation::IInspectable>());
        return adapter;
    }

    winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection CommandlineArgsInterop::Connection(const CommandlineArgsRef& value)
    {
        return value->_state->connection;
    }

    void CommandlineArgsInterop::Connection(const CommandlineArgsRef& value, const winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection& connection)
    {
        value->_state->connection = connection;
    }
}
