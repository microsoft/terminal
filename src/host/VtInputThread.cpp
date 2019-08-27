// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "VtInputThread.hpp"

#include "../interactivity/inc/ServiceLocator.hpp"
#include "input.h"
#include "../terminal/parser/InputStateMachineEngine.hpp"
#include "outputStream.hpp" // For ConhostInternalGetSet
#include "../terminal/adapter/InteractDispatch.hpp"
#include "../types/inc/convert.hpp"
#include "server.h"
#include "output.h"
#include "handle.h"

using namespace Microsoft::Console;
using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::VirtualTerminal;
// Constructor Description:
// - Creates the VT Input Thread.
// Arguments:
// - hPipe - a handle to the file representing the read end of the VT pipe.
// - inheritCursor - a bool indicating if the state machine should expect a
//      cursor positioning sequence. See MSFT:15681311.
VtInputThread::VtInputThread(wil::unique_hfile hPipe,
                             wil::shared_event shutdownEvent,
                             const bool inheritCursor) :
    _hFile{ std::move(hPipe) },
    _shutdownEvent{ shutdownEvent },
    _hThread{},
    _utf8Parser{ CP_UTF8 },
    _dwThreadId{ 0 }
{
    THROW_HR_IF(E_HANDLE, !_shutdownEvent);
    THROW_HR_IF(E_HANDLE, _hFile.get() == INVALID_HANDLE_VALUE);

    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    auto pGetSet = std::make_unique<ConhostInternalGetSet>(gci);
    THROW_IF_NULL_ALLOC(pGetSet.get());

    auto engine = std::make_unique<InputStateMachineEngine>(new InteractDispatch(pGetSet.release()), inheritCursor);
    THROW_IF_NULL_ALLOC(engine.get());

    _pInputStateMachine = std::make_unique<StateMachine>(engine.release());
    THROW_IF_NULL_ALLOC(_pInputStateMachine.get());

    _shutdownWatchdog = std::async(std::launch::async, [&] {
        _shutdownEvent.wait();
        if (_dwThreadId != 0)
        {
            wil::unique_handle threadHandle(OpenThread(STANDARD_RIGHTS_ALL | THREAD_TERMINATE, FALSE, _dwThreadId));
            LOG_LAST_ERROR_IF_NULL(threadHandle.get());
            if (threadHandle)
            {
                LOG_IF_WIN32_BOOL_FALSE(CancelSynchronousIo(threadHandle.get()));
            }
        }
    });
}

VtInputThread::~VtInputThread()
{
    if (_shutdownEvent)
    {
        _shutdownEvent.SetEvent();
    }
}

// Method Description:
// - Processes a buffer of input characters. The characters should be utf-8
//      encoded, and will get converted to wchar_t's to be processed by the
//      input state machine.
// Arguments:
// - charBuffer - the UTF-8 characters recieved.
// - cch - number of UTF-8 characters in charBuffer
// Return Value:
// - S_OK on success, otherwise an appropriate failure.
[[nodiscard]] HRESULT VtInputThread::_HandleRunInput(_In_reads_(cch) const byte* const charBuffer, const int cch)
{
    // Make sure to call the GLOBAL Lock/Unlock, not the gci's lock/unlock.
    // Only the global unlock attempts to dispatch ctrl events. If you use the
    //      gci's unlock, when you press C-c, it won't be dispatched until the
    //      next console API call. For something like `powershell sleep 60`,
    //      that won't happen for 60s
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        std::unique_ptr<wchar_t[]> pwsSequence;
        unsigned int cchConsumed;
        unsigned int cchSequence;
        auto hr = _utf8Parser.Parse(charBuffer, cch, cchConsumed, pwsSequence, cchSequence);
        // If we hit a parsing error, eat it. It's bad utf-8, we can't do anything with it.
        if (FAILED(hr))
        {
            return S_FALSE;
        }
        _pInputStateMachine->ProcessString(pwsSequence.get(), cchSequence);
    }
    CATCH_RETURN();

    return S_OK;
}

// Function Description:
// - Static function used for initializing an instance's ThreadProc.
// Arguments:
// - lpParameter - A pointer to the VtInputThread instance that should be called.
// Return Value:
// - The return value of the underlying instance's _InputThread
DWORD WINAPI VtInputThread::StaticVtInputThreadProc(_In_ LPVOID lpParameter)
{
    VtInputThread* const pInstance = reinterpret_cast<VtInputThread*>(lpParameter);
    return pInstance->_InputThread();
}

[[nodiscard]] HRESULT VtInputThread::DoReadInput()
{
    // If there's already a thread pumping VT input, it's not valid to read this from the outside.
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_STATE), _dwThreadId != 0);

    // Store which thread is attempting to read VT input. It may get blocked indefinitely and need
    // to get unstuck by a shutdown event.
    _dwThreadId = GetCurrentThreadId();

    // Set it back to 0 on the way out.
    auto restoreThreadId = wil::scope_exit([&] {
        _dwThreadId = 0;
    });

    // Perform the blocking read operation.
    return _ReadInput();
}

// Method Description:
// - Do a single ReadFile from our pipe, and try and handle it. 
// Arguments:
// - <none>
// Return Value:
// - S_OK or relevant error
[[nodiscard]] HRESULT VtInputThread::_ReadInput()
{
    // TODO: Make it so my only public call can be during initialization.

    byte buffer[256];
    DWORD dwRead = 0;

    RETURN_IF_WIN32_BOOL_FALSE(ReadFile(_hFile.get(), buffer, ARRAYSIZE(buffer), &dwRead, nullptr));

    RETURN_IF_FAILED(_HandleRunInput(buffer, dwRead));

    return S_OK;
}

// Method Description:
// - The ThreadProc for the VT Input Thread. Reads input from the pipe, and
//      passes it to _HandleRunInput to be processed by the
//      InputStateMachineEngine.
// Return Value:
// - Any error from reading the pipe or writing to the input buffer that might
//      have caused us to exit.
DWORD VtInputThread::_InputThread()
{
    auto onExitTriggerShutdown = wil::scope_exit([&] {
        _shutdownEvent.SetEvent();
    });

    while (true)
    {
        RETURN_IF_FAILED(_ReadInput());
    }

    return S_OK;
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

    return S_OK;
}
