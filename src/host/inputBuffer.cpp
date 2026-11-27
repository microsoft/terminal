// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inputBuffer.hpp"

#include <til/bytes.h>
#include <til/unicode.h>

#include "misc.h"
#include "stream.h"
#include "../interactivity/inc/ServiceLocator.hpp"

#define INPUT_BUFFER_DEFAULT_INPUT_MODE (ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_ECHO_INPUT | ENABLE_MOUSE_INPUT)

using Microsoft::Console::Interactivity::CharToKeyEvents;
using Microsoft::Console::Interactivity::CharToTypedChar;
using Microsoft::Console::Interactivity::ServiceLocator;
using Microsoft::Console::Interactivity::SynthesizeNumpadEvents;
using Microsoft::Console::Interactivity::TypedChar;
using Microsoft::Console::VirtualTerminal::TerminalInput;
using namespace Microsoft::Console;

// Routine Description:
// - This method creates an input buffer.
// Arguments:
// - None
// Return Value:
// - A new instance of InputBuffer
InputBuffer::InputBuffer() :
    InputMode{ INPUT_BUFFER_DEFAULT_INPUT_MODE }
{
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

            for (const auto& s : til::utf16_iterator{ source })
            {
                char buffer[8];
                const auto length = WideCharToMultiByte(cp, 0, s.data(), gsl::narrow_cast<int>(s.size()), &buffer[0], sizeof(buffer), nullptr, nullptr);
                THROW_LAST_ERROR_IF(length <= 0);

                std::string_view slice{ &buffer[0], gsl::narrow_cast<size_t>(length) };
                til::bytes_transfer(target, slice);

                ++read;

                // The _cached members store characters in `target`'s encoding that didn't fit into
                // the client's buffer. So, if slice.empty() == false, then we'll store `slice` there.
                //
                // But it would be incorrect to test for slice.empty() == false, because the exit
                // condition is actually "if the target has no space left" and that's subtly different.
                // This difference can be seen when `source` contains "abc" and `target` is 1 character large.
                // Testing for `target.empty() will ensure we:
                // * exit right after copying "a"
                // * don't store anything in `_cachedTextA`
                // * leave "bc" in the `source` string, for the caller to handle
                // Otherwise we'll copy "a", store "b" and return "c", which is wrong. See GH#16223.
                if (target.empty())
                {
                    if (!slice.empty())
                    {
                        _cachedTextA = slice;
                        _cachedTextReaderA = _cachedTextA;
                    }
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

        target.push_back(e);
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

    _cachedInputEvents = std::deque<INPUT_RECORD>{};

    _readingMode = mode;
}

// Routine Description:
// - checks if any partial char data is available for writing
// operation.
// Arguments:
// - None
// Return Value:
// - true if partial char data is available, false otherwise
bool InputBuffer::IsWritePartialByteSequenceAvailable() const noexcept
{
    return _writePartialByteSequenceAvailable;
}

// Routine Description:
// - writes any write partial char data available
// Arguments:
// - peek - if true, data will not be removed after being fetched
// Return Value:
// - the partial char data. may be nullptr if no data is available
const INPUT_RECORD& InputBuffer::FetchWritePartialByteSequence() noexcept
{
    _writePartialByteSequenceAvailable = false;
    return _writePartialByteSequence;
}

// Routine Description:
// - stores partial write char data. will overwrite any previously
// stored data.
// Arguments:
// - event - The event to store
// Return Value:
// - None
void InputBuffer::StoreWritePartialByteSequence(const INPUT_RECORD& event) noexcept
{
    _writePartialByteSequenceAvailable = true;
    _writePartialByteSequence = event;
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
    return _storage.events;
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
    auto& s = _storage;
    size_t read = 0;
    size_t write = 0;

    // Characters are all key events, so only runs of INPUT_RECORDs need filtering.
    for (size_t i = 0; i < s.runs.size(); ++i)
    {
        auto& run = s.runs[i];
        if (run.kind != RunKind::Records)
        {
            continue;
        }

        const auto end = read + run.count;
        for (; read < end; ++read)
        {
            const auto event = s.records[read];
            if (event.EventType == KEY_EVENT)
            {
                s.records[write++] = event;
            }
            else
            {
                run.count--;
                s.events--;
            }
        }
    }

    s.records.erase(write, s.records.size() - write);

    // Drop the runs that are now empty and merge the neighbors that are left with the same kind.
    write = 0;
    for (size_t i = 0; i < s.runs.size(); ++i)
    {
        const auto run = s.runs[i];
        if (run.count == 0)
        {
            continue;
        }
        if (write != 0 && s.runs[write - 1].kind == run.kind)
        {
            s.runs[write - 1].count += run.count;
            continue;
        }
        s.runs[write++] = run;
    }

    s.runs.erase(write, s.runs.size() - write);
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
[[nodiscard]] NTSTATUS InputBuffer::Read(_Out_ InputEventQueue& OutEvents,
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

    // Copies the event into OutEvents and returns how much of its repeat count is left.
    // Only stream reads split up repeat counts, so for all others the result is always 0.
    const auto readEvent = [&](const INPUT_RECORD& stored) -> WORD {
        if (stored.EventType != KEY_EVENT)
        {
            OutEvents.push_back(stored);
            return 0;
        }

        auto event = stored;
        WORD repeat = 1;

        // for stream reads we need to split any key events that have been coalesced
        if (Stream)
        {
            repeat = std::max<WORD>(1, event.Event.KeyEvent.wRepeatCount);
            event.Event.KeyEvent.wRepeatCount = 1;
        }

        if (Unicode)
        {
            do
            {
                OutEvents.push_back(event);
                repeat--;
            } while (repeat > 0 && OutEvents.size() < AmountToRead);
        }
        else
        {
            const auto wch = event.Event.KeyEvent.uChar.UnicodeChar;

            char buffer[8];
            const auto length = WideCharToMultiByte(cp, 0, &wch, 1, &buffer[0], sizeof(buffer), nullptr, nullptr);
            THROW_LAST_ERROR_IF(length <= 0);

            const std::string_view str{ &buffer[0], gsl::narrow_cast<size_t>(length) };

            do
            {
                for (const auto& ch : str)
                {
                    // char is signed and assigning it to UnicodeChar would cause sign-extension.
                    // unsigned char doesn't have this problem.
                    event.Event.KeyEvent.uChar.UnicodeChar = std::bit_cast<uint8_t>(ch);
                    OutEvents.push_back(event);
                }
                repeat--;
            } while (repeat > 0 && OutEvents.size() < AmountToRead);
        }

        return repeat;
    };

    auto& s = _storage;

    // Callers usually read thousands of events at a time. Growing OutEvents step by step would copy them repeatedly.
    OutEvents.reserve(std::min(AmountToRead, OutEvents.size() + s.events));

    size_t run = 0; // index into s.runs
    size_t item = 0; // index of the record or character within s.runs[run]
    size_t record = 0; // index into s.records
    size_t character = 0; // index into s.chars
    size_t sub = s.frontEventsRead; // index of the key event within s.chars[character]
    size_t eventsRead = 0;

    while (run < s.runs.size() && OutEvents.size() < AmountToRead)
    {
        const auto& r = s.runs[run];

        if (r.kind == RunKind::Records && Unicode && !Stream)
        {
            // Without stream reads, each record maps 1:1 to OutEvents, so they get copied in bulk.
            const auto count = std::min(r.count - item, AmountToRead - OutEvents.size());
            s.records.copy_to(record, count, OutEvents);
            record += count;
            eventsRead += count;
            item += count;
            if (item == r.count)
            {
                run++;
                item = 0;
            }
            continue;
        }

        if (r.kind == RunKind::Records)
        {
            auto& stored = s.records[record];
            const auto repeat = readEvent(stored);

            if (repeat && !Peek)
            {
                stored.Event.KeyEvent.wRepeatCount = repeat;
                break;
            }

            record++;
            eventsRead++;
        }
        else
        {
            std::array<INPUT_RECORD, 4> events;
            const auto count = _charToEvents(s.chars[character], r.kind, events);
            const auto first = sub;

            if (Unicode && !Stream)
            {
                // Synthesized events have a repeat count of 1, so they map 1:1 to OutEvents.
                const auto end = std::min(count, sub + (AmountToRead - OutEvents.size()));
                for (; sub < end; ++sub)
                {
                    OutEvents.push_back(til::at(events, sub));
                }
            }
            else
            {
                do
                {
                    readEvent(til::at(events, sub));
                    sub++;
                } while (sub < count && OutEvents.size() < AmountToRead);
            }

            eventsRead += sub - first;

            if (sub < count)
            {
                break;
            }

            sub = 0;
            character++;
        }

        if (++item == r.count)
        {
            run++;
            item = 0;
        }
    }

    if (!Peek)
    {
        s.records.erase(0, record);
        s.chars.erase(0, character);
        s.runs.erase(0, run);
        if (!s.runs.empty())
        {
            s.runs.front().count -= item;
        }
        s.frontEventsRead = sub;
        s.events -= eventsRead;
    }

    Cache(Unicode, OutEvents, AmountToRead);

    if (OutEvents.empty())
    {
        return WaitForData ? CONSOLE_STATUS_WAIT : STATUS_SUCCESS;
    }
    if (_storage.events == 0)
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
// -  Writes events to the beginning of the input buffer.
// Arguments:
// - inEvents - events to write to buffer.
// - eventsWritten - The number of events written to the buffer on exit.
// Return Value:
// S_OK if successful.
// Note:
// - The console lock must be held when calling this routine.
size_t InputBuffer::Prepend(const std::span<const INPUT_RECORD>& inEvents)
{
    try
    {
        if (inEvents.empty())
        {
            return STATUS_SUCCESS;
        }

        const auto wakeup = _wakeupReadersOnExit();

        // read all of the records out of the buffer, then write the
        // prepend ones, then write the original set. We need to do it
        // this way to handle any coalescing that might occur.

        // get all of the existing records, "emptying" the buffer
        auto existingStorage = std::exchange(_storage, Storage{});

        // write the prepend records
        size_t prependEventsWritten;
        _WriteBuffer(inEvents, prependEventsWritten);

        _append(existingStorage);

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
size_t InputBuffer::Write(const INPUT_RECORD& inEvent)
{
    return Write(std::span{ &inEvent, 1 });
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
size_t InputBuffer::Write(const std::span<const INPUT_RECORD>& inEvents)
{
    try
    {
        if (inEvents.empty())
        {
            return 0;
        }

        const auto wakeup = _wakeupReadersOnExit();
        size_t EventsWritten;
        _WriteBuffer(inEvents, EventsWritten);
        return EventsWritten;
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
        return 0;
    }
}

void InputBuffer::WriteString(const std::wstring_view& text)
try
{
    if (text.empty())
    {
        return;
    }

    const auto wakeup = _wakeupReadersOnExit();
    _writeString(text);
}
CATCH_LOG()

// Writes the text as if it was typed on the keyboard. This is equivalent to calling
// CharToKeyEvents() for each character and writing the result with Write(), except that
// in the common case it stores the characters as-is. They turn into key events when read.
void InputBuffer::WriteKeyboardText(const std::wstring_view text, const UINT codepage)
try
{
    if (text.empty())
    {
        return;
    }

    const auto wakeup = _wakeupReadersOnExit();
    const auto vtInputMode = IsInVirtualTerminalInputMode();
    InputEventQueue keyEvents;
    size_t eventsWritten = 0;

    if (vtInputMode)
    {
        for (const auto wch : text)
        {
            CharToKeyEvents(wch, codepage, keyEvents);
        }
        _WriteBuffer(keyEvents, eventsWritten);
        return;
    }

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    for (const auto wch : text)
    {
        TypedChar ch;
        if (!CharToTypedChar(wch, ch))
        {
            keyEvents.clear();
            SynthesizeNumpadEvents(wch, codepage, keyEvents);
            for (const auto& event : keyEvents)
            {
                _writeRecord(gci, event, vtInputMode, false, eventsWritten);
            }
            continue;
        }

        // Key presses that suspend or resume the output take effect when they're written,
        // so they go through the same path as INPUT_RECORDs. The shift and AltGr
        // key presses that may surround the character are never pause keys.
        const auto ctrlButNotAlt = (ch.modifiers & (2 | 4)) == 2;
        const auto pauseKey = ch.virtualKey == VK_PAUSE || (ctrlButNotAlt && ch.virtualKey == L'S');
        if (WI_IsFlagSet(gci.Flags, CONSOLE_SUSPENDED) || (WI_IsFlagSet(InputMode, ENABLE_LINE_INPUT) && pauseKey))
        {
            std::array<INPUT_RECORD, 4> events;
            const auto count = TypedCharToKeyEvents(ch, events);
            for (size_t i = 0; i < count; ++i)
            {
                _writeRecord(gci, til::at(events, i), vtInputMode, false, eventsWritten);
            }
            continue;
        }

        _pushChar(ch, RunKind::Typed);
    }
}
CATCH_LOG()

// This can be considered a "privileged" variant of Write() which allows FOCUS_EVENTs to generate focus VT sequences.
// If we didn't do this, someone could write a FOCUS_EVENT_RECORD with WriteConsoleInput, exit without flushing the
// input buffer and the next application will suddenly get a "\x1b[I" sequence in their input. See GH#13238.
void InputBuffer::WriteFocusEvent(bool focused) noexcept
{
    const auto wakeup = _wakeupReadersOnExit();

    if (IsInVirtualTerminalInputMode())
    {
        if (const auto out = _termInput.HandleFocus(focused))
        {
            _writeString(*out);
        }
    }
    else
    {
        _pushRecord(SynthesizeFocusEvent(focused));
    }
}

// Returns true when mouse input started. You should then capture the mouse and produce further events.
bool InputBuffer::WriteMouseEvent(til::point position, const unsigned int button, const short keyState, const short wheelDelta)
{
    const auto wakeup = _wakeupReadersOnExit();

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

        if (const auto out = _termInput.HandleMouse(position, button, keyState, wheelDelta, state))
        {
            _writeString(*out);
            return true;
        }
    }

    return false;
}

// Ctrl-S is traditionally considered an alias for the pause key.
// This returns true if it's either of the two.
static bool IsPauseKey(const KEY_EVENT_RECORD& event)
{
    if (event.wVirtualKeyCode == VK_PAUSE)
    {
        return true;
    }

    const auto ctrlButNotAlt = WI_IsAnyFlagSet(event.dwControlKeyState, CTRL_PRESSED) && WI_AreAllFlagsClear(event.dwControlKeyState, ALT_PRESSED);
    return ctrlButNotAlt && event.wVirtualKeyCode == L'S';
}

void InputBuffer::_wakeupReadersImpl(bool initiallyEmpty)
{
    if (_storage.events != 0)
    {
        // It would be fine to call SetEvent() unconditionally,
        // but technically we only need to ResetEvent() if the buffer is empty,
        // and SetEvent() once it stopped being so, which is what this code does.
        if (initiallyEmpty)
        {
            ServiceLocator::LocateGlobals().hInputEvent.SetEvent();
        }

        WakeUpReadersWaitingForData();
    }
}

// Routine Description:
// - Coalesces input events and transfers them to storage queue.
// Arguments:
// - inRecords - The events to store.
// - eventsWritten - The number of events written since this function
// was called.
// Return Value:
// - None
// Note:
// - The console lock must be held when calling this routine.
// - will throw on failure
void InputBuffer::_WriteBuffer(const std::span<const INPUT_RECORD>& inEvents, _Out_ size_t& eventsWritten)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    eventsWritten = 0;
    const auto vtInputMode = IsInVirtualTerminalInputMode();
    // we only check for possible coalescing when storing one
    // record at a time because this is the original behavior of
    // the input buffer. Changing this behavior may break stuff
    // that was depending on it.
    const auto coalesce = inEvents.size() == 1;

    if (vtInputMode || coalesce)
    {
        for (const auto& inEvent : inEvents)
        {
            _writeRecord(gci, inEvent, vtInputMode, coalesce, eventsWritten);
        }
        return;
    }

    // Without VT input or coalescing, records are stored as-is, except for the key presses
    // that suspend or resume the output. Everything between those gets appended in bulk.
    size_t begin = 0;
    for (size_t i = 0; i < inEvents.size(); ++i)
    {
        const auto& inEvent = til::at(inEvents, i);
        if (_isOutputControlKey(gci, inEvent))
        {
            _pushRecords(inEvents.subspan(begin, i - begin));
            eventsWritten += i - begin;
            _writeRecord(gci, inEvent, vtInputMode, coalesce, eventsWritten);
            begin = i + 1;
        }
    }
    _pushRecords(inEvents.subspan(begin));
    eventsWritten += inEvents.size() - begin;
}

// Returns true if writing this record suspends or resumes the output, instead of storing it.
bool InputBuffer::_isOutputControlKey(const CONSOLE_INFORMATION& gci, const INPUT_RECORD& record) const noexcept
{
    if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
    {
        return false;
    }
    return (WI_IsFlagSet(gci.Flags, CONSOLE_SUSPENDED) && !IsSystemKey(record.Event.KeyEvent.wVirtualKeyCode)) ||
           (WI_IsFlagSet(InputMode, ENABLE_LINE_INPUT) && IsPauseKey(record.Event.KeyEvent));
}

void InputBuffer::_writeRecord(CONSOLE_INFORMATION& gci, const INPUT_RECORD& inEvent, const bool vtInputMode, const bool coalesce, size_t& eventsWritten)
{
    if (inEvent.EventType == KEY_EVENT && inEvent.Event.KeyEvent.bKeyDown)
    {
        // if output is suspended, any keyboard input releases it.
        if (WI_IsFlagSet(gci.Flags, CONSOLE_SUSPENDED) && !IsSystemKey(inEvent.Event.KeyEvent.wVirtualKeyCode))
        {
            UnblockWriteConsole(CONSOLE_OUTPUT_SUSPENDED);
            return;
        }
        // intercept control-s
        if (WI_IsFlagSet(InputMode, ENABLE_LINE_INPUT) && IsPauseKey(inEvent.Event.KeyEvent))
        {
            WI_SetFlag(gci.Flags, CONSOLE_SUSPENDED);
            return;
        }
    }

    // If we're in vt mode, try and handle it with the vt input module.
    // If it was handled, do nothing else for it.
    // If there was one event passed in, try coalescing it with the previous event currently in the buffer.
    // If it's not coalesced, append it to the buffer.
    if (vtInputMode)
    {
        // GH#11682: TerminalInput::HandleKey can handle both KeyEvents and Focus events seamlessly
        if (const auto out = _termInput.HandleKey(inEvent))
        {
            _writeString(*out);
            eventsWritten++;
            return;
        }
    }

    if (coalesce && !_storage.runs.empty() && _CoalesceEvent(inEvent))
    {
        eventsWritten++;
        return;
    }

    // At this point, the event was neither coalesced, nor processed by VT.
    _pushRecord(inEvent);
    eventsWritten++;
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
// redundant/out-of-date with the most current state).
bool InputBuffer::_CoalesceEvent(const INPUT_RECORD& inEvent)
{
    auto& s = _storage;

    switch (s.runs.back().kind)
    {
    case RunKind::Records:
        return _coalesceInto(s.records.back(), inEvent);
    case RunKind::Raw:
    {
        std::array<INPUT_RECORD, 4> events;
        _charToEvents(s.chars.back(), RunKind::Raw, events);
        auto lastEvent = til::at(events, 0);
        if (!_coalesceInto(lastEvent, inEvent))
        {
            return false;
        }

        // A character can't hold a repeat count, so it gets replaced with the coalesced record.
        // Reserving first ensures that we don't lose the character if we run out of memory.
        s.records.reserve_one();
        s.runs.reserve_one();
        s.chars.pop_back();
        if (--s.runs.back().count == 0)
        {
            s.runs.pop_back();
        }
        s.events--;
        _pushRecord(lastEvent);
        return true;
    }
    default:
        // Typed characters end with a key up event, which never coalesces.
        return false;
    }
}

bool InputBuffer::_coalesceInto(INPUT_RECORD& lastEvent, const INPUT_RECORD& inEvent) noexcept
{
    if (lastEvent.EventType == MOUSE_EVENT && inEvent.EventType == MOUSE_EVENT)
    {
        const auto& inMouse = inEvent.Event.MouseEvent;
        auto& lastMouse = lastEvent.Event.MouseEvent;

        if (lastMouse.dwEventFlags == MOUSE_MOVED && inMouse.dwEventFlags == MOUSE_MOVED)
        {
            lastMouse.dwMousePosition = inMouse.dwMousePosition;
            return true;
        }
    }
    else if (lastEvent.EventType == WINDOW_BUFFER_SIZE_EVENT && inEvent.EventType == WINDOW_BUFFER_SIZE_EVENT)
    {
        lastEvent = inEvent;
        return true;
    }
    else if (lastEvent.EventType == KEY_EVENT && inEvent.EventType == KEY_EVENT)
    {
        const auto& inKey = inEvent.Event.KeyEvent;
        auto& lastKey = lastEvent.Event.KeyEvent;

        if (lastKey.bKeyDown && inKey.bKeyDown &&
            (lastKey.wVirtualScanCode == inKey.wVirtualScanCode || WI_IsFlagSet(inKey.dwControlKeyState, NLS_IME_CONVERSION)) &&
            lastKey.uChar.UnicodeChar == inKey.uChar.UnicodeChar &&
            lastKey.dwControlKeyState == inKey.dwControlKeyState &&
            // A single repeat count cannot represent two INPUT_RECORDs simultaneously,
            // and so it cannot represent a surrogate pair either.
            !til::is_surrogate(inKey.uChar.UnicodeChar))
        {
            lastKey.wRepeatCount += inKey.wRepeatCount;
            return true;
        }
    }

    return false;
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

void InputBuffer::_writeString(const std::wstring_view& text)
{
    for (const auto& wch : text)
    {
        TypedChar ch{ wch, 0, 0, 0 };
        if (wch == UNICODE_NULL)
        {
            // Convert null byte back to input event with proper control state
            const auto zeroKey = OneCoreSafeVkKeyScanW(0);
            ch.virtualKey = LOBYTE(zeroKey);
            ch.modifiers = HIBYTE(zeroKey);
        }
        _pushChar(ch, RunKind::Raw);
    }
}

size_t InputBuffer::_charEventCount(const TypedChar& ch, const RunKind kind) noexcept
{
    return kind == RunKind::Typed ? TypedCharEventCount(ch) : 1;
}

// Writes the key events that `ch` expands to into `out` and returns how many there are.
size_t InputBuffer::_charToEvents(const TypedChar& ch, const RunKind kind, std::array<INPUT_RECORD, 4>& out) noexcept
{
    if (kind == RunKind::Typed)
    {
        return TypedCharToKeyEvents(ch, out);
    }

    uint32_t ctrlState = 0;
    WI_SetFlagIf(ctrlState, SHIFT_PRESSED, WI_IsFlagSet(ch.modifiers, 1));
    WI_SetFlagIf(ctrlState, LEFT_CTRL_PRESSED, WI_IsFlagSet(ch.modifiers, 2));
    WI_SetFlagIf(ctrlState, LEFT_ALT_PRESSED, WI_IsFlagSet(ch.modifiers, 4));
    til::at(out, 0) = SynthesizeKeyEvent(true, 1, ch.virtualKey, ch.scanCode, ch.wch, ctrlState);
    return 1;
}

void InputBuffer::_pushRun(const RunKind kind)
{
    auto& runs = _storage.runs;
    if (!runs.empty() && runs.back().kind == kind)
    {
        runs.back().count++;
        return;
    }
    runs.push_back({ 1, kind });
}

void InputBuffer::_pushRecord(const INPUT_RECORD& record)
{
    // Reserve first, so that the records and runs stay in sync if we run out of memory.
    _storage.records.reserve_one();
    _storage.runs.reserve_one();
    _storage.records.push_back(record);
    _pushRun(RunKind::Records);
    _storage.events++;
}

void InputBuffer::_pushRecords(const std::span<const INPUT_RECORD> records)
{
    if (records.empty())
    {
        return;
    }

    auto& s = _storage;
    // Reserve first, so that the records and runs stay in sync if we run out of memory.
    s.records.reserve(records.size());
    s.runs.reserve_one();
    s.records.append(records);
    if (!s.runs.empty() && s.runs.back().kind == RunKind::Records)
    {
        s.runs.back().count += records.size();
    }
    else
    {
        s.runs.push_back({ records.size(), RunKind::Records });
    }
    s.events += records.size();
}

void InputBuffer::_pushChar(const TypedChar& ch, const RunKind kind)
{
    _storage.chars.reserve_one();
    _storage.runs.reserve_one();
    _storage.chars.push_back(ch);
    _pushRun(kind);
    _storage.events += _charEventCount(ch, kind);
}

// Appends the contents of source to the buffer, without coalescing.
void InputBuffer::_append(Storage& source)
{
    size_t record = 0;
    size_t character = 0;

    for (size_t i = 0; i < source.runs.size(); ++i)
    {
        const auto run = source.runs[i];

        for (size_t j = 0; j < run.count; ++j)
        {
            if (run.kind == RunKind::Records)
            {
                _pushRecord(source.records[record++]);
                continue;
            }

            const auto& ch = source.chars[character++];

            // If the first character was partially read already, only its remaining events are left.
            if (i == 0 && j == 0 && source.frontEventsRead != 0)
            {
                std::array<INPUT_RECORD, 4> events;
                const auto count = _charToEvents(ch, run.kind, events);
                for (auto sub = source.frontEventsRead; sub < count; ++sub)
                {
                    _pushRecord(til::at(events, sub));
                }
                continue;
            }

            _pushChar(ch, run.kind);
        }
    }
}

TerminalInput& InputBuffer::GetTerminalInput()
{
    return _termInput;
}
