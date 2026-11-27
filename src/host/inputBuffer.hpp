// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "readData.hpp"
#include "../types/inc/IInputEvent.hpp"

#include "../server/ObjectHandle.h"
#include "../server/ObjectHeader.h"
#include "../terminal/input/terminalInput.hpp"
#include "../interactivity/inc/EventSynthesis.hpp"

#include <bit>
#include <deque>

namespace Microsoft::Console::Render
{
    class Renderer;
}

class InputBuffer final : public ConsoleObjectHeader
{
public:
    DWORD InputMode;
    ConsoleWaitQueue WaitQueue; // formerly ReadWaitQueue

    InputBuffer();

    // String oriented APIs
    void Consume(bool isUnicode, std::wstring_view& source, std::span<char>& target);
    void ConsumeCached(bool isUnicode, std::span<char>& target);
    void Cache(std::wstring_view source);
    // INPUT_RECORD oriented APIs
    size_t ConsumeCached(bool isUnicode, size_t count, InputEventQueue& target);
    size_t PeekCached(bool isUnicode, size_t count, InputEventQueue& target);
    void Cache(bool isUnicode, InputEventQueue& source, size_t expectedSourceSize);

    // storage API for partial dbcs bytes being written to the buffer
    bool IsWritePartialByteSequenceAvailable() const noexcept;
    const INPUT_RECORD& FetchWritePartialByteSequence() noexcept;
    void StoreWritePartialByteSequence(const INPUT_RECORD& event) noexcept;

    void ReinitializeInputBuffer();
    void WakeUpReadersWaitingForData();
    void TerminateRead(_In_ WaitTerminationReason Flag);
    size_t GetNumberOfReadyEvents() const noexcept;
    void Flush();
    void FlushAllButKeys();

    [[nodiscard]] NTSTATUS Read(_Out_ InputEventQueue& OutEvents,
                                const size_t AmountToRead,
                                const bool Peek,
                                const bool WaitForData,
                                const bool Unicode,
                                const bool Stream);

    size_t Prepend(const std::span<const INPUT_RECORD>& inEvents);
    size_t Write(const INPUT_RECORD& inEvent);
    size_t Write(const std::span<const INPUT_RECORD>& inEvents);
    void WriteString(const std::wstring_view& text);
    void WriteKeyboardText(std::wstring_view text, UINT codepage);
    void WriteFocusEvent(bool focused) noexcept;
    bool WriteMouseEvent(til::point position, unsigned int button, short keyState, short wheelDelta);

    bool IsInVirtualTerminalInputMode() const;
    Microsoft::Console::VirtualTerminal::TerminalInput& GetTerminalInput();

private:
    enum class ReadingMode : uint8_t
    {
        StringA,
        StringW,
        InputEventsA,
        InputEventsW,
    };

    std::string _cachedTextA;
    std::string_view _cachedTextReaderA;
    std::wstring _cachedTextW;
    std::wstring_view _cachedTextReaderW;
    std::deque<INPUT_RECORD> _cachedInputEvents;
    ReadingMode _readingMode = ReadingMode::StringA;

    // A ring buffer for trivially copyable types. Its capacity is a power of 2.
    // It returns memory to the OS once it shrinks to a fraction of a large capacity.
    template<typename T>
    class Ring
    {
        static_assert(std::is_trivially_copyable_v<T>);

    public:
        Ring() = default;
        Ring(const Ring&) = delete;
        Ring& operator=(const Ring&) = delete;
        Ring(Ring&& other) noexcept :
            _data{ std::move(other._data) },
            _capacity{ std::exchange(other._capacity, 0) },
            _head{ std::exchange(other._head, 0) },
            _size{ std::exchange(other._size, 0) }
        {
        }
        Ring& operator=(Ring&& other) noexcept
        {
            _data = std::move(other._data);
            _capacity = std::exchange(other._capacity, 0);
            _head = std::exchange(other._head, 0);
            _size = std::exchange(other._size, 0);
            return *this;
        }

        size_t size() const noexcept { return _size; }
        bool empty() const noexcept { return _size == 0; }
        T& operator[](size_t i) noexcept { return til::at(_data, (_head + i) & (_capacity - 1)); }
        const T& operator[](size_t i) const noexcept { return til::at(_data, (_head + i) & (_capacity - 1)); }
        T& front() noexcept { return til::at(*this, 0); }
        T& back() noexcept { return til::at(*this, _size - 1); }

        // Ensures that the next push_back() won't throw.
        void reserve_one()
        {
            if (_size == _capacity)
            {
                _reallocate(std::max<size_t>(16, _capacity * 2));
            }
        }

        // Ensures that the next `count` push_back() calls won't throw.
        void reserve(size_t count)
        {
            if (count > _capacity - _size)
            {
                _reallocate(std::max<size_t>(16, std::bit_ceil(_size + count)));
            }
        }

        void push_back(const T& value)
        {
            const auto copy = value;
            reserve_one();
            til::at(*this, _size) = copy;
            _size++;
        }

        // Appends all values, or none of them if we run out of memory.
        void append(const std::span<const T> values)
        {
            if (values.empty())
            {
                return;
            }
            reserve(values.size());
            const std::span data{ _data.get(), _capacity };
            const auto tail = (_head + _size) & (_capacity - 1);
            const auto first = std::min(values.size(), _capacity - tail);
            std::copy_n(values.begin(), first, data.begin() + tail);
            std::copy_n(values.begin() + first, values.size() - first, data.begin());
            _size += values.size();
        }

        // Appends [first, first + count) to the end of out.
        template<typename Container>
        void copy_to(size_t first, size_t count, Container& out) const
        {
            if (count == 0)
            {
                return;
            }
            const std::span<const T> data{ _data.get(), _capacity };
            const auto begin = (_head + first) & (_capacity - 1);
            const auto contiguous = std::min(count, _capacity - begin);
            out.insert(out.end(), data.begin() + begin, data.begin() + begin + contiguous);
            out.insert(out.end(), data.begin(), data.begin() + (count - contiguous));
        }

        void pop_back() noexcept
        {
            _size--;
        }

        // Removes [first, first + count). Erasing at the front just advances the head.
        void erase(size_t first, size_t count) noexcept
        {
            if (count == 0)
            {
                return;
            }
            if (first == 0)
            {
                _head = (_head + count) & (_capacity - 1);
            }
            else
            {
                for (auto i = first + count; i < _size; ++i)
                {
                    til::at(*this, i - count) = til::at(*this, i);
                }
            }
            _size -= count;
            _shrink();
        }

        void clear() noexcept
        {
            _head = 0;
            _size = 0;
            _shrink();
        }

    private:
        // Capacity we keep around for reuse, so steady-state input doesn't reallocate.
        static constexpr size_t retainCapacity = std::bit_floor(64 * 1024 / sizeof(T));

        void _reallocate(size_t capacity)
        {
            auto data = std::make_unique_for_overwrite<T[]>(capacity);
            for (size_t i = 0; i < _size; ++i)
            {
                til::at(data, i) = til::at(*this, i);
            }
            _data = std::move(data);
            _capacity = capacity;
            _head = 0;
        }

        void _shrink() noexcept
        {
            if (_size == 0)
            {
                _head = 0;
            }
            if (_capacity > retainCapacity && _size <= _capacity / 8)
            {
                // Shrinking is best-effort. If it fails we just keep the memory.
                try
                {
                    _reallocate(std::max(retainCapacity, std::bit_ceil(_size * 2)));
                }
                CATCH_LOG()
            }
        }

        std::unique_ptr<T[]> _data;
        size_t _capacity = 0;
        size_t _head = 0;
        size_t _size = 0;
    };

    // Input is stored as runs of either INPUT_RECORDs or characters. A character expands to its
    // key events only when it's read, which makes large pastes ~10x smaller than as INPUT_RECORDs.
    // The order across runs is preserved, so readers observe exactly the same sequence of events.
    enum class RunKind : uint8_t
    {
        Records, // INPUT_RECORDs as-is
        Raw, // characters from WriteString(): 1 key down event each
        Typed, // characters from WriteKeyboardText(): 2 or 4 key events each, as if typed
    };

    struct Run
    {
        size_t count;
        RunKind kind;
    };

    struct Storage
    {
        Ring<INPUT_RECORD> records;
        Ring<Microsoft::Console::Interactivity::TypedChar> chars;
        Ring<Run> runs;
        // Number of key events already read from chars.front(). Only non-zero if runs.front() is Typed.
        size_t frontEventsRead = 0;
        // Number of events that GetNumberOfReadyEvents() reports.
        size_t events = 0;

        void clear() noexcept
        {
            records.clear();
            chars.clear();
            runs.clear();
            frontEventsRead = 0;
            events = 0;
        }
    };

    Storage _storage;
    INPUT_RECORD _writePartialByteSequence{};
    bool _writePartialByteSequenceAvailable = false;
    Microsoft::Console::VirtualTerminal::TerminalInput _termInput;

    // Wakes up readers waiting for data to be in the input buffer.
    auto _wakeupReadersOnExit() noexcept
    {
        const auto initiallyEmpty = _storage.events == 0;
        return wil::scope_exit([this, initiallyEmpty]() {
            _wakeupReadersImpl(initiallyEmpty);
        });
    }

    static size_t _charEventCount(const Microsoft::Console::Interactivity::TypedChar& ch, RunKind kind) noexcept;
    static size_t _charToEvents(const Microsoft::Console::Interactivity::TypedChar& ch, RunKind kind, std::array<INPUT_RECORD, 4>& out) noexcept;
    static bool _coalesceInto(INPUT_RECORD& lastEvent, const INPUT_RECORD& inEvent) noexcept;

    void _wakeupReadersImpl(bool initiallyEmpty);
    void _switchReadingMode(ReadingMode mode);
    void _switchReadingModeSlowPath(ReadingMode mode);
    void _WriteBuffer(const std::span<const INPUT_RECORD>& inRecords, _Out_ size_t& eventsWritten);
    void _writeRecord(CONSOLE_INFORMATION& gci, const INPUT_RECORD& inEvent, bool vtInputMode, bool coalesce, size_t& eventsWritten);
    bool _CoalesceEvent(const INPUT_RECORD& inEvent);
    void _writeString(const std::wstring_view& text);
    void _pushRun(RunKind kind);
    void _pushRecord(const INPUT_RECORD& record);
    void _pushRecords(std::span<const INPUT_RECORD> records);
    bool _isOutputControlKey(const CONSOLE_INFORMATION& gci, const INPUT_RECORD& record) const noexcept;
    void _pushChar(const Microsoft::Console::Interactivity::TypedChar& ch, RunKind kind);
    void _append(Storage& source);
};
