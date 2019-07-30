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
#include "input.h" // ProcessCtrlEvents
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
    _IoMode(VtIoMode::INVALID)
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
            _pVtInputThread = std::make_unique<VtInputThread>(std::move(_hInput), _lookingForCursorPosition);
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
                                                                    gci,
                                                                    initialViewport,
                                                                    gci.GetColorTable(),
                                                                    static_cast<WORD>(gci.GetColorTableSize()));
                break;
            case VtIoMode::XTERM:
                _pVtRenderEngine = std::make_unique<XtermEngine>(std::move(_hOutput),
                                                                 gci,
                                                                 initialViewport,
                                                                 gci.GetColorTable(),
                                                                 static_cast<WORD>(gci.GetColorTableSize()),
                                                                 false);
                break;
            case VtIoMode::XTERM_ASCII:
                _pVtRenderEngine = std::make_unique<XtermEngine>(std::move(_hOutput),
                                                                 gci,
                                                                 initialViewport,
                                                                 gci.GetColorTable(),
                                                                 static_cast<WORD>(gci.GetColorTableSize()),
                                                                 true);
                break;
            case VtIoMode::WIN_TELNET:
                _pVtRenderEngine = std::make_unique<WinTelnetEngine>(std::move(_hOutput),
                                                                     gci,
                                                                     initialViewport,
                                                                     gci.GetColorTable(),
                                                                     static_cast<WORD>(gci.GetColorTableSize()));
                break;
            default:
                return E_FAIL;
            }
            if (_pVtRenderEngine)
            {
                _pVtRenderEngine->SetTerminalOwner(this);
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
    if (_lookingForCursorPosition && _pVtRenderEngine && _pVtInputThread)
    {
        LOG_IF_FAILED(_pVtRenderEngine->RequestCursor());
        while (_lookingForCursorPosition)
        {
            _pVtInputThread->DoReadInput(false);
        }
    }

    if (_pVtInputThread)
    {
        LOG_IF_FAILED(_pVtInputThread->Start());
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
            _pPtySignalInputThread = std::make_unique<PtySignalInputThread>(std::move(_hSignal));

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

void VtIo::CloseInput()
{
    // This will release the lock when it goes out of scope
    std::lock_guard<std::mutex> lk(_shutdownLock);
    _pVtInputThread = nullptr;
    _ShutdownIfNeeded();
}

void VtIo::CloseOutput()
{
    // This will release the lock when it goes out of scope
    std::lock_guard<std::mutex> lk(_shutdownLock);

    Globals& g = ServiceLocator::LocateGlobals();
    // DON'T RemoveRenderEngine, as that requires the engine list lock, and this
    // is usually being triggered on a paint operation, when the lock is already
    // owned by the paint.
    // Instead we're releasing the Engine here. A pointer to it has already been
    // given to the Renderer, so we don't want the unique_ptr to delete it. The
    // Renderer will own its lifetime now.
    _pVtRenderEngine.release();

    g.getConsoleInformation().GetActiveOutputBuffer().SetTerminalConnection(nullptr);

    _ShutdownIfNeeded();
}

void VtIo::_ShutdownIfNeeded()
{
    // The callers should have both accquired the _shutdownLock at this point -
    //      we dont want a race on who is actually responsible for closing it.
    if (_objectsCreated && _pVtInputThread == nullptr && _pVtRenderEngine == nullptr)
    {
        // At this point, we no longer have a renderer or inthread. So we've
        //      effectively been disconnected from the terminal.

        // If we have any remaining attached processes, this will prepare us to send a ctrl+close to them
        // if we don't, this will cause us to rundown and exit.
        CloseConsoleProcessState();

        // If we haven't terminated by now, that's because there's a client that's still attached.
        // Force the handling of the control events by the attached clients.
        // As of MSFT:19419231, CloseConsoleProcessState will make sure this
        //      happens if this method is called outside of lock, but if we're
        //      currently locked, we want to make sure ctrl events are handled
        //      _before_ we RundownAndExit.
        ProcessCtrlEvents();

        // Make sure we terminate.
        ServiceLocator::RundownAndExit(ERROR_BROKEN_PIPE);
    }
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
