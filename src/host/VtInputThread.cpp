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
    const auto cleanup = wil::scope_exit([this]() {
        ServiceLocator::LocateGlobals().getConsoleInformation().GetVtIo()->CloseInput();
    });

    char buffer[4096];
    DWORD dwRead = 0;

    til::u8state u8State;
    std::wstring wstr;

    for (;;)
    {
        const auto ok = ReadFile(_hFile.get(), buffer, ARRAYSIZE(buffer), &dwRead, nullptr);

        // The ReadFile() documentations calls out that:
        // > If the lpNumberOfBytesRead parameter is zero when ReadFile returns TRUE on a pipe, the other
        // > end of the pipe called the WriteFile function with nNumberOfBytesToWrite set to zero.
        // But I was unable to replicate any such behavior. I'm not sure it's true anymore.
        //
        // However, what the documentations fails to mention is that winsock2 (WSA) handles of the \Device\Afd type are
        // transparently compatible with ReadFile() and the WSARecv() documentations contains this important information:
        // > For byte streams, zero bytes having been read [..] indicates graceful closure and that no more bytes will ever be read.
        // In other words, for pipe HANDLE of unknown type you should consider `lpNumberOfBytesRead == 0` as an exit indicator.
        //
        // Here, `dwRead == 0` fixes a deadlock when exiting conhost while being in use by WSL whose hypervisor pipes are WSA.
        if (!ok || dwRead == 0)
        {
            break;
        }

        // If we hit a parsing error, eat it. It's bad utf-8, we can't do anything with it.
        if (FAILED_LOG(til::u8u16({ buffer, gsl::narrow_cast<size_t>(dwRead) }, wstr, u8State)))
        {
            continue;
        }

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
