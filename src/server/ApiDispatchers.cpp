// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ApiDispatchers.h"

#include "../host/directio.h"
#include "../host/getset.h"
#include "../host/stream.h"
#include "../host/srvinit.h"
#include "../host/telemetry.hpp"
#include "../host/cmdline.h"

// Assumes that it will find <m> in the calling environment.
#define TraceConsoleAPICallWithOrigin(ApiName, ...)                  \
    TraceLoggingWrite(                                               \
        g_hConhostV2EventTraceProvider,                              \
        "API_" ApiName,                                              \
        TraceLoggingPid(TraceGetProcessId(m), "OriginatingProcess"), \
        TraceLoggingTid(TraceGetThreadId(m), "OriginatingThread"),   \
        __VA_ARGS__ __VA_OPT__(, )                                   \
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),               \
        TraceLoggingKeyword(TIL_KEYWORD_TRACE));

static DWORD TraceGetProcessId(CONSOLE_API_MSG* const m)
{
    const auto p = m->GetProcessHandle();
    return p ? p->dwProcessId : 0;
}

static DWORD TraceGetThreadId(CONSOLE_API_MSG* const m)
{
    const auto p = m->GetProcessHandle();
    return p ? p->dwThreadId : 0;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetConsoleCP(_Inout_ CONSOLE_API_MSG* const m,
                                                         _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL1.GetConsoleCP;

    if (a->Output)
    {
        Telemetry::Instance().LogApiCall(Telemetry::ApiCall::GetConsoleOutputCP);
        m->_pApiRoutines->GetConsoleOutputCodePageImpl(a->CodePage);
    }
    else
    {
        Telemetry::Instance().LogApiCall(Telemetry::ApiCall::GetConsoleCP);
        m->_pApiRoutines->GetConsoleInputCodePageImpl(a->CodePage);
    }
    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetConsoleMode(_Inout_ CONSOLE_API_MSG* const m,
                                                           _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::GetConsoleMode);
    const auto a = &m->u.consoleMsgL1.GetConsoleMode;

    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    TraceConsoleAPICallWithOrigin(
        "GetConsoleMode",
        TraceLoggingBool(pObjectHandle->IsInputHandle(), "InputHandle"));

    if (pObjectHandle->IsInputHandle())
    {
        InputBuffer* pObj;
        RETURN_IF_FAILED(pObjectHandle->GetInputBuffer(GENERIC_READ, &pObj));
        m->_pApiRoutines->GetConsoleInputModeImpl(*pObj, a->Mode);
    }
    else
    {
        SCREEN_INFORMATION* pObj;
        RETURN_IF_FAILED(pObjectHandle->GetScreenBuffer(GENERIC_READ, &pObj));
        m->_pApiRoutines->GetConsoleOutputModeImpl(*pObj, a->Mode);
    }
    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerSetConsoleMode(_Inout_ CONSOLE_API_MSG* const m,
                                                           _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::SetConsoleMode);
    const auto a = &m->u.consoleMsgL1.SetConsoleMode;

    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    TraceConsoleAPICallWithOrigin(
        "SetConsoleMode",
        TraceLoggingBool(pObjectHandle->IsInputHandle(), "InputHandle"),
        TraceLoggingHexULong(a->Mode, "Mode"));

    if (pObjectHandle->IsInputHandle())
    {
        InputBuffer* pObj;
        RETURN_IF_FAILED(pObjectHandle->GetInputBuffer(GENERIC_WRITE, &pObj));
        return m->_pApiRoutines->SetConsoleInputModeImpl(*pObj, a->Mode);
    }
    else
    {
        SCREEN_INFORMATION* pObj;
        RETURN_IF_FAILED(pObjectHandle->GetScreenBuffer(GENERIC_WRITE, &pObj));
        return m->_pApiRoutines->SetConsoleOutputModeImpl(*pObj, a->Mode);
    }
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetNumberOfInputEvents(_Inout_ CONSOLE_API_MSG* const m,
                                                                   _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::GetNumberOfConsoleInputEvents);
    const auto a = &m->u.consoleMsgL1.GetNumberOfConsoleInputEvents;

    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    InputBuffer* pObj;
    RETURN_IF_FAILED(pObjectHandle->GetInputBuffer(GENERIC_READ, &pObj));

    return m->_pApiRoutines->GetNumberOfConsoleInputEventsImpl(*pObj, a->ReadyEvents);
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetConsoleInput(_Inout_ CONSOLE_API_MSG* const m,
                                                            _Inout_ BOOL* const pbReplyPending)
{
    *pbReplyPending = FALSE;

    const auto a = &m->u.consoleMsgL1.GetConsoleInput;
    if (WI_IsFlagSet(a->Flags, CONSOLE_READ_NOREMOVE))
    {
        Telemetry::Instance().LogApiCall(Telemetry::ApiCall::PeekConsoleInput, a->Unicode);
    }
    else
    {
        Telemetry::Instance().LogApiCall(Telemetry::ApiCall::ReadConsoleInput, a->Unicode);
    }

    a->NumRecords = 0;

    // If any flags are set that are not within our enum, it's invalid.
    if (WI_IsAnyFlagSet(a->Flags, ~CONSOLE_READ_VALID))
    {
        return E_INVALIDARG;
    }

    // Make sure we have a valid input buffer.
    const auto pHandleData = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pHandleData);
    InputBuffer* pInputBuffer;
    RETURN_IF_FAILED(pHandleData->GetInputBuffer(GENERIC_READ, &pInputBuffer));

    // Get output buffer.
    PVOID pvBuffer;
    ULONG cbBufferSize;
    RETURN_IF_FAILED(m->GetOutputBuffer(&pvBuffer, &cbBufferSize));

    const auto rgRecords = reinterpret_cast<INPUT_RECORD*>(pvBuffer);
    const auto cRecords = cbBufferSize / sizeof(INPUT_RECORD);

    const auto fIsPeek = WI_IsFlagSet(a->Flags, CONSOLE_READ_NOREMOVE);
    const auto fIsWaitAllowed = WI_IsFlagClear(a->Flags, CONSOLE_READ_NOWAIT);

    const auto pInputReadHandleData = pHandleData->GetClientInput();

    std::unique_ptr<IWaitRoutine> waiter;
    InputEventQueue outEvents;
    auto hr = m->_pApiRoutines->GetConsoleInputImpl(
        *pInputBuffer,
        outEvents,
        cRecords,
        *pInputReadHandleData,
        a->Unicode,
        fIsPeek,
        waiter);

    // We must return the number of records in the message payload (to alert the client)
    // as well as in the message headers (below in SetReplyInformation) to alert the driver.
    LOG_IF_FAILED(SizeTToULong(outEvents.size(), &a->NumRecords));

    size_t cbWritten;
    LOG_IF_FAILED(SizeTMult(outEvents.size(), sizeof(INPUT_RECORD), &cbWritten));

    if (nullptr != waiter.get())
    {
        // In some circumstances, the read may have told us to wait because it didn't have data,
        // but the client explicitly asked us to return immediate. In that case, we'll convert the
        // wait request into a "0 bytes found, OK".

        if (fIsWaitAllowed)
        {
            hr = ConsoleWaitQueue::s_CreateWait(m, waiter.release());
            if (SUCCEEDED(hr))
            {
                *pbReplyPending = TRUE;
                hr = CONSOLE_STATUS_WAIT;
            }
        }
        else
        {
            // If wait isn't allowed and the routine generated a
            // waiter, say there was nothing to be
            // retrieved right now.
            // The waiter will be auto-freed in the smart pointer.

            cbWritten = 0;
            hr = S_OK;
        }
    }
    else
    {
        std::ranges::copy(outEvents, rgRecords);
    }

    if (SUCCEEDED(hr))
    {
        m->SetReplyInformation(cbWritten);
    }

    return hr;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerReadConsole(_Inout_ CONSOLE_API_MSG* const m,
                                                        _Inout_ BOOL* const pbReplyPending)
{
    *pbReplyPending = FALSE;

    const auto a = &m->u.consoleMsgL1.ReadConsole;
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::ReadConsole, a->Unicode);

    a->NumBytes = 0; // we return 0 until proven otherwise.

    // Make sure we have a valid input buffer.
    const auto HandleData = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, HandleData);
    InputBuffer* pInputBuffer;
    RETURN_IF_FAILED(HandleData->GetInputBuffer(GENERIC_READ, &pInputBuffer));

    // Get output parameter buffer.
    PVOID pvBuffer;
    ULONG cbBufferSize;
    RETURN_IF_FAILED(m->GetOutputBuffer(&pvBuffer, &cbBufferSize));

    // This might need to go on the other side of the fence (inside host) because the server doesn't know what we're going to do with initial num bytes.
    // (This restriction exists because it's going to copy initial into the final buffer, but we don't know that.)
    RETURN_HR_IF(E_INVALIDARG, a->InitialNumBytes > cbBufferSize);

    // Retrieve input parameters.
    // 1. Exe name making the request
    const auto cchExeName = a->ExeNameLength;
    ULONG cbExeName;
    RETURN_IF_FAILED(ULongMult(cchExeName, sizeof(wchar_t), &cbExeName));
    wistd::unique_ptr<wchar_t[]> pwsExeName;

    if (cchExeName > 0)
    {
        pwsExeName = wil::make_unique_nothrow<wchar_t[]>(cchExeName);
        RETURN_IF_NULL_ALLOC(pwsExeName);
        RETURN_IF_FAILED(m->ReadMessageInput(0, pwsExeName.get(), cbExeName));
    }
    const std::wstring_view exeView(pwsExeName.get(), cchExeName);

    // 2. Existing data in the buffer that was passed in.
    std::unique_ptr<char[]> pbInitialData;
    std::wstring_view initialData;

    try
    {
        const auto cbInitialData = a->InitialNumBytes;
        if (cbInitialData > 0)
        {
            // InitialNumBytes is only supported for ReadConsoleW (via CONSOLE_READCONSOLE_CONTROL::nInitialChars).
            RETURN_HR_IF(E_INVALIDARG, !a->Unicode);

            pbInitialData = std::make_unique<char[]>(cbInitialData);

            // This parameter starts immediately after the exe name so skip by that many bytes.
            RETURN_IF_FAILED(m->ReadMessageInput(cbExeName, pbInitialData.get(), cbInitialData));

            initialData = { reinterpret_cast<const wchar_t*>(pbInitialData.get()), cbInitialData / sizeof(wchar_t) };
        }
    }
    CATCH_RETURN();

    // ReadConsole needs this to get details associated with an attached process (such as the command history list, telemetry metadata).
    const auto hConsoleClient = (HANDLE)m->GetProcessHandle();

    // ReadConsole needs this to store context information across "processed reads" e.g. reads on the same handle
    // across multiple calls when we are simulating a command prompt input line for the client application.
    const auto pInputReadHandleData = HandleData->GetClientInput();

    std::unique_ptr<IWaitRoutine> waiter;
    size_t cbWritten;

    const std::span<char> outputBuffer(reinterpret_cast<char*>(pvBuffer), cbBufferSize);
    auto hr = m->_pApiRoutines->ReadConsoleImpl(*pInputBuffer,
                                                outputBuffer,
                                                cbWritten, // We must set the reply length in bytes.
                                                waiter,
                                                initialData,
                                                exeView,
                                                *pInputReadHandleData,
                                                a->Unicode,
                                                hConsoleClient,
                                                a->CtrlWakeupMask,
                                                a->ControlKeyState);

    LOG_IF_FAILED(SizeTToULong(cbWritten, &a->NumBytes));

    if (nullptr != waiter.get())
    {
        // If we received a waiter, we need to queue the wait and not reply.
        hr = ConsoleWaitQueue::s_CreateWait(m, waiter.release());

        if (SUCCEEDED(hr))
        {
            *pbReplyPending = TRUE;
        }
    }
    else
    {
        // - This routine is called when a ReadConsole or ReadFile request is about to be completed.
        // - It sets the number of bytes written as the information to be written with the completion status and,
        //   if CTRL+Z processing is enabled and a CTRL+Z is detected, switches the number of bytes read to zero.
        if (a->ProcessControlZ != FALSE &&
            a->NumBytes > 0 &&
            m->State.OutputBuffer != nullptr &&
            *(PUCHAR)m->State.OutputBuffer == 0x1a)
        {
            a->NumBytes = 0;
        }

        m->SetReplyInformation(a->NumBytes);
    }

    return hr;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerWriteConsole(_Inout_ CONSOLE_API_MSG* const m,
                                                         _Inout_ BOOL* const pbReplyPending)
{
    *pbReplyPending = FALSE;

    const auto a = &m->u.consoleMsgL1.WriteConsole;
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::WriteConsole, a->Unicode);

    // Make sure we have a valid screen buffer.
    auto HandleData = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, HandleData);
    SCREEN_INFORMATION* pScreenInfo;
    RETURN_IF_FAILED(HandleData->GetScreenBuffer(GENERIC_WRITE, &pScreenInfo));

    // Get input parameter buffer
    PVOID pvBuffer;
    ULONG cbBufferSize;
    RETURN_IF_FAILED(m->GetInputBuffer(&pvBuffer, &cbBufferSize));

    std::unique_ptr<IWaitRoutine> waiter;
    size_t cbRead;

    const auto requiresVtQuirk{ m->GetProcessHandle()->GetShimPolicy().IsVtColorQuirkRequired() };

    // We have to hold onto the HR from the call and return it.
    // We can't return some other error after the actual API call.
    // This is because the write console function is allowed to write part of the string and then return an error.
    // It then must report back how far it got before it failed.
    HRESULT hr;
    if (a->Unicode)
    {
        const std::wstring_view buffer(reinterpret_cast<wchar_t*>(pvBuffer), cbBufferSize / sizeof(wchar_t));
        size_t cchInputRead;

        TraceConsoleAPICallWithOrigin(
            "WriteConsoleW",
            TraceLoggingUInt32(a->NumBytes, "NumBytes"),
            TraceLoggingCountedWideString(buffer.data(), static_cast<ULONG>(buffer.size()), "Buffer"));

        hr = m->_pApiRoutines->WriteConsoleWImpl(*pScreenInfo, buffer, cchInputRead, requiresVtQuirk, waiter);

        // We must set the reply length in bytes. Convert back from characters.
        LOG_IF_FAILED(SizeTMult(cchInputRead, sizeof(wchar_t), &cbRead));
    }
    else
    {
        const std::string_view buffer(reinterpret_cast<char*>(pvBuffer), cbBufferSize);
        size_t cchInputRead;

        TraceConsoleAPICallWithOrigin(
            "WriteConsoleA",
            TraceLoggingUInt32(a->NumBytes, "NumBytes"),
            TraceLoggingCountedString(buffer.data(), static_cast<ULONG>(buffer.size()), "Buffer"));

        hr = m->_pApiRoutines->WriteConsoleAImpl(*pScreenInfo, buffer, cchInputRead, requiresVtQuirk, waiter);

        // Reply length is already in bytes (chars), don't need to convert.
        cbRead = cchInputRead;
    }

    // We must return the byte length of the read data in the message.
    LOG_IF_FAILED(SizeTToULong(cbRead, &a->NumBytes));

    if (nullptr != waiter.get())
    {
        // If we received a waiter, we need to queue the wait and not reply.
        hr = ConsoleWaitQueue::s_CreateWait(m, waiter.release());
        if (SUCCEEDED(hr))
        {
            *pbReplyPending = TRUE;
        }
    }
    else
    {
        // If no waiter, fill the response data and return.
        m->SetReplyInformation(a->NumBytes);
    }

    return hr;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerFillConsoleOutput(_Inout_ CONSOLE_API_MSG* const m,
                                                              _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL2.FillConsoleOutput;

    switch (a->ElementType)
    {
    case CONSOLE_ATTRIBUTE:
        Telemetry::Instance().LogApiCall(Telemetry::ApiCall::FillConsoleOutputAttribute);
        break;
    case CONSOLE_ASCII:
        Telemetry::Instance().LogApiCall(Telemetry::ApiCall::FillConsoleOutputCharacter, false);
        break;
    case CONSOLE_REAL_UNICODE:
    case CONSOLE_FALSE_UNICODE:
        Telemetry::Instance().LogApiCall(Telemetry::ApiCall::FillConsoleOutputCharacter, true);
        break;
    }

    // Capture length of initial fill.
    size_t fill = a->Length;

    // Set written length to 0 in case we early return.
    a->Length = 0;

    // Make sure we have a valid screen buffer.
    auto HandleData = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, HandleData);
    SCREEN_INFORMATION* pScreenInfo;
    RETURN_IF_FAILED(HandleData->GetScreenBuffer(GENERIC_WRITE, &pScreenInfo));

    HRESULT hr;
    size_t amountWritten;
    switch (a->ElementType)
    {
    case CONSOLE_ATTRIBUTE:
    {
        hr = m->_pApiRoutines->FillConsoleOutputAttributeImpl(*pScreenInfo,
                                                              a->Element,
                                                              fill,
                                                              til::wrap_coord(a->WriteCoord),
                                                              amountWritten);
        break;
    }
    case CONSOLE_REAL_UNICODE:
    case CONSOLE_FALSE_UNICODE:
    {
        // GH#3126 if the client application is powershell.exe, then we might
        // need to enable a compatibility shim.
        hr = m->_pApiRoutines->FillConsoleOutputCharacterWImpl(*pScreenInfo,
                                                               a->Element,
                                                               fill,
                                                               til::wrap_coord(a->WriteCoord),
                                                               amountWritten,
                                                               m->GetProcessHandle()->GetShimPolicy().IsPowershellExe());
        break;
    }
    case CONSOLE_ASCII:
    {
        hr = m->_pApiRoutines->FillConsoleOutputCharacterAImpl(*pScreenInfo,
                                                               static_cast<char>(a->Element),
                                                               fill,
                                                               til::wrap_coord(a->WriteCoord),
                                                               amountWritten);
        break;
    }
    default:
        return E_INVALIDARG;
    }

    LOG_IF_FAILED(SizeTToDWord(amountWritten, &a->Length));

    return hr;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerSetConsoleActiveScreenBuffer(_Inout_ CONSOLE_API_MSG* const m,
                                                                         _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::SetConsoleActiveScreenBuffer);
    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    SCREEN_INFORMATION* pObj;
    RETURN_IF_FAILED(pObjectHandle->GetScreenBuffer(GENERIC_WRITE, &pObj));

    m->_pApiRoutines->SetConsoleActiveScreenBufferImpl(*pObj);
    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerFlushConsoleInputBuffer(_Inout_ CONSOLE_API_MSG* const m,
                                                                    _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::FlushConsoleInputBuffer);
    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    InputBuffer* pObj;
    RETURN_IF_FAILED(pObjectHandle->GetInputBuffer(GENERIC_WRITE, &pObj));

    m->_pApiRoutines->FlushConsoleInputBuffer(*pObj);
    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerSetConsoleCP(_Inout_ CONSOLE_API_MSG* const m,
                                                         _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL2.SetConsoleCP;

    if (a->Output)
    {
        Telemetry::Instance().LogApiCall(Telemetry::ApiCall::SetConsoleOutputCP);
        return m->_pApiRoutines->SetConsoleOutputCodePageImpl(a->CodePage);
    }
    else
    {
        Telemetry::Instance().LogApiCall(Telemetry::ApiCall::SetConsoleCP);
        return m->_pApiRoutines->SetConsoleInputCodePageImpl(a->CodePage);
    }
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetConsoleCursorInfo(_Inout_ CONSOLE_API_MSG* const m,
                                                                 _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::GetConsoleCursorInfo);
    const auto a = &m->u.consoleMsgL2.GetConsoleCursorInfo;

    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    SCREEN_INFORMATION* pObj;
    RETURN_IF_FAILED(pObjectHandle->GetScreenBuffer(GENERIC_WRITE, &pObj));

    auto visible = false;
    m->_pApiRoutines->GetConsoleCursorInfoImpl(*pObj, a->CursorSize, visible);
    a->Visible = !!visible;
    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerSetConsoleCursorInfo(_Inout_ CONSOLE_API_MSG* const m,
                                                                 _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::SetConsoleCursorInfo);
    const auto a = &m->u.consoleMsgL2.SetConsoleCursorInfo;

    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    SCREEN_INFORMATION* pObj;
    RETURN_IF_FAILED(pObjectHandle->GetScreenBuffer(GENERIC_WRITE, &pObj));

    return m->_pApiRoutines->SetConsoleCursorInfoImpl(*pObj, a->CursorSize, a->Visible);
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetConsoleScreenBufferInfo(_Inout_ CONSOLE_API_MSG* const m,
                                                                       _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::GetConsoleScreenBufferInfoEx);
    const auto a = &m->u.consoleMsgL2.GetConsoleScreenBufferInfo;

    CONSOLE_SCREEN_BUFFER_INFOEX ex = { 0 };
    ex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);

    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    SCREEN_INFORMATION* pObj;
    RETURN_IF_FAILED(pObjectHandle->GetScreenBuffer(GENERIC_READ, &pObj));

    m->_pApiRoutines->GetConsoleScreenBufferInfoExImpl(*pObj, ex);

    a->FullscreenSupported = !!ex.bFullscreenSupported;
    const auto ColorTableSizeInBytes = RTL_NUMBER_OF_V2(ex.ColorTable) * sizeof(*ex.ColorTable);
    CopyMemory(a->ColorTable, ex.ColorTable, ColorTableSizeInBytes);
    a->CursorPosition = ex.dwCursorPosition;
    a->MaximumWindowSize = ex.dwMaximumWindowSize;
    a->Size = ex.dwSize;
    a->ScrollPosition.X = ex.srWindow.Left;
    a->ScrollPosition.Y = ex.srWindow.Top;
    a->CurrentWindowSize.X = ex.srWindow.Right - ex.srWindow.Left;
    a->CurrentWindowSize.Y = ex.srWindow.Bottom - ex.srWindow.Top;
    a->Attributes = ex.wAttributes;
    a->PopupAttributes = ex.wPopupAttributes;

    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerSetConsoleScreenBufferInfo(_Inout_ CONSOLE_API_MSG* const m,
                                                                       _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::SetConsoleScreenBufferInfoEx);
    const auto a = &m->u.consoleMsgL2.SetConsoleScreenBufferInfo;

    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    SCREEN_INFORMATION* pObj;
    RETURN_IF_FAILED(pObjectHandle->GetScreenBuffer(GENERIC_WRITE, &pObj));

    CONSOLE_SCREEN_BUFFER_INFOEX ex = { 0 };
    ex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    ex.bFullscreenSupported = a->FullscreenSupported;
    const auto ColorTableSizeInBytes = RTL_NUMBER_OF_V2(ex.ColorTable) * sizeof(*ex.ColorTable);
    CopyMemory(ex.ColorTable, a->ColorTable, ColorTableSizeInBytes);
    ex.dwCursorPosition = a->CursorPosition;
    ex.dwMaximumWindowSize = a->MaximumWindowSize;
    ex.dwSize = a->Size;
    ex.srWindow = { 0 };
    ex.srWindow.Left = a->ScrollPosition.X;
    ex.srWindow.Top = a->ScrollPosition.Y;
    ex.srWindow.Right = ex.srWindow.Left + a->CurrentWindowSize.X;
    ex.srWindow.Bottom = ex.srWindow.Top + a->CurrentWindowSize.Y;
    ex.wAttributes = a->Attributes;
    ex.wPopupAttributes = a->PopupAttributes;

    TraceConsoleAPICallWithOrigin(
        "SetConsoleScreenBufferInfoEx",
        TraceLoggingConsoleCoord(a->Size, "BufferSize"),
        TraceLoggingConsoleCoord(a->CurrentWindowSize, "WindowSize"),
        TraceLoggingConsoleCoord(a->MaximumWindowSize, "MaxWindowSize"));

    return m->_pApiRoutines->SetConsoleScreenBufferInfoExImpl(*pObj, ex);
}

[[nodiscard]] HRESULT ApiDispatchers::ServerSetConsoleScreenBufferSize(_Inout_ CONSOLE_API_MSG* const m,
                                                                       _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::SetConsoleScreenBufferSize);
    const auto a = &m->u.consoleMsgL2.SetConsoleScreenBufferSize;

    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    SCREEN_INFORMATION* pObj;
    RETURN_IF_FAILED(pObjectHandle->GetScreenBuffer(GENERIC_WRITE, &pObj));

    TraceConsoleAPICallWithOrigin(
        "SetConsoleScreenBufferSize",
        TraceLoggingConsoleCoord(a->Size, "BufferSize"));

    return m->_pApiRoutines->SetConsoleScreenBufferSizeImpl(*pObj, til::wrap_coord_size(a->Size));
}

[[nodiscard]] HRESULT ApiDispatchers::ServerSetConsoleCursorPosition(_Inout_ CONSOLE_API_MSG* const m,
                                                                     _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::SetConsoleCursorPosition);
    const auto a = &m->u.consoleMsgL2.SetConsoleCursorPosition;

    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    SCREEN_INFORMATION* pObj;
    RETURN_IF_FAILED(pObjectHandle->GetScreenBuffer(GENERIC_WRITE, &pObj));

    return m->_pApiRoutines->SetConsoleCursorPositionImpl(*pObj, til::wrap_coord(a->CursorPosition));
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetLargestConsoleWindowSize(_Inout_ CONSOLE_API_MSG* const m,
                                                                        _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::GetLargestConsoleWindowSize);
    const auto a = &m->u.consoleMsgL2.GetLargestConsoleWindowSize;

    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    SCREEN_INFORMATION* pObj;
    RETURN_IF_FAILED(pObjectHandle->GetScreenBuffer(GENERIC_WRITE, &pObj));

    auto size = til::wrap_coord_size(a->Size);
    m->_pApiRoutines->GetLargestConsoleWindowSizeImpl(*pObj, size);
    return til::unwrap_coord_size_hr(size, a->Size);
}

[[nodiscard]] HRESULT ApiDispatchers::ServerScrollConsoleScreenBuffer(_Inout_ CONSOLE_API_MSG* const m,
                                                                      _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL2.ScrollConsoleScreenBuffer;
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::ScrollConsoleScreenBuffer, a->Unicode);

    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    SCREEN_INFORMATION* pObj;
    RETURN_IF_FAILED(pObjectHandle->GetScreenBuffer(GENERIC_WRITE, &pObj));

    if (a->Unicode)
    {
        // GH#3126 if the client application is cmd.exe, then we might need to
        // enable a compatibility shim.
        return m->_pApiRoutines->ScrollConsoleScreenBufferWImpl(*pObj,
                                                                til::wrap_small_rect(a->ScrollRectangle),
                                                                til::wrap_coord(a->DestinationOrigin),
                                                                a->Clip ? std::optional{ til::wrap_small_rect(a->ClipRectangle) } : std::nullopt,
                                                                a->Fill.Char.UnicodeChar,
                                                                a->Fill.Attributes,
                                                                m->GetProcessHandle()->GetShimPolicy().IsCmdExe());
    }
    else
    {
        return m->_pApiRoutines->ScrollConsoleScreenBufferAImpl(*pObj,
                                                                til::wrap_small_rect(a->ScrollRectangle),
                                                                til::wrap_coord(a->DestinationOrigin),
                                                                a->Clip ? std::optional{ til::wrap_small_rect(a->ClipRectangle) } : std::nullopt,
                                                                a->Fill.Char.AsciiChar,
                                                                a->Fill.Attributes);
    }
}

[[nodiscard]] HRESULT ApiDispatchers::ServerSetConsoleTextAttribute(_Inout_ CONSOLE_API_MSG* const m,
                                                                    _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::SetConsoleTextAttribute);
    const auto a = &m->u.consoleMsgL2.SetConsoleTextAttribute;

    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    SCREEN_INFORMATION* pObj;
    RETURN_IF_FAILED(pObjectHandle->GetScreenBuffer(GENERIC_WRITE, &pObj));

    TraceConsoleAPICallWithOrigin(
        "SetConsoleTextAttribute",
        TraceLoggingHexUInt16(a->Attributes, "Attributes"));

    RETURN_HR(m->_pApiRoutines->SetConsoleTextAttributeImpl(*pObj, a->Attributes));
}

[[nodiscard]] HRESULT ApiDispatchers::ServerSetConsoleWindowInfo(_Inout_ CONSOLE_API_MSG* const m,
                                                                 _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::SetConsoleWindowInfo);
    const auto a = &m->u.consoleMsgL2.SetConsoleWindowInfo;

    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    SCREEN_INFORMATION* pObj;
    RETURN_IF_FAILED(pObjectHandle->GetScreenBuffer(GENERIC_WRITE, &pObj));

    TraceConsoleAPICallWithOrigin(
        "SetConsoleWindowInfo",
        TraceLoggingBool(a->Absolute, "IsWindowRectAbsolute"),
        TraceLoggingInt32(a->Window.Left, "WindowRectLeft"),
        TraceLoggingInt32(a->Window.Right, "WindowRectRight"),
        TraceLoggingInt32(a->Window.Top, "WindowRectTop"),
        TraceLoggingInt32(a->Window.Bottom, "WindowRectBottom"));

    return m->_pApiRoutines->SetConsoleWindowInfoImpl(*pObj, a->Absolute, til::wrap_small_rect(a->Window));
}

[[nodiscard]] HRESULT ApiDispatchers::ServerReadConsoleOutputString(_Inout_ CONSOLE_API_MSG* const m,
                                                                    _Inout_ BOOL* const /*pbReplyPending*/)
{
    RETURN_HR_IF(E_ACCESSDENIED, !m->GetProcessHandle()->GetPolicy().CanReadOutputBuffer());

    const auto a = &m->u.consoleMsgL2.ReadConsoleOutputString;

    switch (a->StringType)
    {
    case CONSOLE_ATTRIBUTE:
        Telemetry::Instance().LogApiCall(Telemetry::ApiCall::ReadConsoleOutputAttribute);
        break;
    case CONSOLE_ASCII:
        Telemetry::Instance().LogApiCall(Telemetry::ApiCall::ReadConsoleOutputCharacter, false);
        break;
    case CONSOLE_REAL_UNICODE:
    case CONSOLE_FALSE_UNICODE:
        Telemetry::Instance().LogApiCall(Telemetry::ApiCall::ReadConsoleOutputCharacter, true);
        break;
    }

    a->NumRecords = 0; // Set to 0 records returned in case we have failures.

    PVOID pvBuffer;
    ULONG cbBuffer;
    RETURN_IF_FAILED(m->GetOutputBuffer(&pvBuffer, &cbBuffer));

    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    SCREEN_INFORMATION* pScreenInfo;
    RETURN_IF_FAILED(pObjectHandle->GetScreenBuffer(GENERIC_READ, &pScreenInfo));

    size_t written;
    switch (a->StringType)
    {
    case CONSOLE_ATTRIBUTE:
    {
        const std::span<WORD> buffer(reinterpret_cast<WORD*>(pvBuffer), cbBuffer / sizeof(WORD));
        RETURN_IF_FAILED(m->_pApiRoutines->ReadConsoleOutputAttributeImpl(*pScreenInfo, til::wrap_coord(a->ReadCoord), buffer, written));
        break;
    }
    case CONSOLE_REAL_UNICODE:
    case CONSOLE_FALSE_UNICODE:
    {
        const std::span<wchar_t> buffer(reinterpret_cast<wchar_t*>(pvBuffer), cbBuffer / sizeof(wchar_t));
        RETURN_IF_FAILED(m->_pApiRoutines->ReadConsoleOutputCharacterWImpl(*pScreenInfo, til::wrap_coord(a->ReadCoord), buffer, written));
        break;
    }
    case CONSOLE_ASCII:
    {
        const std::span<char> buffer(reinterpret_cast<char*>(pvBuffer), cbBuffer);
        RETURN_IF_FAILED(m->_pApiRoutines->ReadConsoleOutputCharacterAImpl(*pScreenInfo, til::wrap_coord(a->ReadCoord), buffer, written));
        break;
    }
    default:
        return E_INVALIDARG;
    }

    // Report count of records now in the buffer (varies based on type)
    RETURN_IF_FAILED(SizeTToULong(written, &a->NumRecords));

    m->SetReplyInformation(cbBuffer); // Set the reply buffer size to what we were originally told the buffer size was (on the way in)

    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerWriteConsoleInput(_Inout_ CONSOLE_API_MSG* const m,
                                                              _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL2.WriteConsoleInput;

    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::WriteConsoleInput, a->Unicode);

    a->NumRecords = 0;

    RETURN_HR_IF(E_ACCESSDENIED, !m->GetProcessHandle()->GetPolicy().CanWriteInputBuffer());

    PVOID pvBuffer;
    ULONG cbSize;
    RETURN_IF_FAILED(m->GetInputBuffer(&pvBuffer, &cbSize));

    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    InputBuffer* pInputBuffer;
    RETURN_IF_FAILED(pObjectHandle->GetInputBuffer(GENERIC_WRITE, &pInputBuffer));

    size_t written;
    std::span<const INPUT_RECORD> buffer(reinterpret_cast<INPUT_RECORD*>(pvBuffer), cbSize / sizeof(INPUT_RECORD));
    if (!a->Unicode)
    {
        RETURN_IF_FAILED(m->_pApiRoutines->WriteConsoleInputAImpl(*pInputBuffer, buffer, written, !!a->Append));
    }
    else
    {
        RETURN_IF_FAILED(m->_pApiRoutines->WriteConsoleInputWImpl(*pInputBuffer, buffer, written, !!a->Append));
    }

    RETURN_IF_FAILED(SizeTToULong(written, &a->NumRecords));

    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerWriteConsoleOutput(_Inout_ CONSOLE_API_MSG* const m,
                                                               _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL2.WriteConsoleOutput;

    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::WriteConsoleOutput, a->Unicode);

    // Backup originalRegion and set the written area to a 0 size rectangle in case of failures.
    const auto originalRegion = Microsoft::Console::Types::Viewport::FromInclusive(til::wrap_small_rect(a->CharRegion));
    auto writtenRegion = Microsoft::Console::Types::Viewport::FromDimensions(originalRegion.Origin(), { 0, 0 });
    RETURN_IF_FAILED(til::unwrap_small_rect_hr(writtenRegion.ToInclusive(), a->CharRegion));

    // Get input parameter buffer
    PVOID pvBuffer;
    ULONG cbSize;
    RETURN_IF_FAILED(m->GetInputBuffer(&pvBuffer, &cbSize));

    // Make sure we have a valid screen buffer.
    auto HandleData = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, HandleData);
    SCREEN_INFORMATION* pScreenInfo;
    RETURN_IF_FAILED(HandleData->GetScreenBuffer(GENERIC_WRITE, &pScreenInfo));

    // Validate parameters
    size_t regionArea;
    RETURN_IF_FAILED(SizeTMult(originalRegion.Dimensions().width, originalRegion.Dimensions().height, &regionArea));
    size_t regionBytes;
    RETURN_IF_FAILED(SizeTMult(regionArea, sizeof(CHAR_INFO), &regionBytes));
    RETURN_HR_IF(E_INVALIDARG, cbSize < regionBytes); // If given fewer bytes on input than we need to do this write, it's invalid.

    const std::span<CHAR_INFO> buffer(reinterpret_cast<CHAR_INFO*>(pvBuffer), cbSize / sizeof(CHAR_INFO));
    if (!a->Unicode)
    {
        RETURN_IF_FAILED(m->_pApiRoutines->WriteConsoleOutputAImpl(*pScreenInfo, buffer, originalRegion, writtenRegion));
    }
    else
    {
        RETURN_IF_FAILED(m->_pApiRoutines->WriteConsoleOutputWImpl(*pScreenInfo, buffer, originalRegion, writtenRegion));
    }

    // Update the written region if we were successful
    return til::unwrap_small_rect_hr(writtenRegion.ToInclusive(), a->CharRegion);
}

[[nodiscard]] HRESULT ApiDispatchers::ServerWriteConsoleOutputString(_Inout_ CONSOLE_API_MSG* const m,
                                                                     _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL2.WriteConsoleOutputString;

    switch (a->StringType)
    {
    case CONSOLE_ATTRIBUTE:
        Telemetry::Instance().LogApiCall(Telemetry::ApiCall::WriteConsoleOutputAttribute);
        break;
    case CONSOLE_ASCII:
        Telemetry::Instance().LogApiCall(Telemetry::ApiCall::WriteConsoleOutputCharacter, false);
        break;
    case CONSOLE_REAL_UNICODE:
    case CONSOLE_FALSE_UNICODE:
        Telemetry::Instance().LogApiCall(Telemetry::ApiCall::WriteConsoleOutputCharacter, true);
        break;
    }

    // Set written records to 0 in case we early return.
    a->NumRecords = 0;

    // Make sure we have a valid screen buffer.
    auto HandleData = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, HandleData);
    SCREEN_INFORMATION* pScreenInfo;
    RETURN_IF_FAILED(HandleData->GetScreenBuffer(GENERIC_WRITE, &pScreenInfo));

    // Get input parameter buffer
    PVOID pvBuffer;
    ULONG cbBufferSize;
    RETURN_IF_FAILED(m->GetInputBuffer(&pvBuffer, &cbBufferSize));

    HRESULT hr;
    size_t used;
    switch (a->StringType)
    {
    case CONSOLE_ASCII:
    {
        const std::string_view text(reinterpret_cast<char*>(pvBuffer), cbBufferSize);

        TraceConsoleAPICallWithOrigin(
            "WriteConsoleOutputCharacterA",
            TraceLoggingConsoleCoord(a->WriteCoord, "WriteCoord"),
            TraceLoggingUInt32(a->NumRecords, "NumRecords"));

        hr = m->_pApiRoutines->WriteConsoleOutputCharacterAImpl(*pScreenInfo,
                                                                text,
                                                                til::wrap_coord(a->WriteCoord),
                                                                used);

        break;
    }
    case CONSOLE_REAL_UNICODE:
    case CONSOLE_FALSE_UNICODE:
    {
        const std::wstring_view text(reinterpret_cast<wchar_t*>(pvBuffer), cbBufferSize / sizeof(wchar_t));

        TraceConsoleAPICallWithOrigin(
            "WriteConsoleOutputCharacterW",
            TraceLoggingConsoleCoord(a->WriteCoord, "WriteCoord"),
            TraceLoggingUInt32(a->NumRecords, "NumRecords"));

        hr = m->_pApiRoutines->WriteConsoleOutputCharacterWImpl(*pScreenInfo,
                                                                text,
                                                                til::wrap_coord(a->WriteCoord),
                                                                used);

        break;
    }
    case CONSOLE_ATTRIBUTE:
    {
        const std::span<const WORD> text(reinterpret_cast<WORD*>(pvBuffer), cbBufferSize / sizeof(WORD));

        TraceConsoleAPICallWithOrigin(
            "WriteConsoleOutputAttribute",
            TraceLoggingConsoleCoord(a->WriteCoord, "WriteCoord"),
            TraceLoggingUInt32(a->NumRecords, "NumRecords"));

        hr = m->_pApiRoutines->WriteConsoleOutputAttributeImpl(*pScreenInfo,
                                                               text,
                                                               til::wrap_coord(a->WriteCoord),
                                                               used);

        break;
    }
    default:
        return E_INVALIDARG;
    }

    // We need to return how many records were consumed off of the string
    LOG_IF_FAILED(SizeTToULong(used, &a->NumRecords));

    return hr;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerReadConsoleOutput(_Inout_ CONSOLE_API_MSG* const m,
                                                              _Inout_ BOOL* const /*pbReplyPending*/)
{
    RETURN_HR_IF(E_ACCESSDENIED, !m->GetProcessHandle()->GetPolicy().CanReadOutputBuffer());

    const auto a = &m->u.consoleMsgL2.ReadConsoleOutput;

    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::ReadConsoleOutput, a->Unicode);

    // Backup data region passed and set it to a zero size region in case we exit early for failures.
    const auto originalRegion = Microsoft::Console::Types::Viewport::FromInclusive(til::wrap_small_rect(a->CharRegion));
    const auto zeroRegion = Microsoft::Console::Types::Viewport::FromDimensions(originalRegion.Origin(), { 0, 0 });
    RETURN_IF_FAILED(til::unwrap_small_rect_hr(zeroRegion.ToInclusive(), a->CharRegion));

    PVOID pvBuffer;
    ULONG cbBuffer;
    RETURN_IF_FAILED(m->GetOutputBuffer(&pvBuffer, &cbBuffer));

    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    SCREEN_INFORMATION* pScreenInfo;
    RETURN_IF_FAILED(pObjectHandle->GetScreenBuffer(GENERIC_READ, &pScreenInfo));

    // Validate parameters
    size_t regionArea;
    RETURN_IF_FAILED(SizeTMult(originalRegion.Dimensions().width, originalRegion.Dimensions().height, &regionArea));
    size_t regionBytes;
    RETURN_IF_FAILED(SizeTMult(regionArea, sizeof(CHAR_INFO), &regionBytes));
    RETURN_HR_IF(E_INVALIDARG, regionArea > 0 && ((regionArea > ULONG_MAX / sizeof(CHAR_INFO)) || (cbBuffer < regionBytes)));

    std::span<CHAR_INFO> buffer(reinterpret_cast<CHAR_INFO*>(pvBuffer), cbBuffer / sizeof(CHAR_INFO));
    auto finalRegion = Microsoft::Console::Types::Viewport::Empty(); // the actual region read out of the buffer
    if (!a->Unicode)
    {
        RETURN_IF_FAILED(m->_pApiRoutines->ReadConsoleOutputAImpl(*pScreenInfo,
                                                                  buffer,
                                                                  originalRegion,
                                                                  finalRegion));
    }
    else
    {
        RETURN_IF_FAILED(m->_pApiRoutines->ReadConsoleOutputWImpl(*pScreenInfo,
                                                                  buffer,
                                                                  originalRegion,
                                                                  finalRegion));
    }

    RETURN_IF_FAILED(til::unwrap_small_rect_hr(finalRegion.ToInclusive(), a->CharRegion));

    // We have to reply back with the entire buffer length. The client side in kernelbase will trim out
    // the correct region of the buffer for return to the original caller.
    m->SetReplyInformation(cbBuffer);

    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetConsoleTitle(_Inout_ CONSOLE_API_MSG* const m,
                                                            _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL2.GetConsoleTitle;
    Telemetry::Instance().LogApiCall(a->Original ? Telemetry::ApiCall::GetConsoleOriginalTitle : Telemetry::ApiCall::GetConsoleTitle, a->Unicode);

    PVOID pvBuffer;
    ULONG cbBuffer;
    RETURN_IF_FAILED(m->GetOutputBuffer(&pvBuffer, &cbBuffer));

    auto hr = S_OK;
    if (a->Unicode)
    {
        std::span<wchar_t> buffer(reinterpret_cast<wchar_t*>(pvBuffer), cbBuffer / sizeof(wchar_t));
        size_t written;
        size_t needed;
        if (a->Original)
        {
            // This API traditionally doesn't return an HRESULT. Log and discard.
            LOG_IF_FAILED(m->_pApiRoutines->GetConsoleOriginalTitleWImpl(buffer, written, needed));
        }
        else
        {
            // This API traditionally doesn't return an HRESULT. Log and discard.
            LOG_IF_FAILED(m->_pApiRoutines->GetConsoleTitleWImpl(buffer, written, needed));
        }

        // We must return the needed length of the title string in the TitleLength.
        LOG_IF_FAILED(SizeTToULong(needed, &a->TitleLength));

        // We must return the actually written length of the title string in the reply.
        m->SetReplyInformation(written * sizeof(wchar_t));
    }
    else
    {
        std::span<char> buffer(reinterpret_cast<char*>(pvBuffer), cbBuffer);
        size_t written;
        size_t needed;
        if (a->Original)
        {
            hr = m->_pApiRoutines->GetConsoleOriginalTitleAImpl(buffer, written, needed);
        }
        else
        {
            hr = m->_pApiRoutines->GetConsoleTitleAImpl(buffer, written, needed);
        }

        // We must return the needed length of the title string in the TitleLength.
        LOG_IF_FAILED(SizeTToULong(needed, &a->TitleLength));

        // We must return the actually written length of the title string in the reply.
        m->SetReplyInformation(written * sizeof(char));
    }

    return hr;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerSetConsoleTitle(_Inout_ CONSOLE_API_MSG* const m,
                                                            _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL2.SetConsoleTitle;
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::SetConsoleTitle, a->Unicode);

    PVOID pvBuffer;
    ULONG cbOriginalLength;

    RETURN_IF_FAILED(m->GetInputBuffer(&pvBuffer, &cbOriginalLength));

    if (a->Unicode)
    {
        const std::wstring_view title(reinterpret_cast<wchar_t*>(pvBuffer), cbOriginalLength / sizeof(wchar_t));
        return m->_pApiRoutines->SetConsoleTitleWImpl(title);
    }
    else
    {
        const std::string_view title(reinterpret_cast<char*>(pvBuffer), cbOriginalLength);
        return m->_pApiRoutines->SetConsoleTitleAImpl(title);
    }
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetConsoleMouseInfo(_Inout_ CONSOLE_API_MSG* const m,
                                                                _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::GetNumberOfConsoleMouseButtons);
    const auto a = &m->u.consoleMsgL3.GetConsoleMouseInfo;

    m->_pApiRoutines->GetNumberOfConsoleMouseButtonsImpl(a->NumButtons);
    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetConsoleFontSize(_Inout_ CONSOLE_API_MSG* const m,
                                                               _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::GetConsoleFontSize);
    const auto a = &m->u.consoleMsgL3.GetConsoleFontSize;

    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    SCREEN_INFORMATION* pObj;
    RETURN_IF_FAILED(pObjectHandle->GetScreenBuffer(GENERIC_READ, &pObj));

    auto size = til::wrap_coord_size(a->FontSize);
    RETURN_IF_FAILED(m->_pApiRoutines->GetConsoleFontSizeImpl(*pObj, a->FontIndex, size));
    return til::unwrap_coord_size_hr(size, a->FontSize);
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetConsoleCurrentFont(_Inout_ CONSOLE_API_MSG* const m,
                                                                  _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::GetCurrentConsoleFontEx);
    const auto a = &m->u.consoleMsgL3.GetCurrentConsoleFont;

    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    SCREEN_INFORMATION* pObj;
    RETURN_IF_FAILED(pObjectHandle->GetScreenBuffer(GENERIC_READ, &pObj));

    CONSOLE_FONT_INFOEX FontInfo = { 0 };
    FontInfo.cbSize = sizeof(FontInfo);

    RETURN_IF_FAILED(m->_pApiRoutines->GetCurrentConsoleFontExImpl(*pObj, a->MaximumWindow, FontInfo));

    CopyMemory(a->FaceName, FontInfo.FaceName, RTL_NUMBER_OF_V2(a->FaceName) * sizeof(a->FaceName[0]));
    a->FontFamily = FontInfo.FontFamily;
    a->FontIndex = FontInfo.nFont;
    a->FontSize = FontInfo.dwFontSize;
    a->FontWeight = FontInfo.FontWeight;

    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerSetConsoleDisplayMode(_Inout_ CONSOLE_API_MSG* const m,
                                                                  _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::SetConsoleDisplayMode);
    const auto a = &m->u.consoleMsgL3.SetConsoleDisplayMode;

    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    SCREEN_INFORMATION* pObj;
    RETURN_IF_FAILED(pObjectHandle->GetScreenBuffer(GENERIC_WRITE, &pObj));

    auto size = til::wrap_coord_size(a->ScreenBufferDimensions);
    RETURN_IF_FAILED(m->_pApiRoutines->SetConsoleDisplayModeImpl(*pObj, a->dwFlags, size));
    return til::unwrap_coord_size_hr(size, a->ScreenBufferDimensions);
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetConsoleDisplayMode(_Inout_ CONSOLE_API_MSG* const m,
                                                                  _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::GetConsoleDisplayMode);
    const auto a = &m->u.consoleMsgL3.GetConsoleDisplayMode;

    // Historically this has never checked the handles. It just returns global state.

    m->_pApiRoutines->GetConsoleDisplayModeImpl(a->ModeFlags);
    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerAddConsoleAlias(_Inout_ CONSOLE_API_MSG* const m,
                                                            _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL3.AddConsoleAliasW;
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::AddConsoleAlias, a->Unicode);

    // Read the input buffer and validate the strings.
    PVOID pvBuffer;
    ULONG cbBufferSize;
    RETURN_IF_FAILED(m->GetInputBuffer(&pvBuffer, &cbBufferSize));

    // There are 3 strings stored back-to-back within the message payload.
    // First we verify that their size and alignment are alright and then we extract them.
    const ULONG cbInputExeName = a->ExeLength;
    const ULONG cbInputSource = a->SourceLength;
    const ULONG cbInputTarget = a->TargetLength;

    const auto alignment = a->Unicode ? alignof(wchar_t) : alignof(char);
    // ExeLength, SourceLength and TargetLength are USHORT and summing them up will not overflow a ULONG.
    const auto badLength = cbInputTarget + cbInputExeName + cbInputSource > cbBufferSize;
    // Since (any) alignment is a power of 2, we can use bit tricks to test if the alignment is right:
    // a) Combining the values with OR works, because we're only interested whether the lowest bits are 0 (= aligned).
    // b) x % y can be replaced with x & (y - 1) if y is a power of 2.
    const auto badAlignment = ((cbInputExeName | cbInputSource | cbInputTarget) & (alignment - 1)) != 0;
    RETURN_HR_IF(E_INVALIDARG, badLength || badAlignment);

    const auto pvInputExeName = static_cast<char*>(pvBuffer);
    const auto pvInputSource = pvInputExeName + cbInputExeName;
    const auto pvInputTarget = pvInputSource + cbInputSource;

    if (a->Unicode)
    {
        const std::wstring_view inputExeName(reinterpret_cast<wchar_t*>(pvInputExeName), cbInputExeName / sizeof(wchar_t));
        const std::wstring_view inputSource(reinterpret_cast<wchar_t*>(pvInputSource), cbInputSource / sizeof(wchar_t));
        const std::wstring_view inputTarget(reinterpret_cast<wchar_t*>(pvInputTarget), cbInputTarget / sizeof(wchar_t));

        return m->_pApiRoutines->AddConsoleAliasWImpl(inputSource, inputTarget, inputExeName);
    }
    else
    {
        const std::string_view inputExeName(pvInputExeName, cbInputExeName);
        const std::string_view inputSource(pvInputSource, cbInputSource);
        const std::string_view inputTarget(pvInputTarget, cbInputTarget);

        return m->_pApiRoutines->AddConsoleAliasAImpl(inputSource, inputTarget, inputExeName);
    }
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetConsoleAlias(_Inout_ CONSOLE_API_MSG* const m,
                                                            _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL3.GetConsoleAliasW;
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::GetConsoleAlias, a->Unicode);

    PVOID pvInputBuffer;
    ULONG cbInputBufferSize;
    RETURN_IF_FAILED(m->GetInputBuffer(&pvInputBuffer, &cbInputBufferSize));

    // There are 2 strings stored back-to-back within the message payload.
    // First we verify that their size and alignment are alright and then we extract them.
    const ULONG cbInputExeName = a->ExeLength;
    const ULONG cbInputSource = a->SourceLength;

    const auto alignment = a->Unicode ? alignof(wchar_t) : alignof(char);
    // ExeLength and SourceLength are USHORT and summing them up will not overflow a ULONG.
    const auto badLength = cbInputExeName + cbInputSource > cbInputBufferSize;
    // Since (any) alignment is a power of 2, we can use bit tricks to test if the alignment is right:
    // a) Combining the values with OR works, because we're only interested whether the lowest bits are 0 (= aligned).
    // b) x % y can be replaced with x & (y - 1) if y is a power of 2.
    const auto badAlignment = ((cbInputExeName | cbInputSource) & (alignment - 1)) != 0;
    RETURN_HR_IF(E_INVALIDARG, badLength || badAlignment);

    const auto pvInputExeName = static_cast<char*>(pvInputBuffer);
    const auto pvInputSource = pvInputExeName + cbInputExeName;

    PVOID pvOutputBuffer;
    ULONG cbOutputBufferSize;
    RETURN_IF_FAILED(m->GetOutputBuffer(&pvOutputBuffer, &cbOutputBufferSize));

    HRESULT hr;
    size_t cbWritten;
    if (a->Unicode)
    {
        const std::wstring_view inputExeName{ reinterpret_cast<wchar_t*>(pvInputExeName), cbInputExeName / sizeof(wchar_t) };
        const std::wstring_view inputSource{ reinterpret_cast<wchar_t*>(pvInputSource), cbInputSource / sizeof(wchar_t) };
        const std::span outputBuffer{ static_cast<wchar_t*>(pvOutputBuffer), cbOutputBufferSize / sizeof(wchar_t) };
        size_t cchWritten;

        hr = m->_pApiRoutines->GetConsoleAliasWImpl(inputSource, outputBuffer, cchWritten, inputExeName);

        // We must set the reply length in bytes. Convert back from characters.
        RETURN_IF_FAILED(SizeTMult(cchWritten, sizeof(wchar_t), &cbWritten));
    }
    else
    {
        const std::string_view inputExeName{ pvInputExeName, cbInputExeName };
        const std::string_view inputSource{ pvInputSource, cbInputSource };
        const std::span outputBuffer{ static_cast<char*>(pvOutputBuffer), cbOutputBufferSize };
        size_t cchWritten;

        hr = m->_pApiRoutines->GetConsoleAliasAImpl(inputSource, outputBuffer, cchWritten, inputExeName);

        cbWritten = cchWritten;
    }

    // We must return the byte length of the written data in the message
    RETURN_IF_FAILED(SizeTToUShort(cbWritten, &a->TargetLength));

    m->SetReplyInformation(a->TargetLength);

    // See conlibk.lib. For any "buffer too small condition", we must send the exact status code
    // NTSTATUS = STATUS_BUFFER_TOO_SMALL. If we send Win32 or HRESULT equivalents, the client library
    // will zero out our DWORD return value set in a->TargetLength on our behalf.
    if (ERROR_INSUFFICIENT_BUFFER == hr ||
        HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) == hr)
    {
        hr = STATUS_BUFFER_TOO_SMALL;
    }

    return hr;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetConsoleAliasesLength(_Inout_ CONSOLE_API_MSG* const m,
                                                                    _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL3.GetConsoleAliasesLengthW;
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::GetConsoleAliasesLength, a->Unicode);

    ULONG cbExeNameLength;
    PVOID pvExeName;
    RETURN_IF_FAILED(m->GetInputBuffer(&pvExeName, &cbExeNameLength));

    size_t cbAliasesLength;
    if (a->Unicode)
    {
        const std::wstring_view inputExeName(reinterpret_cast<wchar_t*>(pvExeName), cbExeNameLength / sizeof(wchar_t));
        size_t cchAliasesLength;
        RETURN_IF_FAILED(m->_pApiRoutines->GetConsoleAliasesLengthWImpl(inputExeName, cchAliasesLength));

        RETURN_IF_FAILED(SizeTMult(cchAliasesLength, sizeof(wchar_t), &cbAliasesLength));
    }
    else
    {
        const std::string_view inputExeName(reinterpret_cast<char*>(pvExeName), cbExeNameLength);
        size_t cchAliasesLength;
        RETURN_IF_FAILED(m->_pApiRoutines->GetConsoleAliasesLengthAImpl(inputExeName, cchAliasesLength));

        cbAliasesLength = cchAliasesLength;
    }

    RETURN_IF_FAILED(SizeTToULong(cbAliasesLength, &a->AliasesLength));

    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetConsoleAliasExesLength(_Inout_ CONSOLE_API_MSG* const m,
                                                                      _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL3.GetConsoleAliasExesLengthW;
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::GetConsoleAliasExesLength, a->Unicode);

    size_t cbAliasExesLength;
    if (a->Unicode)
    {
        size_t cchAliasExesLength;
        RETURN_IF_FAILED(m->_pApiRoutines->GetConsoleAliasExesLengthWImpl(cchAliasExesLength));
        cbAliasExesLength = cchAliasExesLength * sizeof(wchar_t);
    }
    else
    {
        size_t cchAliasExesLength;
        RETURN_IF_FAILED(m->_pApiRoutines->GetConsoleAliasExesLengthAImpl(cchAliasExesLength));
        cbAliasExesLength = cchAliasExesLength;
    }

    RETURN_IF_FAILED(SizeTToULong(cbAliasExesLength, &a->AliasExesLength));

    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetConsoleAliases(_Inout_ CONSOLE_API_MSG* const m,
                                                              _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL3.GetConsoleAliasesW;
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::GetConsoleAliases, a->Unicode);

    PVOID pvExeName;
    ULONG cbExeNameLength;
    RETURN_IF_FAILED(m->GetInputBuffer(&pvExeName, &cbExeNameLength));

    PVOID pvOutputBuffer;
    DWORD cbAliasesBufferLength;
    RETURN_IF_FAILED(m->GetOutputBuffer(&pvOutputBuffer, &cbAliasesBufferLength));

    size_t cbWritten;
    if (a->Unicode)
    {
        const std::wstring_view inputExeName(reinterpret_cast<wchar_t*>(pvExeName), cbExeNameLength / sizeof(wchar_t));
        std::span<wchar_t> outputBuffer(reinterpret_cast<wchar_t*>(pvOutputBuffer), cbAliasesBufferLength / sizeof(wchar_t));
        size_t cchWritten;

        RETURN_IF_FAILED(m->_pApiRoutines->GetConsoleAliasesWImpl(inputExeName, outputBuffer, cchWritten));

        // We must set the reply length in bytes. Convert back from characters.
        RETURN_IF_FAILED(SizeTMult(cchWritten, sizeof(wchar_t), &cbWritten));
    }
    else
    {
        const std::string_view inputExeName(reinterpret_cast<char*>(pvExeName), cbExeNameLength);
        std::span<char> outputBuffer(reinterpret_cast<char*>(pvOutputBuffer), cbAliasesBufferLength);
        size_t cchWritten;

        RETURN_IF_FAILED(m->_pApiRoutines->GetConsoleAliasesAImpl(inputExeName, outputBuffer, cchWritten));

        cbWritten = cchWritten;
    }

    RETURN_IF_FAILED(SizeTToULong(cbWritten, &a->AliasesBufferLength));

    m->SetReplyInformation(a->AliasesBufferLength);

    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetConsoleAliasExes(_Inout_ CONSOLE_API_MSG* const m,
                                                                _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL3.GetConsoleAliasExesW;
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::GetConsoleAliasExes, a->Unicode);

    PVOID pvBuffer;
    ULONG cbAliasExesBufferLength;
    RETURN_IF_FAILED(m->GetOutputBuffer(&pvBuffer, &cbAliasExesBufferLength));

    size_t cbWritten;
    if (a->Unicode)
    {
        std::span<wchar_t> outputBuffer(reinterpret_cast<wchar_t*>(pvBuffer), cbAliasExesBufferLength / sizeof(wchar_t));
        size_t cchWritten;
        RETURN_IF_FAILED(m->_pApiRoutines->GetConsoleAliasExesWImpl(outputBuffer, cchWritten));

        RETURN_IF_FAILED(SizeTMult(cchWritten, sizeof(wchar_t), &cbWritten));
    }
    else
    {
        std::span<char> outputBuffer(reinterpret_cast<char*>(pvBuffer), cbAliasExesBufferLength);
        size_t cchWritten;
        RETURN_IF_FAILED(m->_pApiRoutines->GetConsoleAliasExesAImpl(outputBuffer, cchWritten));

        cbWritten = cchWritten;
    }

    // We must return the byte length of the written data in the message
    RETURN_IF_FAILED(SizeTToULong(cbWritten, &a->AliasExesBufferLength));

    m->SetReplyInformation(a->AliasExesBufferLength);

    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerExpungeConsoleCommandHistory(_Inout_ CONSOLE_API_MSG* const m,
                                                                         _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL3.ExpungeConsoleCommandHistoryW;

    PVOID pvExeName;
    ULONG cbExeNameLength;
    RETURN_IF_FAILED(m->GetInputBuffer(&pvExeName, &cbExeNameLength));

    if (a->Unicode)
    {
        const std::wstring_view inputExeName(reinterpret_cast<wchar_t*>(pvExeName), cbExeNameLength / sizeof(wchar_t));

        return m->_pApiRoutines->ExpungeConsoleCommandHistoryWImpl(inputExeName);
    }
    else
    {
        const std::string_view inputExeName(reinterpret_cast<char*>(pvExeName), cbExeNameLength);

        return m->_pApiRoutines->ExpungeConsoleCommandHistoryAImpl(inputExeName);
    }
}

[[nodiscard]] HRESULT ApiDispatchers::ServerSetConsoleNumberOfCommands(_Inout_ CONSOLE_API_MSG* const m,
                                                                       _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL3.SetConsoleNumberOfCommandsW;
    PVOID pvExeName;
    ULONG cbExeNameLength;
    RETURN_IF_FAILED(m->GetInputBuffer(&pvExeName, &cbExeNameLength));

    const size_t NumberOfCommands = a->NumCommands;
    if (a->Unicode)
    {
        const std::wstring_view inputExeName(reinterpret_cast<wchar_t*>(pvExeName), cbExeNameLength / sizeof(wchar_t));

        return m->_pApiRoutines->SetConsoleNumberOfCommandsWImpl(inputExeName, NumberOfCommands);
    }
    else
    {
        const std::string_view inputExeName(reinterpret_cast<char*>(pvExeName), cbExeNameLength);

        return m->_pApiRoutines->SetConsoleNumberOfCommandsAImpl(inputExeName, NumberOfCommands);
    }
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetConsoleCommandHistoryLength(_Inout_ CONSOLE_API_MSG* const m,
                                                                           _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL3.GetConsoleCommandHistoryLengthW;

    PVOID pvExeName;
    ULONG cbExeNameLength;
    RETURN_IF_FAILED(m->GetInputBuffer(&pvExeName, &cbExeNameLength));

    size_t cbCommandHistoryLength;
    if (a->Unicode)
    {
        size_t cchCommandHistoryLength;
        const std::wstring_view inputExeName(reinterpret_cast<wchar_t*>(pvExeName), cbExeNameLength / sizeof(wchar_t));

        RETURN_IF_FAILED(m->_pApiRoutines->GetConsoleCommandHistoryLengthWImpl(inputExeName, cchCommandHistoryLength));

        // We must set the reply length in bytes. Convert back from characters.
        RETURN_IF_FAILED(SizeTMult(cchCommandHistoryLength, sizeof(wchar_t), &cbCommandHistoryLength));
    }
    else
    {
        size_t cchCommandHistoryLength;
        const std::string_view inputExeName(reinterpret_cast<char*>(pvExeName), cbExeNameLength);

        RETURN_IF_FAILED(m->_pApiRoutines->GetConsoleCommandHistoryLengthAImpl(inputExeName, cchCommandHistoryLength));

        cbCommandHistoryLength = cchCommandHistoryLength;
    }

    // Fit return value into structure memory size
    RETURN_IF_FAILED(SizeTToULong(cbCommandHistoryLength, &a->CommandHistoryLength));

    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetConsoleCommandHistory(_Inout_ CONSOLE_API_MSG* const m,
                                                                     _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL3.GetConsoleCommandHistoryW;

    PVOID pvExeName;
    ULONG cbExeNameLength;
    RETURN_IF_FAILED(m->GetInputBuffer(&pvExeName, &cbExeNameLength));

    PVOID pvOutputBuffer;
    ULONG cbOutputBuffer;
    RETURN_IF_FAILED(m->GetOutputBuffer(&pvOutputBuffer, &cbOutputBuffer));

    size_t cbWritten;
    if (a->Unicode)
    {
        const std::wstring_view inputExeName(reinterpret_cast<wchar_t*>(pvExeName), cbExeNameLength / sizeof(wchar_t));
        std::span<wchar_t> outputBuffer(reinterpret_cast<wchar_t*>(pvOutputBuffer), cbOutputBuffer / sizeof(wchar_t));
        size_t cchWritten;
        RETURN_IF_FAILED(m->_pApiRoutines->GetConsoleCommandHistoryWImpl(inputExeName, outputBuffer, cchWritten));

        // We must set the reply length in bytes. Convert back from characters.
        RETURN_IF_FAILED(SizeTMult(cchWritten, sizeof(wchar_t), &cbWritten));
    }
    else
    {
        const std::string_view inputExeName(reinterpret_cast<char*>(pvExeName), cbExeNameLength);
        std::span<char> outputBuffer(reinterpret_cast<char*>(pvOutputBuffer), cbOutputBuffer);
        size_t cchWritten;
        RETURN_IF_FAILED(m->_pApiRoutines->GetConsoleCommandHistoryAImpl(inputExeName, outputBuffer, cchWritten));

        cbWritten = cchWritten;
    }

    // Fit return value into structure memory size.
    RETURN_IF_FAILED(SizeTToULong(cbWritten, &a->CommandBufferLength));

    m->SetReplyInformation(a->CommandBufferLength);

    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetConsoleWindow(_Inout_ CONSOLE_API_MSG* const m,
                                                             _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::GetConsoleWindow);
    const auto a = &m->u.consoleMsgL3.GetConsoleWindow;

    m->_pApiRoutines->GetConsoleWindowImpl(a->hwnd);
    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetConsoleSelectionInfo(_Inout_ CONSOLE_API_MSG* const m,
                                                                    _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::GetConsoleSelectionInfo);
    const auto a = &m->u.consoleMsgL3.GetConsoleSelectionInfo;

    m->_pApiRoutines->GetConsoleSelectionInfoImpl(a->SelectionInfo);
    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerGetConsoleHistory(_Inout_ CONSOLE_API_MSG* const m,
                                                              _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL3.GetConsoleHistory;
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::GetConsoleHistoryInfo);

    CONSOLE_HISTORY_INFO info;
    info.cbSize = sizeof(info);

    m->_pApiRoutines->GetConsoleHistoryInfoImpl(info);

    a->dwFlags = info.dwFlags;
    a->HistoryBufferSize = info.HistoryBufferSize;
    a->NumberOfHistoryBuffers = info.NumberOfHistoryBuffers;

    return S_OK;
}

[[nodiscard]] HRESULT ApiDispatchers::ServerSetConsoleHistory(_Inout_ CONSOLE_API_MSG* const m,
                                                              _Inout_ BOOL* const /*pbReplyPending*/)
{
    const auto a = &m->u.consoleMsgL3.SetConsoleHistory;
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::SetConsoleHistoryInfo);

    CONSOLE_HISTORY_INFO info;
    info.cbSize = sizeof(info);
    info.dwFlags = a->dwFlags;
    info.HistoryBufferSize = a->HistoryBufferSize;
    info.NumberOfHistoryBuffers = a->NumberOfHistoryBuffers;

    return m->_pApiRoutines->SetConsoleHistoryInfoImpl(info);
}

[[nodiscard]] HRESULT ApiDispatchers::ServerSetConsoleCurrentFont(_Inout_ CONSOLE_API_MSG* const m,
                                                                  _Inout_ BOOL* const /*pbReplyPending*/)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::SetCurrentConsoleFontEx);
    const auto a = &m->u.consoleMsgL3.SetCurrentConsoleFont;

    const auto pObjectHandle = m->GetObjectHandle();
    RETURN_HR_IF_NULL(E_HANDLE, pObjectHandle);

    SCREEN_INFORMATION* pObj;
    RETURN_IF_FAILED(pObjectHandle->GetScreenBuffer(GENERIC_WRITE, &pObj));

    CONSOLE_FONT_INFOEX Info;
    Info.cbSize = sizeof(Info);
    Info.dwFontSize = a->FontSize;
    CopyMemory(Info.FaceName, a->FaceName, RTL_NUMBER_OF_V2(Info.FaceName) * sizeof(Info.FaceName[0]));
    Info.FontFamily = a->FontFamily;
    Info.FontWeight = a->FontWeight;

    return m->_pApiRoutines->SetCurrentConsoleFontExImpl(*pObj, a->MaximumWindow, Info);
}
