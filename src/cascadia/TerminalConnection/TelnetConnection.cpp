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
    TelnetConnection::TelnetConnection(const hstring& hostname) :
        _reader{ nullptr },
        _writer{ nullptr },
        _hostname{ hostname }
    {
        _session.install(_nawsServer);
        _nawsServer.activate([&](auto&&) {});
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - creates the output thread
    void TelnetConnection::Start()
    try
    {
        // Create our own output handling thread
        // Each connection needs to make sure to drain the output from its backing host.
        _hOutputThread.reset(CreateThread(
            nullptr,
            0,
            [](LPVOID lpParameter) {
                auto pInstance = reinterpret_cast<TelnetConnection*>(lpParameter);
                return pInstance->_outputThread();
            },
            this,
            0,
            nullptr));

        THROW_LAST_ERROR_IF_NULL(_hOutputThread);

        _transitionToState(ConnectionState::Connecting);

        // Set initial winodw title.
        _TerminalOutputHandlers(L"\x1b]0;Telnet\x7");
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
        _transitionToState(ConnectionState::Failed);
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - handles the different possible inputs in the different states
    // Arguments:
    // the user's input
    void TelnetConnection::WriteInput(hstring const& data)
    {
        if (!_isStateOneOf(ConnectionState::Connected, ConnectionState::Connecting))
        {
            return;
        }

        auto str = winrt::to_string(data);
        if (str.size() == 1 && str.at(0) == L'\r')
        {
            str = "\r\n";
        }

        telnetpp::bytes bytes(reinterpret_cast<const uint8_t*>(str.data()), str.size());
        _session.send(bytes, [=](telnetpp::bytes data) {
            _socketSend(data);
        });
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - resizes the terminal
    // Arguments:
    // - the new rows/cols values
    void TelnetConnection::Resize(uint32_t rows, uint32_t columns)
    {
        if (_prevResize.has_value() && _prevResize.value().first == rows && _prevResize.value().second == columns)
        {
            return;
        }

        _prevResize.emplace(std::pair{ rows, columns });

        _nawsServer.set_window_size(gsl::narrow<uint16_t>(columns),
                                    gsl::narrow<uint16_t>(rows),
                                    [=](telnetpp::subnegotiation sub) {
                                        _session.send(sub,
                                                      [=](telnetpp::bytes data) {
                                                          _socketBufferedSend(data);
                                                      });
                                        _socketFlushBuffer();
                                    });
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - closes the socket connection and the output thread
    void TelnetConnection::Close()
    try
    {
        if (_transitionToState(ConnectionState::Closing))
        {
            _socket.Close();
            if (_hOutputThread)
            {
                // Tear down our output thread
                WaitForSingleObject(_hOutputThread.get(), INFINITE);
                _hOutputThread.reset();
            }

            _transitionToState(ConnectionState::Closed);
        }
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
        _transitionToState(ConnectionState::Failed);
    }

    // Method description:
    // - this is the output thread, where we initiate the connection to the remote host
    //  and establish a socket connection
    // Return value:
    // - return status
    DWORD TelnetConnection::_outputThread()
    try
    {
        while (true)
        {
            if (_isStateOneOf(ConnectionState::Failed))
            {
                _TerminalOutputHandlers(RS_(L"TelnetInternetOrServerIssue") + L"\r\n");
                return E_FAIL;
            }
            else if (_isStateAtOrBeyond(ConnectionState::Closing))
            {
                return S_FALSE;
            }
            else if (_isStateOneOf(ConnectionState::Connecting))
            {
                try
                {
                    const auto host = Windows::Networking::HostName(_hostname);
                    _socket.ConnectAsync(host, L"23").get();
                    _writer = Windows::Storage::Streams::DataWriter(_socket.OutputStream());
                    _reader = Windows::Storage::Streams::DataReader(_socket.InputStream());
                    _reader.InputStreamOptions(Windows::Storage::Streams::InputStreamOptions::Partial); // returns when 1 or more bytes ready.
                    _transitionToState(ConnectionState::Connected);

                    // Send newline to bypass User Name prompt.
                    const auto newline = winrt::to_hstring("\r\n");
                    WriteInput(newline);

                    // Wait for login.
                    Sleep(1000);

                    // Send "cls" enter to clear the thing and just look like a prompt.
                    const auto clearScreen = winrt::to_hstring("cls\r\n");
                    WriteInput(clearScreen);
                }
                catch (...)
                {
                    LOG_CAUGHT_EXCEPTION();
                    _transitionToState(ConnectionState::Failed);
                }
            }
            else if (_isStateOneOf(ConnectionState::Connected))
            {
                // Read from socket
                const auto amountReceived = _socketReceive(_receiveBuffer);

                _session.receive(
                    telnetpp::bytes{ _receiveBuffer.data(), amountReceived },
                    [=](telnetpp::bytes data,
                        std::function<void(telnetpp::bytes)> const& send) {
                        _applicationReceive(data, send);
                    },
                    [=](telnetpp::bytes data) {
                        _socketSend(data);
                    });
            }
        }
    }
    catch (...)
    {
        // If the exception was thrown while we were already supposed to be closing, fine. We're closed.
        // This is because the socket got mad things were being torn down.
        if (_isStateAtOrBeyond(ConnectionState::Closing))
        {
            _transitionToState(ConnectionState::Closed);
            return S_OK;
        }
        else
        {
            LOG_CAUGHT_EXCEPTION();
            _transitionToState(ConnectionState::Failed);
            return E_FAIL;
        }
    }

    // Routine Description:
    // - Call to buffer up bytes to send to the remote device.
    // - You must flush before they'll go out.
    // Arguments:
    // - data - View of bytes to be sent
    // Return Value:
    // - <none>
    void TelnetConnection::_socketBufferedSend(telnetpp::bytes data)
    {
        const uint8_t* first = data.data();
        const uint8_t* last = data.data() + data.size();
        const winrt::array_view<const uint8_t> arrayView(first, last);
        _writer.WriteBytes(arrayView);
    }

    // Routine Description:
    // - Flushes any buffered bytes to the underlying socket
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TelnetConnection::_socketFlushBuffer()
    {
        _writer.StoreAsync().get();
    }

    // Routine Description:
    // - Used to send bytes into the socket to the remote device
    // Arguments:
    // - data - View of bytes to be sent
    // Return Value:
    // - <none>
    void TelnetConnection::_socketSend(telnetpp::bytes data)
    {
        _socketBufferedSend(data);
        _socketFlushBuffer();
    }

    // Routine Description:
    // - Reads bytes from the socket into the given array.
    // Arguments:
    // - buffer - The array of bytes to use for storage
    // Return Value:
    // - The number of bytes actually read (less than or equal to input array size)
    size_t TelnetConnection::_socketReceive(gsl::span<telnetpp::byte> buffer)
    {
        const auto bytesLoaded = _reader.LoadAsync(gsl::narrow<uint32_t>(buffer.size())).get();

        // winrt::array_view, despite having a pointer and size constructor
        // hides it as protected.
        // So we have to get first/last (even though cppcorechecks will be
        // mad at us for it) to use a public array_view constructor.
        // The WinRT team isn't fixing this because std::span is coming
        // soon and that will do it.
        // So just do this for now and suppress the warnings.
        const auto first = buffer.data();
        const auto last = first + bytesLoaded;
        const winrt::array_view<uint8_t> arrayView(first, last);
        _reader.ReadBytes(arrayView);
        return bytesLoaded;
    }

    // Routine Description:
    // - Called by telnetpp framework when application data is received on the channel
    //   In contrast, telnet metadata payload is consumed by telnetpp and not forwarded to us.
    // Arguments:
    // - data - The relevant application-level payload received
    // - send - A function where we can send a reply to given data immediately
    //          in reaction to the received message.
    // Return Value:
    // - <none>
    void TelnetConnection::_applicationReceive(telnetpp::bytes data,
                                               std::function<void(telnetpp::bytes)> const& /*send*/)
    {
        // Convert telnetpp bytes to standard string_view
        const auto stringView = std::string_view{ reinterpret_cast<const char*>(data.data()), gsl::narrow<size_t>(data.size()) };

        // Convert to hstring
        const auto hstr = winrt::to_hstring(stringView);

        // Pass the output to our registered event handlers
        _TerminalOutputHandlers(hstr);
    }
}
