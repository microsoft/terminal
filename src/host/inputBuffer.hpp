// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../types/inc/IInputEvent.hpp"
#include "../server/ObjectHandle.h"
#include "../server/ObjectHeader.h"
#include "../terminal/input/terminalInput.hpp"

namespace Microsoft::Console::Render
{
    class Renderer;
    class VtEngine;
}

class InputBuffer final : public ConsoleObjectHeader
{
public:
    ConsoleWaitQueue WaitQueue; // formerly ReadWaitQueue
    bool fInComposition; // specifies if there's an ongoing text composition

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

    void WakeUpReadersWaitingForData();
    void TerminateRead(_In_ WaitTerminationReason Flag);
    size_t GetNumberOfReadyEvents() const noexcept;
    void Flush();
    void FlushAllButKeys();

    struct ReadDescriptor
    {
        bool wide;
        bool records;
        bool peek;
    };
    size_t Read(ReadDescriptor desc, void* data, size_t capacityInBytes);

    void Write(const INPUT_RECORD& record);
    void Write(const std::span<const INPUT_RECORD>& records);
    void Write(const std::wstring_view& text);
    void WriteFocusEvent(bool focused) noexcept;
    bool WriteMouseEvent(til::point position, unsigned int button, short keyState, short wheelDelta);

    bool IsInVirtualTerminalInputMode() const;
    Microsoft::Console::VirtualTerminal::TerminalInput& GetTerminalInput();
    
    // 1 INPUT_RECORD = 20 bytes = 10 wchar_t
    // On 64-Bit architectures this results in std::list nodes of 1008 bytes (heap alloc headers are 16 bytes).
    // Optimally this should use a single ring buffer and not a bunch of glued together container classes.
    using RecordVec = til::small_vector<INPUT_RECORD, 48>;
    using TextVec = til::small_vector<wchar_t, 480>;
    using VecVariant = std::variant<RecordVec, TextVec>;
    std::list<VecVariant> _storage;

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

    INPUT_RECORD _writePartialByteSequence{};
    bool _writePartialByteSequenceAvailable = false;
    Microsoft::Console::VirtualTerminal::TerminalInput _termInput;

    void _switchReadingMode(ReadingMode mode);
    void _switchReadingModeSlowPath(ReadingMode mode);
    void _WriteBuffer(const std::span<const INPUT_RECORD>& inRecords, _Out_ size_t& eventsWritten, _Out_ bool& setWaitEvent);
    bool _CoalesceEvent(const INPUT_RECORD& inEvent) noexcept;

#ifdef UNIT_TESTING
    friend class InputBufferTests;
#endif
};
