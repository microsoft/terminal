// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ConptyConnection.h"

#include <conpty-static.h>
#include <winmeta.h>

#include "CTerminalHandoff.h"
#include "LibraryResources.h"
#include "../../types/inc/utils.hpp"

#include "ConptyConnection.g.cpp"

using namespace ::Microsoft::Console;

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
    // - launches the client application attached to the new pseudoconsole
    void ConptyConnection::_LaunchAttachedClient()
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
        THROW_IF_WIN32_BOOL_FALSE(InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &size));

        THROW_IF_WIN32_BOOL_FALSE(UpdateProcThreadAttribute(
            siEx.lpAttributeList,
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

            // WSLENV.1: Get a handle to the WSLENV environment variable.
            auto& wslEnv = environment.as_map()[L"WSLENV"];
            std::wstring additionalWslEnv;

            // WSLENV.2: Figure out what variables are already in WSLENV.
            std::unordered_set<std::wstring> wslEnvVars{
                // We never want to put a custom Windows PATH variable into WSLENV,
                // because that would override WSL's computation of the NIX PATH.
                L"PATH",
            };
            for (const auto& part : til::split_iterator{ std::wstring_view{ wslEnv }, L':' })
            {
                // Each part may contain a variable name and flags (e.g., /p, /l, etc.)
                // We only care about the variable name for WSLENV.
                const auto key = til::safe_slice_len(part, 0, part.rfind(L'/'));
                wslEnvVars.emplace(key);
            }

            // WSLENV.3: Add our terminal-specific environment variables to WSLENV.
            static constexpr std::wstring_view builtinWslEnvVars[] = {
                L"WT_SESSION",
                L"WT_PROFILE_ID",
            };
            // Misdiagnosis in MSVC 14.44.35207. No pointer arithmetic in sight.
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
            for (const auto& key : builtinWslEnvVars)
            {
                if (wslEnvVars.emplace(key).second)
                {
                    additionalWslEnv.append(key);
                    additionalWslEnv.push_back(L':');
                }
            }

            // add additional env vars
            if (_environment)
            {
                for (const auto item : _environment)
                {
                    try
                    {
                        const auto key = item.Key();
                        // This will throw if the value isn't a string. If that
                        // happens, then just skip this entry.
                        const auto value = winrt::unbox_value<hstring>(_environment.Lookup(key));

                        environment.set_user_environment_var(key, value);

                        // WSLENV.4: Add custom user environment variables to WSLENV.
                        if (wslEnvVars.emplace(key).second)
                        {
                            additionalWslEnv.append(key);
                            additionalWslEnv.push_back(L':');
                        }
                    }
                    CATCH_LOG();
                }
            }

            if (!additionalWslEnv.empty())
            {
                // WSLENV.5: In the next step we'll prepend `additionalWslEnv` to `wslEnv`,
                // so make sure that we have a single colon in between them.
                const auto hasColon = additionalWslEnv.ends_with(L':');
                const auto needsColon = !wslEnv.starts_with(L':');
                if (hasColon != needsColon)
                {
                    if (hasColon)
                    {
                        additionalWslEnv.pop_back();
                    }
                    else
                    {
                        additionalWslEnv.push_back(L':');
                    }
                }

                // WSLENV.6: Prepend our additional environment variables to WSLENV.
                wslEnv.insert(0, additionalWslEnv);
            }
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

        THROW_IF_WIN32_BOOL_FALSE(CreateProcessW(
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
    }

    // Who decided that?
#pragma warning(suppress : 26455) // Default constructor should not throw. Declare it 'noexcept' (f.6).
    ConptyConnection::ConptyConnection() :
        _writeOverlappedEvent{ CreateEventExW(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS) }
    {
        THROW_LAST_ERROR_IF(!_writeOverlappedEvent);
        _writeOverlapped.hEvent = _writeOverlappedEvent.get();
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
            _profileGuid = unbox_prop_or<winrt::guid>(settings, L"profileGuid", _profileGuid);

            _flags = 0;

            // If we're using an existing buffer, we want the new connection
            // to reuse the existing cursor. When not setting this flag, the
            // PseudoConsole sends a clear screen VT code which our renderer
            // interprets into making all the previous lines be outside the
            // current viewport.
            const auto inheritCursor = unbox_prop_or<bool>(settings, L"inheritCursor", false);
            if (inheritCursor)
            {
                _flags |= PSEUDOCONSOLE_INHERIT_CURSOR;
            }

            const auto textMeasurement = unbox_prop_or<winrt::hstring>(settings, L"textMeasurement", winrt::hstring{});
            if (!textMeasurement.empty())
            {
                if (textMeasurement == L"graphemes")
                {
                    _flags |= PSEUDOCONSOLE_GLYPH_WIDTH_GRAPHEMES;
                }
                else if (textMeasurement == L"wcswidth")
                {
                    _flags |= PSEUDOCONSOLE_GLYPH_WIDTH_WCSWIDTH;
                }
                else if (textMeasurement == L"console")
                {
                    _flags |= PSEUDOCONSOLE_GLYPH_WIDTH_CONSOLE;
                }
            }

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

    static wil::unique_hfile duplicateHandle(const HANDLE in)
    {
        wil::unique_hfile h;
        THROW_IF_WIN32_BOOL_FALSE(DuplicateHandle(GetCurrentProcess(), in, GetCurrentProcess(), h.addressof(), 0, FALSE, DUPLICATE_SAME_ACCESS));
        return h;
    }

    // Misdiagnosis: out is being tested right in the first line.
#pragma warning(suppress : 26430) // Symbol 'out' is not tested for nullness on all paths (f.23).
    void ConptyConnection::InitializeFromHandoff(HANDLE* in, HANDLE* out, HANDLE signal, HANDLE reference, HANDLE server, HANDLE client, const TERMINAL_STARTUP_INFO* startupInfo)
    {
        THROW_HR_IF(E_UNEXPECTED, !in || !out || !startupInfo);

        _sessionId = Utils::CreateGuid();

        auto pipe = Utils::CreateOverlappedPipe(PIPE_ACCESS_DUPLEX, 128 * 1024);
        auto pipeClientClone = duplicateHandle(pipe.client.get());

        auto ownedSignal = duplicateHandle(signal);
        auto ownedReference = duplicateHandle(reference);
        auto ownedServer = duplicateHandle(server);
        wil::unique_hfile ownedClient;
        LOG_IF_WIN32_BOOL_FALSE(DuplicateHandle(GetCurrentProcess(), client, GetCurrentProcess(), ownedClient.addressof(), PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_SET_INFORMATION | SYNCHRONIZE, FALSE, 0));
        if (!ownedClient)
        {
            // If we couldn't reopen the handle with SET_INFORMATION, which may be required to do things like QoS management, fall back.
            ownedClient = duplicateHandle(client);
        }

        THROW_IF_FAILED(ConptyPackPseudoConsole(ownedServer.get(), ownedReference.get(), ownedSignal.get(), &_hPC));
        ownedServer.release();
        ownedReference.release();
        ownedSignal.release();

        _piClient.hProcess = ownedClient.release();

        _startupInfo.title = winrt::hstring{ startupInfo->pszTitle, SysStringLen(startupInfo->pszTitle) };
        _startupInfo.iconPath = winrt::hstring{ startupInfo->pszIconPath, SysStringLen(startupInfo->pszIconPath) };
        _startupInfo.iconIndex = startupInfo->iconIndex;
        _startupInfo.showWindow = startupInfo->wShowWindow;

        try
        {
            _commandline = _commandlineFromProcess(_piClient.hProcess);
        }
        CATCH_LOG()

        try
        {
            auto processImageName{ wil::QueryFullProcessImageNameW<std::wstring>(_piClient.hProcess) };
            _clientName = std::filesystem::path{ std::move(processImageName) }.filename().wstring();
        }
        CATCH_LOG()

        _pipe = std::move(pipe.server);
        *in = pipe.client.release();
        *out = pipeClientClone.release();
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
        if (!_pipe)
        {
            auto pipe = Utils::CreateOverlappedPipe(PIPE_ACCESS_DUPLEX, 128 * 1024);
            THROW_IF_FAILED(ConptyCreatePseudoConsole(til::unwrap_coord_size(dimensions), pipe.client.get(), pipe.client.get(), _flags, &_hPC));
            _pipe = std::move(pipe.server);

            if (_initialParentHwnd != 0)
            {
                THROW_IF_FAILED(ConptyReparentPseudoConsole(_hPC.get(), reinterpret_cast<HWND>(_initialParentHwnd)));
            }

            // GH#12515: The conpty assumes it's hidden at the start. If we're visible, let it know now.
            if (_initialVisibility)
            {
                THROW_IF_FAILED(ConptyShowHidePseudoConsole(_hPC.get(), _initialVisibility));
            }

            _LaunchAttachedClient();
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
        const auto failureText = RS_fmt(L"ProcessFailedToLaunch", _formatStatus(hr), _commandline);
        TerminalOutput.raise(failureText);

        // If the path was invalid, let's present an informative message to the user
        if (hr == HRESULT_FROM_WIN32(ERROR_DIRECTORY))
        {
            const auto badPathText = RS_fmt(L"BadPathText", _startingDirectory);
            TerminalOutput.raise(L"\r\n");
            TerminalOutput.raise(badPathText);
        }
        // If the requested action requires elevation, display appropriate message
        else if (hr == HRESULT_FROM_WIN32(ERROR_ELEVATION_REQUIRED))
        {
            const auto elevationText = RS_(L"ElevationRequired");
            TerminalOutput.raise(L"\r\n");
            TerminalOutput.raise(elevationText);
        }
        // If the requested executable was not found, display appropriate message
        else if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        {
            const auto fileNotFoundText = RS_(L"FileNotFound");
            TerminalOutput.raise(L"\r\n");
            TerminalOutput.raise(fileNotFoundText);
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
            const auto msg1 = RS_fmt(L"ProcessExited", _formatStatus(status));
            const auto msg2 = RS_(L"CtrlDToClose");
            const auto msg = fmt::format(FMT_COMPILE(L"\r\n{}\r\n{}\r\n"), msg1, msg2);
            TerminalOutput.raise(msg);
        }
        CATCH_LOG();
    }

    std::wstring ConptyConnection::_formatStatus(uint32_t status)
    {
        return fmt::format(FMT_COMPILE(L"{0} ({0:#010x})"), status);
    }

    // Method Description:
    // - called when the client application (not necessarily its pty) exits for any reason
    void ConptyConnection::_LastConPtyClientDisconnected() noexcept
    try
    {
        DWORD exitCode{ 0 };
        GetExitCodeProcess(_piClient.hProcess, &exitCode);

        _piClient.reset();

        // Signal the closing or failure of the process.
        // exitCode might be STILL_ACTIVE if a client has called FreeConsole() and
        // thus caused the tab to close, even though the CLI app is still running.
        _transitionToState(exitCode == 0 || exitCode == STILL_ACTIVE ? ConnectionState::Closed : ConnectionState::Failed);
        _indicateExitWithStatus(exitCode);
    }
    CATCH_LOG()

    void ConptyConnection::WriteInput(const winrt::array_view<const char16_t> buffer)
    {
        const auto data = winrt_array_to_wstring_view(buffer);

        if (!_isConnected())
        {
            return;
        }

        // Ensure a linear and predictable write order, even across multiple threads.
        // A ticket lock is the perfect fit for this as it acts as first-come-first-serve.
        std::lock_guard guard{ _writeLock };

        if (_writePending)
        {
            _writePending = false;

            DWORD read;
            if (!GetOverlappedResult(_pipe.get(), &_writeOverlapped, &read, TRUE))
            {
                // Not much we can do when the wait fails. This will kill the connection.
                LOG_LAST_ERROR();
                _hPC.reset();
                return;
            }
        }

        if (FAILED_LOG(til::u16u8(data, _writeBuffer)))
        {
            return;
        }

        if (!WriteFile(_pipe.get(), _writeBuffer.data(), gsl::narrow_cast<DWORD>(_writeBuffer.length()), nullptr, &_writeOverlapped))
        {
            switch (const auto gle = GetLastError())
            {
            case ERROR_BROKEN_PIPE:
                _hPC.reset();
                break;
            case ERROR_IO_PENDING:
                _writePending = true;
                break;
            default:
                LOG_WIN32(gle);
                break;
            }
        }
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

    void ConptyConnection::ResetSize()
    {
        if (_isConnected())
        {
            THROW_IF_FAILED(ConptyResizePseudoConsole(_hPC.get(), { Utils::ClampToShortMax(_cols, 1), Utils::ClampToShortMax(_rows, 1) }));
        }
    }

    void ConptyConnection::ClearBuffer(bool keepCursorRow)
    {
        // If we haven't connected yet, then we really don't need to do
        // anything. The connection should already start clear!
        if (_isConnected())
        {
            THROW_IF_FAILED(ConptyClearPseudoConsole(_hPC.get(), keepCursorRow));
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

    uint64_t ConptyConnection::RootProcessHandle() noexcept
    {
#pragma warning(disable : 26490) // Don't use reinterpret_cast (type.1).
        return reinterpret_cast<uint64_t>(_piClient.hProcess);
    }

    void ConptyConnection::Close() noexcept
    try
    {
        _transitionToState(ConnectionState::Closing);

        // This will signal ConPTY to send out a CTRL_CLOSE_EVENT to all attached clients.
        // Once they're all disconnected it'll close its half of the pipes.
        _hPC.reset();

        if (_hOutputThread)
        {
            // Loop around `CancelIoEx()` just in case the signal to shut down was missed.
            for (;;)
            {
                // The output thread may be stuck waiting for the OVERLAPPED to be signaled.
                CancelIoEx(_pipe.get(), nullptr);

                // Waiting for the output thread to exit ensures that all pending TerminalOutput.raise()
                // calls have returned and won't notify our caller (ControlCore) anymore. This ensures that
                // we don't call a destroyed event handler asynchronously from a background thread (GH#13880).
                const auto result = WaitForSingleObject(_hOutputThread.get(), 1000);
                if (result == WAIT_OBJECT_0)
                {
                    break;
                }
            }
        }

        _hOutputThread.reset();
        _piClient.reset();
        _pipe.reset();

        // The output thread should have already transitioned us to Closed.
        // This exists just in case there was no output thread.
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

        const auto cleanup = wil::scope_exit([this]() noexcept {
            _LastConPtyClientDisconnected();
        });

        const wil::unique_event overlappedEvent{ CreateEventExW(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS) };
        OVERLAPPED overlapped{ .hEvent = overlappedEvent.get() };
        bool overlappedPending = false;
        char buffer[128 * 1024];
        DWORD read = 0;

        til::u8state u8State;
        std::wstring wstr;

        // If we use overlapped IO We want to queue ReadFile() calls before processing the
        // string, because TerminalOutput.raise() may take a while (relatively speaking).
        // That's why the loop looks a little weird as it starts a read, processes the
        // previous string, and finally converts the previous read to the next string.
        for (;;)
        {
            // When we have a `wstr` that's ready for processing we must do so without blocking.
            // Otherwise, whatever the user typed will be delayed until the next IO operation.
            // With overlapped IO that's not a problem because the ReadFile() calls won't block.
            if (!ReadFile(_pipe.get(), &buffer[0], sizeof(buffer), &read, &overlapped))
            {
                if (GetLastError() != ERROR_IO_PENDING)
                {
                    break;
                }
                overlappedPending = true;
            }

            // wstr can be empty in two situations:
            // * The previous call to til::u8u16 failed.
            // * We're using overlapped IO, and it's the first iteration.
            if (!wstr.empty())
            {
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

                try
                {
                    TerminalOutput.raise(wstr);
                }
                CATCH_LOG();
            }

            // Here's the counterpart to the start of the loop. We processed whatever was in `wstr`,
            // so blocking synchronously on the pipe is now possible.
            // If we used overlapped IO, we need to wait for the ReadFile() to complete.
            // If we didn't, we can now safely block on our ReadFile() call.
            if (overlappedPending)
            {
                overlappedPending = false;
                if (FAILED(Utils::GetOverlappedResultSameThread(&overlapped, &read)))
                {
                    break;
                }
            }

            // winsock2 (WSA) handles of the \Device\Afd type are transparently compatible with
            // ReadFile() and the WSARecv() documentations contains this important information:
            // > For byte streams, zero bytes having been read [..] indicates graceful closure and that no more bytes will ever be read.
            // --> Exit if we've read 0 bytes.
            if (read == 0)
            {
                break;
            }

            if (_isStateAtOrBeyond(ConnectionState::Closing))
            {
                break;
            }

            TraceLoggingWrite(
                g_hTerminalConnectionProvider,
                "ReadFile",
                TraceLoggingCountedUtf8String(&buffer[0], read, "buffer"),
                TraceLoggingGuid(_sessionId, "session"),
                TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                TraceLoggingKeyword(TIL_KEYWORD_TRACE));

            // If we hit a parsing error, eat it. It's bad utf-8, we can't do anything with it.
            FAILED_LOG(til::u8u16({ &buffer[0], gsl::narrow_cast<size_t>(read) }, wstr, u8State));
        }

        return 0;
    }

    static winrt::event<NewConnectionHandler> _newConnectionHandlers;

    winrt::event_token ConptyConnection::NewConnection(const NewConnectionHandler& handler) { return _newConnectionHandlers.add(handler); };
    void ConptyConnection::NewConnection(const winrt::event_token& token) { _newConnectionHandlers.remove(token); };

    void ConptyConnection::closePseudoConsoleAsync(HPCON hPC) noexcept
    {
        ::ConptyClosePseudoConsole(hPC);
    }

    HRESULT ConptyConnection::NewHandoff(HANDLE* in, HANDLE* out, HANDLE signal, HANDLE reference, HANDLE server, HANDLE client, const TERMINAL_STARTUP_INFO* startupInfo) noexcept
    try
    {
        auto conn = winrt::make_self<ConptyConnection>();
        conn->InitializeFromHandoff(in, out, signal, reference, server, client, startupInfo);
        _newConnectionHandlers(*std::move(conn));
        return S_OK;
    }
    CATCH_RETURN()

    void ConptyConnection::StartInboundListener()
    {
        static const auto init = []() noexcept {
            CTerminalHandoff::s_setCallback(&ConptyConnection::NewHandoff);
            return true;
        }();

        CTerminalHandoff::s_StartListening();
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
    safe_void_coroutine ConptyConnection::final_release(std::unique_ptr<ConptyConnection> connection)
    {
        co_await winrt::resume_background(); // move to background
        connection.reset(); // explicitly destruct
    }
}
