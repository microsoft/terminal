// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace winrt::TerminalApp::implementation
{
    struct TmuxConnection : winrt::implements<TmuxConnection, Microsoft::Terminal::TerminalConnection::ITerminalConnection>
    {
        // ITerminalConnection methods

        void Initialize(const Windows::Foundation::Collections::ValueSet& /*settings*/) const noexcept;

        void Start() noexcept;
        void WriteInput(winrt::array_view<const char16_t> buffer);
        void Resize(uint32_t rows, uint32_t columns) noexcept;
        void Close() noexcept;

        til::event<Microsoft::Terminal::TerminalConnection::TerminalOutputHandler> TerminalOutput;
        til::typed_event<Microsoft::Terminal::TerminalConnection::ITerminalConnection, IInspectable> StateChanged;

        winrt::guid SessionId() const noexcept;
        Microsoft::Terminal::TerminalConnection::ConnectionState State() const noexcept;

        // TmuxConnection methods

        void WriteOutput(winrt::array_view<const char16_t> wstr);

        til::event<Microsoft::Terminal::TerminalConnection::TerminalOutputHandler> TerminalInput;
    };
}
