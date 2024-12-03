// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "readData.hpp"
#include "../types/inc/IInputEvent.hpp"

#include "../server/ObjectHandle.h"
#include "../server/ObjectHeader.h"
#include "../terminal/input/terminalInput.hpp"

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

    std::deque<INPUT_RECORD> _storage;
    INPUT_RECORD _writePartialByteSequence{};
    bool _writePartialByteSequenceAvailable = false;
    Microsoft::Console::VirtualTerminal::TerminalInput _termInput;

    // Wakes up readers waiting for data to be in the input buffer.
    auto _wakeupReadersOnExit() noexcept
    {
        const auto initiallyEmpty = _storage.empty();
        return wil::scope_exit([this, initiallyEmpty]() {
            _wakeupReadersImpl(initiallyEmpty);
        });
    }

    void _wakeupReadersImpl(bool initiallyEmpty);
    void _switchReadingMode(ReadingMode mode);
    void _switchReadingModeSlowPath(ReadingMode mode);
    void _WriteBuffer(const std::span<const INPUT_RECORD>& inRecords, _Out_ size_t& eventsWritten);
    bool _CoalesceEvent(const INPUT_RECORD& inEvent) noexcept;
    void _writeString(const std::wstring_view& text);

#ifdef UNIT_TESTING
    friend class InputBufferTests;
#endif
};
