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

#include "inputReadHandleData.h"
#include "readData.hpp"
#include "../types/inc/IInputEvent.hpp"

#include "../server/ObjectHandle.h"
#include "../server/ObjectHeader.h"
#include "../terminal/input/terminalInput.hpp"

#include "../inc/ITerminalOutputConnection.hpp"

#include <deque>

class InputBuffer final : public ConsoleObjectHeader
{
public:
    DWORD InputMode;
    ConsoleWaitQueue WaitQueue; // formerly ReadWaitQueue
    bool fInComposition; // specifies if there's an ongoing text composition

    InputBuffer();
    ~InputBuffer();

    // storage API for partial dbcs bytes being read from the buffer
    bool IsReadPartialByteSequenceAvailable();
    std::unique_ptr<IInputEvent> FetchReadPartialByteSequence(_In_ bool peek);
    void StoreReadPartialByteSequence(std::unique_ptr<IInputEvent> event);

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

    bool IsInVirtualTerminalInputMode() const;
    Microsoft::Console::VirtualTerminal::TerminalInput& GetTerminalInput();
    void SetTerminalConnection(_In_ Microsoft::Console::ITerminalOutputConnection* const pTtyConnection);
    void PassThroughWin32MouseRequest(bool enable);

private:
    std::deque<std::unique_ptr<IInputEvent>> _storage;
    std::unique_ptr<IInputEvent> _readPartialByteSequence;
    std::unique_ptr<IInputEvent> _writePartialByteSequence;
    Microsoft::Console::VirtualTerminal::TerminalInput _termInput;
    Microsoft::Console::ITerminalOutputConnection* _pTtyConnection;

    // This flag is used in _HandleTerminalInputCallback
    // If the InputBuffer leads to a _HandleTerminalInputCallback call,
    //    we should suppress the wakeup functions.
    // Otherwise, we should be calling them.
    bool _vtInputShouldSuppress{ false };

    void _ReadBuffer(_Out_ std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                     const size_t readCount,
                     _Out_ size_t& eventsRead,
                     const bool peek,
                     _Out_ bool& resetWaitEvent,
                     const bool unicode,
                     const bool streamRead);

    void _WriteBuffer(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& inRecords,
                      _Out_ size_t& eventsWritten,
                      _Out_ bool& setWaitEvent);

    bool _CanCoalesce(const KeyEvent& a, const KeyEvent& b) const noexcept;
    bool _CoalesceMouseMovedEvents(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& inEvents);
    bool _CoalesceRepeatedKeyPressEvents(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& inEvents);
    void _HandleConsoleSuspensionEvents(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& inEvents);

    void _HandleTerminalInputCallback(_In_ std::deque<std::unique_ptr<IInputEvent>>& inEvents);

#ifdef UNIT_TESTING
    friend class InputBufferTests;
#endif
};
