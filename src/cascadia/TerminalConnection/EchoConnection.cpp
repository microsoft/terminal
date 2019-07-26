// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "EchoConnection.h"
#include <sstream>

#include "EchoConnection.g.cpp"

// FIXME: idk how to include this form cppwinrt_utils.h
#define DEFINE_EVENT(className, name, eventHandler, args)                                         \
    winrt::event_token className::name(args const& handler) { return eventHandler.add(handler); } \
    void className::name(winrt::event_token const& token) noexcept { eventHandler.remove(token); }

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    EchoConnection::EchoConnection()
    {
    }

    void EchoConnection::Start()
    {
        _connectHandlers(true);
    }

    void EchoConnection::WriteInput(hstring const& data)
    {
        std::wstringstream prettyPrint;
        for (wchar_t wch : data)
        {
            if (wch < 0x20)
            {
                prettyPrint << L"^" << (wchar_t)(wch + 0x40);
            }
            else if (wch == 0x7f)
            {
                prettyPrint << L"0x7f";
            }
            else
            {
                prettyPrint << wch;
            }
        }
        _outputHandlers(prettyPrint.str());
    }

    void EchoConnection::Resize(uint32_t rows, uint32_t columns)
    {
        rows;
        columns;

        throw hresult_not_implemented();
    }

    void EchoConnection::Close()
    {
        throw hresult_not_implemented();
    }

    hstring EchoConnection::GetConnectionFailatureMessage()
    {
        return L"[Echo connection failed]";
    }

    hstring EchoConnection::GetConnectionFailatureTabTitle()
    {
        return L"Failature";
    }

    hstring EchoConnection::GetDisconnectionMessage()
    {
        return L"[Echo connection closed]";
    }

    hstring EchoConnection::GetDisconnectionTabTitle(hstring previousTitle)
    {
        previousTitle;
        return L"Exited";
    }

    DEFINE_EVENT(EchoConnection, TerminalConnected, _connectHandlers, TerminalConnection::TerminalConnectedEventArgs);
    DEFINE_EVENT(EchoConnection, TerminalDisconnected, _disconnectHandlers, TerminalConnection::TerminalDisconnectedEventArgs);
    DEFINE_EVENT(EchoConnection, TerminalOutput, _outputHandlers, TerminalConnection::TerminalOutputEventArgs);
}
