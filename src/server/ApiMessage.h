/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ApiMessage.h

Abstract:
- This file extends the published structure of an API message to provide encapsulation and helper methods

Author:
- Michael Niksa (miniksa) 12-Oct-2016

Revision History:
- Adapted from original items in util.cpp & conapi.h & csrutil.cpp
--*/

#pragma once

#include "ApiMessageState.h"
#include "IApiRoutines.h"

class ConsoleProcessHandle;
class ConsoleHandleData;

class IDeviceComm;

typedef struct _CONSOLE_API_MSG
{
    _CONSOLE_API_MSG();

    CD_IO_COMPLETE Complete;
    CONSOLE_API_STATE State;

    IDeviceComm* _pDeviceComm;
    IApiRoutines* _pApiRoutines;

private:
    boost::container::small_vector<BYTE, 128> _inputBuffer;
    boost::container::small_vector<BYTE, 128> _outputBuffer;

public:
    // From here down is the actual packet data sent/received.
    CD_IO_DESCRIPTOR Descriptor;
    union
    {
        struct
        {
            CD_CREATE_OBJECT_INFORMATION CreateObject;
            CONSOLE_CREATESCREENBUFFER_MSG CreateScreenBuffer;
        };
        struct
        {
            CONSOLE_MSG_HEADER msgHeader;
            union
            {
                CONSOLE_MSG_BODY_L1 consoleMsgL1;
                CONSOLE_MSG_BODY_L2 consoleMsgL2;
                CONSOLE_MSG_BODY_L3 consoleMsgL3;
            } u;
        };
    };
    // End packet data

public:
    // DO NOT PUT MORE FIELDS DOWN HERE.
    // The tail end of this structure will have a console driver packet
    // copied over it and it will overwrite any fields declared here.

    ConsoleProcessHandle* GetProcessHandle() const;
    ConsoleHandleData* GetObjectHandle() const;

    [[nodiscard]] HRESULT ReadMessageInput(const ULONG cbOffset, _Out_writes_bytes_(cbSize) PVOID pvBuffer, const ULONG cbSize);
    [[nodiscard]] HRESULT GetAugmentedOutputBuffer(const ULONG cbFactor,
                                                   _Outptr_result_bytebuffer_(*pcbSize) PVOID* ppvBuffer,
                                                   _Out_ PULONG pcbSize);
    [[nodiscard]] HRESULT GetOutputBuffer(_Outptr_result_bytebuffer_(*pcbSize) void** const ppvBuffer, _Out_ ULONG* const pcbSize);
    [[nodiscard]] HRESULT GetInputBuffer(_Outptr_result_bytebuffer_(*pcbSize) void** const ppvBuffer, _Out_ ULONG* const pcbSize);

    [[nodiscard]] HRESULT ReleaseMessageBuffers();

    void SetReplyStatus(const NTSTATUS Status);
    void SetReplyInformation(const ULONG_PTR pInformation);

    // MSFT-33127449, GH#9692
    // We are not writing a copy constructor for this class
    // so as to scope a fix as narrowly as possible to the
    // "invalid user buffer" crash.
    // TODO GH#10076: remove this.
    void UpdateUserBufferPointers();

    // DO NOT PUT MORE FIELDS DOWN HERE.
    // The tail end of this structure will have a console driver packet
    // copied over it and it will overwrite any fields declared here.

} CONSOLE_API_MSG, *PCONSOLE_API_MSG, * const PCCONSOLE_API_MSG;
