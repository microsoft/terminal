// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TelnetConnection.h"
#include <LibraryResources.h>

#include "TelnetConnection.g.cpp"

#include "../../types/inc/Utils.hpp"

using namespace ::Microsoft::Console;

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    TelnetConnection::TelnetConnection() :
        _reader(nullptr)
    {
       
    }

    // Method description:
    // - helper that will write an unterminated string (generally, from a resource) to the output stream.
    // Arguments:
    // - str: the string to write.
    void TelnetConnection::_WriteStringWithNewline(const winrt::hstring& str)
    {
        _outputHandlers(str + L"\r\n");
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - registers an output event handler
    // Arguments:
    // - the handler
    // Return value:
    // - the event token for the handler
    winrt::event_token TelnetConnection::TerminalOutput(Microsoft::Terminal::TerminalConnection::TerminalOutputEventArgs const& handler)
    {
        return _outputHandlers.add(handler);
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - revokes an output event handler
    // Arguments:
    // - the event token for the handler
    void TelnetConnection::TerminalOutput(winrt::event_token const& token) noexcept
    {
        _outputHandlers.remove(token);
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - registers a terminal-disconnected event handler
    // Arguments:
    // - the handler
    // Return value:
    // - the event token for the handler
    winrt::event_token TelnetConnection::TerminalDisconnected(Microsoft::Terminal::TerminalConnection::TerminalDisconnectedEventArgs const& handler)
    {
        return _disconnectHandlers.add(handler);
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - revokes a terminal-disconnected event handler
    // Arguments:
    // - the event token for the handler
    void TelnetConnection::TerminalDisconnected(winrt::event_token const& token) noexcept
    {
        _disconnectHandlers.remove(token);
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - creates the output thread (where we will do the authentication and actually connect to Azure)
    void TelnetConnection::Start()
    {
        // Create our own output handling thread
        // Each connection needs to make sure to drain the output from its backing host.
        _hOutputThread.reset(CreateThread(nullptr,
                                          0,
                                          StaticOutputThreadProc,
                                          this,
                                          0,
                                          nullptr));

        THROW_LAST_ERROR_IF_NULL(_hOutputThread);

        _connected = true;
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - handles the different possible inputs in the different states
    // Arguments:
    // the user's input
    void TelnetConnection::WriteInput(hstring const& data)
    {
        if (!_connected || _closing.load())
        {
            return;
        }

        // Parse the input differently depending on which state we're in
        switch (_state)
        {
        // The user has stored connection settings, let them choose one of them, create a new one or remove all stored ones
        case State::AccessStored:
        {
            return;
        }
        // The user has multiple tenants in their Azure account, let them choose one of them
        case State::TenantChoice:
        {
            return;
        }
        // User has the option to save their connection settings for future logins
        case State::StoreTokens:
        {
            return;
        }
        // We are connected, send user's input over the websocket
        case State::TermConnected:
        {
            const auto str = winrt::to_string(data);
            telnetpp::bytes bytes(reinterpret_cast<const uint8_t*>(str.data()), str.size());
            _session.send(bytes, [=](telnetpp::bytes data) {
                _socketSend(data);
            });

            return;
        }
        default:
            return;
        }
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - resizes the terminal
    // Arguments:
    // - the new rows/cols values
    void TelnetConnection::Resize(uint32_t /*rows*/, uint32_t /*columns*/)
    {
        if (!_connected || !(_state == State::TermConnected))
        {
            /*_initialRows = rows;
            _initialCols = columns;*/
        }
        else if (!_closing.load())
        {
        }
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - closes the websocket connection and the output thread
    void TelnetConnection::Close()
    {
        if (!_connected)
        {
            return;
        }

        if (!_closing.exchange(true))
        {
            _canProceed.notify_all();
            if (_state == State::TermConnected)
            {
                // Close the websocket connection
            }

            // Tear down our output thread
            WaitForSingleObject(_hOutputThread.get(), INFINITE);
            _hOutputThread.reset();
        }
    }

    // Method description:
    // - this method bridges the thread to the Azure connection instance
    // Arguments:
    // - lpParameter: the Azure connection parameter
    // Return value:
    // - the exit code of the thread
    DWORD WINAPI TelnetConnection::StaticOutputThreadProc(LPVOID lpParameter)
    {
        TelnetConnection* const pInstance = (TelnetConnection*)lpParameter;
        return pInstance->_OutputThread();
    }

    // Method description:
    // - this is the output thread, where we initiate the connection to Azure
    //  and establish a websocket connection
    // Return value:
    // - return status
    DWORD TelnetConnection::_OutputThread()
    {
        while (true)
        {
            try
            {
                switch (_state)
                {
                // Initial state, check if the user has any stored connection settings and allow them to login with those
                // or allow them to login with a different account or allow them to remove the saved settings
                case State::AccessStored:
                {
                    break;
                }
                // User has no saved connection settings or has opted to login with a different account
                // Azure authentication happens here
                case State::DeviceFlow:
                {
                    break;
                }
                // User has multiple tenants in their Azure account, they need to choose which one to connect to
                case State::TenantChoice:
                {
                    break;
                }
                // Ask the user if they want to save these connection settings for future logins
                case State::StoreTokens:
                {
                    break;
                }
                // Connect to Azure, we only get here once we have everything we need (tenantID, accessToken, refreshToken)
                case State::TermConnecting:
                {
                    const auto host = Windows::Networking::HostName(L"10.137.116.249");
                    _socket = Windows::Networking::Sockets::StreamSocket();
                    _socket.ConnectAsync(host, L"23").get();
                    _writer = Windows::Storage::Streams::DataWriter(_socket.OutputStream());
                    _reader = Windows::Storage::Streams::DataReader(_socket.InputStream());
                    _reader.InputStreamOptions(Windows::Storage::Streams::InputStreamOptions::Partial); // returns when 1 or more bytes ready.
                    _connected = true;
                    _state = State::TermConnected;
                    break;
                }
                // We are connected, continuously read from the websocket until its closed
                case State::TermConnected:
                {
                    while (true)
                    {
                        // Read from websocket
                        const auto amountReceived = _socketReceive(_receiveBuffer, ARRAYSIZE(_receiveBuffer));

                        _session.receive(
                            telnetpp::bytes{ _receiveBuffer, gsl::narrow<size_t>(amountReceived) },
                            [=](telnetpp::bytes data,
                                std::function<void(telnetpp::bytes)> const& send) {
                                _applicationReceive(data, send);
                            },
                            [=](telnetpp::bytes data) {
                                _socketSend(data);
                            });
                    }
                    return S_OK;
                }
                case State::NoConnect:
                {
                    _WriteStringWithNewline(RS_(L"AzureInternetOrServerIssue"));
                    _disconnectHandlers();
                    return E_FAIL;
                }
                }
            }
            catch (...)
            {
                _state = State::NoConnect;
            }
        }
    }

    winrt::fire_and_forget TelnetConnection::_socketSend(telnetpp::bytes data)
    {
        const uint8_t* first = data.data();
        const uint8_t* last = data.data() + data.size();
        const winrt::array_view<const uint8_t> arrayView(first, last);
        _writer.WriteBytes(arrayView);
        co_await _writer.FlushAsync();
        co_await _socket.OutputStream().FlushAsync();
    }

    int TelnetConnection::_socketReceive(telnetpp::byte* buffer, int size)
    {
        const auto read = _reader.LoadAsync(size).get();

        uint8_t* first = buffer;
        uint8_t* last = buffer + read;
        const winrt::array_view<uint8_t> arrayView(first, last);
        _reader.ReadBytes(arrayView);

        return read;
    }

    void TelnetConnection::_applicationReceive(telnetpp::bytes data,
                                               std::function<void(telnetpp::bytes)> const& /*send*/)
    {
        const auto stringView = std::string_view{ reinterpret_cast<const char*>(data.data()), gsl::narrow<size_t>(data.size()) };

        //Convert to hstring
        const auto hstr = winrt::to_hstring(stringView);

        // Pass the output to our registered event handlers
        _outputHandlers(hstr);
    }

}
