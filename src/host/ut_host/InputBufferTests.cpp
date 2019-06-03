// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"
#include "CommonState.hpp"

#include "..\interactivity\inc\ServiceLocator.hpp"
#include "..\types\inc\IInputEvent.hpp"

using namespace WEX::Logging;
using Microsoft::Console::Interactivity::ServiceLocator;

class InputBufferTests
{
    TEST_CLASS(InputBufferTests);

    std::unique_ptr<CommonState> m_state;

    TEST_CLASS_SETUP(ClassSetup)
    {
        m_state = std::make_unique<CommonState>();
        m_state->InitEvents();
        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        WI_ClearFlag(gci.Flags, CONSOLE_OUTPUT_SUSPENDED);
        return true;
    }

    static const size_t RECORD_INSERT_COUNT = 12;

    INPUT_RECORD MakeKeyEvent(BOOL bKeyDown,
                              WORD wRepeatCount,
                              WORD wVirtualKeyCode,
                              WORD wVirtualScanCode,
                              WCHAR UnicodeChar,
                              DWORD dwControlKeyState)
    {
        INPUT_RECORD retval;
        retval.EventType = KEY_EVENT;
        retval.Event.KeyEvent.bKeyDown = bKeyDown;
        retval.Event.KeyEvent.wRepeatCount = wRepeatCount;
        retval.Event.KeyEvent.wVirtualKeyCode = wVirtualKeyCode;
        retval.Event.KeyEvent.wVirtualScanCode = wVirtualScanCode;
        retval.Event.KeyEvent.uChar.UnicodeChar = UnicodeChar;
        retval.Event.KeyEvent.dwControlKeyState = dwControlKeyState;
        return retval;
    }

    TEST_METHOD(CanGetNumberOfReadyEvents)
    {
        InputBuffer inputBuffer;
        INPUT_RECORD record = MakeKeyEvent(true, 1, L'a', 0, L'a', 0);
        VERIFY_IS_GREATER_THAN(inputBuffer.Write(IInputEvent::Create(record)), 0u);
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), 1u);
        // add another event, check again
        INPUT_RECORD record2;
        record2.EventType = MENU_EVENT;
        VERIFY_IS_GREATER_THAN(inputBuffer.Write(IInputEvent::Create(record2)), 0u);
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), 2u);
    }

    TEST_METHOD(CanInsertIntoInputBufferIndividually)
    {
        InputBuffer inputBuffer;
        for (size_t i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            INPUT_RECORD record;
            record.EventType = MENU_EVENT;
            VERIFY_IS_GREATER_THAN(inputBuffer.Write(IInputEvent::Create(record)), 0u);
            VERIFY_ARE_EQUAL(record, inputBuffer._storage.back()->ToInputRecord());
        }
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), RECORD_INSERT_COUNT);
    }

    TEST_METHOD(CanBulkInsertIntoInputBuffer)
    {
        InputBuffer inputBuffer;
        std::deque<std::unique_ptr<IInputEvent>> events;
        INPUT_RECORD record;
        record.EventType = MENU_EVENT;
        for (size_t i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            events.push_back(IInputEvent::Create(record));
        }
        VERIFY_IS_GREATER_THAN(inputBuffer.Write(events), 0u);
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), RECORD_INSERT_COUNT);
        // verify that the events are the same in storage
        for (size_t i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            VERIFY_ARE_EQUAL(inputBuffer._storage[i]->ToInputRecord(), record);
        }
    }

    TEST_METHOD(InputBufferCoalescesMouseEvents)
    {
        InputBuffer inputBuffer;

        INPUT_RECORD mouseRecord;
        mouseRecord.EventType = MOUSE_EVENT;
        mouseRecord.Event.MouseEvent.dwEventFlags = MOUSE_MOVED;

        // add a bunch of mouse event records
        for (size_t i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            mouseRecord.Event.MouseEvent.dwMousePosition.X = static_cast<SHORT>(i + 1);
            mouseRecord.Event.MouseEvent.dwMousePosition.Y = static_cast<SHORT>(i + 1) * 2;
            VERIFY_IS_GREATER_THAN(inputBuffer.Write(IInputEvent::Create(mouseRecord)), 0u);
        }

        // check that they coalesced
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), 1u);
        // check that the mouse position is being updated correctly
        const IInputEvent* const pOutEvent = inputBuffer._storage.front().get();
        const MouseEvent* const pMouseEvent = static_cast<const MouseEvent* const>(pOutEvent);
        VERIFY_ARE_EQUAL(pMouseEvent->GetPosition().X, static_cast<SHORT>(RECORD_INSERT_COUNT));
        VERIFY_ARE_EQUAL(pMouseEvent->GetPosition().Y, static_cast<SHORT>(RECORD_INSERT_COUNT * 2));

        // add a key event and another mouse event to make sure that
        // an event between two mouse events stopped the coalescing.
        INPUT_RECORD keyRecord;
        keyRecord.EventType = KEY_EVENT;
        VERIFY_IS_GREATER_THAN(inputBuffer.Write(IInputEvent::Create(keyRecord)), 0u);
        VERIFY_IS_GREATER_THAN(inputBuffer.Write(IInputEvent::Create(mouseRecord)), 0u);

        // verify
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), 3u);
    }

    TEST_METHOD(InputBufferDoesNotCoalesceBulkMouseEvents)
    {
        Log::Comment(L"The input buffer should not coalesce mouse events if more than one event is sent at a time");

        InputBuffer inputBuffer;
        INPUT_RECORD mouseRecords[RECORD_INSERT_COUNT];
        std::deque<std::unique_ptr<IInputEvent>> events;

        for (size_t i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            mouseRecords[i].EventType = MOUSE_EVENT;
            mouseRecords[i].Event.MouseEvent.dwEventFlags = MOUSE_MOVED;
            events.push_back(IInputEvent::Create(mouseRecords[i]));
        }
        // add an extra event
        events.push_front(IInputEvent::Create(mouseRecords[0]));
        inputBuffer.Flush();
        // send one mouse event to possibly coalesce into later
        VERIFY_IS_GREATER_THAN(inputBuffer.Write(std::move(events.front())), 0u);
        events.pop_front();
        // write the others in bulk
        VERIFY_IS_GREATER_THAN(inputBuffer.Write(events), 0u);
        // no events should have been coalesced
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), RECORD_INSERT_COUNT + 1);
        // check that the events stored match those inserted
        VERIFY_ARE_EQUAL(inputBuffer._storage.front()->ToInputRecord(), mouseRecords[0]);
        for (size_t i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            VERIFY_ARE_EQUAL(inputBuffer._storage[i + 1]->ToInputRecord(), mouseRecords[i]);
        }
    }

    TEST_METHOD(InputBufferCoalescesKeyEvents)
    {
        Log::Comment(L"The input buffer should coalesce identical key events if they are send one at a time");

        InputBuffer inputBuffer;
        INPUT_RECORD record = MakeKeyEvent(true, 1, L'a', 0, L'a', 0);

        // send a bunch of identical events
        inputBuffer.Flush();
        for (size_t i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            VERIFY_IS_GREATER_THAN(inputBuffer.Write(IInputEvent::Create(record)), 0u);
        }

        // all events should have been coalesced into one
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), 1u);

        // the single event should have a repeat count for each
        // coalesced event
        std::unique_ptr<IInputEvent> outEvent;
        VERIFY_SUCCESS_NTSTATUS(inputBuffer.Read(outEvent,
                                                 true,
                                                 false,
                                                 false,
                                                 false));

        VERIFY_ARE_NOT_EQUAL(nullptr, outEvent.get());
        const KeyEvent* const pKeyEvent = static_cast<const KeyEvent* const>(outEvent.get());
        VERIFY_ARE_EQUAL(pKeyEvent->GetRepeatCount(), RECORD_INSERT_COUNT);
    }

    TEST_METHOD(InputBufferDoesNotCoalesceBulkKeyEvents)
    {
        Log::Comment(L"The input buffer should not coalesce key events if more than one event is sent at a time");

        InputBuffer inputBuffer;
        INPUT_RECORD keyRecords[RECORD_INSERT_COUNT];
        std::deque<std::unique_ptr<IInputEvent>> events;

        for (size_t i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            keyRecords[i] = MakeKeyEvent(true, 1, L'a', 0, L'a', 0);
            events.push_back(IInputEvent::Create(keyRecords[i]));
        }
        inputBuffer.Flush();
        // send one key event to possibly coalesce into later
        VERIFY_IS_GREATER_THAN(inputBuffer.Write(IInputEvent::Create(keyRecords[0])), 0u);
        // write the others in bulk
        VERIFY_IS_GREATER_THAN(inputBuffer.Write(events), 0u);
        // no events should have been coalesced
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), RECORD_INSERT_COUNT + 1);
        // check that the events stored match those inserted
        VERIFY_ARE_EQUAL(inputBuffer._storage.front()->ToInputRecord(), keyRecords[0]);
        for (size_t i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            VERIFY_ARE_EQUAL(inputBuffer._storage[i + 1]->ToInputRecord(), keyRecords[i]);
        }
    }

    TEST_METHOD(InputBufferDoesNotCoalesceFullWidthChars)
    {
        InputBuffer inputBuffer;
        WCHAR hiraganaA = 0x3042; // U+3042 hiragana A
        INPUT_RECORD record = MakeKeyEvent(true, 1, hiraganaA, 0, hiraganaA, 0);

        // send a bunch of identical events
        inputBuffer.Flush();
        for (size_t i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            VERIFY_IS_GREATER_THAN(inputBuffer.Write(IInputEvent::Create(record)), 0u);
            VERIFY_ARE_EQUAL(inputBuffer._storage.back()->ToInputRecord(), record);
        }

        // The events shouldn't be coalesced
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), RECORD_INSERT_COUNT);
    }

    TEST_METHOD(CanFlushAllOutput)
    {
        InputBuffer inputBuffer;
        std::deque<std::unique_ptr<IInputEvent>> events;

        // put some events in the buffer so we can remove them
        INPUT_RECORD record;
        record.EventType = MENU_EVENT;
        for (size_t i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            events.push_back(IInputEvent::Create(record));
        }
        VERIFY_IS_GREATER_THAN(inputBuffer.Write(events), 0u);
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), RECORD_INSERT_COUNT);

        // remove them
        inputBuffer.Flush();
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), 0u);
    }

    TEST_METHOD(CanFlushAllButKeys)
    {
        InputBuffer inputBuffer;
        INPUT_RECORD records[RECORD_INSERT_COUNT] = { 0 };
        std::deque<std::unique_ptr<IInputEvent>> inEvents;

        // create alternating mouse and key events
        for (size_t i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            records[i].EventType = (i % 2 == 0) ? MENU_EVENT : KEY_EVENT;
            inEvents.push_back(IInputEvent::Create(records[i]));
        }
        VERIFY_IS_GREATER_THAN(inputBuffer.Write(inEvents), 0u);
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), RECORD_INSERT_COUNT);

        // remove them
        inputBuffer.FlushAllButKeys();
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), RECORD_INSERT_COUNT / 2);

        // make sure that the non key events were the ones removed
        std::deque<std::unique_ptr<IInputEvent>> outEvents;
        size_t amountToRead = RECORD_INSERT_COUNT / 2;
        VERIFY_SUCCESS_NTSTATUS(inputBuffer.Read(outEvents,
                                                 amountToRead,
                                                 false,
                                                 false,
                                                 false,
                                                 false));
        VERIFY_ARE_EQUAL(amountToRead, outEvents.size());

        for (size_t i = 0; i < outEvents.size(); ++i)
        {
            VERIFY_ARE_EQUAL(outEvents[i]->EventType(), InputEventType::KeyEvent);
        }
    }

    TEST_METHOD(CanReadInput)
    {
        InputBuffer inputBuffer;
        INPUT_RECORD records[RECORD_INSERT_COUNT];
        std::deque<std::unique_ptr<IInputEvent>> inEvents;

        // write some input records
        for (unsigned int i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            records[i] = MakeKeyEvent(TRUE, 1, static_cast<WCHAR>(L'A' + i), 0, static_cast<WCHAR>(L'A' + i), 0);
            inEvents.push_back(IInputEvent::Create(records[i]));
        }
        VERIFY_IS_GREATER_THAN(inputBuffer.Write(inEvents), 0u);

        // read them back out
        std::deque<std::unique_ptr<IInputEvent>> outEvents;
        size_t amountToRead = RECORD_INSERT_COUNT;
        VERIFY_SUCCESS_NTSTATUS(inputBuffer.Read(outEvents,
                                                 amountToRead,
                                                 false,
                                                 false,
                                                 false,
                                                 false));
        VERIFY_ARE_EQUAL(amountToRead, outEvents.size());
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), 0u);
        for (size_t i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            VERIFY_ARE_EQUAL(records[i], outEvents[i]->ToInputRecord());
        }
    }

    TEST_METHOD(CanPeekAtEvents)
    {
        InputBuffer inputBuffer;

        // add some events so that we have something to peek at
        INPUT_RECORD records[RECORD_INSERT_COUNT];
        std::deque<std::unique_ptr<IInputEvent>> inEvents;
        for (unsigned int i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            records[i] = MakeKeyEvent(TRUE, 1, static_cast<WCHAR>(L'A' + i), 0, static_cast<WCHAR>(L'A' + i), 0);
            inEvents.push_back(IInputEvent::Create(records[i]));
        }
        VERIFY_IS_GREATER_THAN(inputBuffer.Write(inEvents), 0u);

        // peek at events
        std::deque<std::unique_ptr<IInputEvent>> outEvents;
        size_t amountToRead = RECORD_INSERT_COUNT;
        VERIFY_SUCCESS_NTSTATUS(inputBuffer.Read(outEvents,
                                                 amountToRead,
                                                 true,
                                                 false,
                                                 false,
                                                 false));

        VERIFY_ARE_EQUAL(amountToRead, outEvents.size());
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), RECORD_INSERT_COUNT);
        for (unsigned int i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            VERIFY_ARE_EQUAL(records[i], outEvents[i]->ToInputRecord());
        }
    }

    TEST_METHOD(EmptyingBufferDuringReadSetsResetWaitEvent)
    {
        Log::Comment(L"ResetWaitEvent should be true if a read to the buffer completely empties it");

        InputBuffer inputBuffer;

        // add some events so that we have something to stick in front of
        INPUT_RECORD records[RECORD_INSERT_COUNT];
        std::deque<std::unique_ptr<IInputEvent>> inEvents;
        for (unsigned int i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            records[i] = MakeKeyEvent(TRUE, 1, static_cast<WCHAR>(L'A' + i), 0, static_cast<WCHAR>(L'A' + i), 0);
            inEvents.push_back(IInputEvent::Create(records[i]));
        }
        VERIFY_IS_GREATER_THAN(inputBuffer.Write(inEvents), 0u);

        // read one record, make sure ResetWaitEvent isn't set
        std::deque<std::unique_ptr<IInputEvent>> outEvents;
        size_t eventsRead = 0;
        bool resetWaitEvent = false;
        inputBuffer._ReadBuffer(outEvents,
                                1,
                                eventsRead,
                                false,
                                resetWaitEvent,
                                true,
                                false);
        VERIFY_ARE_EQUAL(eventsRead, 1u);
        VERIFY_IS_FALSE(!!resetWaitEvent);

        // read the rest, resetWaitEvent should be set to true
        outEvents.clear();
        inputBuffer._ReadBuffer(outEvents,
                                RECORD_INSERT_COUNT - 1,
                                eventsRead,
                                false,
                                resetWaitEvent,
                                true,
                                false);
        VERIFY_ARE_EQUAL(eventsRead, RECORD_INSERT_COUNT - 1);
        VERIFY_IS_TRUE(!!resetWaitEvent);
    }

    TEST_METHOD(ReadingDbcsCharsPadsOutputArray)
    {
        Log::Comment(L"During a non-unicode read, the input buffer should count twice for each dbcs key event");

        // write a mouse event, key event, dbcs key event, mouse event
        InputBuffer inputBuffer;
        const unsigned int recordInsertCount = 4;
        INPUT_RECORD inRecords[recordInsertCount];
        inRecords[0].EventType = MOUSE_EVENT;
        inRecords[1] = MakeKeyEvent(TRUE, 1, L'A', 0, L'A', 0);
        inRecords[2] = MakeKeyEvent(TRUE, 1, 0x3042, 0, 0x3042, 0); // U+3042 hiragana A
        inRecords[3].EventType = MOUSE_EVENT;

        std::deque<std::unique_ptr<IInputEvent>> inEvents;
        for (size_t i = 0; i < recordInsertCount; ++i)
        {
            inEvents.push_back(IInputEvent::Create(inRecords[i]));
        }

        inputBuffer.Flush();
        VERIFY_IS_GREATER_THAN(inputBuffer.Write(inEvents), 0u);

        // read them out non-unicode style and compare
        std::deque<std::unique_ptr<IInputEvent>> outEvents;
        size_t eventsRead = 0;
        bool resetWaitEvent = false;
        inputBuffer._ReadBuffer(outEvents,
                                recordInsertCount,
                                eventsRead,
                                false,
                                resetWaitEvent,
                                false,
                                false);
        // the dbcs record should have counted for two elements in
        // the array, making it so that we get less events read
        VERIFY_ARE_EQUAL(eventsRead, recordInsertCount - 1);
        VERIFY_ARE_EQUAL(eventsRead, outEvents.size());
        for (size_t i = 0; i < eventsRead; ++i)
        {
            VERIFY_ARE_EQUAL(outEvents[i]->ToInputRecord(), inRecords[i]);
        }
    }

    TEST_METHOD(CanPrependEvents)
    {
        InputBuffer inputBuffer;

        // add some events so that we have something to stick in front of
        INPUT_RECORD records[RECORD_INSERT_COUNT];
        std::deque<std::unique_ptr<IInputEvent>> inEvents;
        for (unsigned int i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            records[i] = MakeKeyEvent(TRUE, 1, static_cast<WCHAR>(L'A' + i), 0, static_cast<WCHAR>(L'A' + i), 0);
            inEvents.push_back(IInputEvent::Create(records[i]));
        }
        VERIFY_IS_GREATER_THAN(inputBuffer.Write(inEvents), 0u);

        // prepend some other events
        inEvents.clear();
        INPUT_RECORD prependRecords[RECORD_INSERT_COUNT];
        for (unsigned int i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            prependRecords[i] = MakeKeyEvent(TRUE, 1, static_cast<WCHAR>(L'a' + i), 0, static_cast<WCHAR>(L'a' + i), 0);
            inEvents.push_back(IInputEvent::Create(prependRecords[i]));
        }
        size_t eventsWritten = inputBuffer.Prepend(inEvents);
        VERIFY_ARE_EQUAL(eventsWritten, RECORD_INSERT_COUNT);

        // grab the first set of events and ensure they match prependRecords
        std::deque<std::unique_ptr<IInputEvent>> outEvents;
        size_t amountToRead = RECORD_INSERT_COUNT;
        VERIFY_SUCCESS_NTSTATUS(inputBuffer.Read(outEvents,
                                                 amountToRead,
                                                 false,
                                                 false,
                                                 false,
                                                 false));
        VERIFY_ARE_EQUAL(amountToRead, outEvents.size());
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), RECORD_INSERT_COUNT);
        for (unsigned int i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            VERIFY_ARE_EQUAL(prependRecords[i], outEvents[i]->ToInputRecord());
        }

        outEvents.clear();
        // verify the rest of the records
        VERIFY_SUCCESS_NTSTATUS(inputBuffer.Read(outEvents,
                                                 amountToRead,
                                                 false,
                                                 false,
                                                 false,
                                                 false));
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), 0u);
        VERIFY_ARE_EQUAL(amountToRead, outEvents.size());
        for (unsigned int i = 0; i < RECORD_INSERT_COUNT; ++i)
        {
            VERIFY_ARE_EQUAL(records[i], outEvents[i]->ToInputRecord());
        }
    }

    TEST_METHOD(CanReinitializeInputBuffer)
    {
        InputBuffer inputBuffer;
        DWORD originalInputMode = inputBuffer.InputMode;

        // change the buffer's state a bit
        INPUT_RECORD record;
        record.EventType = MENU_EVENT;
        VERIFY_IS_GREATER_THAN(inputBuffer.Write(IInputEvent::Create(record)), 0u);
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), 1u);
        inputBuffer.InputMode = 0x0;
        inputBuffer.ReinitializeInputBuffer();

        // check that the changes were reverted
        VERIFY_ARE_EQUAL(originalInputMode, inputBuffer.InputMode);
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), 0u);
    }

    TEST_METHOD(HandleConsoleSuspensionEventsRemovesPauseKeys)
    {
        const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        InputBuffer inputBuffer;
        INPUT_RECORD pauseRecord = MakeKeyEvent(true, 1, VK_PAUSE, 0, 0, 0);

        // make sure we aren't currently paused and have an empty buffer
        VERIFY_IS_FALSE(WI_IsFlagSet(gci.Flags, CONSOLE_OUTPUT_SUSPENDED));
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), 0u);

        VERIFY_ARE_EQUAL(inputBuffer.Write(IInputEvent::Create(pauseRecord)), 0u);

        // we should now be paused and the input record should be discarded
        VERIFY_IS_TRUE(WI_IsFlagSet(gci.Flags, CONSOLE_OUTPUT_SUSPENDED));
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), 0u);

        // the next key press should unpause us but be discarded
        INPUT_RECORD unpauseRecord = MakeKeyEvent(true, 1, L'a', 0, L'a', 0);
        VERIFY_ARE_EQUAL(inputBuffer.Write(IInputEvent::Create(unpauseRecord)), 0u);

        VERIFY_IS_FALSE(WI_IsFlagSet(gci.Flags, CONSOLE_OUTPUT_SUSPENDED));
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), 0u);
    }

    TEST_METHOD(SystemKeysDontUnpauseConsole)
    {
        const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        InputBuffer inputBuffer;
        INPUT_RECORD pauseRecord = MakeKeyEvent(true, 1, VK_PAUSE, 0, 0, 0);

        // make sure we aren't currently paused and have an empty buffer
        VERIFY_IS_FALSE(WI_IsFlagSet(gci.Flags, CONSOLE_OUTPUT_SUSPENDED));
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), 0u);

        // pause the screen
        VERIFY_ARE_EQUAL(inputBuffer.Write(IInputEvent::Create(pauseRecord)), 0u);

        // we should now be paused and the input record should be discarded
        VERIFY_IS_TRUE(WI_IsFlagSet(gci.Flags, CONSOLE_OUTPUT_SUSPENDED));
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), 0u);

        // sending a system key event should not stop the pause and
        // the record should be stored in the input buffer
        INPUT_RECORD systemRecord = MakeKeyEvent(true, 1, VK_CONTROL, 0, 0, 0);
        VERIFY_IS_GREATER_THAN(inputBuffer.Write(IInputEvent::Create(systemRecord)), 0u);

        VERIFY_IS_TRUE(WI_IsFlagSet(gci.Flags, CONSOLE_OUTPUT_SUSPENDED));
        VERIFY_ARE_EQUAL(inputBuffer.GetNumberOfReadyEvents(), 1u);

        std::deque<std::unique_ptr<IInputEvent>> outEvents;
        size_t amountToRead = 2;
        VERIFY_SUCCESS_NTSTATUS(inputBuffer.Read(outEvents,
                                                 amountToRead,
                                                 true,
                                                 false,
                                                 false,
                                                 false));
    }

    TEST_METHOD(WritingToEmptyBufferSignalsWaitEvent)
    {
        InputBuffer inputBuffer;
        INPUT_RECORD record = MakeKeyEvent(true, 1, L'a', 0, L'a', 0);
        std::unique_ptr<IInputEvent> inputEvent = IInputEvent::Create(record);
        size_t eventsWritten;
        bool waitEvent = false;
        inputBuffer.Flush();
        // write one event to an empty buffer
        std::deque<std::unique_ptr<IInputEvent>> storage;
        storage.push_back(std::move(inputEvent));
        inputBuffer._WriteBuffer(storage, eventsWritten, waitEvent);
        VERIFY_IS_TRUE(waitEvent);
        // write another, it shouldn't signal this time
        INPUT_RECORD record2 = MakeKeyEvent(true, 1, L'b', 0, L'b', 0);
        std::unique_ptr<IInputEvent> inputEvent2 = IInputEvent::Create(record2);
        // write another event to a non-empty buffer
        waitEvent = false;
        storage.push_back(std::move(inputEvent2));
        inputBuffer._WriteBuffer(storage, eventsWritten, waitEvent);

        VERIFY_IS_FALSE(waitEvent);
    }

    TEST_METHOD(StreamReadingDeCoalesces)
    {
        InputBuffer inputBuffer;
        const WORD repeatCount = 5;
        INPUT_RECORD record = MakeKeyEvent(true, repeatCount, L'a', 0, L'a', 0);
        std::deque<std::unique_ptr<IInputEvent>> outEvents;

        VERIFY_ARE_EQUAL(inputBuffer.Write(IInputEvent::Create(record)), 1u);
        VERIFY_SUCCESS_NTSTATUS(inputBuffer.Read(outEvents,
                                                 1,
                                                 false,
                                                 false,
                                                 true,
                                                 true));
        VERIFY_ARE_EQUAL(outEvents.size(), 1u);
        VERIFY_ARE_EQUAL(inputBuffer._storage.size(), 1u);
        VERIFY_ARE_EQUAL(static_cast<const KeyEvent&>(*inputBuffer._storage.front()).GetRepeatCount(), repeatCount - 1);
        VERIFY_ARE_EQUAL(static_cast<const KeyEvent&>(*outEvents.front()).GetRepeatCount(), 1u);
    }

    TEST_METHOD(StreamPeekingDeCoalesces)
    {
        InputBuffer inputBuffer;
        const WORD repeatCount = 5;
        INPUT_RECORD record = MakeKeyEvent(true, repeatCount, L'a', 0, L'a', 0);
        std::deque<std::unique_ptr<IInputEvent>> outEvents;

        VERIFY_ARE_EQUAL(inputBuffer.Write(IInputEvent::Create(record)), 1u);
        VERIFY_SUCCESS_NTSTATUS(inputBuffer.Read(outEvents,
                                                 1,
                                                 true,
                                                 false,
                                                 true,
                                                 true));
        VERIFY_ARE_EQUAL(outEvents.size(), 1u);
        VERIFY_ARE_EQUAL(inputBuffer._storage.size(), 1u);
        VERIFY_ARE_EQUAL(static_cast<const KeyEvent&>(*inputBuffer._storage.front()).GetRepeatCount(), repeatCount);
        VERIFY_ARE_EQUAL(static_cast<const KeyEvent&>(*outEvents.front()).GetRepeatCount(), 1u);
    }
};
