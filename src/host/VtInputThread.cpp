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

    // we need this callback to be able to flush an unknown input sequence to the app
    engine->SetFlushToInputQueueCallback([this] { return _pInputStateMachine->FlushToTerminal(); });

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
    char buffer[4096];

    OVERLAPPED* overlapped = nullptr;
    OVERLAPPED overlappedBuf{};
    wil::unique_event overlappedEvent;
    bool overlappedPending = false;
    DWORD read = 0;

    if (Utils::HandleWantsOverlappedIo(_hFile.get()))
    {
        overlappedEvent.reset(CreateEventExW(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS));
        if (overlappedEvent)
        {
            overlappedBuf.hEvent = overlappedEvent.get();
            overlapped = &overlappedBuf;
        }
    }

    for (;;)
    {
        if (!ReadFile(_hFile.get(), &buffer[0], sizeof(buffer), &read, overlapped))
        {
            if (GetLastError() == ERROR_IO_PENDING)
            {
                overlappedPending = true;
            }
            else
            {
                break;
            }
        }

        // _wstr can be empty in two situations:
        // * The previous call to til::u8u16 failed.
        // * We're using overlapped IO, and it's the first iteration.
        if (!_wstr.empty())
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

                _pInputStateMachine->ProcessString(_wstr);
            }
            CATCH_LOG();
        }

        // If we're using overlapped IO, on the first iteration we'll skip the ProcessString()
        // and call GetOverlappedResult() which blocks until it's done.
        // On every other iteration we'll call ProcessString() while the IO is already queued up.
        if (overlappedPending)
        {
            overlappedPending = false;
            if (!GetOverlappedResult(_hFile.get(), overlapped, &read, TRUE))
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

        // If we hit a parsing error, eat it. It's bad utf-8, we can't do anything with it.
        FAILED_LOG(til::u8u16({ buffer, gsl::narrow_cast<size_t>(read) }, _wstr, _u8State));
    }

    ServiceLocator::LocateGlobals().getConsoleInformation().GetVtIoNoCheck()->CloseInput();
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

void VtInputThread::WaitUntilDSR(DWORD timeout) const noexcept
{
    const auto& engine = static_cast<InputStateMachineEngine&>(_pInputStateMachine->Engine());
    engine.WaitUntilDSR(timeout);
}
