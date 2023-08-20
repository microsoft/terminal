// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ConIoSrvComm.hpp"
#include "ConIoSrv.h"

#include "../../host/dbcs.h"
#include "../../host/input.h"
#include "../../types/inc/IInputEvent.hpp"

#include "../inc/ServiceLocator.hpp"

#pragma hdrstop

// For details on the mechanisms employed in this class, read the comments in
// ConIoSrv.h, included above. For security-related considerations, see Trust.h
// in the ConIoSrv directory.

extern void LockConsole();
extern void UnlockConsole();

using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Interactivity::OneCore;

static std::unique_ptr<ConIoSrvComm> s_conIoSrvComm;
ConIoSrvComm* ConIoSrvComm::GetConIoSrvComm()
{
    static auto initialized = []() {
        s_conIoSrvComm = std::make_unique<ConIoSrvComm>();
        ServiceLocator::SetOneCoreTeardownFunction([]() noexcept {
            s_conIoSrvComm.reset(nullptr);
        });
        return true;
    }();
    return s_conIoSrvComm.get();
}

ConIoSrvComm::ConIoSrvComm() noexcept :
    _inputPipeThreadHandle(nullptr),
    _pipeReadHandle(INVALID_HANDLE_VALUE),
    _pipeWriteHandle(INVALID_HANDLE_VALUE),
    _alpcClientCommunicationPort(INVALID_HANDLE_VALUE),
    _alpcSharedViewSize(0),
    _alpcSharedViewBase(nullptr),
    _displayMode(CIS_DISPLAY_MODE_NONE),
    _fIsInputInitialized(false)
{
}

ConIoSrvComm::~ConIoSrvComm()
{
    // Cancel pending IOs on the input thread that might get us stuck.
    if (_inputPipeThreadHandle)
    {
#pragma warning(suppress : 26447)
        LOG_IF_WIN32_BOOL_FALSE(CancelSynchronousIo(_inputPipeThreadHandle));
        CloseHandle(_inputPipeThreadHandle);
        _inputPipeThreadHandle = nullptr;
    }

    // Free any handles we might have open.
    if (INVALID_HANDLE_VALUE != _pipeReadHandle)
    {
        CloseHandle(_pipeReadHandle);
        _pipeReadHandle = INVALID_HANDLE_VALUE;
    }

    if (INVALID_HANDLE_VALUE != _pipeWriteHandle)
    {
        CloseHandle(_pipeWriteHandle);
        _pipeWriteHandle = INVALID_HANDLE_VALUE;
    }

    if (INVALID_HANDLE_VALUE != _alpcClientCommunicationPort)
    {
        CloseHandle(_alpcClientCommunicationPort);
        _alpcClientCommunicationPort = INVALID_HANDLE_VALUE;
    }
}

#pragma region Communication

[[nodiscard]] NTSTATUS ConIoSrvComm::Connect()
{
    // Port handle and name.
    HANDLE PortHandle;
    static UNICODE_STRING PortName = RTL_CONSTANT_STRING(CIS_ALPC_PORT_NAME);

    // Generic Object Manager attributes for the port object and ALPC-specific
    // port attributes.
    OBJECT_ATTRIBUTES ObjectAttributes;
    ALPC_PORT_ATTRIBUTES PortAttributes;

    // Connection message.
    CIS_MSG ConnectionMessage;
    SIZE_T ConnectionMessageLength;

    // Connection message attributes.
    SIZE_T ConnectionMessageAttributesBufferLength;
    std::aligned_storage_t<CIS_MSG_ATTR_BUFFER_SIZE, alignof(ALPC_MESSAGE_ATTRIBUTES)> ConnectionMessageAttributesBuffer;

    // Structure used to iterate over the handles given to us by the server.
    ALPC_MESSAGE_HANDLE_INFORMATION HandleInfo;

    // Initialize the attributes of the port object.
#pragma warning(suppress : 26477) // This macro contains a bare NULL
    InitializeObjectAttributes(&ObjectAttributes,
                               nullptr,
                               0,
                               nullptr,
                               nullptr);

    // Initialize the connection message attributes.
    const auto ConnectionMessageAttributes = reinterpret_cast<PALPC_MESSAGE_ATTRIBUTES>(&ConnectionMessageAttributesBuffer);

    auto Status = AlpcInitializeMessageAttribute(CIS_MSG_ATTR_FLAGS,
                                                 ConnectionMessageAttributes,
                                                 CIS_MSG_ATTR_BUFFER_SIZE,
                                                 &ConnectionMessageAttributesBufferLength);

    // Set up the default security QoS descriptor.
    const SECURITY_QUALITY_OF_SERVICE DefaultQoS = {
        sizeof(SECURITY_QUALITY_OF_SERVICE),
        SecurityAnonymous,
        SECURITY_DYNAMIC_TRACKING,
        FALSE
    };

    // Set up the port attributes.
    PortAttributes.Flags = ALPC_PORFLG_ACCEPT_DUP_HANDLES |
                           ALPC_PORFLG_ACCEPT_INDIRECT_HANDLES;
    PortAttributes.MaxMessageLength = sizeof(CIS_MSG);
    PortAttributes.MaxPoolUsage = 0x4000;
    PortAttributes.MaxSectionSize = 0;
    PortAttributes.MaxTotalSectionSize = 0;
    PortAttributes.MaxViewSize = 0;
    PortAttributes.MemoryBandwidth = 0;
    PortAttributes.SecurityQos = DefaultQoS;
    PortAttributes.DupObjectTypes = OB_FILE_OBJECT_TYPE;

    // Initialize the connection message structure.
    ConnectionMessage.AlpcHeader.MessageId = 0;
    ConnectionMessage.AlpcHeader.u2.ZeroInit = 0;

    ConnectionMessage.AlpcHeader.u1.s1.TotalLength = sizeof(CIS_MSG);
    ConnectionMessage.AlpcHeader.u1.s1.DataLength = sizeof(CIS_MSG) - sizeof(PORT_MESSAGE);

    ConnectionMessage.AlpcHeader.ClientId.UniqueProcess = nullptr;
    ConnectionMessage.AlpcHeader.ClientId.UniqueThread = nullptr;

    // Request to connect to the server.
    ConnectionMessageLength = sizeof(CIS_MSG);
    Status = NtAlpcConnectPort(&PortHandle,
                               &PortName,
                               nullptr,
                               &PortAttributes,
                               ALPC_MSGFLG_SYNC_REQUEST,
                               nullptr,
                               &ConnectionMessage.AlpcHeader,
                               &ConnectionMessageLength,
                               nullptr,
                               ConnectionMessageAttributes,
                               nullptr);
    if (SUCCEEDED_NTSTATUS(Status))
    {
        const auto ViewAttributes = ALPC_GET_DATAVIEW_ATTRIBUTES(ConnectionMessageAttributes);
        const auto HandleAttributes = ALPC_GET_HANDLE_ATTRIBUTES(ConnectionMessageAttributes);

        // We must have exactly two handles, one for read, and one for write for
        // the pipe.
        if (HandleAttributes->HandleCount != 2)
        {
            Status = STATUS_UNSUCCESSFUL;
        }

        if (SUCCEEDED_NTSTATUS(Status))
        {
            // Get each handle out. ALPC does not allow to pass indirect handles
            // all at once; they must be retrieved one by one.
            for (ULONG Index = 0; Index < HandleAttributes->HandleCount; Index++)
            {
                HandleInfo.Index = Index;

                Status = NtAlpcQueryInformationMessage(PortHandle,
                                                       &ConnectionMessage.AlpcHeader,
                                                       AlpcMessageHandleInformation,
                                                       &HandleInfo,
                                                       sizeof(HandleInfo),
                                                       nullptr);
                if (SUCCEEDED_NTSTATUS(Status))
                {
                    if (Index == 0)
                    {
                        _pipeReadHandle = ULongToHandle(HandleInfo.Handle);
                    }
                    else if (Index == 1)
                    {
                        _pipeWriteHandle = ULongToHandle(HandleInfo.Handle);
                    }
                }
            }

            // Keep the shared view information.
            _alpcSharedViewSize = ViewAttributes->ViewSize;
            _alpcSharedViewBase = ViewAttributes->ViewBase;

            // As well as a pointer to our communication port.
            _alpcClientCommunicationPort = PortHandle;

            // Zero out the view.
            memset(_alpcSharedViewBase, 0, _alpcSharedViewSize);

            // Get the display mode out of the connection message.
            _displayMode = ConnectionMessage.GetDisplayModeParams.DisplayMode;
        }
    }

    return Status;
}

[[nodiscard]] NTSTATUS ConIoSrvComm::EnsureConnection()
{
    NTSTATUS Status;

    if (_alpcClientCommunicationPort == INVALID_HANDLE_VALUE)
    {
        Status = Connect();
    }
    else
    {
        Status = STATUS_SUCCESS;
    }

    return Status;
}

VOID ConIoSrvComm::ServiceInputPipe()
{
    // Save off a handle to the thread that is coming in here in case it gets blocked and we need to tear down.
    THROW_HR_IF(E_NOT_VALID_STATE, !!_inputPipeThreadHandle); // We can't store two of them, so it's invalid if there are two.
    THROW_IF_WIN32_BOOL_FALSE(DuplicateHandle(GetCurrentProcess(),
                                              GetCurrentThread(),
                                              GetCurrentProcess(),
                                              &_inputPipeThreadHandle,
                                              0,
                                              FALSE,
                                              DUPLICATE_SAME_ACCESS));

    CIS_EVENT Event{};

    while (TRUE)
    {
        const auto Ret = ReadFile(_pipeReadHandle,
                                  &Event,
                                  sizeof(CIS_EVENT),
                                  nullptr,
                                  nullptr);

        if (Ret != FALSE)
        {
            LockConsole();
            switch (Event.Type)
            {
            case CIS_EVENT_TYPE_INPUT:
                try
                {
                    HandleGenericKeyEvent(Event.InputEvent.Record, false);
                }
                catch (...)
                {
                    LOG_HR(wil::ResultFromCaughtException());
                }
                break;
            case CIS_EVENT_TYPE_FOCUS:
                HandleFocusEvent(&Event);
                break;
            default:
                break;
            }
            UnlockConsole();
        }
        else
        {
            // If we get disconnected, terminate.
            ServiceLocator::RundownAndExit(GetLastError());
        }
    }
}

[[nodiscard]] NTSTATUS ConIoSrvComm::SendRequestReceiveReply(PCIS_MSG Message) const
{
    Message->AlpcHeader.MessageId = 0;
    Message->AlpcHeader.u2.ZeroInit = 0;

    Message->AlpcHeader.u1.s1.TotalLength = sizeof(CIS_MSG);
    Message->AlpcHeader.u1.s1.DataLength = sizeof(CIS_MSG) - sizeof(PORT_MESSAGE);

    Message->AlpcHeader.ClientId.UniqueProcess = nullptr;
    Message->AlpcHeader.ClientId.UniqueThread = nullptr;

    SIZE_T ActualReceiveMessageLength = sizeof(CIS_MSG);

    const auto Status = NtAlpcSendWaitReceivePort(_alpcClientCommunicationPort,
                                                  0,
                                                  &Message->AlpcHeader,
                                                  nullptr,
                                                  &Message->AlpcHeader,
                                                  &ActualReceiveMessageLength,
                                                  nullptr,
                                                  nullptr);

    return Status;
}

VOID ConIoSrvComm::HandleFocusEvent(const CIS_EVENT* const Event)
{
    CIS_EVENT ReplyEvent;

    auto Renderer = ServiceLocator::LocateGlobals().pRender;

    switch (_displayMode)
    {
    case CIS_DISPLAY_MODE_BGFX:
        if (Event->FocusEvent.IsActive)
        {
            // Allow the renderer to paint (this has an effect only on
            // the first call).
            Renderer->EnablePainting();

            // Force a complete redraw.
            Renderer->TriggerRedrawAll();
        }
        break;

    case CIS_DISPLAY_MODE_DIRECTX:
    {
        auto& globals = ServiceLocator::LocateGlobals();

        if (Event->FocusEvent.IsActive)
        {
            auto hr = S_OK;

            // Lazy-initialize the WddmCon engine.
            //
            // This is necessary because the engine cannot be allowed to
            // request ownership of the display before whatever instance
            // of conhost was using it before has relinquished it.
            if (!pWddmConEngine->IsInitialized())
            {
                hr = pWddmConEngine->Initialize();
                LOG_IF_FAILED(hr);

                // Right after we initialize, synchronize the screen/viewport states with the WddmCon surface dimensions
                if (SUCCEEDED(hr))
                {
                    const til::rect rcOld;

                    // WddmEngine reports display size in characters, adjust to pixels for resize window calc.
                    auto rcDisplay = pWddmConEngine->GetDisplaySize();

                    // Get font to adjust char to pixels.
                    til::size coordFont;
                    LOG_IF_FAILED(pWddmConEngine->GetFontSize(&coordFont));

                    rcDisplay.right *= coordFont.width;
                    rcDisplay.bottom *= coordFont.height;

                    // Ask the screen buffer to resize itself (and all related components) based on the screen size.
                    globals.getConsoleInformation().GetActiveOutputBuffer().ProcessResizeWindow(&rcDisplay, &rcOld);
                }
            }

            if (SUCCEEDED(hr))
            {
                // Allow acquiring device resources before drawing.
                hr = pWddmConEngine->Enable();
                LOG_IF_FAILED(hr);
                if (SUCCEEDED(hr))
                {
                    // Allow the renderer to paint.
                    Renderer->EnablePainting();

                    // Force a complete redraw.
                    Renderer->TriggerRedrawAll();
                }
            }
        }
        else
        {
            if (pWddmConEngine->IsInitialized())
            {
                // Wait for the currently running paint operation, if any,
                // and prevent further attempts to render.
                Renderer->WaitForPaintCompletionAndDisable(1000);

                // Relinquish control of the graphics device (only one
                // DirectX application may control the device at any one
                // time).
                LOG_IF_FAILED(pWddmConEngine->Disable());

                // Let the Console IO Server that we have relinquished
                // control of the display.
                ReplyEvent.Type = CIS_EVENT_TYPE_FOCUS_ACK;
                WriteFile(_pipeWriteHandle,
                          &ReplyEvent,
                          sizeof(CIS_EVENT),
                          nullptr,
                          nullptr);
            }
        }
    }
    break;

    case CIS_DISPLAY_MODE_NONE:
    default:
        // Focus events have no meaning in a headless environment.
        break;
    }
}

VOID ConIoSrvComm::CleanupForHeadless(const NTSTATUS status)
{
    if (!_fIsInputInitialized)
    {
        // Free any handles we might have open.
        if (INVALID_HANDLE_VALUE != _pipeReadHandle)
        {
            CloseHandle(_pipeReadHandle);
            _pipeReadHandle = INVALID_HANDLE_VALUE;
        }

        if (INVALID_HANDLE_VALUE != _pipeWriteHandle)
        {
            CloseHandle(_pipeWriteHandle);
            _pipeWriteHandle = INVALID_HANDLE_VALUE;
        }

        if (INVALID_HANDLE_VALUE != _alpcClientCommunicationPort)
        {
            CloseHandle(_alpcClientCommunicationPort);
            _alpcClientCommunicationPort = INVALID_HANDLE_VALUE;
        }

        // Set the status for the IO thread to find.
        ServiceLocator::LocateGlobals().ntstatusConsoleInputInitStatus = status;

        // Signal that input is ready to go.
        ServiceLocator::LocateGlobals().hConsoleInputInitEvent.SetEvent();

        _fIsInputInitialized = true;
    }
}

#pragma endregion

#pragma region Request Methods

[[nodiscard]] NTSTATUS ConIoSrvComm::RequestGetDisplaySize(_Inout_ PCD_IO_DISPLAY_SIZE pCdDisplaySize) const
{
    CIS_MSG Message{};
    Message.Type = CIS_MSG_TYPE_GETDISPLAYSIZE;

    auto Status = SendRequestReceiveReply(&Message);
    if (SUCCEEDED_NTSTATUS(Status))
    {
        *pCdDisplaySize = Message.GetDisplaySizeParams.DisplaySize;
        Status = Message.GetDisplaySizeParams.ReturnValue;
    }

    return Status;
}

[[nodiscard]] NTSTATUS ConIoSrvComm::RequestGetFontSize(_Inout_ PCD_IO_FONT_SIZE pCdFontSize) const
{
    CIS_MSG Message{};
    Message.Type = CIS_MSG_TYPE_GETFONTSIZE;

    auto Status = SendRequestReceiveReply(&Message);
    if (SUCCEEDED_NTSTATUS(Status))
    {
        *pCdFontSize = Message.GetFontSizeParams.FontSize;
        Status = Message.GetFontSizeParams.ReturnValue;
    }

    return Status;
}

[[nodiscard]] NTSTATUS ConIoSrvComm::RequestSetCursor(_In_ const CD_IO_CURSOR_INFORMATION* const pCdCursorInformation) const
{
    CIS_MSG Message{};
    Message.Type = CIS_MSG_TYPE_SETCURSOR;
    Message.SetCursorParams.CursorInformation = *pCdCursorInformation;

    auto Status = SendRequestReceiveReply(&Message);
    if (SUCCEEDED_NTSTATUS(Status))
    {
        Status = Message.SetCursorParams.ReturnValue;
    }

    return Status;
}

[[nodiscard]] NTSTATUS ConIoSrvComm::RequestUpdateDisplay(_In_ til::CoordType RowIndex) const
{
    CIS_MSG Message{};
    Message.Type = CIS_MSG_TYPE_UPDATEDISPLAY;
    Message.UpdateDisplayParams.RowIndex = gsl::narrow<SHORT>(RowIndex);

    auto Status = SendRequestReceiveReply(&Message);
    if (SUCCEEDED_NTSTATUS(Status))
    {
        Status = Message.UpdateDisplayParams.ReturnValue;
    }

    return Status;
}

[[nodiscard]] NTSTATUS ConIoSrvComm::RequestMapVirtualKey(_In_ UINT uCode, _In_ UINT uMapType, _Out_ UINT* puReturnValue)
{
    NTSTATUS Status;

    Status = EnsureConnection();
    if (SUCCEEDED_NTSTATUS(Status))
    {
        CIS_MSG Message = { 0 };
        Message.Type = CIS_MSG_TYPE_MAPVIRTUALKEY;
        Message.MapVirtualKeyParams.Code = uCode;
        Message.MapVirtualKeyParams.MapType = uMapType;

        Status = SendRequestReceiveReply(&Message);
        if (SUCCEEDED_NTSTATUS(Status))
        {
            *puReturnValue = Message.MapVirtualKeyParams.ReturnValue;
        }
    }

    return Status;
}

[[nodiscard]] NTSTATUS ConIoSrvComm::RequestVkKeyScan(_In_ WCHAR wCharacter, _Out_ SHORT* psReturnValue)
{
    NTSTATUS Status;

    Status = EnsureConnection();
    if (SUCCEEDED_NTSTATUS(Status))
    {
        CIS_MSG Message = { 0 };
        Message.Type = CIS_MSG_TYPE_VKKEYSCAN;
        Message.VkKeyScanParams.Character = wCharacter;

        Status = SendRequestReceiveReply(&Message);
        if (SUCCEEDED_NTSTATUS(Status))
        {
            *psReturnValue = Message.VkKeyScanParams.ReturnValue;
        }
    }

    return Status;
}

[[nodiscard]] NTSTATUS ConIoSrvComm::RequestGetKeyState(_In_ int iVirtualKey, _Out_ SHORT* psReturnValue)
{
    NTSTATUS Status;

    Status = EnsureConnection();
    if (SUCCEEDED_NTSTATUS(Status))
    {
        CIS_MSG Message = { 0 };
        Message.Type = CIS_MSG_TYPE_GETKEYSTATE;
        Message.GetKeyStateParams.VirtualKey = iVirtualKey;

        Status = SendRequestReceiveReply(&Message);
        if (SUCCEEDED_NTSTATUS(Status))
        {
            *psReturnValue = Message.GetKeyStateParams.ReturnValue;
        }
    }

    return Status;
}

[[nodiscard]] USHORT ConIoSrvComm::GetDisplayMode() const noexcept
{
    return _displayMode;
}

PVOID ConIoSrvComm::GetSharedViewBase() const noexcept
{
    return _alpcSharedViewBase;
}

#pragma endregion

#pragma region IInputServices Members

UINT ConIoSrvComm::ConIoMapVirtualKeyW(UINT uCode, UINT uMapType)
{
    NTSTATUS Status = STATUS_SUCCESS;

    UINT ReturnValue;
    Status = RequestMapVirtualKey(uCode, uMapType, &ReturnValue);

    if (FAILED_NTSTATUS(Status))
    {
        ReturnValue = 0;
        SetLastError(ERROR_PROC_NOT_FOUND);
    }

    return ReturnValue;
}

SHORT ConIoSrvComm::ConIoVkKeyScanW(WCHAR ch)
{
    NTSTATUS Status = STATUS_SUCCESS;

    SHORT ReturnValue;
    Status = RequestVkKeyScan(ch, &ReturnValue);

    if (FAILED_NTSTATUS(Status))
    {
        ReturnValue = 0;
        SetLastError(ERROR_PROC_NOT_FOUND);
    }

    return ReturnValue;
}

SHORT ConIoSrvComm::ConIoGetKeyState(int nVirtKey)
{
    NTSTATUS Status = STATUS_SUCCESS;

    SHORT ReturnValue;
    Status = RequestGetKeyState(nVirtKey, &ReturnValue);

    if (FAILED_NTSTATUS(Status))
    {
        ReturnValue = 0;
        SetLastError(ERROR_PROC_NOT_FOUND);
    }

    return ReturnValue;
}

#pragma endregion

[[nodiscard]] NTSTATUS ConIoSrvComm::InitializeBgfx()
{
    const auto& globals = ServiceLocator::LocateGlobals();
    FAIL_FAST_IF_NULL(globals.pRender);
    const auto Metrics = ServiceLocator::LocateWindowMetrics();

    // Fetch the display size from the console driver.
    const auto DisplaySize = Metrics->GetMaxClientRectInPixels();
    auto Status = GetLastError();

    if (SUCCEEDED_NTSTATUS(Status))
    {
        // Same with the font size.
        CD_IO_FONT_SIZE FontSize{};
        Status = RequestGetFontSize(&FontSize);

        if (SUCCEEDED_NTSTATUS(Status))
        {
            try
            {
                // MSFT:40226902 - HOTFIX shutdown on OneCore, by leaking the renderer, thereby
                // reducing the change for existing race conditions to turn into deadlocks.
#pragma warning(suppress : 26409) // Avoid calling new and delete explicitly, use std::make_unique<T> instead (r.11).
                _bgfxEngine = new BgfxEngine(
                    GetSharedViewBase(),
                    DisplaySize.bottom / FontSize.Height,
                    DisplaySize.right / FontSize.Width,
                    FontSize.Width,
                    FontSize.Height);

                globals.pRender->AddRenderEngine(_bgfxEngine);
            }
            catch (...)
            {
                Status = NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
            }
        }
    }

    return Status;
}

[[nodiscard]] NTSTATUS ConIoSrvComm::InitializeWddmCon()
{
    const auto& globals = ServiceLocator::LocateGlobals();
    FAIL_FAST_IF_NULL(globals.pRender);

    try
    {
        // MSFT:40226902 - HOTFIX shutdown on OneCore, by leaking the renderer, thereby
        // reducing the change for existing race conditions to turn into deadlocks.
#pragma warning(suppress : 26409) // Avoid calling new and delete explicitly, use std::make_unique<T> instead (r.11).
        pWddmConEngine = new WddmConEngine();
        globals.pRender->AddRenderEngine(pWddmConEngine);
    }
    catch (...)
    {
        return NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
    }

    return STATUS_SUCCESS;
}
