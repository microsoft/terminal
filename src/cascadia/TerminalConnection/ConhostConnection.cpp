// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ConhostConnection.h"
#include "windows.h"
#include <sstream>
// STARTF_USESTDHANDLES is only defined in WINAPI_PARTITION_DESKTOP
// We're just gonna manually define it for this prototyping code
#ifndef STARTF_USESTDHANDLES
#define STARTF_USESTDHANDLES       0x00000100
#endif

#include <conpty-universal.h>

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    ConhostConnection::ConhostConnection(hstring const& commandline,
                                         hstring const& startingDirectory,
                                        uint32_t initialRows,
                                        uint32_t initialCols) :
        _connected{ false },
        _inPipe{ INVALID_HANDLE_VALUE },
        _outPipe{ INVALID_HANDLE_VALUE },
        _signalPipe{ INVALID_HANDLE_VALUE },
        _outputThreadId{ 0 },
        _hOutputThread{ INVALID_HANDLE_VALUE },
        _piConhost{ 0 },
        _closing{ false }
    {
        _commandline = commandline;
        _startingDirectory = startingDirectory;
        _initialRows = initialRows;
        _initialCols = initialCols;

    }

    winrt::event_token ConhostConnection::TerminalOutput(Microsoft::Terminal::TerminalConnection::TerminalOutputEventArgs const& handler)
    {
        return _outputHandlers.add(handler);
    }

    void ConhostConnection::TerminalOutput(winrt::event_token const& token) noexcept
    {
        _outputHandlers.remove(token);
    }

    winrt::event_token ConhostConnection::TerminalDisconnected(Microsoft::Terminal::TerminalConnection::TerminalDisconnectedEventArgs const& handler)
    {
        return _disconnectHandlers.add(handler);
    }

    void ConhostConnection::TerminalDisconnected(winrt::event_token const& token) noexcept
    {
        _disconnectHandlers.remove(token);
    }

    void ConhostConnection::Start()
    {
        std::wstring cmdline = _commandline.c_str();
        std::optional<std::wstring> startingDirectory;
        if (!_startingDirectory.empty())
        {
            startingDirectory = _startingDirectory;
        }

        CreateConPty(cmdline,
                     startingDirectory,
                     static_cast<short>(_initialCols),
                     static_cast<short>(_initialRows),
                     &_inPipe,
                     &_outPipe,
                     &_signalPipe,
                     &_piConhost);

        _connected = true;

        // Create our own output handling thread
        // Each console needs to make sure to drain the output from it's backing host.
        _outputThreadId = (DWORD)-1;
        _hOutputThread = CreateThread(nullptr,
                                      0,
                                      StaticOutputThreadProc,
                                      this,
                                      0,
                                      &_outputThreadId);
    }

    void ConhostConnection::WriteInput(hstring const& data)
    {
        if (!_connected || _closing)
        {
            return;
        }

        // convert from UTF-16LE to UTF-8 as ConPty expects UTF-8
        std::string str = winrt::to_string(data);
        bool fSuccess = !!WriteFile(_inPipe, str.c_str(), (DWORD)str.length(), nullptr, nullptr);
        fSuccess;
    }

    void ConhostConnection::Resize(uint32_t rows, uint32_t columns)
    {
        if (!_connected)
        {
            _initialRows = rows;
            _initialCols = columns;
        }
        else if (!_closing)
        {
            SignalResizeWindow(_signalPipe, static_cast<unsigned short>(columns), static_cast<unsigned short>(rows));
        }
    }

    void ConhostConnection::Close()
    {
        if (!_connected) return;
        if (_closing) return;
        _closing = true;
        // TODO:
        //      terminate the output thread
        //      Close our handles
        //      Close the Pseudoconsole
        //      terminate our processes
        CloseHandle(_signalPipe);
        CloseHandle(_inPipe);
        CloseHandle(_outPipe);
        // What? CreateThread is in app partition but TerminateThread isn't?
        //TerminateThread(_hOutputThread, 0);
        TerminateProcess(_piConhost.hProcess, 0);
        CloseHandle(_piConhost.hProcess);
    }

    DWORD WINAPI ConhostConnection::StaticOutputThreadProc(LPVOID lpParameter)
    {
        ConhostConnection* const pInstance = (ConhostConnection*)lpParameter;
        return pInstance->_OutputThread();
    }

    DWORD ConhostConnection::_OutputThread()
    {
        const size_t bufferSize = 4096;
        BYTE buffer[bufferSize];
        DWORD dwRead;
        while (true)
        {
            dwRead = 0;
            bool fSuccess = false;

            fSuccess = !!ReadFile(_outPipe, buffer, bufferSize, &dwRead, nullptr);
            if (!fSuccess)
            {
                if (_closing)
                {
                    // This is okay, break out to kill the thread
                    return 0;
                }
                else
                {
                    _disconnectHandlers();
                    return (DWORD)-1;
                }

            }
            if (dwRead == 0) continue;
            // Convert buffer to hstring
            char* pchStr = (char*)(buffer);
            std::string str{pchStr, dwRead};
            auto hstr = winrt::to_hstring(str);

            // Pass the output to our registered event handlers
            _outputHandlers(hstr);
        }
    }
}
