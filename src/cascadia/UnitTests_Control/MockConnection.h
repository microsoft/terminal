// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// This is literally just the EchoConnection, but we can't use the
// EchoConnection because it's in TerminalConnection.dll and loading that in
// these tests is fraught with peril. Easier just to have a local copy.

#pragma once

namespace ControlUnitTests
{
    class MockConnection : public winrt::implements<MockConnection, MTConnection::ITerminalConnection>
    {
    public:
        MockConnection() noexcept = default;

        void Initialize(const WFC::ValueSet& /*settings*/){};
        void Start() noexcept {};
        void WriteInput(const winrt::hstring& data)
        {
            _TerminalOutputHandlers(data);
        }
        void Resize(uint32_t /*rows*/, uint32_t /*columns*/) noexcept {}
        void Close() noexcept {}

        MTConnection::ConnectionState State() const noexcept { return MTConnection::ConnectionState::Connected; }

        WINRT_CALLBACK(TerminalOutput, MTConnection::TerminalOutputHandler);
        TYPED_EVENT(StateChanged, MTConnection::ITerminalConnection, IInspectable);
    };
}
