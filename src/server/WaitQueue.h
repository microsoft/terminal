/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- WaitQueue.h

Abstract:
- This file manages a queue of wait blocks

Author:
- Michael Niksa (miniksa) 17-Oct-2016

Revision History:
- Adapted from original items in handle.h
--*/

#pragma once

#include <list>

#include "..\host\conapi.h"

#include "IWaitRoutine.h"
#include "WaitBlock.h"
#include "WaitTerminationReason.h"

class ConsoleWaitQueue
{
public:
    ConsoleWaitQueue();

    ~ConsoleWaitQueue();

    bool NotifyWaiters(const bool fNotifyAll);

    bool NotifyWaiters(const bool fNotifyAll,
                       const WaitTerminationReason TerminationReason);

    [[nodiscard]] static HRESULT s_CreateWait(_Inout_ CONSOLE_API_MSG* const pWaitReplyMessage,
                                              _In_ IWaitRoutine* const pWaiter);

private:
    bool _NotifyBlock(_In_ ConsoleWaitBlock* pWaitBlock,
                      const WaitTerminationReason TerminationReason);

    std::list<ConsoleWaitBlock*> _blocks;

    friend class ConsoleWaitBlock; // Blocks live in multiple queues so we let them manage the lifetime.
};
