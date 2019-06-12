/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- WaitBlock.h

Abstract:
- This file defines a queued operation when a console buffer object cannot currently satisfy the request.

Author:
- Michael Niksa (miniksa) 12-Oct-2016

Revision History:
- Adapted from original items in handle.h
--*/

#pragma once

#include "..\host\conapi.h"
#include "IWaitRoutine.h"
#include "WaitTerminationReason.h"

#include <list>

class ConsoleWaitQueue;

class ConsoleWaitBlock
{
public:
    ~ConsoleWaitBlock();

    bool Notify(const WaitTerminationReason TerminationReason);

    [[nodiscard]] static HRESULT s_CreateWait(_Inout_ CONSOLE_API_MSG* const pWaitReplymessage,
                                              _In_ IWaitRoutine* const pWaiter);

private:
    ConsoleWaitBlock(_In_ ConsoleWaitQueue* const pProcessQueue,
                     _In_ ConsoleWaitQueue* const pObjectQueue,
                     const CONSOLE_API_MSG* const pWaitReplyMessage,
                     _In_ IWaitRoutine* const pWaiter);

    ConsoleWaitQueue* const _pProcessQueue;
    std::_List_const_iterator<std::_List_val<std::_List_simple_types<ConsoleWaitBlock*>>> _itProcessQueue;

    ConsoleWaitQueue* const _pObjectQueue;
    std::_List_const_iterator<std::_List_val<std::_List_simple_types<ConsoleWaitBlock*>>> _itObjectQueue;

    CONSOLE_API_MSG _WaitReplyMessage;

    IWaitRoutine* const _pWaiter;
};
