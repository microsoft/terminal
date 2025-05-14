// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "WaitBlock.h"
#include "WaitQueue.h"

#include "ApiSorter.h"

#include "../host/globals.h"
#include "../host/utils.hpp"

#include "../interactivity/inc/ServiceLocator.hpp"

// Routine Description:
// - Initializes a ConsoleWaitBlock
// - ConsoleWaitBlocks will mostly self-manage their position in their two queues.
// - They will be pushed into the tail and the resulting iterator stored for constant deletion time later.
// Arguments:
// - pProcessQueue - The queue attached to the client process ID that requested this action
// - pObjectQueue - The queue attached to the console object that will service the action when data arrives
// - pWaitReplyMessage - The original API message related to the client process's service request
// - pWaiter - The context to return to later when the wait is satisfied.
ConsoleWaitBlock::ConsoleWaitBlock(_In_ ConsoleWaitQueue* const pProcessQueue,
                                   _In_ ConsoleWaitQueue* const pObjectQueue,
                                   const CONSOLE_API_MSG* const pWaitReplyMessage,
                                   _In_ IWaitRoutine* const pWaiter) :
    _pProcessQueue(THROW_HR_IF_NULL(E_INVALIDARG, pProcessQueue)),
    _pObjectQueue(THROW_HR_IF_NULL(E_INVALIDARG, pObjectQueue)),
    _WaitReplyMessage(*pWaitReplyMessage),
    _pWaiter(THROW_HR_IF_NULL(E_INVALIDARG, pWaiter))
{
    // MSFT-33127449, GH#9692
    // Until there's a "Wait", there's only one API message inflight at a time. In our
    // quest for performance, we put that single API message in charge of its own
    // buffer management- instead of allocating buffers on the heap and deleting them
    // later (storing pointers to them at the far corners of the earth), it would
    // instead allocate them from small internal pools (if possible) and only heap
    // allocate (transparently) if necessary. The pointers flung to the corners of the
    // earth would be pointers (1) back into the API_MSG or (2) to a heap block owned
    // by til::small_vector.
    //
    // It took us months to realize that those bare pointers were being held by
    // COOKED_READ and RAW_READ and not actually being updated when the API message was
    // _copied_ as it was shuffled off to the background to become a "Wait" message.
    //
    // It turns out that it's trivially possible to crash the console by sending two
    // API calls -- one that waits and one that completes immediately -- when the
    // waiting message or the "wait completer" has a bunch of dangling pointers in it.
    // Oops.
    //
    // Here, we tell the wait completion routine (which is going to be a COOKED_READ,
    // RAW_READ, DirectRead or WriteData) about the new buffer location.

    if (pWaitReplyMessage->State.InputBuffer)
    {
        _pWaiter->MigrateUserBuffersOnTransitionToBackgroundWait(pWaitReplyMessage->State.InputBuffer, _WaitReplyMessage.State.InputBuffer);
    }

    if (pWaitReplyMessage->State.OutputBuffer)
    {
        _pWaiter->MigrateUserBuffersOnTransitionToBackgroundWait(pWaitReplyMessage->State.OutputBuffer, _WaitReplyMessage.State.OutputBuffer);
    }
}

// Routine Description:
// - Destroys a ConsolewaitBlock
// - On deletion, ConsoleWaitBlocks will erase themselves from the process and object queues in
//   constant time with the iterator acquired on construction.
ConsoleWaitBlock::~ConsoleWaitBlock()
{
    _pProcessQueue->_blocks.erase(_itProcessQueue);
    _pObjectQueue->_blocks.erase(_itObjectQueue);
    delete _pWaiter;
}

// Routine Description:
// - Creates and enqueues a new wait for later callback when a routine cannot be serviced at this time.
// - Will extract the process ID and the target object, enqueuing in both to know when to callback
// Arguments:
// - pWaitReplyMessage - The original API message from the client asking for servicing
// - pWaiter - The context/callback information to restore and dispatch the call later.
// Return Value:
// - S_OK if queued and ready to go. Appropriate HRESULT value if it failed.
[[nodiscard]] HRESULT ConsoleWaitBlock::s_CreateWait(_Inout_ CONSOLE_API_MSG* const pWaitReplyMessage,
                                                     _In_ IWaitRoutine* const pWaiter)
{
    if (!pWaitReplyMessage || !pWaiter)
    {
        if (pWaiter)
        {
            delete pWaiter;
        }
        return E_INVALIDARG;
    }

    const auto ProcessData = pWaitReplyMessage->GetProcessHandle();
    FAIL_FAST_IF_NULL(ProcessData);

    const auto pProcessQueue = ProcessData->pWaitBlockQueue.get();

    const auto pHandleData = pWaitReplyMessage->GetObjectHandle();
    FAIL_FAST_IF_NULL(pHandleData);

    ConsoleWaitQueue* pObjectQueue = nullptr;
    LOG_IF_FAILED(pHandleData->GetWaitQueue(&pObjectQueue));
    FAIL_FAST_IF_NULL(pObjectQueue);

    ConsoleWaitBlock* pWaitBlock;
    try
    {
        pWaitBlock = new ConsoleWaitBlock(pProcessQueue,
                                          pObjectQueue,
                                          pWaitReplyMessage,
                                          pWaiter);

        // Set the iterators on the wait block so that it can remove itself later.
        pWaitBlock->_itProcessQueue = pProcessQueue->_blocks.insert(pProcessQueue->_blocks.end(), pWaitBlock);
        pWaitBlock->_itObjectQueue = pObjectQueue->_blocks.insert(pObjectQueue->_blocks.end(), pWaitBlock);
    }
    catch (...)
    {
        const auto hr = wil::ResultFromCaughtException();
        pWaitReplyMessage->SetReplyStatus(NTSTATUS_FROM_HRESULT(hr));
        return hr;
    }

    return S_OK;
}

// Routine Description:
// - Used to trigger the callback routine inside this wait block.
// Arguments:
// - TerminationReason - A reason to tell the callback to terminate early or 0 if it should operate normally.
// Return Value:
// - True if the routine was able to successfully return data (or terminate). False otherwise.
bool ConsoleWaitBlock::Notify(const WaitTerminationReason TerminationReason)
{
    bool fRetVal;

    NTSTATUS status;
    size_t NumBytes = 0;
    DWORD dwControlKeyState;
    auto fIsUnicode = true;

    InputEventQueue outEvents;
    // TODO: MSFT 14104228 - get rid of this void* and get the data
    // out of the read wait object properly.
    void* pOutputData = nullptr;
    // 1. Get unicode status of notify call based on message type.
    // We still need to know the Unicode status on reads as they will be converted after the wait operation.
    // Writes will have been converted before hitting the wait state.
    switch (_WaitReplyMessage.msgHeader.ApiNumber)
    {
    case API_NUMBER_GETCONSOLEINPUT:
    {
        auto a = &(_WaitReplyMessage.u.consoleMsgL1.GetConsoleInput);
        fIsUnicode = !!a->Unicode;
        pOutputData = &outEvents;
        break;
    }
    case API_NUMBER_READCONSOLE:
    {
        auto a = &(_WaitReplyMessage.u.consoleMsgL1.ReadConsole);
        fIsUnicode = !!a->Unicode;
        break;
    }
    case API_NUMBER_WRITECONSOLE:
    {
        auto a = &(_WaitReplyMessage.u.consoleMsgL1.WriteConsole);
        fIsUnicode = !!a->Unicode;
        break;
    }
    default:
    {
        FAIL_FAST_HR(E_NOTIMPL); // we shouldn't be getting a wait/notify on API numbers we don't support.
        break;
    }
    }

    // 2. If we have a waiter, dispatch to it.
    if (_pWaiter->Notify(TerminationReason, fIsUnicode, &status, &NumBytes, &dwControlKeyState, pOutputData))
    {
        // 3. If the wait was successful, set reply info and attach any additional return information that this request type might need.
        _WaitReplyMessage.SetReplyStatus(status);
        _WaitReplyMessage.SetReplyInformation(NumBytes);

        if (API_NUMBER_GETCONSOLEINPUT == _WaitReplyMessage.msgHeader.ApiNumber)
        {
            // ReadConsoleInput/PeekConsoleInput has this extra reply
            // information with the number of records, not number of
            // bytes.
            auto a = &(_WaitReplyMessage.u.consoleMsgL1.GetConsoleInput);

            void* buffer;
            ULONG cbBuffer;
            if (FAILED(_WaitReplyMessage.GetOutputBuffer(&buffer, &cbBuffer)))
            {
                return false;
            }

            const auto pRecordBuffer = static_cast<INPUT_RECORD* const>(buffer);
            a->NumRecords = static_cast<ULONG>(outEvents.size());
            std::ranges::copy(outEvents, pRecordBuffer);
        }
        else if (API_NUMBER_READCONSOLE == _WaitReplyMessage.msgHeader.ApiNumber)
        {
            // ReadConsole has this extra reply information with the control key state.
            auto a = &(_WaitReplyMessage.u.consoleMsgL1.ReadConsole);
            a->ControlKeyState = dwControlKeyState;
            a->NumBytes = gsl::narrow<ULONG>(NumBytes);

            // - This routine is called when a ReadConsole or ReadFile request is about to be completed.
            // - It sets the number of bytes written as the information to be written with the completion status and,
            //   if CTRL+Z processing is enabled and a CTRL+Z is detected, switches the number of bytes read to zero.
            if (a->ProcessControlZ != FALSE &&
                a->NumBytes > 0 &&
                _WaitReplyMessage.State.OutputBuffer != nullptr &&
                *(PUCHAR)_WaitReplyMessage.State.OutputBuffer == 0x1a)
            {
                // On changing this, we also need to notify the Reply Information because it was stowed above into the reply packet.
                a->NumBytes = 0;
                // Setting the reply length to 0 and returning successfully from a blocked wait
                // will imply that the user has reached "End of File" on a raw read file stream.
                _WaitReplyMessage.SetReplyInformation(0);
            }
        }
        else if (API_NUMBER_WRITECONSOLE == _WaitReplyMessage.msgHeader.ApiNumber)
        {
            auto a = &(_WaitReplyMessage.u.consoleMsgL1.WriteConsoleW);
            a->NumBytes = gsl::narrow<ULONG>(NumBytes);
        }

        _WaitReplyMessage.ReleaseMessageBuffers();

        // This call fails when the server pipe is closed on us,
        // which results in log spam in practice.
        std::ignore = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().pDeviceComm->CompleteIo(&_WaitReplyMessage.Complete);

        fRetVal = true;
    }
    else
    {
        // If fThreadDying is TRUE we need to make sure that we removed the pWaitBlock from the list (which we don't do on this branch).
        FAIL_FAST_IF(!(WI_IsFlagClear(TerminationReason, WaitTerminationReason::ThreadDying)));
        fRetVal = false;
    }

    return fRetVal;
}
