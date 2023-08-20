// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "EchoConnection.g.h"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct EchoConnection : EchoConnectionT<EchoConnection>
    {
        EchoConnection() noexcept;

        void Start() noexcept;
        void WriteInput(const hstring& data);
        void Resize(uint32_t rows, uint32_t columns) noexcept;
        void Close() noexcept;

        void Initialize(const Windows::Foundation::Collections::ValueSet& /*settings*/) const noexcept {};

        ConnectionState State() const noexcept { return ConnectionState::Connected; }

        WINRT_CALLBACK(TerminalOutput, TerminalOutputHandler);
        TYPED_EVENT(StateChanged, ITerminalConnection, IInspectable);
    };
}

namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    BASIC_FACTORY(EchoConnection);
}
