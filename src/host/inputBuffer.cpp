// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inputBuffer.hpp"

#include <til/bytes.h>
#include <til/unicode.h>

#include "misc.h"
#include "stream.h"
#include "../interactivity/inc/ServiceLocator.hpp"
#include "../types/inc/GlyphWidth.hpp"

#pragma warning(disable : 4100)

using Microsoft::Console::Interactivity::ServiceLocator;
using Microsoft::Console::VirtualTerminal::TerminalInput;
using namespace Microsoft::Console;

// Routine Description:
// - This method creates an input buffer.
// Arguments:
// - None
// Return Value:
// - A new instance of InputBuffer
InputBuffer::InputBuffer()
{
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
    return 0;
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
    return 0;
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
}

static void transfer(InputBuffer::RecordVec& in, std::span<INPUT_RECORD> out)
{
    const auto count = std::min(in.size(), out.size());
    std::copy_n(in.begin(), count, out.begin());
    if (count == out.size())
    {
        in.clear();
    }
    else
    {
        in.erase(in.begin(), in.begin() + count);
    }
}

static void transfer(InputBuffer::RecordVec& in, std::span<wchar_t> out)
{
    size_t inUsed = 0;
    size_t outUsed = 0;

    for (auto& r : in)
    {
        if (outUsed == out.size())
        {
            break;
        }

        if (r.EventType == KEY_EVENT && r.Event.KeyEvent.uChar.UnicodeChar != 0)
        {
            out[outUsed++] = r.Event.KeyEvent.uChar.UnicodeChar;
        }

        inUsed++;
    }
}

static void transfer(InputBuffer::TextVec& in, std::span<INPUT_RECORD> out)
{
    // TODO: MSFT 14150722 - can these const values be generated at
    // runtime without breaking compatibility?
    static constexpr WORD altScanCode = 0x38;
    static constexpr WORD leftShiftScanCode = 0x2A;

    size_t inUsed = 0;
    size_t outUsed = 0;

    for (const auto wch : in)
    {
        if (outUsed + 4 > out.size())
        {
            break;
        }

        const auto keyState = OneCoreSafeVkKeyScanW(wch);
        const auto vk = LOBYTE(keyState);
        const auto sc = gsl::narrow<WORD>(OneCoreSafeMapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
        // The caller provides us with the result of VkKeyScanW() in keyState.
        // The magic constants below are the expected (documented) return values from VkKeyScanW().
        const auto modifierState = HIBYTE(keyState);
        const auto shiftSet = WI_IsFlagSet(modifierState, 1);
        const auto ctrlSet = WI_IsFlagSet(modifierState, 2);
        const auto altSet = WI_IsFlagSet(modifierState, 4);
        const auto altGrSet = WI_AreAllFlagsSet(modifierState, 4 | 2);

        if (altGrSet)
        {
            out[outUsed++] = SynthesizeKeyEvent(true, 1, VK_MENU, altScanCode, 0, ENHANCED_KEY | LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED);
        }
        else if (shiftSet)
        {
            out[outUsed++] = SynthesizeKeyEvent(true, 1, VK_SHIFT, leftShiftScanCode, 0, SHIFT_PRESSED);
        }

        auto keyEvent = SynthesizeKeyEvent(true, 1, vk, sc, wch, 0);
        WI_SetFlagIf(keyEvent.Event.KeyEvent.dwControlKeyState, SHIFT_PRESSED, shiftSet);
        WI_SetFlagIf(keyEvent.Event.KeyEvent.dwControlKeyState, LEFT_CTRL_PRESSED, ctrlSet);
        WI_SetFlagIf(keyEvent.Event.KeyEvent.dwControlKeyState, RIGHT_ALT_PRESSED, altSet);

        out[outUsed++] = keyEvent;
        keyEvent.Event.KeyEvent.bKeyDown = FALSE;
        out[outUsed++] = keyEvent;

        // handle yucky alt-gr keys
        if (altGrSet)
        {
            out[outUsed++] = SynthesizeKeyEvent(false, 1, VK_MENU, altScanCode, 0, ENHANCED_KEY);
        }
        else if (shiftSet)
        {
            out[outUsed++] = SynthesizeKeyEvent(false, 1, VK_SHIFT, leftShiftScanCode, 0, 0);
        }

        inUsed++;
    }
}

static void transfer(InputBuffer::TextVec& in, std::span<wchar_t> out)
{
    const auto count = std::min(in.size(), out.size());
    std::copy_n(in.begin(), count, out.begin());
    if (count == out.size())
    {
        in.clear();
    }
    else
    {
        in.erase(in.begin(), in.begin() + count);
    }
}

size_t InputBuffer::Read(ReadDescriptor desc, void* data, size_t capacityInBytes)
{
    auto remaining = capacityInBytes;
    auto it = _storage.begin();
    const auto end = _storage.end();

    for (; it != end; ++it)
    {
        std::visit(
            [&]<typename T>(T& arg) {
                if constexpr (std::is_same_v<T, RecordVec>)
                {
                    if (desc.records)
                    {
                        transfer(arg, { static_cast<INPUT_RECORD*>(data), capacityInBytes / sizeof(INPUT_RECORD) });
                    }
                    else
                    {
                        transfer(arg, { static_cast<wchar_t*>(data), capacityInBytes / sizeof(wchar_t) });
                    }
                }
                else if constexpr (std::is_same_v<T, TextVec>)
                {
                    if (desc.records)
                    {
                        transfer(arg, { static_cast<INPUT_RECORD*>(data), capacityInBytes / sizeof(INPUT_RECORD) });
                    }
                    else
                    {
                        transfer(arg, { static_cast<wchar_t*>(data), capacityInBytes / sizeof(wchar_t) });
                    }
                }
                else
                {
                    static_assert(sizeof(arg) == 0, "non-exhaustive visitor!");
                }
            },
            *it);
    }

    if (!desc.peek)
    {
        _storage.erase(_storage.begin(), it);
    }

    return capacityInBytes - remaining;
}

void InputBuffer::Write(const INPUT_RECORD& record)
{
    Write(std::span{ &record, 1 });
}

void InputBuffer::Write(const std::span<const INPUT_RECORD>& records)
try
{
    if (records.empty())
    {
        return;
    }

    const auto initiallyEmpty = _storage.empty();

    if (initiallyEmpty || _storage.back().index() != 0)
    {
        _storage.emplace_back(RecordVec{});
    }

    auto& v = *std::get_if<RecordVec>(&_storage.back());
    v.insert(v.end(), records.begin(), records.end());

    if (initiallyEmpty)
    {
        ServiceLocator::LocateGlobals().hInputEvent.SetEvent();
    }
    WakeUpReadersWaitingForData();
}
CATCH_LOG()

void InputBuffer::Write(const std::wstring_view& text)
try
{
    if (text.empty())
    {
        return;
    }

    const auto initiallyEmpty = _storage.empty();

    if (initiallyEmpty || _storage.back().index() != 1)
    {
        _storage.emplace_back(TextVec{});
    }

    auto& v = *std::get_if<TextVec>(&_storage.back());
    v.insert(v.end(), text.begin(), text.end());

    if (initiallyEmpty)
    {
        ServiceLocator::LocateGlobals().hInputEvent.SetEvent();
    }
    WakeUpReadersWaitingForData();
}
CATCH_LOG()

// This can be considered a "privileged" variant of Write() which allows FOCUS_EVENTs to generate focus VT sequences.
// If we didn't do this, someone could write a FOCUS_EVENT_RECORD with WriteConsoleInput, exit without flushing the
// input buffer and the next application will suddenly get a "\x1b[I" sequence in their input. See GH#13238.
void InputBuffer::WriteFocusEvent(bool focused) noexcept
{
    //Write(SynthesizeFocusEvent(focused));
}

// Returns true when mouse input started. You should then capture the mouse and produce further events.
bool InputBuffer::WriteMouseEvent(til::point position, const unsigned int button, const short keyState, const short wheelDelta)
{
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

TerminalInput& InputBuffer::GetTerminalInput()
{
    return _termInput;
}
