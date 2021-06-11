// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "HostSignalInputThread.hpp"
#include "../inc/HostSignals.hpp"

#include "../interactivity/inc/ServiceLocator.hpp"

using namespace Microsoft::Console;
using namespace Microsoft::Console::Interactivity;

// Constructor Description:
// - Creates the PTY Signal Input Thread.
// Arguments:
// - hPipe - a handle to the file representing the read end of the Host Signal pipe.
HostSignalInputThread::HostSignalInputThread(_In_ wil::unique_hfile hPipe) :
    _hFile{ std::move(hPipe) },
    _hThread{},
    _dwThreadId{ 0 },
    _consoleConnected{ false }
{
    THROW_HR_IF(E_HANDLE, _hFile.get() == INVALID_HANDLE_VALUE);
}

HostSignalInputThread::~HostSignalInputThread()
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
// - lpParameter - A pointer to the HostSignalInputThread instance that should be called.
// Return Value:
// - The return value of the underlying instance's _InputThread
DWORD WINAPI HostSignalInputThread::StaticThreadProc(_In_ LPVOID lpParameter)
{
    HostSignalInputThread* const pInstance = reinterpret_cast<HostSignalInputThread*>(lpParameter);
    return pInstance->_InputThread();
}

// Method Description:
// - Tell us that there's a client attached to the console, so we can actually
//      do something with the messages we receive now. Before this is set, there
//      is no guarantee that a client has attached, so most parts of the console
//      (in and screen buffers) haven't yet been initialized.
// Arguments:
// - <none>
// Return Value:
// - <none>
void HostSignalInputThread::ConnectConsole() noexcept
{
    _consoleConnected = true;
}

// Method Description:
// - The ThreadProc for the Host Signal Input Thread.
// Return Value:
// - S_OK if the thread runs to completion.
// - Otherwise it may cause an application termination another route and never return.
[[nodiscard]] HRESULT HostSignalInputThread::_InputThread()
{
    unsigned short signalId;
    while (_GetData(&signalId, sizeof(signalId)))
    {
        switch (signalId)
        {
        case HOST_SIGNAL_NOTIFY_APP:
        {
            HOST_SIGNAL_NOTIFY_APP_DATA msg = { 0 };
            if (_GetData(&msg, sizeof(msg)))
            {
                if (msg.cbSize >= sizeof(msg))
                {
                    _AdvanceReader(msg.cbSize - sizeof(msg));
                }
                else
                {
                    THROW_HR(E_ILLEGAL_METHOD_CALL);
                }
                
                LOG_IF_NTSTATUS_FAILED(ServiceLocator::LocateConsoleControl()->NotifyConsoleApplication(msg.dwProcessId));
            }
            else
            {
                return E_ABORT;
            }

            break;
        }
        case HOST_SIGNAL_SET_FOREGROUND:
        {
            HOST_SIGNAL_SET_FOREGROUND_DATA msg = { 0 };
            if (_GetData(&msg, sizeof(msg)))
            {
                if (msg.cbSize >= sizeof(msg))
                {
                    _AdvanceReader(msg.cbSize - sizeof(msg));
                }
                else
                {
                    THROW_HR(E_ILLEGAL_METHOD_CALL);
                }

                LOG_IF_NTSTATUS_FAILED(ServiceLocator::LocateConsoleControl()->SetForeground(ULongToHandle(msg.dwProcessId), msg.fForeground));
            }
            else
            {
                return E_ABORT;
            }
            break;
        }
        case HOST_SIGNAL_END_TASK:
        {
            HOST_SIGNAL_END_TASK_DATA msg = { 0 };
            if (_GetData(&msg, sizeof(msg)))
            {
                if (msg.cbSize >= sizeof(msg))
                {
                    _AdvanceReader(msg.cbSize - sizeof(msg));
                }
                else
                {
                    THROW_HR(E_ILLEGAL_METHOD_CALL);
                }

                LOG_IF_NTSTATUS_FAILED(ServiceLocator::LocateConsoleControl()->EndTask(ULongToHandle(msg.dwProcessId), msg.dwEventType, msg.ulCtrlFlags));
            }
            else
            {
                return E_ABORT;
            }
            break;
        }
        default:
        {
            THROW_HR(E_UNEXPECTED);
            break;
        }
        }
    }
    return S_OK;
}

// Method Description:
// - Skips the file stream forward by the specified number of bytes.zs
// Arguments:
// - cbBytes - Count of bytes to skip forward
// Return Value:
// - True if we could skip forward successfully. False otherwise.
bool HostSignalInputThread::_AdvanceReader(const DWORD cbBytes)
{
    std::array<BYTE, 8> buffer;
    auto bytesRemaining = cbBytes;

    while (bytesRemaining > 0)
    {
        const auto advance = std::min(bytesRemaining, gsl::narrow_cast<DWORD>(buffer.max_size()));

        if (!_GetData(buffer.data(), advance))
        {
            return false;
        }

        bytesRemaining -= advance;
    }

    return true;
}

// Method Description:
// - Retrieves bytes from the file stream and exits or throws errors should the pipe state
//   be compromised.
// Arguments:
// - pBuffer - Buffer to fill with data.
// - cbBuffer - Count of bytes in the given buffer.
// Return Value:
// - True if data was retrieved successfully. False otherwise.
bool HostSignalInputThread::_GetData(_Out_writes_bytes_(cbBuffer) void* const pBuffer,
                                    const DWORD cbBuffer)
{
    DWORD dwRead = 0;
    // If we failed to read because the terminal broke our pipe (usually due
    //      to dying itself), close gracefully with ERROR_BROKEN_PIPE.
    // Otherwise throw an exception. ERROR_BROKEN_PIPE is the only case that
    //       we want to gracefully close in.
    if (FALSE == ReadFile(_hFile.get(), pBuffer, cbBuffer, &dwRead, nullptr))
    {
        DWORD lastError = GetLastError();
        if (lastError == ERROR_BROKEN_PIPE)
        {
            _Shutdown();
            return false;
        }
        else
        {
            THROW_WIN32(lastError);
        }
    }
    else if (dwRead != cbBuffer)
    {
        _Shutdown();
        return false;
    }

    return true;
}

// Method Description:
// - Starts the PTY Signal input thread.
[[nodiscard]] HRESULT HostSignalInputThread::Start() noexcept
{
    RETURN_LAST_ERROR_IF(!_hFile);

    HANDLE hThread = nullptr;
    // 0 is the right value, https://blogs.msdn.microsoft.com/oldnewthing/20040223-00/?p=40503
    DWORD dwThreadId = 0;

    hThread = CreateThread(nullptr,
                           0,
                           HostSignalInputThread::StaticThreadProc,
                           this,
                           0,
                           &dwThreadId);

    RETURN_LAST_ERROR_IF_NULL(hThread);
    _hThread.reset(hThread);
    _dwThreadId = dwThreadId;
    LOG_IF_FAILED(SetThreadDescription(hThread, L"Host Signal Handler Thread"));

    return S_OK;
}

// Method Description:
// - Perform a shutdown of the console. This happens when the signal pipe is
//      broken, which means either the parent terminal process has died, or they
//      called ClosePseudoConsole.
//  CloseConsoleProcessState is not enough by itself - it will disconnect
//      clients as if the X was pressed, but then we need to actually still die,
//      so then call RundownAndExit.
// Arguments:
// - <none>
// Return Value:
// - <none>
void HostSignalInputThread::_Shutdown()
{
    // Make sure we terminate.
    ServiceLocator::RundownAndExit(ERROR_BROKEN_PIPE);
}
