// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <intsafe.h>

#include "ApiMessage.h"
#include "DeviceComm.h"

_CONSOLE_API_MSG::_CONSOLE_API_MSG() :
    Complete{ 0 },
    State{ 0 },
    _pDeviceComm(nullptr),
    _pApiRoutines(nullptr),
    _inputBuffer{},
    _outputBuffer{},
    Descriptor{ 0 },
    CreateObject{ 0 },
    CreateScreenBuffer{ 0 },
    msgHeader{ 0 }
{
}

ConsoleProcessHandle* _CONSOLE_API_MSG::GetProcessHandle() const
{
    return reinterpret_cast<ConsoleProcessHandle*>(_pDeviceComm->GetHandle(Descriptor.Process));
}

ConsoleHandleData* _CONSOLE_API_MSG::GetObjectHandle() const
{
    return reinterpret_cast<ConsoleHandleData*>(_pDeviceComm->GetHandle(Descriptor.Object));
}

// Routine Description:
// - This routine reads some or all of the input payload of the given message (depending on the given offset).
// Arguments:
// - cbOffset - Supplies the offset in bytes from which to start reading the payload.
// - pvBuffer - Receives the payload.
// - cbSize - Supplies the number of bytes to be read into the buffer.
// Return Value:
// - HRESULT indicating if the payload was successfully read.
[[nodiscard]] HRESULT _CONSOLE_API_MSG::ReadMessageInput(const ULONG cbOffset,
                                                         _Out_writes_bytes_(cbSize) PVOID pvBuffer,
                                                         const ULONG cbSize)
{
    CD_IO_OPERATION IoOperation;
    IoOperation.Identifier = Descriptor.Identifier;
    IoOperation.Buffer.Offset = State.ReadOffset + cbOffset;
    IoOperation.Buffer.Data = pvBuffer;
    IoOperation.Buffer.Size = cbSize;

    return _pDeviceComm->ReadInput(&IoOperation);
}

// Routine Description:
// - This routine retrieves the input buffer associated with this message. It will allocate one if needed.
// - Before completing the message, ReleaseMessageBuffers must be called to free any allocation performed by this routine.
// Arguments:
// - Message - Supplies the message whose input buffer will be retrieved.
// - Buffer - Receives a pointer to the input buffer.
// - Size - Receives the size, in bytes, of the input buffer.
// Return Value:
// -  HRESULT indicating if the input buffer was successfully retrieved.
[[nodiscard]] HRESULT _CONSOLE_API_MSG::GetInputBuffer(_Outptr_result_bytebuffer_(*pcbSize) void** const ppvBuffer,
                                                       _Out_ ULONG* const pcbSize)
try
{
    // Initialize the buffer if it hasn't been initialized yet.
    if (State.InputBuffer == nullptr)
    {
        RETURN_HR_IF(E_FAIL, State.ReadOffset > Descriptor.InputSize);

        ULONG const cbReadSize = Descriptor.InputSize - State.ReadOffset;

        _inputBuffer.resize(cbReadSize);

        RETURN_IF_FAILED(ReadMessageInput(0, _inputBuffer.data(), cbReadSize));

        State.InputBuffer = _inputBuffer.data();
        State.InputBufferSize = cbReadSize;
    }

    // Return the buffer.
    *ppvBuffer = State.InputBuffer;
    *pcbSize = State.InputBufferSize;

    return S_OK;
}
CATCH_RETURN();

// Routine Description:
// - This routine retrieves the output buffer associated with this message. It will allocate one if needed.
//   The allocated will be bigger than the actual output size by the requested factor.
// - Before completing the message, ReleaseMessageBuffers must be called to free any allocation performed by this routine.
// Arguments:
// - Factor - Supplies the factor to multiply the allocated buffer by.
// - Buffer - Receives a pointer to the output buffer.
// - Size - Receives the size, in bytes, of the output buffer.
//  Return Value:
// - HRESULT indicating if the output buffer was successfully retrieved.
[[nodiscard]] HRESULT _CONSOLE_API_MSG::GetAugmentedOutputBuffer(const ULONG cbFactor,
                                                                 _Outptr_result_bytebuffer_(*pcbSize) PVOID* const ppvBuffer,
                                                                 _Out_ PULONG pcbSize)
try
{
    // Initialize the buffer if it hasn't been initialized yet.
    if (State.OutputBuffer == nullptr)
    {
        RETURN_HR_IF(E_FAIL, State.WriteOffset > Descriptor.OutputSize);

        ULONG cbWriteSize = Descriptor.OutputSize - State.WriteOffset;
        RETURN_IF_FAILED(ULongMult(cbWriteSize, cbFactor, &cbWriteSize));

        _outputBuffer.resize(cbWriteSize);

        // 0 it out.
        std::fill(_outputBuffer.begin(), _outputBuffer.end(), (BYTE)0);

        State.OutputBuffer = _outputBuffer.data();
        State.OutputBufferSize = cbWriteSize;
    }

    // Return the buffer.
    *ppvBuffer = State.OutputBuffer;
    *pcbSize = State.OutputBufferSize;

    return S_OK;
}
CATCH_RETURN();

// Routine Description:
// - This routine retrieves the output buffer associated with this message. It will allocate one if needed.
// - Before completing the message, ReleaseMessageBuffers must be called to free any allocation performed by this routine.
// Arguments:
// - Message - Supplies the message whose output buffer will be retrieved.
// - Buffer - Receives a pointer to the output buffer.
// - Size - Receives the size, in bytes, of the output buffer.
// Return Value:
// - HRESULT indicating if the output buffer was successfully retrieved.
[[nodiscard]] HRESULT _CONSOLE_API_MSG::GetOutputBuffer(_Outptr_result_bytebuffer_(*pcbSize) void** const ppvBuffer,
                                                        _Out_ ULONG* const pcbSize)
{
    return GetAugmentedOutputBuffer(1, ppvBuffer, pcbSize);
}

// Routine Description:
// - This routine releases output or input buffers that might have been allocated
//   during the processing of the given message. If the current completion status
//   of the message indicates success, this routine also writes the output buffer
//   (if any) to the message.
// Arguments:
// - <none>
// Return Value:
// - HRESULT indicating if the payload was successfully written if applicable.
[[nodiscard]] HRESULT _CONSOLE_API_MSG::ReleaseMessageBuffers()
{
    HRESULT hr = S_OK;

    if (State.InputBuffer != nullptr)
    {
        _inputBuffer.clear();
        State.InputBuffer = nullptr;
        State.InputBufferSize = 0;
    }

    if (State.OutputBuffer != nullptr)
    {
        if (NT_SUCCESS(Complete.IoStatus.Status))
        {
            CD_IO_OPERATION IoOperation;
            IoOperation.Identifier = Descriptor.Identifier;
            IoOperation.Buffer.Offset = State.WriteOffset;
            IoOperation.Buffer.Data = State.OutputBuffer;
            IoOperation.Buffer.Size = (ULONG)Complete.IoStatus.Information;

            LOG_IF_FAILED(_pDeviceComm->WriteOutput(&IoOperation));
        }

        _outputBuffer.clear();
        State.OutputBuffer = nullptr;
        State.OutputBufferSize = 0;
    }

    return hr;
}

void _CONSOLE_API_MSG::SetReplyStatus(const NTSTATUS Status)
{
    Complete.IoStatus.Status = Status;
}

void _CONSOLE_API_MSG::SetReplyInformation(const ULONG_PTR pInformation)
{
    Complete.IoStatus.Information = pInformation;
}

void _CONSOLE_API_MSG::UpdateUserBufferPointers()
{
    // There are some instances where an API message may get copied.
    // Because it is infeasible to write a copy constructor for this class
    // without rewriting large swaths of conhost (because of the unnamed union)
    // we have chosen to introduce a "post-copy" step.
    // This makes sure the buffers in State are in sync with the actual
    // buffers in the object.
    if (State.InputBuffer)
    {
        State.InputBuffer = _inputBuffer.data();
    }

    if (State.OutputBuffer)
    {
        State.OutputBuffer = _outputBuffer.data();
    }
}
