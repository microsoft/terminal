// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ConhostConnection.h"
#include "windows.h"
#include <sstream>
#include <string_view>
#include <algorithm>
#include <type_traits>

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
        BYTE buffer[bufferSize]{ 0 };
        BYTE utf8Partials[4]{ 0 }; // buffer for code units of a partial UTF-8 code point that have to be cached
        DWORD dwRead{}; // length of a chunk read
        DWORD dwPartialsLen{}; // number of cached UTF-8 code units
        bool fSuccess{};
        bool fStreamBegin{}; // indicates whether text is left in utf8Partials after the BOM check
        const BYTE bitmasks[]{ 0, 0b11000000, 0b11100000, 0b11110000 }; // for comparisons after the Lead Byte was found

        // check whether the UTF-8 stream has a Byte Order Mark
        fSuccess = !!ReadFile(_outPipe.get(), utf8Partials, 3UL, &dwPartialsLen, nullptr);
        if (!fSuccess)
        {
            if (_closing.load())
            {
                // This is okay, break out to kill the thread
                return 0;
            }

            _disconnectHandlers();
            return (DWORD)-1;
        }

        if (utf8Partials[0] == 0xEF && utf8Partials[1] == 0xBB && utf8Partials[2] == 0xBF)
        {
            dwPartialsLen = 0; // discard the BOM
        }
        else
        {
            fStreamBegin = true;
        }

        // process the data of the standard input in a loop
        while (true)
        {
            // copy UTF-8 code units that were remaining from the previously read chunk (if any)
            if (dwPartialsLen != 0)
            {
                std::move(utf8Partials, utf8Partials + dwPartialsLen, buffer);
            }

            // try to read data
            fSuccess = !!ReadFile(_outPipe.get(), &buffer[dwPartialsLen], std::extent<decltype(buffer)>::value - dwPartialsLen, &dwRead, nullptr);

            dwRead += dwPartialsLen;
            dwPartialsLen = 0;

            if (dwRead == 0) // quit if no data has been read and no cached data was left over
            {
                return 0;
            }
            else if (!fSuccess && !fStreamBegin) // cached data without bytes for completion in the buffer
            {
                if (_closing.load())
                {
                    // This is okay, break out to kill the thread
                    return 0;
                }

                _disconnectHandlers();
                return (DWORD)-1;
            }

            const BYTE* const endPtr{ buffer + dwRead };
            const BYTE* backIter{ endPtr - 1 };
            // if necessary put up to three bytes in the cache:
            if ((*backIter & 0b10000000) == 0b10000000) // last byte in the buffer was a byte belonging to a UTF-8 multi-byte character
            {
                for (DWORD dwSequenceLen{ 1UL }, stop{ dwRead < 4UL ? dwRead : 4UL }; dwSequenceLen < stop; ++dwSequenceLen, --backIter) // check only up to 3 last bytes, if no Lead Byte was found then the byte before must be the Lead Byte and no partials are in the buffer
                {
                    if ((*backIter & 0b11000000) == 0b11000000) // Lead Byte found
                    {
                        if ((*backIter & bitmasks[dwSequenceLen]) != bitmasks[dwSequenceLen - 1]) // if the Lead Byte indicates that the last bytes in the buffer is a partial UTF-8 code point then cache them
                        {
                            std::move(backIter, endPtr, utf8Partials);
                            dwRead -= dwSequenceLen;
                            dwPartialsLen = dwSequenceLen;
                        }

                        break;
                    }
                }
            }
            // Convert buffer to hstring
            const std::string_view strView{ reinterpret_cast<char*>(buffer), dwRead };
            auto hstr{ winrt::to_hstring(strView) };

            // Pass the output to our registered event handlers
            _outputHandlers(hstr);

            fStreamBegin = false;
        }

        return 0;
    }
}
