// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "ConptyConnection.h"

#include <windows.h>
#include <userenv.h>

#include "ConptyConnection.g.cpp"
#include "CTerminalHandoff.h"

#include "../../types/inc/utils.hpp"
#include "../../types/inc/Environment.hpp"
#include "LibraryResources.h"

using namespace ::Microsoft::Console;

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
    // Function Description:
    // - creates some basic anonymous pipes and passes them to CreatePseudoConsole
    // Arguments:
    // - size: The size of the conpty to create, in characters.
    // - phInput: Receives the handle to the newly-created anonymous pipe for writing input to the conpty.
    // - phOutput: Receives the handle to the newly-created anonymous pipe for reading the output of the conpty.
    // - phPc: Receives a token value to identify this conpty
#pragma warning(suppress : 26430) // This statement sufficiently checks the out parameters. Analyzer cannot find this.
    static HRESULT _CreatePseudoConsoleAndPipes(const COORD size, const DWORD dwFlags, HANDLE* phInput, HANDLE* phOutput, HPCON* phPC) noexcept
    {
        RETURN_HR_IF(E_INVALIDARG, phPC == nullptr || phInput == nullptr || phOutput == nullptr);

        wil::unique_hfile outPipeOurSide, outPipePseudoConsoleSide;
        wil::unique_hfile inPipeOurSide, inPipePseudoConsoleSide;

        RETURN_IF_WIN32_BOOL_FALSE(CreatePipe(&inPipePseudoConsoleSide, &inPipeOurSide, nullptr, 0));
        RETURN_IF_WIN32_BOOL_FALSE(CreatePipe(&outPipeOurSide, &outPipePseudoConsoleSide, nullptr, 0));
        RETURN_IF_FAILED(ConptyCreatePseudoConsole(size, inPipePseudoConsoleSide.get(), outPipePseudoConsoleSide.get(), dwFlags, phPC));
        *phInput = inPipeOurSide.release();
        *phOutput = outPipeOurSide.release();
        return S_OK;
    }

    // Function Description:
    // - launches the client application attached to the new pseudoconsole
    HRESULT ConptyConnection::_LaunchAttachedClient() noexcept
    try
    {
        STARTUPINFOEX siEx{ 0 };
        siEx.StartupInfo.cb = sizeof(STARTUPINFOEX);
        siEx.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        SIZE_T size{};
        // This call will return an error (by design); we are ignoring it.
        InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
#pragma warning(suppress : 26414) // We don't move/touch this smart pointer, but we have to allocate strangely for the adjustable size list.
        auto attrList{ std::make_unique<std::byte[]>(size) };
#pragma warning(suppress : 26490) // We have to use reinterpret_cast because we allocated a byte array as a proxy for the adjustable size list.
        siEx.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attrList.get());
        RETURN_IF_WIN32_BOOL_FALSE(InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &size));

        RETURN_IF_WIN32_BOOL_FALSE(UpdateProcThreadAttribute(siEx.lpAttributeList,
                                                             0,
                                                             PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                                             _hPC.get(),
                                                             sizeof(HPCON),
                                                             nullptr,
                                                             nullptr));

        std::wstring cmdline{ wil::ExpandEnvironmentStringsW<std::wstring>(_commandline.c_str()) }; // mutable copy -- required for CreateProcessW

        Utils::EnvironmentVariableMapW environment;
        auto zeroEnvMap = wil::scope_exit([&]() noexcept {
            // Can't zero the keys, but at least we can zero the values.
            for (auto& [name, value] : environment)
            {
                ::SecureZeroMemory(value.data(), value.size() * sizeof(decltype(value.begin())::value_type));
            }

            environment.clear();
        });

        {
            const auto newEnvironmentBlock{ Utils::CreateEnvironmentBlock() };
            // Populate the environment map with the current environment.
            RETURN_IF_FAILED(Utils::UpdateEnvironmentMapW(environment, newEnvironmentBlock.get()));
        }

        {
            // Convert connection Guid to string and ignore the enclosing '{}'.
            std::wstring wsGuid{ Utils::GuidToString(_guid) };
            wsGuid.pop_back();

            const auto guidSubStr = std::wstring_view{ wsGuid }.substr(1);

            // Ensure every connection has the unique identifier in the environment.
            environment.insert_or_assign(L"WT_SESSION", guidSubStr.data());

            if (_environment)
            {
                // add additional WT env vars like WT_SETTINGS, WT_DEFAULTS and WT_PROFILE_ID
                for (auto item : _environment)
                {
                    auto key = item.Key();
                    auto value = item.Value();

                    // avoid clobbering WSLENV
                    if (std::wstring_view{ key } == L"WSLENV")
                    {
                        auto current = environment[L"WSLENV"];
                        value = current + L":" + value;
                    }

                    environment.insert_or_assign(key.c_str(), value.c_str());
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

        LPWCH lpEnvironment = newEnvVars.empty() ? nullptr : newEnvVars.data();

        // If we have a startingTitle, create a mutable character buffer to add
        // it to the STARTUPINFO.
        std::wstring mutableTitle{};
        if (!_startingTitle.empty())
        {
            mutableTitle = _startingTitle;
            siEx.StartupInfo.lpTitle = mutableTitle.data();
        }

        const wchar_t* const startingDirectory = _startingDirectory.size() > 0 ? _startingDirectory.c_str() : nullptr;

        RETURN_IF_WIN32_BOOL_FALSE(CreateProcessW(
            nullptr,
            cmdline.data(),
            nullptr, // lpProcessAttributes
            nullptr, // lpThreadAttributes
            false, // bInheritHandles
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

    ConptyConnection::ConptyConnection(const HANDLE hSig,
                                       const HANDLE hIn,
                                       const HANDLE hOut,
                                       const HANDLE hClientProcess) :
        _initialRows{ 25 },
        _initialCols{ 80 },
        _commandline{ L"" },
        _startingDirectory{ L"" },
        _startingTitle{ L"" },
        _environment{ nullptr },
        _guid{},
        _u8State{},
        _u16Str{},
        _buffer{},
        _inPipe{ hIn },
        _outPipe{ hOut }
    {
        hSig; // TODO: GH 9464 this needs to be packed into the hpcon
        if (_guid == guid{})
        {
            _guid = Utils::CreateGuid();
        }

        _piClient.hProcess = hClientProcess;
    }

    ConptyConnection::ConptyConnection(const hstring& commandline,
                                       const hstring& startingDirectory,
                                       const hstring& startingTitle,
                                       const Windows::Foundation::Collections::IMapView<hstring, hstring>& environment,
                                       const uint32_t initialRows,
                                       const uint32_t initialCols,
                                       const guid& initialGuid) :
        _initialRows{ initialRows },
        _initialCols{ initialCols },
        _commandline{ commandline },
        _startingDirectory{ startingDirectory },
        _startingTitle{ startingTitle },
        _environment{ environment },
        _guid{ initialGuid },
        _u8State{},
        _u16Str{},
        _buffer{}
    {
        if (_guid == guid{})
        {
            _guid = Utils::CreateGuid();
        }
    }

    winrt::guid ConptyConnection::Guid() const noexcept
    {
        return _guid;
    }

    void ConptyConnection::Start()
    try
    {
        if (!_inPipe)
        {
            const COORD dimensions{ gsl::narrow_cast<SHORT>(_initialCols), gsl::narrow_cast<SHORT>(_initialRows) };
            THROW_IF_FAILED(_CreatePseudoConsoleAndPipes(dimensions, PSEUDOCONSOLE_RESIZE_QUIRK | PSEUDOCONSOLE_WIN32_INPUT_MODE, &_inPipe, &_outPipe, &_hPC));
            THROW_IF_FAILED(_LaunchAttachedClient());
        }

        _startTime = std::chrono::high_resolution_clock::now();

        // Create our own output handling thread
        // This must be done after the pipes are populated.
        // Each connection needs to make sure to drain the output from its backing host.
        _hOutputThread.reset(CreateThread(
            nullptr,
            0,
            [](LPVOID lpParameter) noexcept {
                ConptyConnection* const pInstance = static_cast<ConptyConnection*>(lpParameter);
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

        LOG_IF_FAILED(SetThreadDescription(_hOutputThread.get(), L"ConptyConnection Output Thread"));

        _clientExitWait.reset(CreateThreadpoolWait(
            [](PTP_CALLBACK_INSTANCE /*callbackInstance*/, PVOID context, PTP_WAIT /*wait*/, TP_WAIT_RESULT /*waitResult*/) noexcept {
                ConptyConnection* const pInstance = static_cast<ConptyConnection*>(context);
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

        winrt::hstring failureText{ fmt::format(std::wstring_view{ RS_(L"ProcessFailedToLaunch") },
                                                gsl::narrow_cast<unsigned long>(hr),
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
        _hPC.reset();
    }

    // Method Description:
    // - prints out the "process exited" message formatted with the exit code
    // Arguments:
    // - status: the exit code.
    void ConptyConnection::_indicateExitWithStatus(unsigned int status) noexcept
    {
        try
        {
            winrt::hstring exitText{ fmt::format(std::wstring_view{ RS_(L"ProcessExited") }, status) };
            _TerminalOutputHandlers(L"\r\n");
            _TerminalOutputHandlers(exitText);
        }
        CATCH_LOG();
    }

    // Method Description:
    // - called when the client application (not necessarily its pty) exits for any reason
    void ConptyConnection::_ClientTerminated() noexcept
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
        _hPC.reset();
        if (auto localOutputThreadHandle = std::move(_hOutputThread))
        {
            LOG_LAST_ERROR_IF(WAIT_FAILED == WaitForSingleObject(localOutputThreadHandle.get(), INFINITE));
        }

        _indicateExitWithStatus(exitCode);

        _piClient.reset();
    }
    CATCH_LOG()

    void ConptyConnection::WriteInput(hstring const& data)
    {
        if (!_isConnected())
        {
            return;
        }

        // convert from UTF-16LE to UTF-8 as ConPty expects UTF-8
        // TODO GH#3378 reconcile and unify UTF-8 converters
        std::string str = winrt::to_string(data);
        LOG_IF_WIN32_BOOL_FALSE(WriteFile(_inPipe.get(), str.c_str(), (DWORD)str.length(), nullptr, nullptr));
    }

    void ConptyConnection::Resize(uint32_t rows, uint32_t columns)
    {
        if (!_hPC)
        {
            _initialRows = rows;
            _initialCols = columns;
        }
        else if (_isConnected())
        {
            THROW_IF_FAILED(ConptyResizePseudoConsole(_hPC.get(), { Utils::ClampToShortMax(columns, 1), Utils::ClampToShortMax(rows, 1) }));
        }
    }

    void ConptyConnection::Close() noexcept
    try
    {
        if (_transitionToState(ConnectionState::Closing))
        {
            // EXIT POINT
            _clientExitWait.reset(); // immediately stop waiting for the client to exit.

            _hPC.reset(); // tear down the pseudoconsole (this is like clicking X on a console window)

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

    DWORD ConptyConnection::_OutputThread()
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

            const HRESULT result{ til::u8u16(std::string_view{ _buffer.data(), read }, _u16Str, _u8State) };
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

    static winrt::event<NewConnectionHandler> _newConnectionHandlers;

    winrt::event_token ConptyConnection::NewConnection(NewConnectionHandler const& handler) { return _newConnectionHandlers.add(handler); };
    void ConptyConnection::NewConnection(winrt::event_token const& token) { _newConnectionHandlers.remove(token); };

    HRESULT ConptyConnection::NewHandoff(HANDLE in, HANDLE out, HANDLE signal, HANDLE process) noexcept
    try
    {
        auto conn = winrt::make<implementation::ConptyConnection>(signal, in, out, process);
        _newConnectionHandlers(conn);

        return S_OK;
    }
    CATCH_RETURN()

    void ConptyConnection::StartInboundListener()
    {
        THROW_IF_FAILED(CTerminalHandoff::s_StartListening(&ConptyConnection::NewHandoff));
    }

    void ConptyConnection::StopInboundListener()
    {
        THROW_IF_FAILED(CTerminalHandoff::s_StopListening());
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
    winrt::fire_and_forget ConptyConnection::final_release(std::unique_ptr<ConptyConnection> connection)
    {
        co_await winrt::resume_background(); // move to background
        connection.reset(); // explicitly destruct
    }

}
