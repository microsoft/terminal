/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IWaitRoutine.h

Abstract:
- This file specifies the interface that must be defined by a host application when queuing an API call to be serviced later.
  Specifically, this defines which method will be called back "later" to service the request.

Author:
- Michael Niksa (miniksa) 01-Mar-2017

Revision History:
- Adapted from original items in srvinit.cpp, getset.cpp, directio.cpp, stream.cpp
--*/

#pragma once

#include "WaitTerminationReason.h"

enum class ReplyDataType
{
    Write = 1,
    Read = 2,
};

class IWaitRoutine
{
public:
    IWaitRoutine(ReplyDataType type) noexcept :
        _ReplyType(type)

    {
    }

    virtual ~IWaitRoutine() = default;

    IWaitRoutine(const IWaitRoutine&) = delete;
    IWaitRoutine(IWaitRoutine&&) = delete;
    IWaitRoutine& operator=(const IWaitRoutine&) & = delete;
    IWaitRoutine& operator=(IWaitRoutine&&) & = delete;

    virtual const SCREEN_INFORMATION* GetScreenBuffer() const noexcept
    {
        return nullptr;
    }
    virtual void MigrateUserBuffersOnTransitionToBackgroundWait(const void* oldBuffer, void* newBuffer) = 0;

    virtual bool Notify(const WaitTerminationReason TerminationReason,
                        const bool fIsUnicode,
                        _Out_ NTSTATUS* const pReplyStatus,
                        _Out_ size_t* const pNumBytes,
                        _Out_ DWORD* const pControlKeyState,
                        _Out_ void* const pOutputData) = 0;

    ReplyDataType GetReplyType() const noexcept
    {
        return _ReplyType;
    }

private:
    const ReplyDataType _ReplyType;
};
