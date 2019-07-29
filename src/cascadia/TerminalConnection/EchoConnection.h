// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "EchoConnection.g.h"

// FIXME: idk how to include this form cppwinrt_utils.h
#define DECLARE_EVENT(name, eventHandler, args)          \
public:                                                  \
    winrt::event_token name(args const& handler);        \
    void name(winrt::event_token const& token) noexcept; \
                                                         \
private:                                                 \
    winrt::event<args> eventHandler;

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct EchoConnection : EchoConnectionT<EchoConnection>
    {
        EchoConnection();

        void Start();
        void WriteInput(hstring const& data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();

        bool AllowsUserInput();
        VisualConnectionState VisualConnectionState();
        hstring GetTabTitle(hstring terminalTitle);

        DECLARE_EVENT(TerminalOutput, _outputHandlers, TerminalConnection::TerminalOutputEventArgs);
        DECLARE_EVENT(TerminalDisconnected, _disconnectHandlers, TerminalConnection::TerminalDisconnectedEventArgs);
        DECLARE_EVENT(StateChanged, _stateChangedHandlers, TerminalConnection::StateChangedEventArgs);
    private:
    };
}

namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    struct EchoConnection : EchoConnectionT<EchoConnection, implementation::EchoConnection>
    {
    };
}
