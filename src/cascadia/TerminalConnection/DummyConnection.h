// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "DummyConnection.g.h"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct DummyConnection : DummyConnectionT<DummyConnection>
    {
        DummyConnection() noexcept;

        void Start() noexcept;
        void WriteInput(const winrt::array_view<const char16_t> buffer);
        void Resize(uint32_t rows, uint32_t columns) noexcept;
        void Close() noexcept;

        void Initialize(const Windows::Foundation::Collections::ValueSet& /*settings*/) const noexcept {};

        winrt::guid SessionId() const noexcept { return {}; }
        ConnectionState State() const noexcept { return ConnectionState::Connected; }

        til::event<TerminalOutputHandler> TerminalOutput;
        til::event<TerminalOutputHandler> TerminalInput;
        til::typed_event<ITerminalConnection, IInspectable> StateChanged;

        bool _rawMode { false };
    };
}

namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    BASIC_FACTORY(DummyConnection);
}
