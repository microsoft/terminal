// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// This is literally just the EchoConnection, but we can't use the
// EchoConnection because it's in Microsoft.Terminal.Connection.dll and loading that in
// these tests is fraught with peril. Easier just to have a local copy.

#pragma once

namespace ControlUnitTests
{
    class MockConnection : public winrt::implements<MockConnection, winrt::Microsoft::Terminal::Connection::ITerminalConnection>
    {
    public:
        MockConnection() noexcept = default;

        void Initialize(const winrt::Windows::Foundation::Collections::ValueSet& /*settings*/){};
        void Start() noexcept {};
        void WriteInput(const winrt::hstring& data)
        {
            _TerminalOutputHandlers(data);
        }
        void Resize(uint32_t /*rows*/, uint32_t /*columns*/) noexcept {}
        void Close() noexcept {}

        winrt::Microsoft::Terminal::Connection::ConnectionState State() const noexcept { return winrt::Microsoft::Terminal::Connection::ConnectionState::Connected; }

        WINRT_CALLBACK(TerminalOutput, winrt::Microsoft::Terminal::Connection::TerminalOutputHandler);
        TYPED_EVENT(StateChanged, winrt::Microsoft::Terminal::Connection::ITerminalConnection, IInspectable);
    };
}
