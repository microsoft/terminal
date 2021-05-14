/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- readData.hpp

Abstract:
- This file defines the interface for read data structures.
- Read data structures are used to pass context between various layers of the read
  as well as to persist state across a read call that must wait until additional
  data is added to the buffer at a later time.

Author:
- Austin Diviness (AustDi) 1-Mar-2017
- Michael Niksa (MiNiksa) 1-Mar-2017

Revision History:
--*/

#pragma once

#include "../server/IWaitRoutine.h"
#include "../server/WaitTerminationReason.h"

class ReadData : public IWaitRoutine
{
public:
    ReadData(_In_ InputBuffer* const pInputBuffer,
             _In_ INPUT_READ_HANDLE_DATA* const pInputReadHandleData);

    virtual ~ReadData();

    ReadData(const ReadData&) = delete;
    ReadData(ReadData&&);
    ReadData& operator=(const ReadData&) & = delete;
    ReadData& operator=(ReadData&&) & = delete;

    virtual void MigrateUserBuffersOnTransitionToBackgroundWait(const void* oldBuffer, void* newBuffer) = 0;
    virtual bool Notify(const WaitTerminationReason TerminationReason,
                        const bool fIsUnicode,
                        _Out_ NTSTATUS* const pReplyStatus,
                        _Out_ size_t* const pNumBytes,
                        _Out_ DWORD* const pControlKeyState,
                        _Out_ void* const pOutputData) = 0;

    InputBuffer* GetInputBuffer() const;
    INPUT_READ_HANDLE_DATA* GetInputReadHandleData() const;

// TODO MSFT:11285829 this is a temporary kludge until the constructors are ironed
// out, so that we can still run the tests in the meantime.
#if UNIT_TESTING
    ReadData() :
        IWaitRoutine(ReplyDataType::Read),
        _pInputBuffer{ nullptr },
        _pInputReadHandleData{ nullptr }
    {
    }
#endif
protected:
    InputBuffer* _pInputBuffer;
    INPUT_READ_HANDLE_DATA* _pInputReadHandleData;
};
