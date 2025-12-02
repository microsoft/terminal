// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "TmuxConnection.g.h"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct TmuxConnection : TmuxConnectionT<TmuxConnection>
    {
        TmuxConnection() noexcept;

        // ---- ITerminalConnection methods ----

        void Initialize(const Windows::Foundation::Collections::ValueSet& /*settings*/) const noexcept;

        void Start() noexcept;
        void WriteInput(const winrt::array_view<const char16_t> buffer);
        void Resize(uint32_t rows, uint32_t columns) noexcept;
        void Close() noexcept;

        til::event<TerminalOutputHandler> TerminalOutput;
        til::typed_event<ITerminalConnection, IInspectable> StateChanged;

        winrt::guid SessionId() const noexcept;
        ConnectionState State() const noexcept;

        // ---- TmuxConnection methods ----

        void WriteOutput(const winrt::array_view<const char16_t> wstr);

        til::event<TerminalOutputHandler> TerminalInput;
    };
}

namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    BASIC_FACTORY(TmuxConnection);
}
