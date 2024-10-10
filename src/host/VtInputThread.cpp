// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "VtInputThread.hpp"

#include "handle.h"
#include "output.h"
#include "server.h"
#include "../interactivity/inc/ServiceLocator.hpp"
#include "../terminal/adapter/InteractDispatch.hpp"
#include "../terminal/parser/InputStateMachineEngine.hpp"
#include "../types/inc/utils.hpp"

using namespace Microsoft::Console;
using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::VirtualTerminal;
// Constructor Description:
// - Creates the VT Input Thread.
// Arguments:
// - hPipe - a handle to the file representing the read end of the VT pipe.
// - inheritCursor - a bool indicating if the state machine should expect a
//      cursor positioning sequence. See MSFT:15681311.
VtInputThread::VtInputThread(_In_ wil::unique_hfile hPipe, const bool inheritCursor) :
    _hFile{ std::move(hPipe) }
{
    THROW_HR_IF(E_HANDLE, _hFile.get() == INVALID_HANDLE_VALUE);

    auto dispatch = std::make_unique<InteractDispatch>();
    auto engine = std::make_unique<InputStateMachineEngine>(std::move(dispatch), inheritCursor);
    _pInputStateMachine = std::make_unique<StateMachine>(std::move(engine));
}

// Function Description:
// - Static function used for initializing an instance's ThreadProc.
// Arguments:
// - lpParameter - A pointer to the VtInputThread instance that should be called.
// Return Value:
// - The return value of the underlying instance's _InputThread
DWORD WINAPI VtInputThread::StaticVtInputThreadProc(_In_ LPVOID lpParameter)
{
    const auto pInstance = static_cast<VtInputThread*>(lpParameter);
    pInstance->_InputThread();
    return S_OK;
}

// Method Description:
// - The ThreadProc for the VT Input Thread. Reads input from the pipe, and
//      passes it to _HandleRunInput to be processed by the
//      InputStateMachineEngine.
void VtInputThread::_InputThread()
{
    const auto cleanup = wil::scope_exit([this]() {
        _hFile.reset();
        ServiceLocator::LocateGlobals().getConsoleInformation().GetVtIo()->SendCloseEvent();
    });

    OVERLAPPED* overlapped = nullptr;
    OVERLAPPED overlappedBuf{};
    wil::unique_event overlappedEvent;
    bool overlappedPending = false;
    char buffer[4096];
    DWORD read = 0;

    til::u8state u8State;
    std::wstring wstr;

    if (Utils::HandleWantsOverlappedIo(_hFile.get()))
    {
        overlappedEvent.reset(CreateEventExW(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS));
        if (overlappedEvent)
        {
            overlappedBuf.hEvent = overlappedEvent.get();
            overlapped = &overlappedBuf;
        }
    }

    // If we use overlapped IO We want to queue ReadFile() calls before processing the
    // string, because LockConsole/ProcessString may take a while (relatively speaking).
    // That's why the loop looks a little weird as it starts a read, processes the
    // previous string, and finally converts the previous read to the next string.
    for (;;)
    {
        // When we have a `wstr` that's ready for processing we must do so without blocking.
        // Otherwise, whatever the user typed will be delayed until the next IO operation.
        // With overlapped IO that's not a problem because the ReadFile() calls won't block.
        if (overlapped)
        {
            if (!ReadFile(_hFile.get(), &buffer[0], sizeof(buffer), &read, overlapped))
            {
                if (GetLastError() != ERROR_IO_PENDING)
                {
                    break;
                }
                overlappedPending = true;
            }
        }

        // wstr can be empty in two situations:
        // * The previous call to til::u8u16 failed.
        // * We're using overlapped IO, and it's the first iteration.
        if (!wstr.empty())
        {
            try
            {
                // Make sure to call the GLOBAL Lock/Unlock, not the gci's lock/unlock.
                // Only the global unlock attempts to dispatch ctrl events. If you use the
                //      gci's unlock, when you press C-c, it won't be dispatched until the
                //      next console API call. For something like `powershell sleep 60`,
                //      that won't happen for 60s
                LockConsole();
                const auto unlock = wil::scope_exit([&] { UnlockConsole(); });

                _pInputStateMachine->ProcessString(wstr);
            }
            CATCH_LOG();
        }

        // Here's the counterpart to the start of the loop. We processed whatever was in `wstr`,
        // so blocking synchronously on the pipe is now possible.
        // If we used overlapped IO, we need to wait for the ReadFile() to complete.
        // If we didn't, we can now safely block on our ReadFile() call.
        if (overlapped)
        {
            if (overlappedPending)
            {
                overlappedPending = false;
                if (FAILED(Utils::GetOverlappedResultSameThread(overlapped, &read)))
                {
                    break;
                }
            }
        }
        else
        {
            if (!ReadFile(_hFile.get(), &buffer[0], sizeof(buffer), &read, overlapped))
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

        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ConPTY ReadFile",
            TraceLoggingCountedUtf8String(&buffer[0], read, "buffer"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));

        // If we hit a parsing error, eat it. It's bad utf-8, we can't do anything with it.
        FAILED_LOG(til::u8u16({ &buffer[0], gsl::narrow_cast<size_t>(read) }, wstr, u8State));
    }
}

// Method Description:
// - Starts the VT input thread.
[[nodiscard]] HRESULT VtInputThread::Start()
{
    RETURN_HR_IF(E_HANDLE, !_hFile);

    HANDLE hThread = nullptr;
    // 0 is the right value, https://blogs.msdn.microsoft.com/oldnewthing/20040223-00/?p=40503
    DWORD dwThreadId = 0;

    hThread = CreateThread(nullptr,
                           0,
                           VtInputThread::StaticVtInputThreadProc,
                           this,
                           0,
                           &dwThreadId);

    RETURN_LAST_ERROR_IF_NULL(hThread);
    _hThread.reset(hThread);
    _dwThreadId = dwThreadId;
    LOG_IF_FAILED(SetThreadDescription(hThread, L"ConPTY Input Handler Thread"));

    return S_OK;
}

til::enumset<DeviceAttribute, uint64_t> VtInputThread::WaitUntilDA1(DWORD timeout) const noexcept
{
    const auto& engine = static_cast<InputStateMachineEngine&>(_pInputStateMachine->Engine());
    return engine.WaitUntilDA1(timeout);
}
