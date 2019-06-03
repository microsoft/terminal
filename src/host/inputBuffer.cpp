// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inputBuffer.hpp"
#include "dbcs.h"
#include "stream.h"
#include "../types/inc/GlyphWidth.hpp"

#include <functional>

#include "..\interactivity\inc\ServiceLocator.hpp"

#define INPUT_BUFFER_DEFAULT_INPUT_MODE (ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_ECHO_INPUT | ENABLE_MOUSE_INPUT)

using Microsoft::Console::Interactivity::ServiceLocator;
using Microsoft::Console::VirtualTerminal::TerminalInput;

// Routine Description:
// - This method creates an input buffer.
// Arguments:
// - None
// Return Value:
// - A new instance of InputBuffer
InputBuffer::InputBuffer() :
    InputMode{ INPUT_BUFFER_DEFAULT_INPUT_MODE },
    WaitQueue{},
    _termInput(std::bind(&InputBuffer::_HandleTerminalInputCallback, this, std::placeholders::_1))
{
    // The _termInput's constructor takes a reference to this object's _HandleTerminalInputCallback.
    // We need to use std::bind to create a reference to that function without a reference to this InputBuffer

    // initialize buffer header
    fInComposition = false;
}

// Routine Description:
// - This routine frees the resources associated with an input buffer.
// Arguments:
// - None
// Return Value:
InputBuffer::~InputBuffer()
{
}

// Routine Description:
// - checks if any partial char data is available for reading operation
// Arguments:
// - None
// Return Value:
// - true if partial char data is available, false otherwise
bool InputBuffer::IsReadPartialByteSequenceAvailable()
{
    return _readPartialByteSequence.get() != nullptr;
}

// Routine Description:
// - reads any read partial char data available
// Arguments:
// - peek - if true, data will not be removed after being fetched
// Return Value:
// - the partial char data. may be nullptr if no data is available
std::unique_ptr<IInputEvent> InputBuffer::FetchReadPartialByteSequence(_In_ bool peek)
{
    if (!IsReadPartialByteSequenceAvailable())
    {
        return std::unique_ptr<IInputEvent>();
    }

    if (peek)
    {
        return IInputEvent::Create(_readPartialByteSequence->ToInputRecord());
    }
    else
    {
        std::unique_ptr<IInputEvent> outEvent;
        outEvent.swap(_readPartialByteSequence);
        return outEvent;
    }
}

// Routine Description:
// - stores partial read char data for a later read. will overwrite
// any previously stored data.
// Arguments:
// - event - The event to store
// Return Value:
// - None
void InputBuffer::StoreReadPartialByteSequence(std::unique_ptr<IInputEvent> event)
{
    _readPartialByteSequence.swap(event);
}

// Routine Description:
// - checks if any partial char data is available for writing
// operation.
// Arguments:
// - None
// Return Value:
// - true if partial char data is available, false otherwise
bool InputBuffer::IsWritePartialByteSequenceAvailable()
{
    return _writePartialByteSequence.get() != nullptr;
}

// Routine Description:
// - writes any write partial char data available
// Arguments:
// - peek - if true, data will not be removed after being fetched
// Return Value:
// - the partial char data. may be nullptr if no data is available
std::unique_ptr<IInputEvent> InputBuffer::FetchWritePartialByteSequence(_In_ bool peek)
{
    if (!IsWritePartialByteSequenceAvailable())
    {
        return std::unique_ptr<IInputEvent>();
    }

    if (peek)
    {
        return IInputEvent::Create(_writePartialByteSequence->ToInputRecord());
    }
    else
    {
        std::unique_ptr<IInputEvent> outEvent;
        outEvent.swap(_writePartialByteSequence);
        return outEvent;
    }
}

// Routine Description:
// - stores partial write char data. will overwrite any previously
// stored data.
// Arguments:
// - event - The event to store
// Return Value:
// - None
void InputBuffer::StoreWritePartialByteSequence(std::unique_ptr<IInputEvent> event)
{
    _writePartialByteSequence.swap(event);
}

// Routine Description:
// - This routine resets the input buffer information fields to their initial values.
// Arguments:
// Return Value:
// Note:
// - The console lock must be held when calling this routine.
void InputBuffer::ReinitializeInputBuffer()
{
    ServiceLocator::LocateGlobals().hInputEvent.ResetEvent();
    InputMode = INPUT_BUFFER_DEFAULT_INPUT_MODE;
    _storage.clear();
}

// Routine Description:
// - Wakes up readers waiting for data to read.
// Arguments:
// - None
// Return Value:
// - None
void InputBuffer::WakeUpReadersWaitingForData()
{
    WaitQueue.NotifyWaiters(false);
}

// Routine Description:
// - Wakes up any readers waiting for data when a ctrl-c or ctrl-break is input.
// Arguments:
// - Flag - Indicates reason to terminate the readers.
// Return Value:
// - None
void InputBuffer::TerminateRead(_In_ WaitTerminationReason Flag)
{
    WaitQueue.NotifyWaiters(true, Flag);
}

// Routine Description:
// - Returns the number of events in the input buffer.
// Arguments:
// - None
// Return Value:
// - The number of events currently in the input buffer.
// Note:
// - The console lock must be held when calling this routine.
size_t InputBuffer::GetNumberOfReadyEvents() const noexcept
{
    return _storage.size();
}

// Routine Description:
// - This routine empties the input buffer
// Arguments:
// - None
// Return Value:
// - None
// Note:
// - The console lock must be held when calling this routine.
void InputBuffer::Flush()
{
    _storage.clear();
    ServiceLocator::LocateGlobals().hInputEvent.ResetEvent();
}

// Routine Description:
// - This routine removes all but the key events from the buffer.
// Arguments:
// - None
// Return Value:
// - None
// Note:
// - The console lock must be held when calling this routine.
void InputBuffer::FlushAllButKeys()
{
    auto newEnd = std::remove_if(_storage.begin(), _storage.end(), [](const std::unique_ptr<IInputEvent>& event) {
        return event->EventType() != InputEventType::KeyEvent;
    });
    _storage.erase(newEnd, _storage.end());
}

// Routine Description:
// - This routine reads from the input buffer.
// - It can convert returned data to through the currently set Input CP, it can optionally return a wait condition
//   if there isn't enough data in the buffer, and it can be set to not remove records as it reads them out.
// Note:
// - The console lock must be held when calling this routine.
// Arguments:
// - OutEvents - deque to store the read events
// - AmountToRead - the amount of events to try to read
// - Peek - If true, copy events to pInputRecord but don't remove them from the input buffer.
// - WaitForData - if true, wait until an event is input (if there aren't enough to fill client buffer). if false, return immediately
// - Unicode - true if the data in key events should be treated as unicode. false if they should be converted by the current input CP.
// - Stream - true if read should unpack KeyEvents that have a >1 repeat count. AmountToRead must be 1 if Stream is true.
// Return Value:
// - STATUS_SUCCESS if records were read into the client buffer and everything is OK.
// - CONSOLE_STATUS_WAIT if there weren't enough records to satisfy the request (and waits are allowed)
// - otherwise a suitable memory/math/string error in NTSTATUS form.
[[nodiscard]] NTSTATUS InputBuffer::Read(_Out_ std::deque<std::unique_ptr<IInputEvent>>& OutEvents,
                                         const size_t AmountToRead,
                                         const bool Peek,
                                         const bool WaitForData,
                                         const bool Unicode,
                                         const bool Stream)
{
    try
    {
        if (_storage.empty())
        {
            if (!WaitForData)
            {
                return STATUS_SUCCESS;
            }
            return CONSOLE_STATUS_WAIT;
        }

        // read from buffer
        std::deque<std::unique_ptr<IInputEvent>> events;
        size_t eventsRead;
        bool resetWaitEvent;
        _ReadBuffer(events,
                    AmountToRead,
                    eventsRead,
                    Peek,
                    resetWaitEvent,
                    Unicode,
                    Stream);

        // copy events to outEvents
        while (!events.empty())
        {
            OutEvents.push_back(std::move(events.front()));
            events.pop_front();
        }

        if (resetWaitEvent)
        {
            ServiceLocator::LocateGlobals().hInputEvent.ResetEvent();
        }
        return STATUS_SUCCESS;
    }
    catch (...)
    {
        return NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
    }
}

// Routine Description:
// - This routine reads a single event from the input buffer.
// - It can convert returned data to through the currently set Input CP, it can optionally return a wait condition
//   if there isn't enough data in the buffer, and it can be set to not remove records as it reads them out.
// Note:
// - The console lock must be held when calling this routine.
// Arguments:
// - outEvent - where the read event is stored
// - Peek - If true, copy events to pInputRecord but don't remove them from the input buffer.
// - WaitForData - if true, wait until an event is input (if there aren't enough to fill client buffer). if false, return immediately
// - Unicode - true if the data in key events should be treated as unicode. false if they should be converted by the current input CP.
// - Stream - true if read should unpack KeyEvents that have a >1 repeat count.
// Return Value:
// - STATUS_SUCCESS if records were read into the client buffer and everything is OK.
// - CONSOLE_STATUS_WAIT if there weren't enough records to satisfy the request (and waits are allowed)
// - otherwise a suitable memory/math/string error in NTSTATUS form.
[[nodiscard]] NTSTATUS InputBuffer::Read(_Out_ std::unique_ptr<IInputEvent>& outEvent,
                                         const bool Peek,
                                         const bool WaitForData,
                                         const bool Unicode,
                                         const bool Stream)
{
    NTSTATUS Status;
    try
    {
        std::deque<std::unique_ptr<IInputEvent>> outEvents;
        Status = Read(outEvents,
                      1,
                      Peek,
                      WaitForData,
                      Unicode,
                      Stream);
        if (!outEvents.empty())
        {
            outEvent.swap(outEvents.front());
        }
    }
    catch (...)
    {
        Status = NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
    }

    return Status;
}

// Routine Description:
// - This routine reads from a buffer. It does the buffer manipulation.
// Arguments:
// - outEvents - where read events are placed
// - readCount - amount of events to read
// - eventsRead - where to store number of events read
// - peek - if true , don't remove data from buffer, just copy it.
// - resetWaitEvent - on exit, true if buffer became empty.
// - unicode - true if read should be done in unicode mode
// - streamRead - true if read should unpack KeyEvents that have a >1 repeat count. readCount must be 1 if streamRead is true.
// Return Value:
// - <none>
// Note:
// - The console lock must be held when calling this routine.
void InputBuffer::_ReadBuffer(_Out_ std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                              const size_t readCount,
                              _Out_ size_t& eventsRead,
                              const bool peek,
                              _Out_ bool& resetWaitEvent,
                              const bool unicode,
                              const bool streamRead)
{
    // when stream reading, the previous behavior was to only allow reading of a single
    // event at a time.
    FAIL_FAST_IF(streamRead && readCount != 1);

    resetWaitEvent = false;

    std::deque<std::unique_ptr<IInputEvent>> readEvents;
    // we need another var to keep track of how many we've read
    // because dbcs records count for two when we aren't doing a
    // unicode read but the eventsRead count should return the number
    // of events actually put into outRecords.
    size_t virtualReadCount = 0;

    while (!_storage.empty() && virtualReadCount < readCount)
    {
        bool performNormalRead = true;
        // for stream reads we need to split any key events that have been coalesced
        if (streamRead)
        {
            if (_storage.front()->EventType() == InputEventType::KeyEvent)
            {
                KeyEvent* const pKeyEvent = static_cast<KeyEvent* const>(_storage.front().get());
                if (pKeyEvent->GetRepeatCount() > 1)
                {
                    // split the key event
                    std::unique_ptr<KeyEvent> streamKeyEvent = std::make_unique<KeyEvent>(*pKeyEvent);
                    streamKeyEvent->SetRepeatCount(1);
                    readEvents.push_back(std::move(streamKeyEvent));
                    pKeyEvent->SetRepeatCount(pKeyEvent->GetRepeatCount() - 1);
                    performNormalRead = false;
                }
            }
        }

        if (performNormalRead)
        {
            readEvents.push_back(std::move(_storage.front()));
            _storage.pop_front();
        }

        ++virtualReadCount;
        if (!unicode)
        {
            if (readEvents.back()->EventType() == InputEventType::KeyEvent)
            {
                const KeyEvent* const pKeyEvent = static_cast<const KeyEvent* const>(readEvents.back().get());
                if (IsGlyphFullWidth(pKeyEvent->GetCharData()))
                {
                    ++virtualReadCount;
                }
            }
        }
    }

    // the amount of events that were actually read
    eventsRead = readEvents.size();

    // copy the events back if we were supposed to peek
    if (peek)
    {
        if (streamRead)
        {
            // we need to check and see if the event was split from a coalesced key event
            // or if it was unrelated to the current front event in storage
            if (!readEvents.empty() &&
                !_storage.empty() &&
                readEvents.back()->EventType() == InputEventType::KeyEvent &&
                _storage.front()->EventType() == InputEventType::KeyEvent &&
                _CanCoalesce(static_cast<const KeyEvent&>(*readEvents.back()),
                             static_cast<const KeyEvent&>(*_storage.front())))
            {
                KeyEvent& keyEvent = static_cast<KeyEvent&>(*_storage.front());
                keyEvent.SetRepeatCount(keyEvent.GetRepeatCount() + 1);
            }
            else
            {
                _storage.push_front(IInputEvent::Create(readEvents.back()->ToInputRecord()));
            }
        }
        else
        {
            for (auto it = readEvents.crbegin(); it != readEvents.crend(); ++it)
            {
                _storage.push_front(IInputEvent::Create((*it)->ToInputRecord()));
            }
        }
    }

    // move events read to proper deque
    while (!readEvents.empty())
    {
        outEvents.push_back(std::move(readEvents.front()));
        readEvents.pop_front();
    }

    // signal if we emptied the buffer
    if (_storage.empty())
    {
        resetWaitEvent = true;
    }
}

// Routine Description:
// -  Writes events to the beginning of the input buffer.
// Arguments:
// - inEvents - events to write to buffer.
// - eventsWritten - The number of events written to the buffer on exit.
// Return Value:
// S_OK if successful.
// Note:
// - The console lock must be held when calling this routine.
size_t InputBuffer::Prepend(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& inEvents)
{
    try
    {
        _HandleConsoleSuspensionEvents(inEvents);
        if (inEvents.empty())
        {
            return STATUS_SUCCESS;
        }
        // read all of the records out of the buffer, then write the
        // prepend ones, then write the original set. We need to do it
        // this way to handle any coalescing that might occur.

        // get all of the existing records, "emptying" the buffer
        std::deque<std::unique_ptr<IInputEvent>> existingStorage;
        existingStorage.swap(_storage);

        // We will need this variable to pass to _WriteBuffer so it can attempt to determine wait status.
        // However, because we swapped the storage out from under it with an empty deque, it will always
        // return true after the first one (as it is filling the newly emptied backing deque.)
        // Then after the second one, because we've inserted some input, it will always say false.
        bool unusedWaitStatus = false;

        // write the prepend records
        size_t prependEventsWritten;
        _WriteBuffer(inEvents, prependEventsWritten, unusedWaitStatus);
        FAIL_FAST_IF(!(unusedWaitStatus));

        // write all previously existing records
        size_t existingEventsWritten;
        _WriteBuffer(existingStorage, existingEventsWritten, unusedWaitStatus);
        FAIL_FAST_IF(!(!unusedWaitStatus));

        // We need to set the wait event if there were 0 events in the
        // input queue when we started.
        // Because we did interesting manipulation of the wait queue
        // in order to prepend, we can't trust what _WriteBuffer said
        // and instead need to set the event if the original backing
        // buffer (the one we swapped out at the top) was empty
        // when this whole thing started.
        if (existingStorage.empty())
        {
            ServiceLocator::LocateGlobals().hInputEvent.SetEvent();
        }
        WakeUpReadersWaitingForData();

        return prependEventsWritten;
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
        return 0;
    }
}

// Routine Description:
// - Writes event to the input buffer. Wakes up any readers that are
// waiting for additional input events.
// Arguments:
// - inEvent - input event to store in the buffer.
// Return Value:
// - The number of events that were written to input buffer.
// Note:
// - The console lock must be held when calling this routine.
// - any outside references to inEvent will ben invalidated after
// calling this method.
size_t InputBuffer::Write(_Inout_ std::unique_ptr<IInputEvent> inEvent)
{
    try
    {
        std::deque<std::unique_ptr<IInputEvent>> inEvents;
        inEvents.push_back(std::move(inEvent));
        return Write(inEvents);
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
        return 0;
    }
}

// Routine Description:
// - Writes events to the input buffer. Wakes up any readers that are
// waiting for additional input events.
// Arguments:
// - inEvents - input events to store in the buffer.
// Return Value:
// - The number of events that were written to input buffer.
// Note:
// - The console lock must be held when calling this routine.
size_t InputBuffer::Write(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& inEvents)
{
    try
    {
        _HandleConsoleSuspensionEvents(inEvents);
        if (inEvents.empty())
        {
            return 0;
        }

        // Write to buffer.
        size_t EventsWritten;
        bool SetWaitEvent;
        _WriteBuffer(inEvents, EventsWritten, SetWaitEvent);

        if (SetWaitEvent)
        {
            ServiceLocator::LocateGlobals().hInputEvent.SetEvent();
        }

        // Alert any writers waiting for space.
        WakeUpReadersWaitingForData();
        return EventsWritten;
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
        return 0;
    }
}

// Routine Description:
// - Coalesces input events and transfers them to storage queue.
// Arguments:
// - inRecords - The events to store.
// - eventsWritten - The number of events written since this function
// was called.
// - setWaitEvent - on exit, true if buffer became non-empty.
// Return Value:
// - None
// Note:
// - The console lock must be held when calling this routine.
// - will throw on failure
void InputBuffer::_WriteBuffer(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& inEvents,
                               _Out_ size_t& eventsWritten,
                               _Out_ bool& setWaitEvent)
{
    eventsWritten = 0;
    setWaitEvent = false;
    const bool initiallyEmptyQueue = _storage.empty();
    const size_t initialInEventsSize = inEvents.size();
    const bool vtInputMode = IsInVirtualTerminalInputMode();

    while (!inEvents.empty())
    {
        // Pop the next event.
        // If we're in vt mode, try and handle it with the vt input module.
        // If it was handled, do nothing else for it.
        // If there was one event passed in, try coalescing it with the previous event currently in the buffer.
        // If it's not coalesced, append it to the buffer.
        std::unique_ptr<IInputEvent> inEvent = std::move(inEvents.front());
        inEvents.pop_front();
        if (vtInputMode)
        {
            const bool handled = _termInput.HandleKey(inEvent.get());
            if (handled)
            {
                eventsWritten++;
                continue;
            }
        }

        // we only check for possible coalescing when storing one
        // record at a time because this is the original behavior of
        // the input buffer. Changing this behavior may break stuff
        // that was depending on it.
        if (initialInEventsSize == 1 && !_storage.empty())
        {
            // coalescing requires a deque of events, so push it back onto the front.
            inEvents.push_front(std::move(inEvent));

            bool coalesced = false;
            // this looks kinda weird but we don't want to coalesce a
            // mouse event and then try to coalesce a key event right after.
            //
            // we also pass the whole deque to the coalescing methods
            // even though they only want one event because it should
            // be their responsibility to maintain the correct state
            // of the deque if they process any records in it.
            if (_CoalesceMouseMovedEvents(inEvents))
            {
                coalesced = true;
            }
            else if (_CoalesceRepeatedKeyPressEvents(inEvents))
            {
                coalesced = true;
            }
            if (coalesced)
            {
                eventsWritten = 1;
                return;
            }
            else
            {
                // We didn't coalesce the event. pull it from the queue again,
                //  to keep the state consistent with the start of this block.
                inEvent = std::move(inEvents.front());
                inEvents.pop_front();
            }
        }
        // At this point, the event was neither coalesced, nor processed by VT.
        _storage.push_back(std::move(inEvent));
        ++eventsWritten;
    }
    if (initiallyEmptyQueue && !_storage.empty())
    {
        setWaitEvent = true;
    }
}

// Routine Description:
// - Checks if the last saved event and the first event of inRecords are
// both MOUSE_MOVED events. If they are, the last saved event is
// updated with the new mouse position and the first event of inRecords is
// dropped.
// Arguments:
// - inRecords - The incoming records to process.
// Return Value:
// true if events were coalesced, false if they were not.
// Note:
// - The size of inRecords must be 1.
// - Coalescing here means updating a record that already exists in
// the buffer with updated values from an incoming event, instead of
// storing the incoming event (which would make the original one
// redundant/out of date with the most current state).
bool InputBuffer::_CoalesceMouseMovedEvents(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& inEvents)
{
    FAIL_FAST_IF(!(inEvents.size() == 1));
    FAIL_FAST_IF(_storage.empty());
    const IInputEvent* const pFirstInEvent = inEvents.front().get();
    const IInputEvent* const pLastStoredEvent = _storage.back().get();
    if (pFirstInEvent->EventType() == InputEventType::MouseEvent &&
        pLastStoredEvent->EventType() == InputEventType::MouseEvent)
    {
        const MouseEvent* const pInMouseEvent = static_cast<const MouseEvent* const>(pFirstInEvent);
        const MouseEvent* const pLastMouseEvent = static_cast<const MouseEvent* const>(pLastStoredEvent);

        if (pInMouseEvent->IsMouseMoveEvent() &&
            pLastMouseEvent->IsMouseMoveEvent())
        {
            // update mouse moved position
            MouseEvent* const pMouseEvent = static_cast<MouseEvent* const>(_storage.back().release());
            pMouseEvent->SetPosition(pInMouseEvent->GetPosition());
            std::unique_ptr<IInputEvent> tempPtr(pMouseEvent);
            tempPtr.swap(_storage.back());

            inEvents.pop_front();
            return true;
        }
    }
    return false;
}

// Routine Description:
// - checks two KeyEvents to see if they're similiar enough to be coalesced
// Arguments:
// - a - the first KeyEvent
// - b - the other KeyEvent
// Return Value:
// - true if the events could be coalesced, false otherwise
bool InputBuffer::_CanCoalesce(const KeyEvent& a, const KeyEvent& b) const noexcept
{
    if (WI_IsFlagSet(a.GetActiveModifierKeys(), NLS_IME_CONVERSION) &&
        a.GetCharData() == b.GetCharData() &&
        a.GetActiveModifierKeys() == b.GetActiveModifierKeys())
    {
        return true;
    }
    // other key events check
    else if (a.GetVirtualScanCode() == b.GetVirtualScanCode() &&
             a.GetCharData() == b.GetCharData() &&
             a.GetActiveModifierKeys() == b.GetActiveModifierKeys())
    {
        return true;
    }
    return false;
}

// Routine Description::
// - If the last input event saved and the first input event in inRecords
// are both a keypress down event for the same key, update the repeat
// count of the saved event and drop the first from inRecords.
// Arguments:
// - inRecords - The incoming records to process.
// Return Value:
// true if events were coalesced, false if they were not.
// Note:
// - The size of inRecords must be 1.
// - Coalescing here means updating a record that already exists in
// the buffer with updated values from an incoming event, instead of
// storing the incoming event (which would make the original one
// redundant/out of date with the most current state).
bool InputBuffer::_CoalesceRepeatedKeyPressEvents(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& inEvents)
{
    FAIL_FAST_IF(!(inEvents.size() == 1));
    FAIL_FAST_IF(_storage.empty());
    const IInputEvent* const pFirstInEvent = inEvents.front().get();
    const IInputEvent* const pLastStoredEvent = _storage.back().get();
    if (pFirstInEvent->EventType() == InputEventType::KeyEvent &&
        pLastStoredEvent->EventType() == InputEventType::KeyEvent)
    {
        const KeyEvent* const pInKeyEvent = static_cast<const KeyEvent* const>(pFirstInEvent);
        const KeyEvent* const pLastKeyEvent = static_cast<const KeyEvent* const>(pLastStoredEvent);

        if (pInKeyEvent->IsKeyDown() &&
            pLastKeyEvent->IsKeyDown() &&
            !IsGlyphFullWidth(pInKeyEvent->GetCharData()) &&
            _CanCoalesce(*pInKeyEvent, *pLastKeyEvent))
        {
            // increment repeat count
            KeyEvent* const pKeyEvent = static_cast<KeyEvent* const>(_storage.back().release());
            WORD repeatCount = pKeyEvent->GetRepeatCount() + pInKeyEvent->GetRepeatCount();
            pKeyEvent->SetRepeatCount(repeatCount);
            std::unique_ptr<IInputEvent> tempPtr(pKeyEvent);
            tempPtr.swap(_storage.back());

            inEvents.pop_front();
            return true;
        }
    }
    return false;
}

// Routine Description:
// - Handles records that suspend/resume the console.
// Arguments:
// - records - records to check for pause/unpause events
// Return Value:
// - None
// Note:
// - The console lock must be held when calling this routine.
// - will throw exception on error
void InputBuffer::_HandleConsoleSuspensionEvents(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& inEvents)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    std::deque<std::unique_ptr<IInputEvent>> outEvents;
    while (!inEvents.empty())
    {
        std::unique_ptr<IInputEvent> currEvent = std::move(inEvents.front());
        inEvents.pop_front();
        if (currEvent->EventType() == InputEventType::KeyEvent)
        {
            const KeyEvent* const pKeyEvent = static_cast<const KeyEvent* const>(currEvent.get());
            if (pKeyEvent->IsKeyDown())
            {
                if (WI_IsFlagSet(gci.Flags, CONSOLE_SUSPENDED) &&
                    !IsSystemKey(pKeyEvent->GetVirtualKeyCode()))
                {
                    UnblockWriteConsole(CONSOLE_OUTPUT_SUSPENDED);
                    continue;
                }
                else if (WI_IsFlagSet(InputMode, ENABLE_LINE_INPUT) && pKeyEvent->IsPauseKey())
                {
                    WI_SetFlag(gci.Flags, CONSOLE_SUSPENDED);
                    continue;
                }
            }
        }
        outEvents.push_back(std::move(currEvent));
    }
    inEvents.swap(outEvents);
}

// Routine Description:
// - Returns true if this input buffer is in VT Input mode.
// Arguments:
// <none>
// Return Value:
// - Returns true if this input buffer is in VT Input mode.
bool InputBuffer::IsInVirtualTerminalInputMode() const
{
    return WI_IsFlagSet(InputMode, ENABLE_VIRTUAL_TERMINAL_INPUT);
}

// Routine Description:
// - Handler for inserting key sequences into the buffer when the terminal emulation layer
//   has determined a key can be converted appropriately into a sequence of inputs
// Arguments:
// - rgInput - Series of input records to insert into the buffer
// - cInput - Length of input records array
// Return Value:
// - <none>
void InputBuffer::_HandleTerminalInputCallback(std::deque<std::unique_ptr<IInputEvent>>& inEvents)
{
    try
    {
        // add all input events to the storage queue
        while (!inEvents.empty())
        {
            std::unique_ptr<IInputEvent> inEvent = std::move(inEvents.front());
            inEvents.pop_front();
            _storage.push_back(std::move(inEvent));
        }
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
    }
}

TerminalInput& InputBuffer::GetTerminalInput()
{
    return _termInput;
}
