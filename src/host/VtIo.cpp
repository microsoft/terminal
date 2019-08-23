// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "VtIo.hpp"
#include "../interactivity/inc/ServiceLocator.hpp"

#include "../renderer/vt/XtermEngine.hpp"
#include "../renderer/vt/Xterm256Engine.hpp"
#include "../renderer/vt/WinTelnetEngine.hpp"

#include "../renderer/base/renderer.hpp"
#include "../types/inc/utils.hpp"
#include "handle.h" // LockConsole and UnlockConsole
#include "output.h" // CloseConsoleProcessState

using namespace Microsoft::Console;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::VirtualTerminal;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Utils;
using namespace Microsoft::Console::Interactivity;

VtIo::VtIo() :
    _initialized(false),
    _objectsCreated(false),
    _lookingForCursorPosition(false),
    _IoMode(VtIoMode::INVALID),
    _shutdownEvent()
{
    
}

// Routine Description:
//  Tries to get the VtIoMode from the given string. If it's not one of the
//      *_STRING constants in VtIoMode.hpp, then it returns E_INVALIDARG.
// Arguments:
//  VtIoMode: A string containing the console's requested VT mode. This can be
//      any of the strings in VtIoModes.hpp
//  pIoMode: recieves the VtIoMode that the string prepresents if it's a valid
//      IO mode string
// Return Value:
//  S_OK if we parsed the string successfully, otherwise E_INVALIDARG indicating failure.
[[nodiscard]] HRESULT VtIo::ParseIoMode(const std::wstring& VtMode, _Out_ VtIoMode& ioMode)
{
    ioMode = VtIoMode::INVALID;

    if (VtMode == XTERM_256_STRING)
    {
        ioMode = VtIoMode::XTERM_256;
    }
    else if (VtMode == XTERM_STRING)
    {
        ioMode = VtIoMode::XTERM;
    }
    else if (VtMode == WIN_TELNET_STRING)
    {
        ioMode = VtIoMode::WIN_TELNET;
    }
    else if (VtMode == XTERM_ASCII_STRING)
    {
        ioMode = VtIoMode::XTERM_ASCII;
    }
    else if (VtMode == DEFAULT_STRING)
    {
        ioMode = VtIoMode::XTERM_256;
    }
    else
    {
        return E_INVALIDARG;
    }
    return S_OK;
}

[[nodiscard]] HRESULT VtIo::Initialize(const ConsoleArguments* const pArgs)
{
    _lookingForCursorPosition = pArgs->GetInheritCursor();

    // If we were already given VT handles, set up the VT IO engine to use those.
    if (pArgs->InConptyMode())
    {
        return _Initialize(pArgs->GetVtInHandle(), pArgs->GetVtOutHandle(), pArgs->GetVtMode(), pArgs->GetSignalHandle());
    }
    // Didn't need to initialize if we didn't have VT stuff. It's still OK, but report we did nothing.
    else
    {
        return S_FALSE;
    }
}

// Routine Description:
//  Tries to initialize this VtIo instance from the given pipe handles and
//      VtIoMode. The pipes should have been created already (by the caller of
//      conhost), in non-overlapped mode.
//  The VtIoMode string can be the empty string as a default value.
// Arguments:
//  InHandle: a valid file handle. The console will
//      read VT sequences from this pipe to generate INPUT_RECORDs and other
//      input events.
//  OutHandle: a valid file handle. The console
//      will be "rendered" to this pipe using VT sequences
//  VtIoMode: A string containing the console's requested VT mode. This can be
//      any of the strings in VtIoModes.hpp
//  SignalHandle: an optional file handle that will be used to send signals into the console.
//      This represents the ability to send signals to a *nix tty/pty.
// Return Value:
//  S_OK if we initialized successfully, otherwise an appropriate HRESULT
//      indicating failure.
[[nodiscard]] HRESULT VtIo::_Initialize(const HANDLE InHandle,
                                        const HANDLE OutHandle,
                                        const std::wstring& VtMode,
                                        _In_opt_ const HANDLE SignalHandle)
{
    FAIL_FAST_IF_MSG(_initialized, "Someone attempted to double-_Initialize VtIo");

    RETURN_IF_FAILED(ParseIoMode(VtMode, _IoMode));

    _hInput.reset(InHandle);
    _hOutput.reset(OutHandle);
    _hSignal.reset(SignalHandle);

    // Since we have a valid request for a PTY, set up the events and watchdog mechanisms
    // required to tear everything apart correctly at the end of a PTY session.
    _shutdownEvent.create(wil::EventOptions::ManualReset);

    _shutdownWatchdog = std::async(std::launch::async, [&] {
        _shutdownEvent.wait();

        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });
        
        CloseConsoleProcessState();
    });

    // We also need to register to know when the last process is exiting.
    ServiceLocator::LocateGlobals().getConsoleInformation().ProcessHandleList.RegisterForNotifyOnLastFree(std::bind(&VtIo::_OnLastProcessExit, this));

    // The only way we're initialized is if the args said we're in conpty mode.
    // If the args say so, then at least one of in, out, or signal was specified
    _initialized = true;

    return S_OK;
}

// Method Description:
// - Create the VtRenderer and the VtInputThread for this console.
// MUST BE DONE AFTER CONSOLE IS INITIALIZED, to make sure we've gotten the
//  buffer size from the attached client application.
// Arguments:
// - <none>
// Return Value:
//  S_OK if we initialized successfully,
//  S_FALSE if VtIo hasn't been initialized (or we're not in conpty mode)
//  otherwise an appropriate HRESULT indicating failure.
[[nodiscard]] HRESULT VtIo::CreateIoHandlers() noexcept
{
    if (!_initialized)
    {
        return S_FALSE;
    }

    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    try
    {
        if (IsValidHandle(_hInput.get()))
        {
            _pVtInputThread = std::make_unique<VtInputThread>(std::move(_hInput), _shutdownEvent, _lookingForCursorPosition);
        }

        if (IsValidHandle(_hOutput.get()))
        {
            Viewport initialViewport = Viewport::FromDimensions({ 0, 0 },
                                                                gci.GetWindowSize().X,
                                                                gci.GetWindowSize().Y);
            switch (_IoMode)
            {
            case VtIoMode::XTERM_256:
                _pVtRenderEngine = std::make_unique<Xterm256Engine>(std::move(_hOutput),
                                                                    _shutdownEvent,
                                                                    gci,
                                                                    initialViewport,
                                                                    gci.GetColorTable(),
                                                                    static_cast<WORD>(gci.GetColorTableSize()));
                break;
            case VtIoMode::XTERM:
                _pVtRenderEngine = std::make_unique<XtermEngine>(std::move(_hOutput),
                                                                 _shutdownEvent,
                                                                 gci,
                                                                 initialViewport,
                                                                 gci.GetColorTable(),
                                                                 static_cast<WORD>(gci.GetColorTableSize()),
                                                                 false);
                break;
            case VtIoMode::XTERM_ASCII:
                _pVtRenderEngine = std::make_unique<XtermEngine>(std::move(_hOutput),
                                                                 _shutdownEvent,
                                                                 gci,
                                                                 initialViewport,
                                                                 gci.GetColorTable(),
                                                                 static_cast<WORD>(gci.GetColorTableSize()),
                                                                 true);
                break;
            case VtIoMode::WIN_TELNET:
                _pVtRenderEngine = std::make_unique<WinTelnetEngine>(std::move(_hOutput),
                                                                     _shutdownEvent,
                                                                     gci,
                                                                     initialViewport,
                                                                     gci.GetColorTable(),
                                                                     static_cast<WORD>(gci.GetColorTableSize()));
                break;
            default:
                return E_FAIL;
            }
        }
    }
    CATCH_RETURN();

    _objectsCreated = true;
    return S_OK;
}

bool VtIo::IsUsingVt() const
{
    return _objectsCreated;
}

// Routine Description:
//  Potentially starts this VtIo's input thread and render engine.
//      If the VtIo hasn't yet been given pipes, then this function will
//      silently do nothing. It's the responsibility of the caller to make sure
//      that the pipes are initialized first with VtIo::Initialize
// Arguments:
//  <none>
// Return Value:
//  S_OK if we started successfully or had nothing to start, otherwise an
//      appropriate HRESULT indicating failure.
[[nodiscard]] HRESULT VtIo::StartIfNeeded()
{
    // If we haven't been set up, do nothing (because there's nothing to start)
    if (!_objectsCreated)
    {
        return S_FALSE;
    }
    Globals& g = ServiceLocator::LocateGlobals();

    if (_pVtRenderEngine)
    {
        try
        {
            g.pRender->AddRenderEngine(_pVtRenderEngine.get());
            g.getConsoleInformation().GetActiveOutputBuffer().SetTerminalConnection(_pVtRenderEngine.get());
        }
        CATCH_RETURN();
    }
    
    if (_pVtInputThread)
    {
        RETURN_IF_FAILED(_pVtInputThread->Start());
    }

    // MSFT: 15813316
    // If the terminal application wants us to inherit the cursor position,
    //  we're going to emit a VT sequence to ask for the cursor position, then
    //  read input until we get a response. Terminals who request this behavior
    //  but don't respond will hang.
    // If we get a response, the InteractDispatch will call SetCursorPosition,
    //      which will call to our VtIo::SetCursorPosition method.
    // We need both handles for this initialization to work. If we don't have
    //      both, we'll skip it. They either aren't going to be reading output
    //      (so they can't get the DSR) or they can't write the response to us.
    // GH Microsoft/Terminal#1810:
    // - We can't just infinite loop and let them hang. It needs to recognize an error
    //   condition otherwise it can be getting notified on another thread of a proper shutdown
    //   and be unable to actually leave this loop and let go of the lock.
    if (_lookingForCursorPosition && _pVtRenderEngine && _pVtInputThread)
    {
        RETURN_IF_FAILED(_pVtRenderEngine->RequestCursor());
        while (!_shutdownEvent.is_signaled() && _lookingForCursorPosition)
        {
            std::this_thread::yield(); // Give room for the VT Input thread to go pick up the answer.
            // and timeout?
        }
    }

    if (_pPtySignalInputThread)
    {
        // Let the signal thread know that the console is connected
        _pPtySignalInputThread->ConnectConsole();
    }

    return S_OK;
}

// Method Description:
// - Create and start the signal thread. The signal thread can be created
//      independent of the i/o threads, and doesn't require a client first
//      attaching to the console. We need to create it first and foremost,
//      because it's possible that a terminal application could
//      CreatePseudoConsole, then ClosePseudoConsole without ever attaching a
//      client. Should that happen, we still need to exit.
// Arguments:
// - <none>
// Return Value:
// - S_FALSE if we're not in VtIo mode,
//   S_OK if we succeeded,
//   otherwise an appropriate HRESULT indicating failure.
[[nodiscard]] HRESULT VtIo::CreateAndStartSignalThread() noexcept
{
    if (!_initialized)
    {
        return S_FALSE;
    }

    // If we were passed a signal handle, try to open it and make a signal reading thread.
    if (IsValidHandle(_hSignal.get()))
    {
        try
        {
            _pPtySignalInputThread = std::make_unique<PtySignalInputThread>(std::move(_hSignal), _shutdownEvent);

            // Start it if it was successfully created.
            RETURN_IF_FAILED(_pPtySignalInputThread->Start());
        }
        CATCH_RETURN();
    }

    return S_OK;
}

// Method Description:
// - Prevent the renderer from emitting output on the next resize. This prevents
//      the host from echoing a resize to the terminal that requested it.
// Arguments:
// - <none>
// Return Value:
// - S_OK if the renderer successfully suppressed the next repaint, otherwise an
//      appropriate HRESULT indicating failure.
[[nodiscard]] HRESULT VtIo::SuppressResizeRepaint()
{
    HRESULT hr = S_OK;
    if (_pVtRenderEngine)
    {
        hr = _pVtRenderEngine->SuppressResizeRepaint();
    }
    return hr;
}

// Method Description:
// - Attempts to set the initial cursor position, if we're looking for it.
//      If we're not trying to inherit the cursor, does nothing.
// Arguments:
// - coordCursor: The initial position of the cursor.
// Return Value:
// - S_OK if we successfully inherited the cursor or did nothing, else an
//      appropriate HRESULT
[[nodiscard]] HRESULT VtIo::SetCursorPosition(const COORD coordCursor)
{
    HRESULT hr = S_OK;
    if (_lookingForCursorPosition)
    {
        if (_pVtRenderEngine)
        {
            hr = _pVtRenderEngine->InheritCursor(coordCursor);
        }

        _lookingForCursorPosition = false;
    }
    return hr;
}

// Method Description:
// - Tell the vt renderer to begin a resize operation. During a resize
//   operation, the vt renderer should _not_ request to be repainted during a
//   text buffer circling event. Any callers of this method should make sure to
//   call EndResize to make sure the renderer returns to normal behavior.
//   See GH#1795 for context on this method.
// Arguments:
// - <none>
// Return Value:
// - <none>
void VtIo::BeginResize()
{
    if (_pVtRenderEngine)
    {
        _pVtRenderEngine->BeginResizeRequest();
    }
}

// Method Description:
// - Tell the vt renderer to end a resize operation.
//   See BeginResize for more details.
//   See GH#1795 for context on this method.
// Arguments:
// - <none>
// Return Value:
// - <none>
void VtIo::EndResize()
{
    if (_pVtRenderEngine)
    {
        _pVtRenderEngine->EndResizeRequest();
    }
}

// Method Description:
// - Called when the last client process exits. We'll use this to shut off the main IO thread.
// Arguments:
// - <none>
// Return Value:
// - <none>
void VtIo::_OnLastProcessExit()
{
    // If a shutdown is signaled because one of the PTY pipes has broken connection with the
    // hosting Terminal on top and we've been notified that the last client application has just
    // disconnected, then we want to stop all IO channel communication.
    //
    // Normally IO channel communication stops itself when all outstanding references to the console
    // session are broken with the driver, but in this circumstance, the Terminal application may still
    // be holding a server, reference, or other handle to the session. It is implied that breaking any
    // one of the connections or calling ClosePseudoConsole is a specific request to close all those things off from the
    // Terminal's end. That means those handles are not really valid means of keeping the session open and therefore
    // once the client connections are gone, there's no longer a reason to stick around even if the Terminal on top
    // didn't fully clean up all of its handles before reaching this state.
    if (_shutdownEvent.is_signaled())
    {
        ServiceLocator::LocateGlobals().pDeviceComm->Shutdown();
    }
}
