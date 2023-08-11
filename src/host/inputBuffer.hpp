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
    class VtEngine;
}

class InputBuffer final : public ConsoleObjectHeader
{
public:
    DWORD InputMode;
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
    void WriteFocusEvent(bool focused) noexcept;
    bool WriteMouseEvent(til::point position, unsigned int button, short keyState, short wheelDelta);

    bool IsInVirtualTerminalInputMode() const;
    Microsoft::Console::VirtualTerminal::TerminalInput& GetTerminalInput();
    void SetTerminalConnection(_In_ Microsoft::Console::Render::VtEngine* const pTtyConnection);
    void PassThroughWin32MouseRequest(bool enable);

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
    Microsoft::Console::Render::VtEngine* _pTtyConnection;

    // This flag is used in _HandleTerminalInputCallback
    // If the InputBuffer leads to a _HandleTerminalInputCallback call,
    //    we should suppress the wakeup functions.
    // Otherwise, we should be calling them.
    bool _vtInputShouldSuppress{ false };

    void _switchReadingMode(ReadingMode mode);
    void _switchReadingModeSlowPath(ReadingMode mode);
    void _WriteBuffer(const std::span<const INPUT_RECORD>& inRecords, _Out_ size_t& eventsWritten, _Out_ bool& setWaitEvent);
    bool _CoalesceEvent(const INPUT_RECORD& inEvent) noexcept;
    void _HandleTerminalInputCallback(const Microsoft::Console::VirtualTerminal::TerminalInput::StringType& text);

#ifdef UNIT_TESTING
    friend class InputBufferTests;
#endif
};
