/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- writeData.hpp

Abstract:
- This file defines the interface for write data structures.
- This is used not only within the write call, but also to hold context in case a wait condition is required
  because writing to the buffer is blocked for some reason.

Author:
- Michael Niksa (MiNiksa) 9-Mar-2017

Revision History:
--*/

#pragma once

#include "../server/IWaitRoutine.h"
#include "../server/WaitTerminationReason.h"

class WriteData : public IWaitRoutine
{
public:
    WriteData(SCREEN_INFORMATION& siContext,
              _In_reads_bytes_(cbContext) wchar_t* const pwchContext,
              const size_t cbContext,
              const UINT uiOutputCodepage,
              const bool requiresVtQuirk);
    ~WriteData();

    void SetLeadByteAdjustmentStatus(const bool fLeadByteCaptured,
                                     const bool fLeadByteConsumed);

    void SetUtf8ConsumedCharacters(const size_t cchUtf8Consumed);

    void MigrateUserBuffersOnTransitionToBackgroundWait(const void* oldBuffer, void* newBuffer) override;
    bool Notify(const WaitTerminationReason TerminationReason,
                const bool fIsUnicode,
                _Out_ NTSTATUS* const pReplyStatus,
                _Out_ size_t* const pNumBytes,
                _Out_ DWORD* const pControlKeyState,
                _Out_ void* const pOutputData);

private:
    SCREEN_INFORMATION& _siContext;
    wchar_t* const _pwchContext;
    size_t const _cbContext;
    UINT const _uiOutputCodepage;
    bool _requiresVtQuirk;
    bool _fLeadByteCaptured;
    bool _fLeadByteConsumed;
    size_t _cchUtf8Consumed;
};
