// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Module Name:
// - PreviewConnection.h
//
// Abstract:
// - This class is used to initialize the preview TermControl in the Settings UI
//
// Author:
// - Pankaj Bhojwani March-2021

#pragma once

#include <winrt/Microsoft.Terminal.Connection.h>

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    class PreviewConnection : public winrt::implements<PreviewConnection, winrt::Microsoft::Terminal::Connection::ITerminalConnection>
    {
    public:
        PreviewConnection() noexcept;

        void Initialize(const Windows::Foundation::Collections::ValueSet& settings) noexcept;
        void Start() noexcept;
        void WriteInput(const hstring& data);
        void Resize(uint32_t rows, uint32_t columns) noexcept;
        void Close() noexcept;

        winrt::Microsoft::Terminal::Connection::ConnectionState State() const noexcept { return winrt::Microsoft::Terminal::Connection::ConnectionState::Connected; }

        WINRT_CALLBACK(TerminalOutput, winrt::Microsoft::Terminal::Connection::TerminalOutputHandler);
        TYPED_EVENT(StateChanged, winrt::Microsoft::Terminal::Connection::ITerminalConnection, IInspectable);
    };
}
