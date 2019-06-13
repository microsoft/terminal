// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "WaitQueue.h"
#include "WaitBlock.h"

#include "..\host\globals.h"
#include "..\host\utils.hpp"

// Routine Description:
// - Instantiates a new ConsoleWaitQueue
ConsoleWaitQueue::ConsoleWaitQueue() :
    _blocks()
{
}

// Routine Description:
// - Destructs a ConsoleWaitQueue
// - This will notify any remaining waiting items that the associated process/object is dying.
ConsoleWaitQueue::~ConsoleWaitQueue()
{
    // Notify all blocks that the thread or object is dying when destroyed.
    NotifyWaiters(TRUE, WaitTerminationReason::ThreadDying);
}

// Routine Description:
// - Establishes a wait (call me back later) for a particular message with a given callback routine and its parameter
// Arguments:
// - pWaitReplyMessage - The API message that we're deferring until data is available later.
// - pWaiter - The context/callback information to restore and dispatch the call later.
// Return Value:
// - S_OK if enqueued appropriately and everything is alright. Or suitable HRESULT failure otherwise.
[[nodiscard]] HRESULT ConsoleWaitQueue::s_CreateWait(_Inout_ CONSOLE_API_MSG* const pWaitReplyMessage,
                                                     _In_ IWaitRoutine* const pWaiter)
{
    // Normally we'd have the Wait Queue handle the insertion of the block into the queue, but
    // the console does queues in a somewhat special way.
    //
    // Each block belongs in two queues:
    // 1. The process queue of the client that dispatched the request
    // 2. The object queue that the request will be serviced by
    // As such, when a wait occurs, it gets added to both queues.
    //
    // It will end up being serviced by one or the other queue, but when it is serviced, it must be
    // removed from both so it is not double processed.
    //
    // Therefore, I've inverted the queue management responsibility into the WaitBlock itself
    // and made it a friend to this WaitQueue class.

    return ConsoleWaitBlock::s_CreateWait(pWaitReplyMessage,
                                          pWaiter);
}

// Routine Description:
// - Instructs this queue to attempt to callback waiting requests
// Arguments:
// - fNotifyAll - If true, we will notify all items in the queue. If false, we will only notify the first item.
// Return Value:
// - True if any block was successfully notified. False if no blocks were successful.
bool ConsoleWaitQueue::NotifyWaiters(const bool fNotifyAll)
{
    return NotifyWaiters(fNotifyAll, WaitTerminationReason::NoReason);
}

// Routine Description:
// - Instructs this queue to attempt to callback waiting requests and request termination with the given reason
// Arguments:
// - fNotifyAll - If true, we will notify all items in the queue. If false, we will only notify the first item.
// - TerminationReason - A reason/message to pass to each waiter signaling it should terminate appropriately.
// Return Value:
// - True if any block was successfully notified. False if no blocks were successful.
bool ConsoleWaitQueue::NotifyWaiters(const bool fNotifyAll,
                                     const WaitTerminationReason TerminationReason)
{
    bool fResult = false;

    auto it = _blocks.cbegin();
    while (!_blocks.empty() && it != _blocks.cend())
    {
        ConsoleWaitBlock* const WaitBlock = (*it);
        if (nullptr == WaitBlock)
        {
            break;
        }

        auto const nextIt = std::next(it); // we have to capture next before it is potentially erased

        if (_NotifyBlock(WaitBlock, TerminationReason))
        {
            fResult = true;
        }

        if (!fNotifyAll)
        {
            break;
        }

        it = nextIt;
    }

    return fResult;
}

// Routine Description:
// - A helper to delete successfully notified callbacks
// Arguments:
// - pWaitBlock - A block containing callback data
// - TerminationReason - Optional reason to tell the callback to terminate. If 0, we're not requesting termination.
// Return Value:
// - True if callback successfully delivered data. False if callback still needs to wait longer.
bool ConsoleWaitQueue::_NotifyBlock(_In_ ConsoleWaitBlock* pWaitBlock,
                                    _In_ WaitTerminationReason TerminationReason)
{
    // Attempt to notify block with the given reason.
    bool const fResult = pWaitBlock->Notify(TerminationReason);

    if (fResult)
    {
        // If it was successful, delete it. (It will remove itself from appropriate queues.)
        delete pWaitBlock;
    }

    return fResult;
}
