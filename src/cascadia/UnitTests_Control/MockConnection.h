// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// This is literally just the EchoConnection, but we can't use the
// EchoConnection because it's in TerminalConnection.dll and loading that in
// these tests is fraught with peril. Easier just to have a local copy.

#pragma once

namespace ControlUnitTests
{
    class MockConnection : public winrt::implements<MockConnection, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection>
    {
    public:
        MockConnection() noexcept = default;

        void Initialize(const winrt::Windows::Foundation::Collections::ValueSet& /*settings*/){};
        void Start() noexcept {};
        void WriteInput(const winrt::array_view<const char16_t> data)
        {
            TerminalOutput.raise(winrt_array_to_wstring_view(data));
        }
        void Resize(uint32_t /*rows*/, uint32_t /*columns*/) noexcept {}
        void Close() noexcept {}

        winrt::guid SessionId() const noexcept { return {}; }
        winrt::Microsoft::Terminal::TerminalConnection::ConnectionState State() const noexcept { return winrt::Microsoft::Terminal::TerminalConnection::ConnectionState::Connected; }

        til::event<winrt::Microsoft::Terminal::TerminalConnection::TerminalOutputHandler> TerminalOutput;
        til::typed_event<winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection, IInspectable> StateChanged;
    };
}
