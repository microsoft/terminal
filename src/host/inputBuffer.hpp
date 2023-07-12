/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- inputBuffer.hpp

Abstract:
- storage area for incoming input events.

Author:
- Therese Stowell (Thereses) 12-Nov-1990. Adapted from OS/2 subsystem server\srvpipe.c

Revision History:
- Moved from input.h/input.cpp. (AustDi, 2017)
- Refactored to class, added stl container usage (AustDi, 2017)
--*/

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
    bool IsWritePartialByteSequenceAvailable();
    std::unique_ptr<IInputEvent> FetchWritePartialByteSequence(_In_ bool peek);
    void StoreWritePartialByteSequence(std::unique_ptr<IInputEvent> event);

    void ReinitializeInputBuffer();
    void WakeUpReadersWaitingForData();
    void TerminateRead(_In_ WaitTerminationReason Flag);
    size_t GetNumberOfReadyEvents() const noexcept;
    void Flush();
    void FlushAllButKeys();

    [[nodiscard]] NTSTATUS Read(_Out_ std::deque<std::unique_ptr<IInputEvent>>& OutEvents,
                                const size_t AmountToRead,
                                const bool Peek,
                                const bool WaitForData,
                                const bool Unicode,
                                const bool Stream);

    [[nodiscard]] NTSTATUS Read(_Out_ std::unique_ptr<IInputEvent>& inEvent,
                                const bool Peek,
                                const bool WaitForData,
                                const bool Unicode,
                                const bool Stream);

    size_t Prepend(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& inEvents);

    size_t Write(_Inout_ std::unique_ptr<IInputEvent> inEvent);
    size_t Write(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& inEvents);

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
    std::deque<std::unique_ptr<IInputEvent>> _cachedInputEvents;
    ReadingMode _readingMode = ReadingMode::StringA;

    std::deque<std::unique_ptr<IInputEvent>> _storage;
    std::unique_ptr<IInputEvent> _writePartialByteSequence;
    Microsoft::Console::VirtualTerminal::TerminalInput _termInput;
    Microsoft::Console::Render::VtEngine* _pTtyConnection;

    // This flag is used in _HandleTerminalInputCallback
    // If the InputBuffer leads to a _HandleTerminalInputCallback call,
    //    we should suppress the wakeup functions.
    // Otherwise, we should be calling them.
    bool _vtInputShouldSuppress{ false };

    void _switchReadingMode(ReadingMode mode);
    void _switchReadingModeSlowPath(ReadingMode mode);

    void _WriteBuffer(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& inRecords,
                      _Out_ size_t& eventsWritten,
                      _Out_ bool& setWaitEvent);

    bool _CoalesceEvent(const std::unique_ptr<IInputEvent>& inEvent) const noexcept;
    void _HandleTerminalInputCallback(const Microsoft::Console::VirtualTerminal::TerminalInput::StringType& text);

#ifdef UNIT_TESTING
    friend class InputBufferTests;
#endif
};
