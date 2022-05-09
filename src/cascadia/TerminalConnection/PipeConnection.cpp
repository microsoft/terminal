// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "PipeConnection.h"

#include <winternl.h>

#include "PipeConnection.g.cpp"
#include "CTerminalHandoff.h"

#include "../../types/inc/utils.hpp"
#include "../../types/inc/Environment.hpp"
#include "LibraryResources.h"

using namespace ::Microsoft::Console;
using namespace std::string_view_literals;

// Format is: "DecimalResult (HexadecimalForm)"
static constexpr auto _errorFormat = L"{0} ({0:#010x})"sv;

static constexpr winrt::guid PipeConnectionType = { 0xfffffdfa, 0xa479, 0x412c, { 0x83, 0xb7, 0xc5, 0x64, 0xe, 0x61, 0xcd, 0x62 } };

// Notes:
// There is a number of ways that the Conpty connection can be terminated (voluntarily or not):
// 1. The connection is Close()d
// 2. The pseudoconsole or process cannot be spawned during Start()
// 3. The client process exits with a code.
//    (Successful (0) or any other code)
// 4. The read handle is terminated.
//    (This usually happens when the pseudoconsole host crashes.)
// In each of these termination scenarios, we need to be mindful of tripping the others.
// Closing the pseudoconsole in response to the client exiting (3) can trigger (4).
// Close() (1) will cause the automatic triggering of (3) and (4).
// In a lot of cases, we use the connection state to stop "flapping."
//
// To figure out where we handle these, search for comments containing "EXIT POINT"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    winrt::guid PipeConnection::ConnectionType() noexcept
    {
        return PipeConnectionType;
    }

    // Function Description:
    // - launches the client application attached to the new pseudoconsole
    HRESULT PipeConnection::_LaunchAttachedClient() noexcept
    try
    {
        STARTUPINFOEX siEx{ 0 };
        siEx.StartupInfo.cb = sizeof(STARTUPINFOEX);
        siEx.StartupInfo.dwFlags = STARTF_USESTDHANDLES;

        wil::unique_hfile outPipeOurSide, outPipePseudoConsoleSide;
        wil::unique_hfile inPipeOurSide, inPipePseudoConsoleSide;

        SECURITY_ATTRIBUTES saAttr; 
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        RETURN_IF_WIN32_BOOL_FALSE(CreatePipe(&inPipePseudoConsoleSide, &inPipeOurSide, &saAttr, 0));
        RETURN_IF_WIN32_BOOL_FALSE(CreatePipe(&outPipeOurSide, &outPipePseudoConsoleSide, &saAttr, 0));

        SetHandleInformation(inPipeOurSide.get(), HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(outPipeOurSide.get(), HANDLE_FLAG_INHERIT, 0);

        // RETURN_IF_FAILED(ConptyCreatePseudoConsole(size, inPipePseudoConsoleSide.get(), outPipePseudoConsoleSide.get(), dwFlags, phPC));
        siEx.StartupInfo.hStdInput = inPipePseudoConsoleSide.get();
        siEx.StartupInfo.hStdOutput = siEx.StartupInfo.hStdError = outPipePseudoConsoleSide.get();

        auto inPipe = inPipeOurSide.release();
        _inPipe.reset(inPipe);
        _outPipe.reset(outPipeOurSide.release());


        auto cmdline{ wil::ExpandEnvironmentStringsW<std::wstring>(_commandline.c_str()) }; // mutable copy -- required for CreateProcessW

        Utils::EnvironmentVariableMapW environment;
        auto zeroEnvMap = wil::scope_exit([&]() noexcept {
            // Can't zero the keys, but at least we can zero the values.
            for (auto& [name, value] : environment)
            {
                ::SecureZeroMemory(value.data(), value.size() * sizeof(decltype(value.begin())::value_type));
            }

            environment.clear();
        });

        // Populate the environment map with the current environment.
        RETURN_IF_FAILED(Utils::UpdateEnvironmentMapW(environment));

        {
            // Convert connection Guid to string and ignore the enclosing '{}'.
            auto wsGuid{ Utils::GuidToString(_guid) };
            wsGuid.pop_back();

            const auto guidSubStr = std::wstring_view{ wsGuid }.substr(1);

            // Ensure every connection has the unique identifier in the environment.
            environment.insert_or_assign(L"WT_SESSION", guidSubStr.data());

            if (_environment)
            {
                // add additional WT env vars like WT_SETTINGS, WT_DEFAULTS and WT_PROFILE_ID
                for (auto item : _environment)
                {
                    try
                    {
                        auto key = item.Key();
                        // This will throw if the value isn't a string. If that
                        // happens, then just skip this entry.
                        auto value = winrt::unbox_value<hstring>(item.Value());

                        // avoid clobbering WSLENV
                        if (std::wstring_view{ key } == L"WSLENV")
                        {
                            auto current = environment[L"WSLENV"];
                            value = current + L":" + value;
                        }

                        environment.insert_or_assign(key.c_str(), value.c_str());
                    }
                    CATCH_LOG();
                }
            }

            // WSLENV is a colon-delimited list of environment variables (+flags) that should appear inside WSL
            // https://devblogs.microsoft.com/commandline/share-environment-vars-between-wsl-and-windows/

            auto wslEnv = environment[L"WSLENV"];
            wslEnv = L"WT_SESSION:" + wslEnv; // prepend WT_SESSION to make sure it's visible inside WSL.
            environment.insert_or_assign(L"WSLENV", wslEnv);
        }

        std::vector<wchar_t> newEnvVars;
        auto zeroNewEnv = wil::scope_exit([&]() noexcept {
            ::SecureZeroMemory(newEnvVars.data(),
                               newEnvVars.size() * sizeof(decltype(newEnvVars.begin())::value_type));
        });

        RETURN_IF_FAILED(Utils::EnvironmentMapToEnvironmentStringsW(environment, newEnvVars));

        auto lpEnvironment = newEnvVars.empty() ? nullptr : newEnvVars.data();

        // If we have a startingTitle, create a mutable character buffer to add
        // it to the STARTUPINFO.
        std::wstring mutableTitle{};
        if (!_startingTitle.empty())
        {
            mutableTitle = _startingTitle;
            siEx.StartupInfo.lpTitle = mutableTitle.data();
        }

        auto [newCommandLine, newStartingDirectory] = Utils::MangleStartingDirectoryForWSL(cmdline, _startingDirectory);
        const auto startingDirectory = newStartingDirectory.size() > 0 ? newStartingDirectory.c_str() : nullptr;

        RETURN_IF_WIN32_BOOL_FALSE(CreateProcessW(
            nullptr,
            newCommandLine.data(),
            nullptr, // lpProcessAttributes
            nullptr, // lpThreadAttributes
            true, // bInheritHandles
            EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT, // dwCreationFlags
            lpEnvironment, // lpEnvironment
            startingDirectory,
            &siEx.StartupInfo, // lpStartupInfo
            &_piClient // lpProcessInformation
            ));

        DeleteProcThreadAttributeList(siEx.lpAttributeList);

        const std::filesystem::path processName = wil::GetModuleFileNameExW<std::wstring>(_piClient.hProcess, nullptr);
        _clientName = processName.filename().wstring();

#pragma warning(suppress : 26477 26485 26494 26482 26446) // We don't control TraceLoggingWrite
        TraceLoggingWrite(
            g_hTerminalConnectionProvider,
            "ConPtyConnected",
            TraceLoggingDescription("Event emitted when ConPTY connection is started"),
            TraceLoggingGuid(_guid, "SessionGuid", "The WT_SESSION's GUID"),
            TraceLoggingWideString(_clientName.c_str(), "Client", "The attached client process"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));

        return S_OK;
    }
    CATCH_RETURN();


    void PipeConnection::Initialize(const Windows::Foundation::Collections::ValueSet& settings)
    {
        if (settings)
        {
            // For the record, the following won't crash:
            // auto bad = unbox_value_or<hstring>(settings.TryLookup(L"foo").try_as<IPropertyValue>(), nullptr);
            // It'll just return null

            _commandline = winrt::unbox_value_or<winrt::hstring>(settings.TryLookup(L"commandline").try_as<Windows::Foundation::IPropertyValue>(), _commandline);
            _startingDirectory = winrt::unbox_value_or<winrt::hstring>(settings.TryLookup(L"startingDirectory").try_as<Windows::Foundation::IPropertyValue>(), _startingDirectory);
            _startingTitle = winrt::unbox_value_or<winrt::hstring>(settings.TryLookup(L"startingTitle").try_as<Windows::Foundation::IPropertyValue>(), _startingTitle);
            _initialRows = winrt::unbox_value_or<uint32_t>(settings.TryLookup(L"initialRows").try_as<Windows::Foundation::IPropertyValue>(), _initialRows);
            _initialCols = winrt::unbox_value_or<uint32_t>(settings.TryLookup(L"initialCols").try_as<Windows::Foundation::IPropertyValue>(), _initialCols);
            _guid = winrt::unbox_value_or<winrt::guid>(settings.TryLookup(L"guid").try_as<Windows::Foundation::IPropertyValue>(), _guid);
            _environment = settings.TryLookup(L"environment").try_as<Windows::Foundation::Collections::ValueSet>();
            if constexpr (Feature_VtPassthroughMode::IsEnabled())
            {
                _passthroughMode = winrt::unbox_value_or<bool>(settings.TryLookup(L"passthroughMode").try_as<Windows::Foundation::IPropertyValue>(), _passthroughMode);
            }
        }

        if (_guid == guid{})
        {
            _guid = Utils::CreateGuid();
        }
    }

    winrt::guid PipeConnection::Guid() const noexcept
    {
        return _guid;
    }

    winrt::hstring PipeConnection::Commandline() const
    {
        return _commandline;
    }

    void PipeConnection::Start()
    try
    {
        _transitionToState(ConnectionState::Connecting);


        THROW_IF_FAILED(_LaunchAttachedClient());

        _startTime = std::chrono::high_resolution_clock::now();

        // Create our own output handling thread
        // This must be done after the pipes are populated.
        // Each connection needs to make sure to drain the output from its backing host.
        _hOutputThread.reset(CreateThread(
            nullptr,
            0,
            [](LPVOID lpParameter) noexcept {
                const auto pInstance = static_cast<PipeConnection*>(lpParameter);
                if (pInstance)
                {
                    return pInstance->_OutputThread();
                }
                return gsl::narrow_cast<DWORD>(E_INVALIDARG);
            },
            this,
            0,
            nullptr));

        THROW_LAST_ERROR_IF_NULL(_hOutputThread);

        LOG_IF_FAILED(SetThreadDescription(_hOutputThread.get(), L"PipeConnection Output Thread"));

        _clientExitWait.reset(CreateThreadpoolWait(
            [](PTP_CALLBACK_INSTANCE /*callbackInstance*/, PVOID context, PTP_WAIT /*wait*/, TP_WAIT_RESULT /*waitResult*/) noexcept {
                const auto pInstance = static_cast<PipeConnection*>(context);
                if (pInstance)
                {
                    pInstance->_ClientTerminated();
                }
            },
            this,
            nullptr));

        SetThreadpoolWait(_clientExitWait.get(), _piClient.hProcess, nullptr);

        _transitionToState(ConnectionState::Connected);
    }
    catch (...)
    {
        // EXIT POINT
        const auto hr = wil::ResultFromCaughtException();

        // GH#11556 - make sure to format the error code to this string as an UNSIGNED int
        winrt::hstring failureText{ fmt::format(std::wstring_view{ RS_(L"ProcessFailedToLaunch") },
                                                fmt::format(_errorFormat, static_cast<unsigned int>(hr)),
                                                _commandline) };
        _TerminalOutputHandlers(failureText);

        // If the path was invalid, let's present an informative message to the user
        if (hr == HRESULT_FROM_WIN32(ERROR_DIRECTORY))
        {
            winrt::hstring badPathText{ fmt::format(std::wstring_view{ RS_(L"BadPathText") },
                                                    _startingDirectory) };
            _TerminalOutputHandlers(L"\r\n");
            _TerminalOutputHandlers(badPathText);
        }

        _transitionToState(ConnectionState::Failed);

        // Tear down any state we may have accumulated.
        // _hPC.reset();
    }

    // Method Description:
    // - prints out the "process exited" message formatted with the exit code
    // Arguments:
    // - status: the exit code.
    void PipeConnection::_indicateExitWithStatus(unsigned int status) noexcept
    {
        try
        {
            // GH#11556 - make sure to format the error code to this string as an UNSIGNED int
            winrt::hstring exitText{ fmt::format(std::wstring_view{ RS_(L"ProcessExited") }, fmt::format(_errorFormat, status)) };
            _TerminalOutputHandlers(L"\r\n");
            _TerminalOutputHandlers(exitText);
        }
        CATCH_LOG();
    }

    // Method Description:
    // - called when the client application (not necessarily its pty) exits for any reason
    void PipeConnection::_ClientTerminated() noexcept
    try
    {
        if (_isStateAtOrBeyond(ConnectionState::Closing))
        {
            // This termination was expected.
            return;
        }

        // EXIT POINT
        DWORD exitCode{ 0 };
        GetExitCodeProcess(_piClient.hProcess, &exitCode);

        // Signal the closing or failure of the process.
        // Load bearing. Terminating the pseudoconsole will make the output thread exit unexpectedly,
        // so we need to signal entry into the correct closing state before we do that.
        _transitionToState(exitCode == 0 ? ConnectionState::Closed : ConnectionState::Failed);

        // Close the pseudoconsole and wait for all output to drain.
        // _hPC.reset();
        if (auto localOutputThreadHandle = std::move(_hOutputThread))
        {
            LOG_LAST_ERROR_IF(WAIT_FAILED == WaitForSingleObject(localOutputThreadHandle.get(), INFINITE));
        }

        _indicateExitWithStatus(exitCode);

        _piClient.reset();
    }
    CATCH_LOG()

    void PipeConnection::WriteInput(const hstring& data)
    {
        if (!_isConnected())
        {
            return;
        }

        // convert from UTF-16LE to UTF-8 as ConPty expects UTF-8
        // TODO GH#3378 reconcile and unify UTF-8 converters
        auto str = winrt::to_string(data);
        LOG_IF_WIN32_BOOL_FALSE(WriteFile(_inPipe.get(), str.c_str(), (DWORD)str.length(), nullptr, nullptr));
    }

    void PipeConnection::Resize(uint32_t /*rows*/, uint32_t /*columns*/)
    {
    }

    void PipeConnection::ClearBuffer()
    {
    }

    void PipeConnection::ShowHide(const bool /*show*/)
    {
    }

    void PipeConnection::ReparentWindow(const uint64_t /*newParent*/)
    {
    }

    void PipeConnection::Close() noexcept
    try
    {
        if (_transitionToState(ConnectionState::Closing))
        {
            // EXIT POINT
            _clientExitWait.reset(); // immediately stop waiting for the client to exit.

            // _hPC.reset(); // tear down the pseudoconsole (this is like clicking X on a console window)

            _inPipe.reset(); // break the pipes
            _outPipe.reset();

            if (_hOutputThread)
            {
                // Tear down our output thread -- now that the output pipe was closed on the
                // far side, we can run down our local reader.
                LOG_LAST_ERROR_IF(WAIT_FAILED == WaitForSingleObject(_hOutputThread.get(), INFINITE));
                _hOutputThread.reset();
            }

            if (_piClient.hProcess)
            {
                // Wait for the client to terminate (which it should do successfully)
                LOG_LAST_ERROR_IF(WAIT_FAILED == WaitForSingleObject(_piClient.hProcess, INFINITE));
                _piClient.reset();
            }

            _transitionToState(ConnectionState::Closed);
        }
    }
    CATCH_LOG()

    DWORD PipeConnection::_OutputThread()
    {
        // Keep us alive until the output thread terminates; the destructor
        // won't wait for us, and the known exit points _do_.
        auto strongThis{ get_strong() };

        // process the data of the output pipe in a loop
        while (true)
        {
            DWORD read{};

            const auto readFail{ !ReadFile(_outPipe.get(), _buffer.data(), gsl::narrow_cast<DWORD>(_buffer.size()), &read, nullptr) };
            if (readFail) // reading failed (we must check this first, because read will also be 0.)
            {
                const auto lastError = GetLastError();
                if (lastError != ERROR_BROKEN_PIPE && !_isStateAtOrBeyond(ConnectionState::Closing))
                {
                    // EXIT POINT
                    _indicateExitWithStatus(HRESULT_FROM_WIN32(lastError)); // print a message
                    _transitionToState(ConnectionState::Failed);
                    return gsl::narrow_cast<DWORD>(HRESULT_FROM_WIN32(lastError));
                }
                // else we call convertUTF8ChunkToUTF16 with an empty string_view to convert possible remaining partials to U+FFFD
            }

            const auto result{ til::u8u16(std::string_view{ _buffer.data(), read }, _u16Str, _u8State) };
            if (FAILED(result))
            {
                if (_isStateAtOrBeyond(ConnectionState::Closing))
                {
                    // This termination was expected.
                    return 0;
                }

                // EXIT POINT
                _indicateExitWithStatus(result); // print a message
                _transitionToState(ConnectionState::Failed);
                return gsl::narrow_cast<DWORD>(result);
            }

            if (_u16Str.empty())
            {
                return 0;
            }

            if (!_receivedFirstByte)
            {
                const auto now = std::chrono::high_resolution_clock::now();
                const std::chrono::duration<double> delta = now - _startTime;

#pragma warning(suppress : 26477 26485 26494 26482 26446) // We don't control TraceLoggingWrite
                TraceLoggingWrite(g_hTerminalConnectionProvider,
                                  "ReceivedFirstByte",
                                  TraceLoggingDescription("An event emitted when the connection receives the first byte"),
                                  TraceLoggingGuid(_guid, "SessionGuid", "The WT_SESSION's GUID"),
                                  TraceLoggingFloat64(delta.count(), "Duration"),
                                  TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                                  TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));
                _receivedFirstByte = true;
            }

            // Pass the output to our registered event handlers
            _TerminalOutputHandlers(_u16Str);
        }

        return 0;
    }

    // Function Description:
    // - This function will be called (by C++/WinRT) after the final outstanding reference to
    //   any given connection instance is released.
    //   When a client application exits, its termination will wait for the output thread to
    //   run down. However, because our teardown is somewhat complex, our last reference may
    //   be owned by the very output thread that the client wait threadpool is blocked on.
    //   During destruction, we'll try to release any outstanding handles--including the one
    //   we have to the threadpool wait. As you might imagine, this takes us right to deadlock
    //   city.
    //   Deferring the final destruction of the connection to a background thread that can't
    //   be awaiting our destruction breaks the deadlock.
    // Arguments:
    // - connection: the final living reference to an outgoing connection
    winrt::fire_and_forget PipeConnection::final_release(std::unique_ptr<PipeConnection> connection)
    {
        co_await winrt::resume_background(); // move to background
        connection.reset(); // explicitly destruct
    }

}
