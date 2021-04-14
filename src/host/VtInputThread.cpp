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
VtInputThread::VtInputThread(_In_ wil::unique_hfile hPipe,
                             const bool inheritCursor) :
    _hFile{ std::move(hPipe) },
    _hThread{},
    _u8State{},
    _dwThreadId{ 0 },
    _exitRequested{ false },
    _exitResult{ S_OK }
{
    THROW_HR_IF(E_HANDLE, _hFile.get() == INVALID_HANDLE_VALUE);

    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    auto pGetSet = std::make_unique<ConhostInternalGetSet>(gci);

    auto dispatch = std::make_unique<InteractDispatch>(std::move(pGetSet));

    auto engine = std::make_unique<InputStateMachineEngine>(std::move(dispatch), inheritCursor);

    auto engineRef = engine.get();

    _pInputStateMachine = std::make_unique<StateMachine>(std::move(engine));

    // we need this callback to be able to flush an unknown input sequence to the app
    auto flushCallback = std::bind(&StateMachine::FlushToTerminal, _pInputStateMachine.get());
    engineRef->SetFlushToInputQueueCallback(flushCallback);
}

// Method Description:
// - Processes a string of input characters. The characters should be UTF-8
//      encoded, and will get converted to wstring to be processed by the
//      input state machine.
// Arguments:
// - u8Str - the UTF-8 string received.
// Return Value:
// - S_OK on success, otherwise an appropriate failure.
[[nodiscard]] HRESULT VtInputThread::_HandleRunInput(const std::string_view u8Str)
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
        std::wstring wstr{};
        auto hr = til::u8u16(u8Str, wstr, _u8State);
        // If we hit a parsing error, eat it. It's bad utf-8, we can't do anything with it.
        if (FAILED(hr))
        {
            return S_FALSE;
        }
        _pInputStateMachine->ProcessString(wstr);
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

// Method Description:
// - Do a single ReadFile from our pipe, and try and handle it. If handling
//      failed, throw or log, depending on what the caller wants.
// Arguments:
// - throwOnFail: If true, throw an exception if there was an error processing
//      the input received. Otherwise, log the error.
// Return Value:
// - <none>
void VtInputThread::DoReadInput(const bool throwOnFail)
{
    char buffer[256];
    DWORD dwRead = 0;
    bool fSuccess = !!ReadFile(_hFile.get(), buffer, ARRAYSIZE(buffer), &dwRead, nullptr);

    // If we failed to read because the terminal broke our pipe (usually due
    //      to dying itself), close gracefully with ERROR_BROKEN_PIPE.
    // Otherwise throw an exception. ERROR_BROKEN_PIPE is the only case that
    //       we want to gracefully close in.
    if (!fSuccess)
    {
        _exitRequested = true;
        _exitResult = HRESULT_FROM_WIN32(GetLastError());
        return;
    }

    HRESULT hr = _HandleRunInput({ buffer, gsl::narrow_cast<size_t>(dwRead) });
    if (FAILED(hr))
    {
        if (throwOnFail)
        {
            _exitResult = hr;
            _exitRequested = true;
        }
        else
        {
            LOG_IF_FAILED(hr);
        }
    }
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
    while (!_exitRequested)
    {
        DoReadInput(true);
    }
    ServiceLocator::LocateGlobals().getConsoleInformation().GetVtIo()->CloseInput();

    return _exitResult;
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
