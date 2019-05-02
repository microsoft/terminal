// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "readData.hpp"

// Routine Description:
// - Constructs read data base class to hold input buffer and cross-call handle information
// - Increments count of readers waiting on the given handle.
// Arguments:
// - pInputBuffer - Buffer that data will be read from.
// - pInputReadHandleData - Context stored across calls from the same input handle to return partial data appropriately.
// Return Value:
// - THROW: Throws E_INVALIDARG for invalid pointers.
ReadData::ReadData(_In_ InputBuffer* const pInputBuffer,
                   _In_ INPUT_READ_HANDLE_DATA* const pInputReadHandleData) :
    IWaitRoutine(ReplyDataType::Read),
    _pInputBuffer(THROW_HR_IF_NULL(E_INVALIDARG, pInputBuffer)),
    _pInputReadHandleData(THROW_HR_IF_NULL(E_INVALIDARG, pInputReadHandleData))
{
    _pInputReadHandleData->IncrementReadCount();
}

// Routine Description:
// - Destructs a read data class.
// - Decrements count of readers waiting on the given handle.
ReadData::~ReadData()
{
    // If the contents were moved, this might be null.
    if (_pInputReadHandleData != nullptr)
    {
        _pInputReadHandleData->DecrementReadCount();
    }
}

// Routine Description:
// - Moves another ReadData instance into this one.
// - Effectively steals ownership of the other instance's members, setting them to nullptr so
//   destroying other won't trigger resource cleanup actions.
// Arguments:
// - other - Another ReadData class instance.
ReadData::ReadData(ReadData&& other) :
    IWaitRoutine(other.GetReplyType()),
    _pInputBuffer(other._pInputBuffer),
    _pInputReadHandleData(other._pInputReadHandleData)
{
    other._pInputBuffer = nullptr;
    other._pInputReadHandleData = nullptr;
}

// Routine Description:
// - Retrieves the input buffer pointer associated with this read data context
// Arguments:
// - <none>
// Return Value:
// - Input buffer pointer.
InputBuffer* ReadData::GetInputBuffer() const
{
    return _pInputBuffer;
}

// Routine Description:
// - Retrieves the persistent handle data structure used to store read information across calls
// Arguments:
// - <none>
// Return Value:
// - Input read handle data pointer.
INPUT_READ_HANDLE_DATA* ReadData::GetInputReadHandleData() const
{
    return _pInputReadHandleData;
}
