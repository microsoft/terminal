// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AzureConnection-ARM64.h"
#include "AzureConnection.g.cpp"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    bool AzureConnection::IsAzureConnectionAvailable()
    {
        return false;
    }
    AzureConnection::AzureConnection(uint32_t rows, uint32_t columns)
    {
        throw hresult_not_implemented();
    }
    winrt::event_token AzureConnection::TerminalOutput(Microsoft::Terminal::TerminalConnection::TerminalOutputEventArgs const& handler)
    {
        throw hresult_not_implemented();
    }
    void AzureConnection::TerminalOutput(winrt::event_token const& token)
    {
        throw hresult_not_implemented();
    }
    winrt::event_token AzureConnection::TerminalDisconnected(Microsoft::Terminal::TerminalConnection::TerminalDisconnectedEventArgs const& handler)
    {
        throw hresult_not_implemented();
    }
    void AzureConnection::TerminalDisconnected(winrt::event_token const& token)
    {
        throw hresult_not_implemented();
    }
    void AzureConnection::Start()
    {
        throw hresult_not_implemented();
    }
    void AzureConnection::WriteInput(hstring const& data)
    {
        throw hresult_not_implemented();
    }
    void AzureConnection::Resize(uint32_t rows, uint32_t columns)
    {
        throw hresult_not_implemented();
    }
    void AzureConnection::Close()
    {
        throw hresult_not_implemented();
    }
}
