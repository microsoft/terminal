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
    class PreviewConnection : public winrt::implements<PreviewConnection, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection>
    {
    public:
        PreviewConnection() noexcept;

        void Initialize(const Windows::Foundation::Collections::ValueSet& settings) noexcept;
        void Start() noexcept;
        void WriteInput(const hstring& data);
        void Resize(uint32_t rows, uint32_t columns) noexcept;
        void Close() noexcept;
        uint32_t Rows() noexcept { return 0; }
        uint32_t Columns() noexcept { return 0; }

        void DisplayPowerlineGlyphs(bool d) noexcept;

        winrt::guid SessionId() const noexcept { return {}; }
        winrt::Microsoft::Terminal::TerminalConnection::ConnectionState State() const noexcept { return winrt::Microsoft::Terminal::TerminalConnection::ConnectionState::Connected; }

        til::event<winrt::Microsoft::Terminal::TerminalConnection::TerminalOutputHandler> TerminalOutput;
        til::typed_event<winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection, IInspectable> StateChanged;
        til::typed_event<winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection, IInspectable> SizeChanged;

    private:
        bool _displayPowerlineGlyphs{ false };
    };
}
