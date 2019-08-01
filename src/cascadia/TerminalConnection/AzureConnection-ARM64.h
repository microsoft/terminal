#pragma once
#include "AzureConnection.g.h"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct AzureConnection : AzureConnectionT<AzureConnection>
    {
        AzureConnection() = default;

        AzureConnection(uint32_t rows, uint32_t columns);
        static bool IsAzureConnectionAvailable();
        winrt::event_token TerminalOutput(Microsoft::Terminal::TerminalConnection::TerminalOutputEventArgs const& handler);
        void TerminalOutput(winrt::event_token const& token);
        winrt::event_token TerminalDisconnected(Microsoft::Terminal::TerminalConnection::TerminalDisconnectedEventArgs const& handler);
        void TerminalDisconnected(winrt::event_token const& token);
        void Start();
        void WriteInput(hstring const& data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();
    };
}
namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    struct AzureConnection : AzureConnectionT<AzureConnection, implementation::AzureConnection>
    {
    };
}
