// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "VtInputThread.hpp"

#include "../interactivity/inc/ServiceLocator.hpp"
#include "input.h"
#include "../terminal/parser/InputStateMachineEngine.hpp"
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
    _pfnSetLookingForDSR{}
{
    THROW_HR_IF(E_HANDLE, _hFile.get() == INVALID_HANDLE_VALUE);

    auto dispatch = std::make_unique<InteractDispatch>();

    auto engine = std::make_unique<InputStateMachineEngine>(std::move(dispatch), inheritCursor);

    auto engineRef = engine.get();

    _pInputStateMachine = std::make_unique<StateMachine>(std::move(engine));

    // we need this callback to be able to flush an unknown input sequence to the app
    auto flushCallback = std::bind(&StateMachine::FlushToTerminal, _pInputStateMachine.get());
    engineRef->SetFlushToInputQueueCallback(flushCallback);

    // we need this callback to capture the reply if someone requests a status from the terminal
    _pfnSetLookingForDSR = std::bind(&InputStateMachineEngine::SetLookingForDSR, engineRef, std::placeholders::_1);
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
    const auto pInstance = reinterpret_cast<VtInputThread*>(lpParameter);
    pInstance->_InputThread();
    return S_OK;
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
    auto fSuccess = !!ReadFile(_hFile.get(), buffer, ARRAYSIZE(buffer), &dwRead, nullptr);

    if (!fSuccess)
    {
        _exitRequested = true;
        return;
    }

    auto hr = _HandleRunInput({ buffer, gsl::narrow_cast<size_t>(dwRead) });
    if (FAILED(hr))
    {
        if (throwOnFail)
        {
            _exitRequested = true;
        }
        else
        {
            LOG_IF_FAILED(hr);
        }
    }
}

void VtInputThread::SetLookingForDSR(const bool looking) noexcept
{
    if (_pfnSetLookingForDSR)
    {
        _pfnSetLookingForDSR(looking);
    }
}

// Method Description:
// - The ThreadProc for the VT Input Thread. Reads input from the pipe, and
//      passes it to _HandleRunInput to be processed by the
//      InputStateMachineEngine.
void VtInputThread::_InputThread()
{
    while (!_exitRequested)
    {
        DoReadInput(true);
    }
    ServiceLocator::LocateGlobals().getConsoleInformation().GetVtIo()->CloseInput();
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
