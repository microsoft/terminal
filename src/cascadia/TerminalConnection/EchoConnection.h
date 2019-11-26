// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "EchoConnection.g.h"

#include "../cascadia/inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct EchoConnection : EchoConnectionT<EchoConnection>
    {
        EchoConnection();

        void Start();
        void WriteInput(hstring const& data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();

        ConnectionState State() const noexcept { return ConnectionState::Connected; }

        WINRT_CALLBACK(TerminalOutput, TerminalOutputHandler);
        TYPED_EVENT(StateChanged, ITerminalConnection, IInspectable);
    };
}

namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    struct EchoConnection : EchoConnectionT<EchoConnection, implementation::EchoConnection>
    {
    };
}
