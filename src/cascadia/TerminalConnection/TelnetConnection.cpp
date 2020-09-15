// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TelnetConnection.h"
#include <LibraryResources.h>

#include "TelnetConnection.g.cpp"

#include "../../types/inc/Utils.hpp"

using namespace ::Microsoft::Console;

constexpr std::wstring_view telnetScheme = L"telnet";
constexpr std::wstring_view msTelnetLoopbackScheme = L"ms-telnet-loop";

// {311153fb-d3f0-4ac6-b920-038de7cf5289}
static constexpr winrt::guid TelnetConnectionType = { 0x311153fb, 0xd3f0, 0x4ac6, { 0xb9, 0x20, 0x03, 0x8d, 0xe7, 0xcf, 0x52, 0x89 } };

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    winrt::guid TelnetConnection::ConnectionType()
    {
        return TelnetConnectionType;
    }

    TelnetConnection::TelnetConnection(const hstring& uri) :
        _reader{ nullptr },
        _writer{ nullptr },
        _uri{ uri },
        _receiveBuffer{}
    {
        _session.install(_nawsServer);
        _nawsServer.activate([](auto&&) {});
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
                auto pInstance = static_cast<TelnetConnection*>(lpParameter);
                if (pInstance)
                {
                    return pInstance->_outputThread();
                }
                return gsl::narrow_cast<DWORD>(ERROR_BAD_ARGUMENTS);
            },
            this,
            0,
            nullptr));

        THROW_LAST_ERROR_IF_NULL(_hOutputThread);

        _transitionToState(ConnectionState::Connecting);

        // Set initial window title.
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

#pragma warning(suppress : 26490) // Using something that isn't reinterpret_cast to forward stream bytes is more clumsy than just using it.
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
                    const auto uri = Windows::Foundation::Uri(_uri);
                    const auto host = Windows::Networking::HostName(uri.Host());

                    bool autoLogin = false;
                    // If we specified the special ms loopback scheme, then set autologin and proceed below.
                    if (msTelnetLoopbackScheme == uri.SchemeName())
                    {
                        autoLogin = true;
                    }
                    // Otherwise, make sure we said telnet://, anything else is not supported here.
                    else if (telnetScheme != uri.SchemeName())
                    {
                        THROW_HR(E_INVALIDARG);
                    }

                    _socket.ConnectAsync(host, winrt::to_hstring(uri.Port())).get();
                    _writer = Windows::Storage::Streams::DataWriter(_socket.OutputStream());
                    _reader = Windows::Storage::Streams::DataReader(_socket.InputStream());
                    _reader.InputStreamOptions(Windows::Storage::Streams::InputStreamOptions::Partial); // returns when 1 or more bytes ready.
                    _transitionToState(ConnectionState::Connected);

                    if (autoLogin)
                    {
                        // Send newline to bypass User Name prompt.
                        const auto newline = winrt::to_hstring("\r\n");
                        WriteInput(newline);

                        // Wait for login.
                        Sleep(1000);

                        // Send "cls" enter to clear the thing and just look like a prompt.
                        const auto clearScreen = winrt::to_hstring("cls\r\n");
                        WriteInput(clearScreen);
                    }
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
        // winrt::array_view should take data/size but it doesn't.
        // We contacted the WinRT owners and they said, more or less, that it's not worth fixing
        // with std::span on the horizon instead of this. So we're suppressing the warning
        // and hoping for a std::span future that will eliminate winrt::array_view<T>
#pragma warning(push)
#pragma warning(disable : 26481)
        const uint8_t* first = data.data();
        const uint8_t* last = data.data() + data.size();
#pragma warning(pop)

        const winrt::array_view<const uint8_t> arrayView(first, last);
        _writer.WriteBytes(arrayView);
    }

    // Routine Description:
    // - Flushes any buffered bytes to the underlying socket
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    fire_and_forget TelnetConnection::_socketFlushBuffer()
    {
        co_await _writer.StoreAsync();
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
#pragma warning(push)
#pragma warning(disable : 26481)
        const auto first = buffer.data();
        const auto last = first + bytesLoaded;
#pragma warning(pop)

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
#pragma warning(suppress : 26490) // Using something that isn't reinterpret_cast to forward stream bytes is more clumsy than just using it.
        const auto stringView = std::string_view{ reinterpret_cast<const char*>(data.data()), gsl::narrow<size_t>(data.size()) };

        // Convert to hstring
        const auto hstr = winrt::to_hstring(stringView);

        // Pass the output to our registered event handlers
        _TerminalOutputHandlers(hstr);
    }
}
