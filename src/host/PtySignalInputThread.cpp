// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "PtySignalInputThread.hpp"

#include "output.h"
#include "handle.h"
#include "../interactivity/inc/ServiceLocator.hpp"

using namespace Microsoft::Console;
using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::VirtualTerminal;

// Constructor Description:
// - Creates the PTY Signal Input Thread.
// Arguments:
// - hPipe - a handle to the file representing the read end of the VT pipe.
PtySignalInputThread::PtySignalInputThread(wil::unique_hfile hPipe) :
    _hFile{ std::move(hPipe) },
    _hThread{},
    _api{ ServiceLocator::LocateGlobals().getConsoleInformation() },
    _dwThreadId{ 0 },
    _consoleConnected{ false }
{
    THROW_HR_IF(E_HANDLE, _hFile.get() == INVALID_HANDLE_VALUE);
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
    const auto pInstance = reinterpret_cast<PtySignalInputThread*>(lpParameter);
    return pInstance->_InputThread();
}

// Method Description:
// - Tell us that there's a client attached to the console, so we can actually
//      do something with the messages we receive now. Before this is set, there
//      is no guarantee that a client has attached, so most parts of the console
//      (in and screen buffers) haven't yet been initialized.
// - NOTE: Call under LockConsole() to ensure other threads have an opportunity
//         to set early-work state.
// - We need to do this specifically on the thread with the message pump. If the
//   window is created on another thread, then the window won't have a message
//   pump associated with it, and a DPI change in the connected terminal could
//   end up HANGING THE CONPTY (for example).
// Arguments:
// - <none>
// Return Value:
// - <none>
void PtySignalInputThread::ConnectConsole() noexcept
{
    _consoleConnected = true;
    if (_earlyResize)
    {
        _DoResizeWindow(*_earlyResize);
    }
    if (_initialShowHide)
    {
        _DoShowHide(_initialShowHide->show);
    }

    // If we were given a owner HWND, then manually start the pseudo window now.
    if (_earlyReparent)
    {
        _DoSetWindowParent(*_earlyReparent);
    }
}

// Method Description:
// - The ThreadProc for the PTY Signal Input Thread.
// Return Value:
// - S_OK if the thread runs to completion.
// - Otherwise it may cause an application termination and never return.
[[nodiscard]] HRESULT PtySignalInputThread::_InputThread()
{
    PtySignal signalId;
    while (_GetData(&signalId, sizeof(signalId)))
    {
        switch (signalId)
        {
        case PtySignal::ShowHideWindow:
        {
            ShowHideData msg = { 0 };
            _GetData(&msg, sizeof(msg));

            LockConsole();
            auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

            // If the client app hasn't yet connected, stash our initial
            // visibility for when we do. We default to not being visible - if a
            // terminal wants the ConPTY windows to start "visible", then they
            // should send a ShowHidePseudoConsole(..., true) to tell us to
            // initially be visible.
            //
            // Notably, if they don't, then a ShowWindow(SW_HIDE) on the ConPTY
            // HWND will initially do _nothing_, because the OS will think that
            // the window is already hidden.
            if (!_consoleConnected)
            {
                _initialShowHide = msg;
            }
            else
            {
                _DoShowHide(msg.show);
            }
            break;
        }
        case PtySignal::ClearBuffer:
        {
            LockConsole();
            auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

            // If the client app hasn't yet connected, stash the new size in the launchArgs.
            // We'll later use the value in launchArgs to set up the console buffer
            // We must be under lock here to ensure that someone else doesn't come in
            // and set with `ConnectConsole` while we're looking and modifying this.
            if (_consoleConnected)
            {
                _DoClearBuffer();
            }
            break;
        }
        case PtySignal::ResizeWindow:
        {
            ResizeWindowData resizeMsg = { 0 };
            _GetData(&resizeMsg, sizeof(resizeMsg));

            LockConsole();
            auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

            // If the client app hasn't yet connected, stash the new size in the launchArgs.
            // We'll later use the value in launchArgs to set up the console buffer
            // We must be under lock here to ensure that someone else doesn't come in
            // and set with `ConnectConsole` while we're looking and modifying this.
            if (!_consoleConnected)
            {
                _earlyResize = resizeMsg;
            }
            else
            {
                _DoResizeWindow(resizeMsg);
            }

            break;
        }
        case PtySignal::SetParent:
        {
            SetParentData reparentMessage = { 0 };
            _GetData(&reparentMessage, sizeof(reparentMessage));

            LockConsole();
            auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

            // If the client app hasn't yet connected, stash the new owner.
            // We'll later (PtySignalInputThread::ConnectConsole) use the value
            // to set up the owner of the conpty window.
            if (!_consoleConnected)
            {
                _earlyReparent = reparentMessage;
            }
            else
            {
                _DoSetWindowParent(reparentMessage);
            }

            break;
        }
        default:
        {
            THROW_HR(E_UNEXPECTED);
        }
        }
    }
    return S_OK;
}

// Method Description:
// - Dispatches a resize window message to the rest of the console code
// Arguments:
// - data - Packet information containing width/height (size) information
// Return Value:
// - <none>
void PtySignalInputThread::_DoResizeWindow(const ResizeWindowData& data)
{
    if (_api.ResizeWindow(data.sx, data.sy))
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        THROW_IF_FAILED(gci.GetVtIo()->SuppressResizeRepaint());
    }
}

void PtySignalInputThread::_DoClearBuffer()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    THROW_IF_FAILED(gci.GetActiveOutputBuffer().ClearBuffer());
}

void PtySignalInputThread::_DoShowHide(const bool show)
{
    _api.ShowWindow(show);
}

// Method Description:
// - Update the owner of the pseudo-window we're using for the conpty HWND. This
//   allows to mark the pseudoconsole windows as "owner" by the terminal HWND
//   that's actually hosting them.
// - Refer to GH#2988
// Arguments:
// - data - Packet information containing owner HWND information
// Return Value:
// - <none>
void PtySignalInputThread::_DoSetWindowParent(const SetParentData& data)
{
    // This will initialize s_interactivityFactory for us. It will also
    // conveniently return 0 when we're on OneCore.
    //
    // If the window hasn't been created yet, by some other call to
    // LocatePseudoWindow, then this will also initialize the owner of the
    // window.
    if (const auto pseudoHwnd{ ServiceLocator::LocatePseudoWindow(reinterpret_cast<HWND>(data.handle)) })
    {
        LOG_LAST_ERROR_IF_NULL(::SetParent(pseudoHwnd, reinterpret_cast<HWND>(data.handle)));
    }
}

// Method Description:
// - Retrieves bytes from the file stream and exits or throws errors should the pipe state
//   be compromised.
// Arguments:
// - pBuffer - Buffer to fill with data.
// - cbBuffer - Count of bytes in the given buffer.
// Return Value:
// - True if data was retrieved successfully. False otherwise.
bool PtySignalInputThread::_GetData(_Out_writes_bytes_(cbBuffer) void* const pBuffer,
                                    const DWORD cbBuffer)
{
    DWORD dwRead = 0;
    // If we failed to read because the terminal broke our pipe (usually due
    //      to dying itself), close gracefully with ERROR_BROKEN_PIPE.
    // Otherwise throw an exception. ERROR_BROKEN_PIPE is the only case that
    //       we want to gracefully close in.
    if (FALSE == ReadFile(_hFile.get(), pBuffer, cbBuffer, &dwRead, nullptr))
    {
        auto lastError = GetLastError();
        if (lastError == ERROR_BROKEN_PIPE)
        {
            _Shutdown();
        }
        else
        {
            THROW_WIN32(lastError);
        }
    }
    else if (dwRead != cbBuffer)
    {
        _Shutdown();
    }

    return true;
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
    LOG_IF_FAILED(SetThreadDescription(hThread, L"ConPTY Signal Handler Thread"));

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
void PtySignalInputThread::_Shutdown()
{
    // Trigger process shutdown.
    CloseConsoleProcessState();

    // If we haven't terminated by now, that's because there's a client that's still attached.
    // Force the handling of the control events by the attached clients.
    // As of MSFT:19419231, CloseConsoleProcessState will make sure this
    //      happens if this method is called outside of lock, but if we're
    //      currently locked, we want to make sure ctrl events are handled
    //      _before_ we RundownAndExit.
    LockConsole();
    ProcessCtrlEvents();

    // Make sure we terminate.
    ServiceLocator::RundownAndExit(ERROR_BROKEN_PIPE);
}
