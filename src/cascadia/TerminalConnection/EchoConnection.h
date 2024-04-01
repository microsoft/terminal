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

        winrt::guid SessionId() const noexcept { return {}; }
        ConnectionState State() const noexcept { return ConnectionState::Connected; }

        uint32_t Rows() { return 0; }
        uint32_t Columns() { return 0; }

        til::event<TerminalOutputHandler> TerminalOutput;
        til::typed_event<ITerminalConnection, IInspectable> StateChanged;
        til::typed_event<ITerminalConnection, IInspectable> SizeChanged;
    };
}

namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    BASIC_FACTORY(EchoConnection);
}
