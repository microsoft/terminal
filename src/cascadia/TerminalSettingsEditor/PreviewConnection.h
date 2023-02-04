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

#include <winrt/Microsoft.Terminal.TerminalConnection.h>

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    class PreviewConnection : public winrt::implements<PreviewConnection, MTConnection::ITerminalConnection>
    {
    public:
        PreviewConnection() noexcept;

        void Initialize(const WFC::ValueSet& settings) noexcept;
        void Start() noexcept;
        void WriteInput(const hstring& data);
        void Resize(uint32_t rows, uint32_t columns) noexcept;
        void Close() noexcept;

        MTConnection::ConnectionState State() const noexcept { return MTConnection::ConnectionState::Connected; }

        WINRT_CALLBACK(TerminalOutput, MTConnection::TerminalOutputHandler);
        TYPED_EVENT(StateChanged, MTConnection::ITerminalConnection, IInspectable);
    };
}
