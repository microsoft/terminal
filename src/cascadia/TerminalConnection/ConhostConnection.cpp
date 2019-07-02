// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ConhostConnection.h"
#include "windows.h"
#include <sstream>
// STARTF_USESTDHANDLES is only defined in WINAPI_PARTITION_DESKTOP
// We're just gonna manually define it for this prototyping code
#ifndef STARTF_USESTDHANDLES
#define STARTF_USESTDHANDLES 0x00000100
#endif

#include "ConhostConnection.g.cpp"

#include <conpty-universal.h>
#include "../../types/inc/Utils.hpp"

using namespace ::Microsoft::Console;

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    ConhostConnection::ConhostConnection(const hstring& commandline,
                                         const hstring& startingDirectory,
                                         const uint32_t initialRows,
                                         const uint32_t initialCols,
                                         const guid& initialGuid) :
        _initialRows{ initialRows },
        _initialCols{ initialCols },
        _commandline{ commandline },
        _startingDirectory{ startingDirectory },
        _guid{ initialGuid }
    {
        if (_guid == guid{})
        {
            _guid = Utils::CreateGuid();
        }
    }

    winrt::guid ConhostConnection::Guid() const noexcept
    {
        return _guid;
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
        std::wstring cmdline{ _commandline.c_str() };
        std::optional<std::wstring> startingDirectory;
        if (!_startingDirectory.empty())
        {
            startingDirectory = _startingDirectory;
        }

        EnvironmentVariableMapW extraEnvVars;
        {
            // Convert connection Guid to string and ignore the enclosing '{}'.
            std::wstring wsGuid{ Utils::GuidToString(_guid) };
            wsGuid.pop_back();

            const wchar_t* const pwszGuid{ wsGuid.data() + 1 };

            // Ensure every connection has the unique identifier in the environment.
            extraEnvVars.emplace(L"WT_SESSION", pwszGuid);
        }

        THROW_IF_FAILED(
            CreateConPty(cmdline,
                         startingDirectory,
                         static_cast<short>(_initialCols),
                         static_cast<short>(_initialRows),
                         &_inPipe,
                         &_outPipe,
                         &_signalPipe,
                         &_piConhost,
                         CREATE_SUSPENDED,
                         extraEnvVars));

        _hJob.reset(CreateJobObjectW(nullptr, nullptr));
        THROW_LAST_ERROR_IF_NULL(_hJob);

        // We want the conhost and all associated descendant processes
        // to be terminated when the tab is closed. GUI applications
        // spawned from the shell tend to end up in their own jobs.
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobExtendedInformation{};
        jobExtendedInformation.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

        THROW_IF_WIN32_BOOL_FALSE(SetInformationJobObject(_hJob.get(),
                                                          JobObjectExtendedLimitInformation,
                                                          &jobExtendedInformation,
                                                          sizeof(jobExtendedInformation)));

        THROW_IF_WIN32_BOOL_FALSE(AssignProcessToJobObject(_hJob.get(), _piConhost.hProcess));

        // Create our own output handling thread
        // Each connection needs to make sure to drain the output from its backing host.
        _hOutputThread.reset(CreateThread(nullptr,
                                          0,
                                          StaticOutputThreadProc,
                                          this,
                                          0,
                                          nullptr));

        // Wind up the conhost! We only do this after we've got everything in place.
        THROW_LAST_ERROR_IF(-1 == ResumeThread(_piConhost.hThread));

        _connected = true;
    }

    void ConhostConnection::WriteInput(hstring const& data)
    {
        if (!_connected || _closing.load())
        {
            return;
        }

        // convert from UTF-16LE to UTF-8 as ConPty expects UTF-8
        std::string str = winrt::to_string(data);
        bool fSuccess = !!WriteFile(_inPipe.get(), str.c_str(), (DWORD)str.length(), nullptr, nullptr);
        fSuccess;
    }

    void ConhostConnection::Resize(uint32_t rows, uint32_t columns)
    {
        if (!_connected)
        {
            _initialRows = rows;
            _initialCols = columns;
        }
        else if (!_closing.load())
        {
            SignalResizeWindow(_signalPipe.get(), Utils::ClampToShortMax(columns, 1), Utils::ClampToShortMax(rows, 1));
        }
    }

    void ConhostConnection::Close()
    {
        if (!_connected)
        {
            return;
        }

        if (!_closing.exchange(true))
        {
            // It is imperative that the signal pipe be closed first; this triggers the
            // pseudoconsole host's teardown. See PtySignalInputThread.cpp.
            _signalPipe.reset();
            _inPipe.reset();
            _outPipe.reset();

            // Tear down our output thread -- now that the output pipe was closed on the
            // far side, we can run down our local reader.
            WaitForSingleObject(_hOutputThread.get(), INFINITE);
            _hOutputThread.reset();

            // Wait for conhost to terminate.
            WaitForSingleObject(_piConhost.hProcess, INFINITE);

            _hJob.reset(); // This is a formality.
            _piConhost.reset();
        }
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

            fSuccess = !!ReadFile(_outPipe.get(), buffer, bufferSize, &dwRead, nullptr);
            if (!fSuccess)
            {
                if (_closing.load())
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
            if (dwRead == 0)
            {
                continue;
            }
            // Convert buffer to hstring
            char* pchStr = (char*)(buffer);
            std::string str{ pchStr, dwRead };
            auto hstr = winrt::to_hstring(str);

            // Pass the output to our registered event handlers
            _outputHandlers(hstr);
        }
    }
}
