// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <intsafe.h>

#include "ApiMessage.h"
#include "DeviceComm.h"

constexpr size_t structPacketDataSize = sizeof(_CONSOLE_API_MSG) - offsetof(_CONSOLE_API_MSG, Descriptor);

_CONSOLE_API_MSG::_CONSOLE_API_MSG()
{
    // A union cannot have more than one initializer,
    // but it isn't exactly clear which union case is the largest.
    // --> Just memset() the entire thing.
    memset(&Descriptor, 0, structPacketDataSize);
}

_CONSOLE_API_MSG::_CONSOLE_API_MSG(const _CONSOLE_API_MSG& other)
{
    *this = other;
}

_CONSOLE_API_MSG& _CONSOLE_API_MSG::operator=(const _CONSOLE_API_MSG& other)
{
    Complete = other.Complete;
    State = other.State;
    _pDeviceComm = other._pDeviceComm;
    _pApiRoutines = other._pApiRoutines;
    _inputBuffer = other._inputBuffer;
    _outputBuffer = other._outputBuffer;

    // Since this struct uses anonymous unions and thus cannot
    // explicitly reference it, we have to a bit cheeky to copy it.
    // --> Just memcpy() the entire thing.
    memcpy(&Descriptor, &other.Descriptor, structPacketDataSize);

    if (State.InputBuffer)
    {
        State.InputBuffer = _inputBuffer.data();
    }

    if (State.OutputBuffer)
    {
        State.OutputBuffer = _outputBuffer.data();
    }

    if (Complete.Write.Data)
    {
        Complete.Write.Data = &u;
    }

    return *this;
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

        const auto cbReadSize = Descriptor.InputSize - State.ReadOffset;

        // If we were previously called with a huge buffer we have an equally large _inputBuffer.
        // We shouldn't just keep this huge buffer around, if no one needs it anymore.
        if (_inputBuffer.capacity() > 16 * 1024 && (_inputBuffer.capacity() >> 1) > cbReadSize)
        {
            _inputBuffer.shrink_to_fit();
        }

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
// - Before completing the message, ReleaseMessageBuffers must be called to free any allocation performed by this routine.
// Arguments:
// - Message - Supplies the message whose output buffer will be retrieved.
// - Buffer - Receives a pointer to the output buffer.
// - Size - Receives the size, in bytes, of the output buffer.
// Return Value:
// - HRESULT indicating if the output buffer was successfully retrieved.
[[nodiscard]] HRESULT _CONSOLE_API_MSG::GetOutputBuffer(_Outptr_result_bytebuffer_(*pcbSize) void** const ppvBuffer,
                                                        _Out_ ULONG* const pcbSize)
try
{
    // Initialize the buffer if it hasn't been initialized yet.
    if (State.OutputBuffer == nullptr)
    {
        RETURN_HR_IF(E_FAIL, State.WriteOffset > Descriptor.OutputSize);

        auto cbWriteSize = Descriptor.OutputSize - State.WriteOffset;

        // If we were previously called with a huge buffer we have an equally large _outputBuffer.
        // We shouldn't just keep this huge buffer around, if no one needs it anymore.
        if (_outputBuffer.capacity() > 16 * 1024 && (_outputBuffer.capacity() >> 1) > cbWriteSize)
        {
            _outputBuffer.shrink_to_fit();
        }

        _outputBuffer.resize(cbWriteSize);

        // 0 it out.
        std::fill_n(_outputBuffer.data(), _outputBuffer.size(), BYTE(0));

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
    auto hr = S_OK;

    if (State.InputBuffer != nullptr)
    {
        _inputBuffer.clear();
        State.InputBuffer = nullptr;
        State.InputBufferSize = 0;
    }

    if (State.OutputBuffer != nullptr)
    {
        if (SUCCEEDED_NTSTATUS(Complete.IoStatus.Status))
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
