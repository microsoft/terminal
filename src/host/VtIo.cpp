// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "VtIo.hpp"
#include "../interactivity/inc/ServiceLocator.hpp"

#include "../renderer/vt/XtermEngine.hpp"
#include "../renderer/vt/Xterm256Engine.hpp"

#include "../renderer/base/renderer.hpp"
#include "../types/inc/utils.hpp"
#include "handle.h" // LockConsole
#include "input.h" // ProcessCtrlEvents
#include "output.h" // CloseConsoleProcessState

#include "VtApiRoutines.h"

using namespace Microsoft::Console;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::VirtualTerminal;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Utils;
using namespace Microsoft::Console::Interactivity;

VtIo::VtIo() :
    _initialized(false),
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
//  pIoMode: receives the VtIoMode that the string represents if it's a valid
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
    _resizeQuirk = pArgs->IsResizeQuirkEnabled();
    _passthroughMode = pArgs->IsPassthroughMode();

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
    auto& globals = ServiceLocator::LocateGlobals();

    const auto& gci = globals.getConsoleInformation();
    // SetWindowVisibility uses the console lock to protect access to _pVtRenderEngine.
    assert(gci.IsConsoleLocked());

    try
    {
        if (IsValidHandle(_hInput.get()))
        {
            _pVtInputThread = std::make_unique<VtInputThread>(std::move(_hInput), _lookingForCursorPosition);
        }

        if (IsValidHandle(_hOutput.get()))
        {
            auto initialViewport = Viewport::FromDimensions({ 0, 0 },
                                                            gci.GetWindowSize().width,
                                                            gci.GetWindowSize().height);
            switch (_IoMode)
            {
            case VtIoMode::XTERM_256:
            {
                auto xterm256Engine = std::make_unique<Xterm256Engine>(std::move(_hOutput),
                                                                       initialViewport);
                if constexpr (Feature_VtPassthroughMode::IsEnabled())
                {
                    if (_passthroughMode)
                    {
                        auto vtapi = new VtApiRoutines();
                        vtapi->m_pVtEngine = xterm256Engine.get();
                        vtapi->m_pUsualRoutines = globals.api;

                        xterm256Engine->SetPassthroughMode(true);

                        if (_pVtInputThread)
                        {
                            auto pfnSetListenForDSR = std::bind(&VtInputThread::SetLookingForDSR, _pVtInputThread.get(), std::placeholders::_1);
                            xterm256Engine->SetLookingForDSRCallback(pfnSetListenForDSR);
                        }

                        globals.api = vtapi;
                    }
                }

                _pVtRenderEngine = std::move(xterm256Engine);
                break;
            }
            case VtIoMode::XTERM:
            {
                _pVtRenderEngine = std::make_unique<XtermEngine>(std::move(_hOutput),
                                                                 initialViewport,
                                                                 false);
                if (_passthroughMode)
                {
                    return E_NOTIMPL;
                }
                break;
            }
            case VtIoMode::XTERM_ASCII:
            {
                _pVtRenderEngine = std::make_unique<XtermEngine>(std::move(_hOutput),
                                                                 initialViewport,
                                                                 true);

                if (_passthroughMode)
                {
                    return E_NOTIMPL;
                }
                break;
            }
            default:
            {
                return E_FAIL;
            }
            }
            if (_pVtRenderEngine)
            {
                _pVtRenderEngine->SetTerminalOwner(this);
                _pVtRenderEngine->SetResizeQuirk(_resizeQuirk);
            }
        }
    }
    CATCH_RETURN();

    return S_OK;
}

bool VtIo::IsUsingVt() const
{
    return _initialized;
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
    if (!_initialized)
    {
        return S_FALSE;
    }
    auto& g = ServiceLocator::LocateGlobals();

    if (_pVtRenderEngine)
    {
        try
        {
            g.pRender->AddRenderEngine(_pVtRenderEngine.get());
            g.getConsoleInformation().GetActiveOutputBuffer().SetTerminalConnection(_pVtRenderEngine.get());
            g.getConsoleInformation().GetActiveInputBuffer()->SetTerminalConnection(_pVtRenderEngine.get());

            // Force the whole window to be put together first.
            // We don't really need the handle, we just want to leverage the setup steps.
            ServiceLocator::LocatePseudoWindow();
        }
        CATCH_RETURN();
    }

    // GH#4999 - Send a sequence to the connected terminal to request
    // win32-input-mode from them. This will enable the connected terminal to
    // send us full INPUT_RECORDs as input. If the terminal doesn't understand
    // this sequence, it'll just ignore it.
    LOG_IF_FAILED(_pVtRenderEngine->RequestWin32Input());

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
        // Let the signal thread know that the console is connected.
        //
        // By this point, the pseudo window should have already been created, by
        // ConsoleInputThreadProcWin32. That thread has a message pump, which is
        // needed to ensure that DPI change messages to the owning terminal
        // window don't end up hanging because the pty didn't also process it.
        _pPtySignalInputThread->ConnectConsole();
    }

    return S_OK;
}

// Method Description:
// - Create our pseudo window. This is exclusively called by
//   ConsoleInputThreadProcWin32 on the console input thread.
//    * It needs to be called on that thread, before any other calls to
//      LocatePseudoWindow, to make sure that the input thread is the HWND's
//      message thread.
//    * It needs to be plumbed through the signal thread, because the signal
//      thread knows if someone should be marked as the window's owner. It's
//      VERY IMPORTANT that any initial owners are set up when the window is
//      first created.
// - Refer to GH#13066 for details.
void VtIo::CreatePseudoWindow()
{
    if (_pPtySignalInputThread)
    {
        _pPtySignalInputThread->CreatePseudoWindow();
    }
    else
    {
        ServiceLocator::LocatePseudoWindow();
    }
}

void VtIo::SetWindowVisibility(bool showOrHide) noexcept
{
    auto& gci = ::Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();

    gci.LockConsole();
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    // ConsoleInputThreadProcWin32 calls VtIo::CreatePseudoWindow,
    // which calls CreateWindowExW, which causes a WM_SIZE message.
    // In short, this function might be called before _pVtRenderEngine exists.
    // See PtySignalInputThread::CreatePseudoWindow().
    if (!_pVtRenderEngine)
    {
        return;
    }

    LOG_IF_FAILED(_pVtRenderEngine->SetWindowVisibility(showOrHide));
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
    auto hr = S_OK;
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
[[nodiscard]] HRESULT VtIo::SetCursorPosition(const til::point coordCursor)
{
    auto hr = S_OK;
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

[[nodiscard]] HRESULT VtIo::SwitchScreenBuffer(const bool useAltBuffer)
{
    auto hr = S_OK;
    if (_pVtRenderEngine)
    {
        hr = _pVtRenderEngine->SwitchScreenBuffer(useAltBuffer);
    }
    return hr;
}

void VtIo::CloseInput()
{
    _pVtInputThread = nullptr;
    SendCloseEvent();
}

void VtIo::CloseOutput()
{
    auto& g = ServiceLocator::LocateGlobals();
    g.getConsoleInformation().GetActiveOutputBuffer().SetTerminalConnection(nullptr);
}

void VtIo::SendCloseEvent()
{
    LockConsole();
    const auto unlock = wil::scope_exit([] { UnlockConsole(); });

    // This function is called when the ConPTY signal pipe is closed (PtySignalInputThread) and when the input
    // pipe is closed (VtIo). Usually these two happen at about the same time. This if condition is a bit of
    // a premature optimization and prevents us from sending out a CTRL_CLOSE_EVENT right after another.
    if (!std::exchange(_closeEventSent, true))
    {
        CloseConsoleProcessState();
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

#ifdef UNIT_TESTING
// Method Description:
// - This is a test helper method. It can be used to trick VtIo into responding
//   true to `IsUsingVt`, which will cause the console host to act in conpty
//   mode.
// Arguments:
// - vtRenderEngine: a VT renderer that our VtIo should use as the vt engine during these tests
// Return Value:
// - <none>
void VtIo::EnableConptyModeForTests(std::unique_ptr<Microsoft::Console::Render::VtEngine> vtRenderEngine)
{
    _initialized = true;
    _pVtRenderEngine = std::move(vtRenderEngine);
}
#endif

// Method Description:
// - Returns true if the Resize Quirk is enabled. This changes the behavior of
//   conpty to _not_ InvalidateAll the entire viewport on a resize operation.
//   This is used by the Windows Terminal, because it is prepared to be
//   connected to a conpty, and handles its own buffer specifically for a
//   conpty scenario.
// - See also: GH#3490, #4354, #4741
// Arguments:
// - <none>
// Return Value:
// - true iff we were started with the `--resizeQuirk` flag enabled.
bool VtIo::IsResizeQuirkEnabled() const
{
    return _resizeQuirk;
}

// Method Description:
// - Manually tell the renderer that it should emit a "Erase Scrollback"
//   sequence to the connected terminal. We need to do this in certain cases
//   that we've identified where we believe the client wanted the entire
//   terminal buffer cleared, not just the viewport. For more information, see
//   GH#3126.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we wrote the sequences successfully, otherwise an appropriate HRESULT
[[nodiscard]] HRESULT VtIo::ManuallyClearScrollback() const noexcept
{
    if (_pVtRenderEngine)
    {
        return _pVtRenderEngine->ManuallyClearScrollback();
    }
    return S_OK;
}
