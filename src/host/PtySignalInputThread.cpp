// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "PtySignalInputThread.hpp"

#include "output.h"
#include "handle.h"
#include "..\interactivity\inc\ServiceLocator.hpp"
#include "..\terminal\adapter\DispatchCommon.hpp"

#define PTY_SIGNAL_RESIZE_WINDOW 8u

struct PTY_SIGNAL_RESIZE
{
    unsigned short sx;
    unsigned short sy;
};

using namespace Microsoft::Console;
using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::VirtualTerminal;

// Constructor Description:
// - Creates the PTY Signal Input Thread.
// Arguments:
// - hPipe - a handle to the file representing the read end of the VT pipe.
PtySignalInputThread::PtySignalInputThread(wil::unique_hfile hPipe,
                                           wil::shared_event shutdownEvent) :
    _hFile{ std::move(hPipe) },
    _shutdownEvent{ shutdownEvent },
    _hThread{},
    _pConApi{ std::make_unique<ConhostInternalGetSet>(ServiceLocator::LocateGlobals().getConsoleInformation()) },
    _dwThreadId{ 0 },
    _consoleConnected{ false }
{
    THROW_HR_IF(E_HANDLE, !_shutdownEvent);
    THROW_HR_IF(E_HANDLE, _hFile.get() == INVALID_HANDLE_VALUE);
    THROW_IF_NULL_ALLOC(_pConApi.get());
}

PtySignalInputThread::~PtySignalInputThread()
{
    // Manually terminate our thread during unittesting. Otherwise, the test
    //      will finish, but TAEF will not manually kill the test.
#ifdef UNIT_TESTING
    TerminateThread(_hThread.get(), 0);
#endif
}

// Function Description:
// - Static function used for initializing an instance's ThreadProc.
// Arguments:
// - lpParameter - A pointer to the PtySignalInputTHread instance that should be called.
// Return Value:
// - The return value of the underlying instance's _InputThread
DWORD WINAPI PtySignalInputThread::StaticThreadProc(_In_ LPVOID lpParameter)
{
    PtySignalInputThread* const pInstance = reinterpret_cast<PtySignalInputThread*>(lpParameter);
    return pInstance->_InputThread();
}

// Method Description:
// - Tell us that there's a client attached to the console, so we can actually
//      do something with the messages we recieve now. Before this is set, there
//      is no guarantee that a client has attached, so most parts of the console
//      (in and screen buffers) haven't yet been initialized.
// Arguments:
// - <none>
// Return Value:
// - <none>
void PtySignalInputThread::ConnectConsole() noexcept
{
    _consoleConnected = true;
}

// Method Description:
// - The ThreadProc for the PTY Signal Input Thread.
// Return Value:
// - S_OK if the thread runs to completion.
// - Otherwise it may cause an application termination another route and never return.
[[nodiscard]] HRESULT PtySignalInputThread::_InputThread()
{
    auto onExitTriggerShutdown = wil::scope_exit([&] {
        _shutdownEvent.SetEvent();
    });

    while (true)
    {
        unsigned short signalId;
        RETURN_IF_FAILED(_GetData(&signalId, sizeof(signalId)));

        switch (signalId)
        {
        case PTY_SIGNAL_RESIZE_WINDOW:
        {
            PTY_SIGNAL_RESIZE resizeMsg = { 0 };
            RETURN_IF_FAILED(_GetData(&resizeMsg, sizeof(resizeMsg)));

            LockConsole();
            auto Unlock = wil::scope_exit([&] { UnlockConsole(); });
            // If the client app hasn't yet connected, stash the new size in the launchArgs.
            // We'll later use the value in launchArgs to set up the console buffer
            if (!_consoleConnected)
            {
                short sColumns = 0;
                short sRows = 0;
                if (SUCCEEDED(UShortToShort(resizeMsg.sx, &sColumns)) &&
                    SUCCEEDED(UShortToShort(resizeMsg.sy, &sRows)) &&
                    (sColumns > 0 && sRows > 0))
                {
                    ServiceLocator::LocateGlobals().launchArgs.SetExpectedSize({ sColumns, sRows });
                }
                break;
            }
            else
            {
                if (DispatchCommon::s_ResizeWindow(*_pConApi, resizeMsg.sx, resizeMsg.sy))
                {
                    DispatchCommon::s_SuppressResizeRepaint(*_pConApi);
                }
            }

            break;
        }
        default:
        {
            RETURN_HR(E_UNEXPECTED);
        }
        }
    }
    return S_OK;
}

// Method Description:
// - Retrieves bytes from the file stream and exits or throws errors should the pipe state
//   be compromised.
// Arguments:
// - pBuffer - Buffer to fill with data.
// - cbBuffer - Count of bytes in the given buffer.
// Return Value:
// - True if data was retrieved successfully. False otherwise.
[[nodiscard]] HRESULT PtySignalInputThread::_GetData(_Out_writes_bytes_(cbBuffer) void* const pBuffer,
                                                     const DWORD cbBuffer)
{
    DWORD dwRead = 0;
    // If we failed to read because the terminal broke our pipe (usually due
    //      to dying itself), close gracefully with ERROR_BROKEN_PIPE.
    // Otherwise throw an exception. ERROR_BROKEN_PIPE is the only case that
    //       we want to gracefully close in.
    RETURN_IF_WIN32_BOOL_FALSE(ReadFile(_hFile.get(), pBuffer, cbBuffer, &dwRead, nullptr));

    RETURN_HR_IF(E_UNEXPECTED, dwRead != cbBuffer);

    return S_OK;
}

// Method Description:
// - Starts the PTY Signal input thread.
[[nodiscard]] HRESULT PtySignalInputThread::Start() noexcept
{
    RETURN_LAST_ERROR_IF(!_hFile);

    HANDLE hThread = nullptr;
    // 0 is the right value, https://blogs.msdn.microsoft.com/oldnewthing/20040223-00/?p=40503
    DWORD dwThreadId = 0;

    hThread = CreateThread(nullptr,
                           0,
                           PtySignalInputThread::StaticThreadProc,
                           this,
                           0,
                           &dwThreadId);

    RETURN_LAST_ERROR_IF_NULL(hThread);
    _hThread.reset(hThread);
    _dwThreadId = dwThreadId;

    return S_OK;
}
