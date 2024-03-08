// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ConptyConnection.h"

#include <conpty-static.h>
#include <til/string.h>
#include <winternl.h>

#include "CTerminalHandoff.h"
#include "LibraryResources.h"
#include "../../types/inc/utils.hpp"

#include "ConptyConnection.g.cpp"

using namespace ::Microsoft::Console;
using namespace std::string_view_literals;

// Format is: "DecimalResult (HexadecimalForm)"
static constexpr auto _errorFormat = L"{0} ({0:#010x})"sv;

// Notes:
// There is a number of ways that the Conpty connection can be terminated (voluntarily or not):
// 1. The connection is Close()d
// 2. The pseudoconsole or process cannot be spawned during Start()
// 3. The read handle is terminated (when OpenConsole exits)
// In each of these termination scenarios, we need to be mindful of tripping the others.
// Close() (1) will cause the automatic triggering of (3).
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

        auto cmdline{ wil::ExpandEnvironmentStringsW<std::wstring>(_commandline.c_str()) }; // mutable copy -- required for CreateProcessW
        auto environment = _initialEnv;

        {
            // Ensure every connection has the unique identifier in the environment.
            // Convert connection Guid to string and ignore the enclosing '{}'.
            environment.as_map().insert_or_assign(L"WT_SESSION", Utils::GuidToPlainString(_sessionId));

            // The profile Guid does include the enclosing '{}'
            environment.as_map().insert_or_assign(L"WT_PROFILE_ID", Utils::GuidToString(_profileGuid));

            // WSLENV is a colon-delimited list of environment variables (+flags) that should appear inside WSL
            // https://devblogs.microsoft.com/commandline/share-environment-vars-between-wsl-and-windows/
            std::wstring wslEnv{ L"WT_SESSION:WT_PROFILE_ID:" };
            if (_environment)
            {
                // Order the environment variable names so that resolution order is consistent
                std::set<std::wstring, til::env_key_sorter> keys{};
                for (const auto item : _environment)
                {
                    keys.insert(item.Key().c_str());
                }
                // add additional env vars
                for (const auto& key : keys)
                {
                    try
                    {
                        // This will throw if the value isn't a string. If that
                        // happens, then just skip this entry.
                        const auto value = winrt::unbox_value<hstring>(_environment.Lookup(key));

                        environment.set_user_environment_var(key.c_str(), value.c_str());
                        // For each environment variable added to the environment, also add it to WSLENV
                        wslEnv += key + L":";
                    }
                    CATCH_LOG();
                }
            }

            // We want to prepend new environment variables to WSLENV - that way if a variable already
            // exists in WSLENV but with a flag, the flag will be respected.
            // (This behaviour was empirically observed)
            wslEnv += environment.as_map()[L"WSLENV"];
            environment.as_map().insert_or_assign(L"WSLENV", wslEnv);
        }

        auto newEnvVars = environment.to_string();
        const auto lpEnvironment = newEnvVars.empty() ? nullptr : newEnvVars.data();

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
            TraceLoggingGuid(_sessionId, "SessionGuid", "The WT_SESSION's GUID"),
            TraceLoggingWideString(_clientName.c_str(), "Client", "The attached client process"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));

        return S_OK;
    }
    CATCH_RETURN();

    ConptyConnection::ConptyConnection(const HANDLE hSig,
                                       const HANDLE hIn,
                                       const HANDLE hOut,
                                       const HANDLE hRef,
                                       const HANDLE hServerProcess,
                                       const HANDLE hClientProcess,
                                       TERMINAL_STARTUP_INFO startupInfo) :
        _rows{ 25 },
        _cols{ 80 },
        _inPipe{ hIn },
        _outPipe{ hOut }
    {
        THROW_IF_FAILED(ConptyPackPseudoConsole(hServerProcess, hRef, hSig, &_hPC));
        _piClient.hProcess = hClientProcess;

        _startupInfo.title = winrt::hstring{ startupInfo.pszTitle, SysStringLen(startupInfo.pszTitle) };
        _startupInfo.iconPath = winrt::hstring{ startupInfo.pszIconPath, SysStringLen(startupInfo.pszIconPath) };
        _startupInfo.iconIndex = startupInfo.iconIndex;
        _startupInfo.showWindow = startupInfo.wShowWindow;

        try
        {
            _commandline = _commandlineFromProcess(hClientProcess);
        }
        CATCH_LOG()
    }

    // Function Description:
    // - Helper function for constructing a ValueSet that we can use to get our settings from.
    Windows::Foundation::Collections::ValueSet ConptyConnection::CreateSettings(const winrt::hstring& cmdline,
                                                                                const winrt::hstring& startingDirectory,
                                                                                const winrt::hstring& startingTitle,
                                                                                bool reloadEnvironmentVariables,
                                                                                const winrt::hstring& initialEnvironment,
                                                                                const Windows::Foundation::Collections::IMapView<hstring, hstring>& environmentOverrides,
                                                                                uint32_t rows,
                                                                                uint32_t columns,
                                                                                const winrt::guid& guid,
                                                                                const winrt::guid& profileGuid)
    {
        Windows::Foundation::Collections::ValueSet vs{};

        vs.Insert(L"commandline", Windows::Foundation::PropertyValue::CreateString(cmdline));
        vs.Insert(L"startingDirectory", Windows::Foundation::PropertyValue::CreateString(startingDirectory));
        vs.Insert(L"startingTitle", Windows::Foundation::PropertyValue::CreateString(startingTitle));
        vs.Insert(L"reloadEnvironmentVariables", Windows::Foundation::PropertyValue::CreateBoolean(reloadEnvironmentVariables));
        vs.Insert(L"initialRows", Windows::Foundation::PropertyValue::CreateUInt32(rows));
        vs.Insert(L"initialCols", Windows::Foundation::PropertyValue::CreateUInt32(columns));
        vs.Insert(L"guid", Windows::Foundation::PropertyValue::CreateGuid(guid));
        vs.Insert(L"profileGuid", Windows::Foundation::PropertyValue::CreateGuid(profileGuid));

        if (environmentOverrides)
        {
            Windows::Foundation::Collections::ValueSet env{};
            for (const auto& [k, v] : environmentOverrides)
            {
                env.Insert(k, Windows::Foundation::PropertyValue::CreateString(v));
            }
            vs.Insert(L"environment", env);
        }

        if (!initialEnvironment.empty())
        {
            vs.Insert(L"initialEnvironment", Windows::Foundation::PropertyValue::CreateString(initialEnvironment));
        }
        return vs;
    }

    void ConptyConnection::Initialize(const Windows::Foundation::Collections::ValueSet& settings)
    {
        if (settings)
        {
            // For the record, the following won't crash:
            // auto bad = unbox_value_or<hstring>(settings.TryLookup(L"foo").try_as<IPropertyValue>(), nullptr);
            // It'll just return null

            _commandline = unbox_prop_or<winrt::hstring>(settings, L"commandline", _commandline);
            _startingDirectory = unbox_prop_or<winrt::hstring>(settings, L"startingDirectory", _startingDirectory);
            _startingTitle = unbox_prop_or<winrt::hstring>(settings, L"startingTitle", _startingTitle);
            _rows = unbox_prop_or<uint32_t>(settings, L"initialRows", _rows);
            _cols = unbox_prop_or<uint32_t>(settings, L"initialCols", _cols);
            _sessionId = unbox_prop_or<winrt::guid>(settings, L"sessionId", _sessionId);
            _environment = settings.TryLookup(L"environment").try_as<Windows::Foundation::Collections::ValueSet>();
            if constexpr (Feature_VtPassthroughMode::IsEnabled())
            {
                _passthroughMode = unbox_prop_or<bool>(settings, L"passthroughMode", _passthroughMode);
            }
            _inheritCursor = unbox_prop_or<bool>(settings, L"inheritCursor", _inheritCursor);
            _profileGuid = unbox_prop_or<winrt::guid>(settings, L"profileGuid", _profileGuid);

            const auto& initialEnvironment{ unbox_prop_or<winrt::hstring>(settings, L"initialEnvironment", L"") };

            const bool reloadEnvironmentVariables = unbox_prop_or<bool>(settings, L"reloadEnvironmentVariables", false);

            if (reloadEnvironmentVariables)
            {
                _initialEnv.regenerate();
            }
            else
            {
                if (!initialEnvironment.empty())
                {
                    _initialEnv = til::env{ initialEnvironment.c_str() };
                }
                else
                {
                    // If we were not explicitly provided an "initial" env block to
                    // treat as our original one, then just use our actual current
                    // env block.
                    _initialEnv = til::env::from_current_environment();
                }
            }
        }

        if (_sessionId == guid{})
        {
            _sessionId = Utils::CreateGuid();
        }
    }

    winrt::hstring ConptyConnection::Commandline() const
    {
        return _commandline;
    }

    winrt::hstring ConptyConnection::StartingTitle() const
    {
        return _startupInfo.title;
    }

    WORD ConptyConnection::ShowWindow() const noexcept
    {
        return _startupInfo.showWindow;
    }

    void ConptyConnection::Start()
    try
    {
        _transitionToState(ConnectionState::Connecting);

        const til::size dimensions{ gsl::narrow<til::CoordType>(_cols), gsl::narrow<til::CoordType>(_rows) };

        // If we do not have pipes already, then this is a fresh connection... not an inbound one that is a received
        // handoff from an already-started PTY process.
        if (!_inPipe)
        {
            DWORD flags = PSEUDOCONSOLE_RESIZE_QUIRK;

            // If we're using an existing buffer, we want the new connection
            // to reuse the existing cursor. When not setting this flag, the
            // PseudoConsole sends a clear screen VT code which our renderer
            // interprets into making all the previous lines be outside the
            // current viewport.
            if (_inheritCursor)
            {
                flags |= PSEUDOCONSOLE_INHERIT_CURSOR;
            }

            if constexpr (Feature_VtPassthroughMode::IsEnabled())
            {
                if (_passthroughMode)
                {
                    WI_SetFlag(flags, PSEUDOCONSOLE_PASSTHROUGH_MODE);
                }
            }

            THROW_IF_FAILED(_CreatePseudoConsoleAndPipes(til::unwrap_coord_size(dimensions), flags, &_inPipe, &_outPipe, &_hPC));

            if (_initialParentHwnd != 0)
            {
                THROW_IF_FAILED(ConptyReparentPseudoConsole(_hPC.get(), reinterpret_cast<HWND>(_initialParentHwnd)));
            }

            // GH#12515: The conpty assumes it's hidden at the start. If we're visible, let it know now.
            if (_initialVisibility)
            {
                THROW_IF_FAILED(ConptyShowHidePseudoConsole(_hPC.get(), _initialVisibility));
            }

            THROW_IF_FAILED(_LaunchAttachedClient());
        }
        // But if it was an inbound handoff... attempt to synchronize the size of it with what our connection
        // window is expecting it to be on the first layout.
        else
        {
#pragma warning(suppress : 26477 26485 26494 26482 26446) // We don't control TraceLoggingWrite
            TraceLoggingWrite(
                g_hTerminalConnectionProvider,
                "ConPtyConnectedToDefterm",
                TraceLoggingDescription("Event emitted when ConPTY connection is started, for a defterm session"),
                TraceLoggingGuid(_sessionId, "SessionGuid", "The WT_SESSION's GUID"),
                TraceLoggingWideString(_clientName.c_str(), "Client", "The attached client process"),
                TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));

            THROW_IF_FAILED(ConptyResizePseudoConsole(_hPC.get(), til::unwrap_coord_size(dimensions)));
            THROW_IF_FAILED(ConptyReparentPseudoConsole(_hPC.get(), reinterpret_cast<HWND>(_initialParentHwnd)));

            if (_initialVisibility)
            {
                THROW_IF_FAILED(ConptyShowHidePseudoConsole(_hPC.get(), _initialVisibility));
            }
        }

        THROW_IF_FAILED(ConptyReleasePseudoConsole(_hPC.get()));

        _startTime = std::chrono::high_resolution_clock::now();

        // Create our own output handling thread
        // This must be done after the pipes are populated.
        // Each connection needs to make sure to drain the output from its backing host.
        _hOutputThread.reset(CreateThread(
            nullptr,
            0,
            [](LPVOID lpParameter) noexcept {
                const auto pInstance = static_cast<ConptyConnection*>(lpParameter);
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
            // GH#11556 - make sure to format the error code to this string as an UNSIGNED int
            winrt::hstring exitText{ fmt::format(std::wstring_view{ RS_(L"ProcessExited") }, fmt::format(_errorFormat, status)) };
            _TerminalOutputHandlers(L"\r\n");
            _TerminalOutputHandlers(exitText);
            _TerminalOutputHandlers(L"\r\n");
            _TerminalOutputHandlers(RS_(L"CtrlDToClose"));
            _TerminalOutputHandlers(L"\r\n");
        }
        CATCH_LOG();
    }

    // Method Description:
    // - called when the client application (not necessarily its pty) exits for any reason
    void ConptyConnection::_LastConPtyClientDisconnected() noexcept
    try
    {
        DWORD exitCode{ 0 };
        GetExitCodeProcess(_piClient.hProcess, &exitCode);

        // Signal the closing or failure of the process.
        // exitCode might be STILL_ACTIVE if a client has called FreeConsole() and
        // thus caused the tab to close, even though the CLI app is still running.
        _transitionToState(exitCode == 0 || exitCode == STILL_ACTIVE ? ConnectionState::Closed : ConnectionState::Failed);
        _indicateExitWithStatus(exitCode);
    }
    CATCH_LOG()

    void ConptyConnection::WriteInput(const hstring& data)
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

    void ConptyConnection::Resize(uint32_t rows, uint32_t columns)
    {
        // Always keep these in case we ever want to disconnect/restart
        _rows = rows;
        _cols = columns;

        if (_isConnected())
        {
            THROW_IF_FAILED(ConptyResizePseudoConsole(_hPC.get(), { Utils::ClampToShortMax(columns, 1), Utils::ClampToShortMax(rows, 1) }));
        }
    }

    void ConptyConnection::ClearBuffer()
    {
        // If we haven't connected yet, then we really don't need to do
        // anything. The connection should already start clear!
        if (_isConnected())
        {
            THROW_IF_FAILED(ConptyClearPseudoConsole(_hPC.get()));
        }
    }

    void ConptyConnection::ShowHide(const bool show)
    {
        // If we haven't connected yet, then stash for when we do connect.
        if (_isConnected())
        {
            THROW_IF_FAILED(ConptyShowHidePseudoConsole(_hPC.get(), show));
        }
        else
        {
            _initialVisibility = show;
        }
    }

    void ConptyConnection::ReparentWindow(const uint64_t newParent)
    {
        // If we haven't started connecting at all, stash this HWND to use once we have started.
        if (!_isStateAtOrBeyond(ConnectionState::Connecting))
        {
            _initialParentHwnd = newParent;
        }
        // Otherwise, just inform the conpty of the new owner window handle.
        // This shouldn't be hittable until GH#5000 / GH#1256, when it's
        // possible to reparent terminals to different windows.
        else if (_isConnected())
        {
            THROW_IF_FAILED(ConptyReparentPseudoConsole(_hPC.get(), reinterpret_cast<HWND>(newParent)));
        }
    }

    void ConptyConnection::Close() noexcept
    try
    {
        _transitionToState(ConnectionState::Closing);

        // .reset()ing either of these two will signal ConPTY to send out a CTRL_CLOSE_EVENT to all attached clients.
        // FYI: The other members of this class are concurrently read by the _hOutputThread
        // thread running in the background and so they're not safe to be .reset().
        _hPC.reset();
        _inPipe.reset();

        if (_hOutputThread)
        {
            // Loop around `CancelSynchronousIo()` just in case the signal to shut down was missed.
            // This may happen if we called `CancelSynchronousIo()` while not being stuck
            // in `ReadFile()` and if OpenConsole refuses to exit in a timely manner.
            for (;;)
            {
                // ConptyConnection::Close() blocks the UI thread, because `_TerminalOutputHandlers` might indirectly
                // reference UI objects like `ControlCore`. CancelSynchronousIo() allows us to have the background
                // thread exit as fast as possible by aborting any ongoing writes coming from OpenConsole.
                CancelSynchronousIo(_hOutputThread.get());

                // Waiting for the output thread to exit ensures that all pending _TerminalOutputHandlers()
                // calls have returned and won't notify our caller (ControlCore) anymore. This ensures that
                // we don't call a destroyed event handler asynchronously from a background thread (GH#13880).
                const auto result = WaitForSingleObject(_hOutputThread.get(), 1000);
                if (result == WAIT_OBJECT_0)
                {
                    break;
                }

                LOG_LAST_ERROR();
            }
        }

        // Now that the background thread is done, we can safely clean up the other system objects, without
        // race conditions, or fear of deadlocking ourselves (e.g. by calling CloseHandle() on _outPipe).
        _outPipe.reset();
        _hOutputThread.reset();
        _piClient.reset();

        _transitionToState(ConnectionState::Closed);
    }
    CATCH_LOG()

    // Returns the command line of the given process.
    // Requires PROCESS_BASIC_INFORMATION | PROCESS_VM_READ privileges.
    winrt::hstring ConptyConnection::_commandlineFromProcess(HANDLE process)
    {
        struct PROCESS_BASIC_INFORMATION
        {
            NTSTATUS ExitStatus;
            PPEB PebBaseAddress;
            ULONG_PTR AffinityMask;
            KPRIORITY BasePriority;
            ULONG_PTR UniqueProcessId;
            ULONG_PTR InheritedFromUniqueProcessId;
        } info;
        THROW_IF_NTSTATUS_FAILED(NtQueryInformationProcess(process, ProcessBasicInformation, &info, sizeof(info), nullptr));

        // PEB: Process Environment Block
        // This is a funny structure allocated by the kernel which contains all sorts of useful
        // information, only a tiny fraction of which are documented publicly unfortunately.
        // Fortunately however it contains a copy of the command line the process launched with.
        PEB peb;
        THROW_IF_WIN32_BOOL_FALSE(ReadProcessMemory(process, info.PebBaseAddress, &peb, sizeof(peb), nullptr));

        RTL_USER_PROCESS_PARAMETERS params;
        THROW_IF_WIN32_BOOL_FALSE(ReadProcessMemory(process, peb.ProcessParameters, &params, sizeof(params), nullptr));

        // Yeah I know... Don't use "impl" stuff... But why do you make something _that_ useful private? :(
        // The hstring_builder allows us to create a hstring without intermediate copies. Neat!
        winrt::impl::hstring_builder commandline{ params.CommandLine.Length / 2u };
        THROW_IF_WIN32_BOOL_FALSE(ReadProcessMemory(process, params.CommandLine.Buffer, commandline.data(), params.CommandLine.Length, nullptr));
        return commandline.to_hstring();
    }

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

            // When we call CancelSynchronousIo() in Close() this is the branch that's taken and gets us out of here.
            if (_isStateAtOrBeyond(ConnectionState::Closing))
            {
                return 0;
            }

            if (readFail) // reading failed (we must check this first, because read will also be 0.)
            {
                // EXIT POINT
                const auto lastError = GetLastError();
                if (lastError == ERROR_BROKEN_PIPE)
                {
                    _LastConPtyClientDisconnected();
                    return S_OK;
                }
                else
                {
                    _indicateExitWithStatus(HRESULT_FROM_WIN32(lastError)); // print a message
                    _transitionToState(ConnectionState::Failed);
                    return gsl::narrow_cast<DWORD>(HRESULT_FROM_WIN32(lastError));
                }
            }

            const auto result{ til::u8u16(std::string_view{ _buffer.data(), read }, _u16Str, _u8State) };
            if (FAILED(result))
            {
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
                                  TraceLoggingGuid(_sessionId, "SessionGuid", "The WT_SESSION's GUID"),
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

    winrt::event_token ConptyConnection::NewConnection(const NewConnectionHandler& handler) { return _newConnectionHandlers.add(handler); };
    void ConptyConnection::NewConnection(const winrt::event_token& token) { _newConnectionHandlers.remove(token); };

    void ConptyConnection::closePseudoConsoleAsync(HPCON hPC) noexcept
    {
        ::ConptyClosePseudoConsoleTimeout(hPC, 0);
    }

    HRESULT ConptyConnection::NewHandoff(HANDLE in, HANDLE out, HANDLE signal, HANDLE ref, HANDLE server, HANDLE client, TERMINAL_STARTUP_INFO startupInfo) noexcept
    try
    {
        _newConnectionHandlers(winrt::make<ConptyConnection>(signal, in, out, ref, server, client, startupInfo));

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
