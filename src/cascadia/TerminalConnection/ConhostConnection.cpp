// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ConhostConnection.h"
#include "windows.h"
#include <sstream>

#include "ConhostConnection.g.cpp"

#include <conpty-universal.h>
#include "../../types/inc/Utils.hpp"
#include "../../types/inc/UTF8OutPipeReader.hpp"

using namespace ::Microsoft::Console;

// FIXME: idk how to include this form cppwinrt_utils.h
#define DEFINE_EVENT(className, name, eventHandler, args)                                         \
    winrt::event_token className::name(args const& handler) { return eventHandler.add(handler); } \
    void className::name(winrt::event_token const& token) noexcept { eventHandler.remove(token); }

//FIXME: #include "../../types/inc/convert.hpp" doesn't work :(
[[nodiscard]] static std::string ConvertToA(const UINT codepage, const std::wstring_view source)
{
    // If there's nothing to convert, bail early.
    if (source.empty())
    {
        return {};
    }

    int iSource; // convert to int because Wc2Mb requires it.
    THROW_IF_FAILED(SizeTToInt(source.size(), &iSource));

    // Ask how much space we will need.
    // clang-format off
#pragma prefast(suppress: __WARNING_W2A_BEST_FIT, "WC_NO_BEST_FIT_CHARS doesn't work in many codepages. Retain old behavior.")
    // clang-format on
    int const iTarget = WideCharToMultiByte(codepage, 0, source.data(), iSource, nullptr, 0, nullptr, nullptr);
    THROW_LAST_ERROR_IF(0 == iTarget);

    size_t cchNeeded;
    THROW_IF_FAILED(IntToSizeT(iTarget, &cchNeeded));

    // Allocate ourselves space in a smart pointer
    std::unique_ptr<char[]> psOut = std::make_unique<char[]>(cchNeeded);
    THROW_IF_NULL_ALLOC(psOut.get());

    // Attempt conversion for real.
    // clang-format off
#pragma prefast(suppress: __WARNING_W2A_BEST_FIT, "WC_NO_BEST_FIT_CHARS doesn't work in many codepages. Retain old behavior.")
    // clang-format on
    THROW_LAST_ERROR_IF(0 == WideCharToMultiByte(codepage, 0, source.data(), iSource, psOut.get(), iTarget, nullptr, nullptr));

    // Return as a string
    return std::string(psOut.get(), cchNeeded);
}

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

        // We'll create a temporary pipe which allows conhost to pass information
        // about process it created via passed commandline.
        // In ConPTY connection we will create the process so this pipe won't be needed.
        HANDLE processInfoPipe;

        THROW_IF_FAILED(
            CreateConPty(cmdline,
                         startingDirectory,
                         static_cast<short>(_initialCols),
                         static_cast<short>(_initialRows),
                         &_inPipe,
                         &_outPipe,
                         &_signalPipe,
                         &processInfoPipe,
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

        // Wind up the conhost!
        THROW_LAST_ERROR_IF(-1 == ResumeThread(_piConhost.hThread));

        THROW_IF_WIN32_BOOL_FALSE(ConnectNamedPipe(processInfoPipe, nullptr));

        // First we're given error from creation of commandline
        // process or ERROR_SUCCESS if there was not error.
        DWORD processStartupErrorCode;
        THROW_IF_WIN32_BOOL_FALSE(ReadFile(processInfoPipe, &processStartupErrorCode, sizeof(processStartupErrorCode), nullptr, nullptr));
        _processStartupErrorCode = processStartupErrorCode;

        _open = processStartupErrorCode == ERROR_SUCCESS;
        if (_open)
        {
            // If process was created, send our pid
            DWORD currPid = GetCurrentProcessId();
            THROW_IF_WIN32_BOOL_FALSE(WriteFile(processInfoPipe, &currPid, sizeof(currPid), nullptr, nullptr));

            // Then we'll recive duplicated handle to commandline process
            THROW_IF_WIN32_BOOL_FALSE(ReadFile(processInfoPipe, &_processHandle, sizeof(_processHandle.get()), nullptr, nullptr));

            // Create our own output handling thread
            // Each connection needs to make sure to drain the output from its backing host.
            _hOutputThread.reset(CreateThread(nullptr,
                                              0,
                                              StaticOutputThreadProc,
                                              this,
                                              0,
                                              nullptr));

            THROW_LAST_ERROR_IF_NULL(_hOutputThread);
        }

        THROW_IF_WIN32_BOOL_FALSE(DisconnectNamedPipe(processInfoPipe));

        if (_open)
        {
            _stateChangedHandlers(TerminalConnection::VisualConnectionState::Connected, true);
        }
        else
        {
            _disconnectHandlers(_processExitCode == 0, false);
            _stateChangedHandlers(TerminalConnection::VisualConnectionState::ConnectionFailed, false);

            wil::unique_hlocal_ptr<WCHAR[]> errorMsgBuf;
            size_t errorMsgSize = FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                0,
                processStartupErrorCode,
                0,
                (LPWSTR)&errorMsgBuf,
                0,
                NULL);
            std::wstring_view errorMsg(errorMsgBuf.get(), errorMsgSize - 2); // Remove last "\r\n"

            std::ostringstream ss;
            ss << "[Failed to start process]\r\n";
            ss << "  Error code \x1B[37m" << processStartupErrorCode << "\x1B[0m: \x1B[37m" << ConvertToA(CP_UTF8, errorMsg) << "\x1B[0m\r\n";
            ss << "  Command line: \x1B[37m" << winrt::to_string(_commandline) << "\x1B[0m";

            _outputHandlers(winrt::to_hstring(ss.str()));
        }
    }

    void ConhostConnection::WriteInput(hstring const& data)
    {
        if (!_open || _closing.load())
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
        if (!_open)
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
        if (!_open)
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

            if (_hOutputThread)
            {
                // Tear down our output thread -- now that the output pipe was closed on the
                // far side, we can run down our local reader.
                WaitForSingleObject(_hOutputThread.get(), INFINITE);
                _hOutputThread.reset();
            }

            // Wait for conhost to terminate.
            WaitForSingleObject(_piConhost.hProcess, INFINITE);

            _hJob.reset(); // This is a formality.
            _piConhost.reset();
        }
    }

    hstring ConhostConnection::GetTabTitle(hstring previousTitle)
    {
        if (_processStartupErrorCode.has_value() && _processStartupErrorCode.value() != ERROR_SUCCESS)
        {
            return _commandline;
        }
        else if (_processExitCode.has_value())
        {
            std::wostringstream ss;
            ss << L"[" << _processExitCode.value() << L"] " << std::wstring{ previousTitle };
            return hstring(ss.str());
        }
        else
        {
            return previousTitle;
        }
    }

    DWORD WINAPI ConhostConnection::StaticOutputThreadProc(LPVOID lpParameter)
    {
        ConhostConnection* const pInstance = (ConhostConnection*)lpParameter;
        return pInstance->_OutputThread();
    }

    DWORD ConhostConnection::_OutputThread()
    {
        UTF8OutPipeReader pipeReader{ _outPipe.get() };
        std::string_view strView{};

        // process the data of the output pipe in a loop
        while (true)
        {
            HRESULT result = pipeReader.Read(strView);
            if (FAILED(result) || result == S_FALSE)
            {
                if (_closing.load())
                {
                    // This is okay, break out to kill the thread
                    return 0;
                }

                if (_processHandle)
                {
                    // Wait for application process to terminate to get it's exit code
                    WaitForSingleObject(_processHandle.get(), INFINITE);
                    DWORD exitCode;
                    THROW_IF_WIN32_BOOL_FALSE(GetExitCodeProcess(_processHandle.get(), &exitCode));
                    _processExitCode = exitCode;
                    _processHandle.reset();

                    bool gracefulDisconnection = exitCode == 0;
                    _stateChangedHandlers(gracefulDisconnection ?
                                              TerminalConnection::VisualConnectionState::DisconnectedGracefully :
                                              TerminalConnection::VisualConnectionState::DisconnectedNotGracefully,
                                          false);

                    std::ostringstream ss;
                    ss << "[Process exited with code \x1B[37m" << exitCode << "\x1B[0m]";
                    _outputHandlers(winrt::to_hstring(ss.str()));

                    _disconnectHandlers(gracefulDisconnection, false);
                }
                else
                {
                    // if _processHandle is not set, this thread shouldn't start at all
                    _disconnectHandlers(false, false);
                }

                return (DWORD)-1;
            }

            if (strView.empty())
            {
                return 0;
            }

            // Convert buffer to hstring
            auto hstr{ winrt::to_hstring(strView) };

            // Pass the output to our registered event handlers
            _outputHandlers(hstr);
        }

        return 0;
    }

    DEFINE_EVENT(ConhostConnection, TerminalOutput, _outputHandlers, TerminalConnection::TerminalOutputEventArgs);
    DEFINE_EVENT(ConhostConnection, TerminalDisconnected, _disconnectHandlers, TerminalConnection::TerminalDisconnectedEventArgs);
    DEFINE_EVENT(ConhostConnection, StateChanged, _stateChangedHandlers, TerminalConnection::StateChangedEventArgs);
}
