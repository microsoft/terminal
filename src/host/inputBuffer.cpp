// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inputBuffer.hpp"

#include "stream.h"
#include "../types/inc/GlyphWidth.hpp"

#include <til/bytes.h>

#include "misc.h"
#include "../interactivity/inc/ServiceLocator.hpp"

#define INPUT_BUFFER_DEFAULT_INPUT_MODE (ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_ECHO_INPUT | ENABLE_MOUSE_INPUT)

using Microsoft::Console::Interactivity::ServiceLocator;
using Microsoft::Console::VirtualTerminal::TerminalInput;
using namespace Microsoft::Console;

// Routine Description:
// - This method creates an input buffer.
// Arguments:
// - None
// Return Value:
// - A new instance of InputBuffer
InputBuffer::InputBuffer() :
    InputMode{ INPUT_BUFFER_DEFAULT_INPUT_MODE },
    WaitQueue{},
    _pTtyConnection(nullptr),
    _termInput(std::bind(&InputBuffer::_HandleTerminalInputCallback, this, std::placeholders::_1))
{
    // The _termInput's constructor takes a reference to this object's _HandleTerminalInputCallback.
    // We need to use std::bind to create a reference to that function without a reference to this InputBuffer

    // initialize buffer header
    fInComposition = false;
}

// Transfer as many `wchar_t`s from source over to the `char`/`wchar_t` buffer `target`. After it returns,
// the start of the `source` and `target` slices will be offset by as many bytes as have been copied
// over, so that if you call this function again it'll continue copying from wherever it left off.
//
// It performs the necessary `WideCharToMultiByte` conversion if `isUnicode` is `false`.
// Since not all converted `char`s might fit into `target` it'll cache the remainder. The next
// time this function is called those cached `char`s will then be the first to be copied over.
void InputBuffer::Consume(bool isUnicode, std::wstring_view& source, std::span<char>& target)
{
    // `_cachedTextReaderA` might still contain target data from a previous invocation.
    // `ConsumeCached` calls `_switchReadingMode` for us.
    ConsumeCached(isUnicode, target);

    if (source.empty() || target.empty())
    {
        return;
    }

    if (isUnicode)
    {
        // The above block should either leave `target` or `_cachedTextReaderW` empty (or both).
        // If we're here, `_cachedTextReaderW` should be empty.
        assert(_cachedTextReaderW.empty());

        til::bytes_transfer(target, source);
    }
    else
    {
        // The above block should either leave `target` or `_cachedTextReaderA` empty (or both).
        // If we're here, `_cachedTextReaderA` should be empty.
        assert(_cachedTextReaderA.empty());

        const auto cp = ServiceLocator::LocateGlobals().getConsoleInformation().CP;

        // Fast path: Batch convert all data in case the user provided buffer is large enough.
        {
            const auto wideLength = gsl::narrow<ULONG>(source.size());
            const auto narrowLength = gsl::narrow<ULONG>(target.size());

            const auto length = WideCharToMultiByte(cp, 0, source.data(), wideLength, target.data(), narrowLength, nullptr, nullptr);
            if (length > 0)
            {
                source = {};
                til::bytes_advance(target, gsl::narrow_cast<size_t>(length));
                return;
            }

            const auto error = GetLastError();
            THROW_HR_IF(HRESULT_FROM_WIN32(error), error != ERROR_INSUFFICIENT_BUFFER);
        }

        // Slow path: Character-wise conversion otherwise. We do this in order to only
        // consume as many characters from `source` as necessary to fill `target`.
        {
            size_t read = 0;

            for (const auto& wch : source)
            {
                char buffer[8];
                const auto length = WideCharToMultiByte(cp, 0, &wch, 1, &buffer[0], sizeof(buffer), nullptr, nullptr);
                THROW_LAST_ERROR_IF(length <= 0);

                std::string_view slice{ &buffer[0], gsl::narrow_cast<size_t>(length) };
                til::bytes_transfer(target, slice);

                ++read;

                if (!slice.empty())
                {
                    _cachedTextA = slice;
                    _cachedTextReaderA = _cachedTextA;
                    break;
                }
            }

            source = source.substr(read);
        }
    }
}

// Same as `Consume`, but without any `source` characters.
void InputBuffer::ConsumeCached(bool isUnicode, std::span<char>& target)
{
    _switchReadingMode(isUnicode ? ReadingMode::StringW : ReadingMode::StringA);

    if (isUnicode)
    {
        if (!_cachedTextReaderW.empty())
        {
            til::bytes_transfer(target, _cachedTextReaderW);

            if (_cachedTextReaderW.empty())
            {
                // This is just so that we release memory eagerly.
                _cachedTextW = std::wstring{};
            }
        }
    }
    else
    {
        if (!_cachedTextReaderA.empty())
        {
            til::bytes_transfer(target, _cachedTextReaderA);

            if (_cachedTextReaderA.empty())
            {
                // This is just so that we release memory eagerly.
                _cachedTextA = std::string{};
            }
        }
    }
}

void InputBuffer::Cache(std::wstring_view source)
{
    const auto off = _cachedTextW.empty() ? 0 : _cachedTextReaderW.data() - _cachedTextW.data();
    _cachedTextW.append(source);
    _cachedTextReaderW = std::wstring_view{ _cachedTextW }.substr(off);
}

// Moves up to `count`, previously cached events into `target`.
size_t InputBuffer::ConsumeCached(bool isUnicode, size_t count, InputEventQueue& target)
{
    _switchReadingMode(isUnicode ? ReadingMode::InputEventsW : ReadingMode::InputEventsA);

    size_t i = 0;

    while (i < count && !_cachedInputEvents.empty())
    {
        target.push_back(std::move(_cachedInputEvents.front()));
        _cachedInputEvents.pop_front();
        i++;
    }

    return i;
}

// Copies up to `count`, previously cached events into `target`.
size_t InputBuffer::PeekCached(bool isUnicode, size_t count, InputEventQueue& target)
{
    _switchReadingMode(isUnicode ? ReadingMode::InputEventsW : ReadingMode::InputEventsA);

    size_t i = 0;

    for (const auto& e : _cachedInputEvents)
    {
        if (i >= count)
        {
            break;
        }

        target.push_back(IInputEvent::Create(e->ToInputRecord()));
        i++;
    }

    return i;
}

// Trims `source` to have a size below or equal to `expectedSourceSize` by
// storing any extra events in `_cachedInputEvents` for later retrieval.
void InputBuffer::Cache(bool isUnicode, InputEventQueue& source, size_t expectedSourceSize)
{
    _switchReadingMode(isUnicode ? ReadingMode::InputEventsW : ReadingMode::InputEventsA);

    if (source.size() > expectedSourceSize)
    {
        _cachedInputEvents.insert(
            _cachedInputEvents.end(),
            std::make_move_iterator(source.begin() + expectedSourceSize),
            std::make_move_iterator(source.end()));
        source.resize(expectedSourceSize);
    }
}

void InputBuffer::_switchReadingMode(ReadingMode mode)
{
    if (_readingMode != mode)
    {
        _switchReadingModeSlowPath(mode);
    }
}

void InputBuffer::_switchReadingModeSlowPath(ReadingMode mode)
{
    _cachedTextA = std::string{};
    _cachedTextReaderA = {};

    _cachedTextW = std::wstring{};
    _cachedTextReaderW = {};

    _cachedInputEvents = std::deque<std::unique_ptr<IInputEvent>>{};

    _readingMode = mode;
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

void InputBuffer::SetTerminalConnection(_In_ Render::VtEngine* const pTtyConnection)
{
    this->_pTtyConnection = pTtyConnection;
}

void InputBuffer::PassThroughWin32MouseRequest(bool enable)
{
    if (_pTtyConnection)
    {
        if (enable)
        {
            LOG_IF_FAILED(_pTtyConnection->WriteTerminalW(L"\x1b[?1003;1006h"));
        }
        else
        {
            LOG_IF_FAILED(_pTtyConnection->WriteTerminalW(L"\x1b[?1003;1006l"));
        }
    }
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
try
{
    assert(OutEvents.empty());

    const auto cp = ServiceLocator::LocateGlobals().getConsoleInformation().CP;

    if (Peek)
    {
        PeekCached(Unicode, AmountToRead, OutEvents);
    }
    else
    {
        ConsumeCached(Unicode, AmountToRead, OutEvents);
    }

    auto it = _storage.begin();
    const auto end = _storage.end();

    while (it != end && OutEvents.size() < AmountToRead)
    {
        auto event = IInputEvent::Create((*it)->ToInputRecord());

        if (event->EventType() == InputEventType::KeyEvent)
        {
            const auto keyEvent = static_cast<KeyEvent*>(event.get());
            WORD repeat = 1;

            // for stream reads we need to split any key events that have been coalesced
            if (Stream)
            {
                repeat = keyEvent->GetRepeatCount();
                keyEvent->SetRepeatCount(1);
            }

            if (Unicode)
            {
                do
                {
                    OutEvents.push_back(std::make_unique<KeyEvent>(*keyEvent));
                    repeat--;
                } while (repeat > 0 && OutEvents.size() < AmountToRead);
            }
            else
            {
                const auto wch = keyEvent->GetCharData();

                char buffer[8];
                const auto length = WideCharToMultiByte(cp, 0, &wch, 1, &buffer[0], sizeof(buffer), nullptr, nullptr);
                THROW_LAST_ERROR_IF(length <= 0);

                const std::string_view str{ &buffer[0], gsl::narrow_cast<size_t>(length) };

                do
                {
                    for (const auto& ch : str)
                    {
                        auto tempEvent = std::make_unique<KeyEvent>(*keyEvent);
                        tempEvent->SetCharData(ch);
                        OutEvents.push_back(std::move(tempEvent));
                    }
                    repeat--;
                } while (repeat > 0 && OutEvents.size() < AmountToRead);
            }

            if (repeat && !Peek)
            {
                const auto originalKeyEvent = static_cast<KeyEvent*>((*it).get());
                originalKeyEvent->SetRepeatCount(repeat);
                break;
            }
        }
        else
        {
            OutEvents.push_back(std::move(event));
        }

        ++it;
    }

    if (!Peek)
    {
        _storage.erase(_storage.begin(), it);
    }

    Cache(Unicode, OutEvents, AmountToRead);

    if (OutEvents.empty())
    {
        return WaitForData ? CONSOLE_STATUS_WAIT : STATUS_SUCCESS;
    }
    if (_storage.empty())
    {
        ServiceLocator::LocateGlobals().hInputEvent.ResetEvent();
    }
    return STATUS_SUCCESS;
}
catch (...)
{
    return NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
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
        _vtInputShouldSuppress = true;
        auto resetVtInputSuppress = wil::scope_exit([&]() { _vtInputShouldSuppress = false; });
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
        auto unusedWaitStatus = false;

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
        _vtInputShouldSuppress = true;
        auto resetVtInputSuppress = wil::scope_exit([&]() { _vtInputShouldSuppress = false; });
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

// This can be considered a "privileged" variant of Write() which allows FOCUS_EVENTs to generate focus VT sequences.
// If we didn't do this, someone could write a FOCUS_EVENT_RECORD with WriteConsoleInput, exit without flushing the
// input buffer and the next application will suddenly get a "\x1b[I" sequence in their input. See GH#13238.
void InputBuffer::WriteFocusEvent(bool focused) noexcept
{
    if (IsInVirtualTerminalInputMode())
    {
        _termInput.HandleFocus(focused);
    }
    else
    {
        // This is a mini-version of Write().
        const auto wasEmpty = _storage.empty();
        _storage.push_back(std::make_unique<FocusEvent>(focused));
        if (wasEmpty)
        {
            ServiceLocator::LocateGlobals().hInputEvent.SetEvent();
        }
        WakeUpReadersWaitingForData();
    }
}

// Returns true when mouse input started. You should then capture the mouse and produce further events.
bool InputBuffer::WriteMouseEvent(til::point position, const unsigned int button, const short keyState, const short wheelDelta)
{
    if (IsInVirtualTerminalInputMode())
    {
        // This magic flag is "documented" at https://msdn.microsoft.com/en-us/library/windows/desktop/ms646301(v=vs.85).aspx
        // "If the high-order bit is 1, the key is down; otherwise, it is up."
        static constexpr short KeyPressed{ gsl::narrow_cast<short>(0x8000) };

        const TerminalInput::MouseButtonState state{
            WI_IsFlagSet(OneCoreSafeGetKeyState(VK_LBUTTON), KeyPressed),
            WI_IsFlagSet(OneCoreSafeGetKeyState(VK_MBUTTON), KeyPressed),
            WI_IsFlagSet(OneCoreSafeGetKeyState(VK_RBUTTON), KeyPressed)
        };

        // GH#6401: VT applications should be able to receive mouse events from outside the
        // terminal buffer. This is likely to happen when the user drags the cursor offscreen.
        // We shouldn't throw away perfectly good events when they're offscreen, so we just
        // clamp them to be within the range [(0, 0), (W, H)].
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        gci.GetActiveOutputBuffer().GetViewport().ToOrigin().Clamp(position);

        return _termInput.HandleMouse(position, button, keyState, wheelDelta, state);
    }

    return false;
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
    const auto initiallyEmptyQueue = _storage.empty();
    const auto initialInEventsSize = inEvents.size();
    const auto vtInputMode = IsInVirtualTerminalInputMode();

    while (!inEvents.empty())
    {
        // Pop the next event.
        // If we're in vt mode, try and handle it with the vt input module.
        // If it was handled, do nothing else for it.
        // If there was one event passed in, try coalescing it with the previous event currently in the buffer.
        // If it's not coalesced, append it to the buffer.
        auto inEvent = std::move(inEvents.front());
        inEvents.pop_front();
        if (vtInputMode)
        {
            // GH#11682: TerminalInput::HandleKey can handle both KeyEvents and Focus events seamlessly
            const auto handled = _termInput.HandleKey(inEvent.get());
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

            auto coalesced = false;
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
        const auto pInMouseEvent = static_cast<const MouseEvent* const>(pFirstInEvent);
        const auto pLastMouseEvent = static_cast<const MouseEvent* const>(pLastStoredEvent);

        if (pInMouseEvent->IsMouseMoveEvent() &&
            pLastMouseEvent->IsMouseMoveEvent())
        {
            // update mouse moved position
            const auto pMouseEvent = static_cast<MouseEvent* const>(_storage.back().release());
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
// - checks two KeyEvents to see if they're similar enough to be coalesced
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
        const auto pInKeyEvent = static_cast<const KeyEvent* const>(pFirstInEvent);
        const auto pLastKeyEvent = static_cast<const KeyEvent* const>(pLastStoredEvent);

        if (pInKeyEvent->IsKeyDown() &&
            pLastKeyEvent->IsKeyDown() &&
            !IsGlyphFullWidth(pInKeyEvent->GetCharData()) &&
            _CanCoalesce(*pInKeyEvent, *pLastKeyEvent))
        {
            // increment repeat count
            const auto pKeyEvent = static_cast<KeyEvent* const>(_storage.back().release());
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
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    std::deque<std::unique_ptr<IInputEvent>> outEvents;
    while (!inEvents.empty())
    {
        auto currEvent = std::move(inEvents.front());
        inEvents.pop_front();
        if (currEvent->EventType() == InputEventType::KeyEvent)
        {
            const auto pKeyEvent = static_cast<const KeyEvent* const>(currEvent.get());
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
// - inEvents - Series of input records to insert into the buffer
// Return Value:
// - <none>
void InputBuffer::_HandleTerminalInputCallback(std::deque<std::unique_ptr<IInputEvent>>& inEvents)
{
    try
    {
        // add all input events to the storage queue
        while (!inEvents.empty())
        {
            auto inEvent = std::move(inEvents.front());
            inEvents.pop_front();
            _storage.push_back(std::move(inEvent));
        }

        if (!_vtInputShouldSuppress)
        {
            ServiceLocator::LocateGlobals().hInputEvent.SetEvent();
            WakeUpReadersWaitingForData();
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
