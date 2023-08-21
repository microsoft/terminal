// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "_stream.h"
#include "stream.h"

#include "handle.h"
#include "misc.h"
#include "readDataRaw.hpp"

#include "ApiRoutines.h"

#include "../types/inc/GlyphWidth.hpp"

#include "../interactivity/inc/ServiceLocator.hpp"

using Microsoft::Console::Interactivity::ServiceLocator;

static bool IsCommandLinePopupKey(const KEY_EVENT_RECORD& event)
{
    if (WI_AreAllFlagsClear(event.dwControlKeyState, RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED | RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED))
    {
        switch (event.wVirtualKeyCode)
        {
        case VK_ESCAPE:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_END:
        case VK_HOME:
        case VK_LEFT:
        case VK_UP:
        case VK_RIGHT:
        case VK_DOWN:
        case VK_F2:
        case VK_F4:
        case VK_F7:
        case VK_F9:
        case VK_DELETE:
            return true;
        default:
            break;
        }
    }
    return false;
}

static bool IsCommandLineEditingKey(const KEY_EVENT_RECORD& event)
{
    if (WI_AreAllFlagsClear(event.dwControlKeyState, RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED | RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED))
    {
        switch (event.wVirtualKeyCode)
        {
        case VK_ESCAPE:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_END:
        case VK_HOME:
        case VK_LEFT:
        case VK_UP:
        case VK_RIGHT:
        case VK_DOWN:
        case VK_INSERT:
        case VK_DELETE:
        case VK_F1:
        case VK_F2:
        case VK_F3:
        case VK_F4:
        case VK_F5:
        case VK_F6:
        case VK_F7:
        case VK_F8:
        case VK_F9:
            return true;
        default:
            break;
        }
    }
    if (WI_IsAnyFlagSet(event.dwControlKeyState, RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED))
    {
        switch (event.wVirtualKeyCode)
        {
        case VK_END:
        case VK_HOME:
        case VK_LEFT:
        case VK_RIGHT:
            return true;
        default:
            break;
        }
    }
    if (WI_IsAnyFlagSet(event.dwControlKeyState, RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED))
    {
        switch (event.wVirtualKeyCode)
        {
        case VK_F7:
        case VK_F10:
            return true;
        default:
            break;
        }
    }

    return false;
}

// Routine Description:
// - This routine is used in stream input.  It gets input and filters it for unicode characters.
// Arguments:
// - pInputBuffer - The InputBuffer to read from
// - pwchOut - On a successful read, the char data read
// - Wait - true if a waited read should be performed
// - pCommandLineEditingKeys - if present, arrow keys will be
// returned. on output, if true, pwchOut contains virtual key code for
// arrow key.
// - pPopupKeys - if present, arrow keys will be
// returned. on output, if true, pwchOut contains virtual key code for
// arrow key.
// Return Value:
// - STATUS_SUCCESS on success or a relevant error code on failure.
[[nodiscard]] NTSTATUS GetChar(_Inout_ InputBuffer* const pInputBuffer,
                               _Out_ wchar_t* const pwchOut,
                               const bool Wait,
                               _Out_opt_ bool* const pCommandLineEditingKeys,
                               _Out_opt_ bool* const pPopupKeys,
                               _Out_opt_ DWORD* const pdwKeyState) noexcept
{
    if (nullptr != pCommandLineEditingKeys)
    {
        *pCommandLineEditingKeys = false;
    }

    if (nullptr != pPopupKeys)
    {
        *pPopupKeys = false;
    }

    if (nullptr != pdwKeyState)
    {
        *pdwKeyState = 0;
    }

    for (;;)
    {
        InputEventQueue events;
        const auto Status = pInputBuffer->Read(events, 1, false, Wait, true, true);
        if (FAILED_NTSTATUS(Status))
        {
            return Status;
        }
        if (events.empty())
        {
            assert(!Wait);
            return STATUS_UNSUCCESSFUL;
        }

        const auto& Event = events[0];
        if (Event.EventType == KEY_EVENT)
        {
            auto commandLineEditKey = false;
            if (pCommandLineEditingKeys)
            {
                commandLineEditKey = IsCommandLineEditingKey(Event.Event.KeyEvent);
            }
            else if (pPopupKeys)
            {
                commandLineEditKey = IsCommandLinePopupKey(Event.Event.KeyEvent);
            }

            if (pdwKeyState)
            {
                *pdwKeyState = Event.Event.KeyEvent.dwControlKeyState;
            }

            if (Event.Event.KeyEvent.uChar.UnicodeChar != 0 && !commandLineEditKey)
            {
                // chars that are generated using alt + numpad
                if (!Event.Event.KeyEvent.bKeyDown && Event.Event.KeyEvent.wVirtualKeyCode == VK_MENU)
                {
                    if (WI_IsFlagSet(Event.Event.KeyEvent.dwControlKeyState, ALTNUMPAD_BIT))
                    {
                        if (HIBYTE(Event.Event.KeyEvent.uChar.UnicodeChar))
                        {
                            const char chT[2] = {
                                static_cast<char>(HIBYTE(Event.Event.KeyEvent.uChar.UnicodeChar)),
                                static_cast<char>(LOBYTE(Event.Event.KeyEvent.uChar.UnicodeChar)),
                            };
                            *pwchOut = CharToWchar(chT, 2);
                        }
                        else
                        {
                            // Because USER doesn't know our codepage,
                            // it gives us the raw OEM char and we
                            // convert it to a Unicode character.
                            char chT = LOBYTE(Event.Event.KeyEvent.uChar.UnicodeChar);
                            *pwchOut = CharToWchar(&chT, 1);
                        }
                    }
                    else
                    {
                        *pwchOut = Event.Event.KeyEvent.uChar.UnicodeChar;
                    }
                    return STATUS_SUCCESS;
                }

                // Ignore Escape and Newline chars
                if (Event.Event.KeyEvent.bKeyDown &&
                    (WI_IsFlagSet(pInputBuffer->InputMode, ENABLE_VIRTUAL_TERMINAL_INPUT) ||
                     (Event.Event.KeyEvent.wVirtualKeyCode != VK_ESCAPE &&
                      Event.Event.KeyEvent.uChar.UnicodeChar != UNICODE_LINEFEED)))
                {
                    *pwchOut = Event.Event.KeyEvent.uChar.UnicodeChar;
                    return STATUS_SUCCESS;
                }
            }

            if (Event.Event.KeyEvent.bKeyDown)
            {
                if (pCommandLineEditingKeys && commandLineEditKey)
                {
                    *pCommandLineEditingKeys = true;
                    *pwchOut = static_cast<wchar_t>(Event.Event.KeyEvent.wVirtualKeyCode);
                    return STATUS_SUCCESS;
                }

                if (pPopupKeys && commandLineEditKey)
                {
                    *pPopupKeys = true;
                    *pwchOut = static_cast<char>(Event.Event.KeyEvent.wVirtualKeyCode);
                    return STATUS_SUCCESS;
                }

                const auto zeroKey = OneCoreSafeVkKeyScanW(0);

                if (LOBYTE(zeroKey) == Event.Event.KeyEvent.wVirtualKeyCode &&
                    WI_IsAnyFlagSet(Event.Event.KeyEvent.dwControlKeyState, ALT_PRESSED) == WI_IsFlagSet(zeroKey, 0x400) &&
                    WI_IsAnyFlagSet(Event.Event.KeyEvent.dwControlKeyState, CTRL_PRESSED) == WI_IsFlagSet(zeroKey, 0x200) &&
                    WI_IsAnyFlagSet(Event.Event.KeyEvent.dwControlKeyState, SHIFT_PRESSED) == WI_IsFlagSet(zeroKey, 0x100))
                {
                    // This really is the character 0x0000
                    *pwchOut = Event.Event.KeyEvent.uChar.UnicodeChar;
                    return STATUS_SUCCESS;
                }
            }
        }
    }
}

// Routine Description:
// - This routine returns the total number of screen spaces the characters up to the specified character take up.
til::CoordType RetrieveTotalNumberOfSpaces(const til::CoordType sOriginalCursorPositionX,
                                           _In_reads_(ulCurrentPosition) const WCHAR* const pwchBuffer,
                                           _In_ size_t ulCurrentPosition)
{
    auto XPosition = sOriginalCursorPositionX;
    til::CoordType NumSpaces = 0;

    for (size_t i = 0; i < ulCurrentPosition; i++)
    {
        const auto Char = pwchBuffer[i];

        til::CoordType NumSpacesForChar;
        if (Char == UNICODE_TAB)
        {
            NumSpacesForChar = NUMBER_OF_SPACES_IN_TAB(XPosition);
        }
        else if (IS_CONTROL_CHAR(Char))
        {
            NumSpacesForChar = 2;
        }
        else if (IsGlyphFullWidth(Char))
        {
            NumSpacesForChar = 2;
        }
        else
        {
            NumSpacesForChar = 1;
        }
        XPosition += NumSpacesForChar;
        NumSpaces += NumSpacesForChar;
    }

    return NumSpaces;
}

// Routine Description:
// - This routine returns the number of screen spaces the specified character takes up.
til::CoordType RetrieveNumberOfSpaces(_In_ til::CoordType sOriginalCursorPositionX,
                                      _In_reads_(ulCurrentPosition + 1) const WCHAR* const pwchBuffer,
                                      _In_ size_t ulCurrentPosition)
{
    auto Char = pwchBuffer[ulCurrentPosition];
    if (Char == UNICODE_TAB)
    {
        til::CoordType NumSpaces = 0;
        auto XPosition = sOriginalCursorPositionX;

        for (size_t i = 0; i <= ulCurrentPosition; i++)
        {
            Char = pwchBuffer[i];
            if (Char == UNICODE_TAB)
            {
                NumSpaces = NUMBER_OF_SPACES_IN_TAB(XPosition);
            }
            else if (IS_CONTROL_CHAR(Char))
            {
                NumSpaces = 2;
            }
            else if (IsGlyphFullWidth(Char))
            {
                NumSpaces = 2;
            }
            else
            {
                NumSpaces = 1;
            }
            XPosition += NumSpaces;
        }

        return NumSpaces;
    }
    else if (IS_CONTROL_CHAR(Char))
    {
        return 2;
    }
    else if (IsGlyphFullWidth(Char))
    {
        return 2;
    }
    else
    {
        return 1;
    }
}

// Routine Description:
// - if we have leftover input, copy as much fits into the user's
// buffer and return.  we may have multi line input, if a macro
// has been defined that contains the $T character.
// Arguments:
// - inputBuffer - Pointer to input buffer to read from.
// - buffer - buffer to place read char data into
// - bytesRead - number of bytes read and filled into the buffer
// - readHandleState - input read handle data associated with this read operation
// - unicode - true if read should be unicode, false otherwise
// Return Value:
// - STATUS_NO_MEMORY in low memory situation
// - other relevant NTSTATUS codes
[[nodiscard]] static NTSTATUS _ReadPendingInput(InputBuffer& inputBuffer,
                                                std::span<char> buffer,
                                                size_t& bytesRead,
                                                INPUT_READ_HANDLE_DATA& readHandleState,
                                                const bool unicode)
try
{
    bytesRead = 0;

    const auto pending = readHandleState.GetPendingInput();
    auto input = pending;

    // This is basically the continuation of COOKED_READ_DATA::_handlePostCharInputLoop.
    if (readHandleState.IsMultilineInput())
    {
        const auto firstLineEnd = input.find(UNICODE_LINEFEED) + 1;
        input = input.substr(0, std::min(input.size(), firstLineEnd));
    }

    const auto inputSizeBefore = input.size();
    std::span writer{ buffer };
    inputBuffer.Consume(unicode, input, writer);

    // Since we truncated `input` to only include the first line,
    // we need to restore `input` here to the entirety of the remaining input.
    if (readHandleState.IsMultilineInput())
    {
        const auto inputSizeAfter = input.size();
        const auto amountConsumed = inputSizeBefore - inputSizeAfter;
        input = pending.substr(std::min(pending.size(), amountConsumed));
    }

    if (input.empty())
    {
        readHandleState.CompletePending();
    }
    else
    {
        readHandleState.UpdatePending(input);
    }

    bytesRead = buffer.size() - writer.size();
    return STATUS_SUCCESS;
}
NT_CATCH_RETURN()

// Routine Description:
// - read in characters until the buffer is full or return is read.
// since we may wait inside this loop, store all important variables
// in the read data structure.  if we do wait, a read data structure
// will be allocated from the heap and its pointer will be stored
// in the wait block.  the CookedReadData will be copied into the
// structure.  the data is freed when the read is completed.
// Arguments:
// - inputBuffer - input buffer to read data from
// - processData - process handle of process making read request
// - buffer - buffer to place read char data
// - bytesRead - on output, the number of bytes read into pwchBuffer
// - controlKeyState - set by a cooked read
// - initialData - text of initial data found in the read message
// - ctrlWakeupMask - used by COOKED_READ_DATA
// - readHandleState - input read handle data associated with this read operation
// - exeName - name of the exe requesting the read
// - unicode - true if read should be unicode, false otherwise
// - waiter - If a wait is necessary this will contain the wait
// object on output
// Return Value:
// - STATUS_UNSUCCESSFUL if not able to access current screen buffer
// - STATUS_NO_MEMORY in low memory situation
// - other relevant HRESULT codes
[[nodiscard]] static HRESULT _ReadLineInput(InputBuffer& inputBuffer,
                                            const HANDLE processData,
                                            std::span<char> buffer,
                                            size_t& bytesRead,
                                            DWORD& controlKeyState,
                                            const std::wstring_view initialData,
                                            const DWORD ctrlWakeupMask,
                                            INPUT_READ_HANDLE_DATA& readHandleState,
                                            const std::wstring_view exeName,
                                            const bool unicode,
                                            std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    RETURN_HR_IF(E_FAIL, !gci.HasActiveOutputBuffer());

    auto& screenInfo = gci.GetActiveOutputBuffer();

    try
    {
        auto cookedReadData = std::make_unique<COOKED_READ_DATA>(&inputBuffer, // pInputBuffer
                                                                 &readHandleState, // pInputReadHandleData
                                                                 screenInfo, // pScreenInfo
                                                                 buffer.size_bytes(), // UserBufferSize
                                                                 buffer.data(), // UserBuffer
                                                                 ctrlWakeupMask, // CtrlWakeupMask
                                                                 exeName, // exe name
                                                                 initialData,
                                                                 reinterpret_cast<ConsoleProcessHandle*>(processData)); //pClientProcess

        gci.SetCookedReadData(cookedReadData.get());
        bytesRead = buffer.size_bytes(); // This parameter on the way in is the size to read, on the way out, it will be updated to what is actually read.
        if (CONSOLE_STATUS_WAIT == cookedReadData->Read(unicode, bytesRead, controlKeyState))
        {
            // memory will be cleaned up by wait queue
            waiter.reset(cookedReadData.release());
        }
        else
        {
            gci.SetCookedReadData(nullptr);
        }
    }
    CATCH_RETURN();

    return S_OK;
}

// Routine Description:
// - Character (raw) mode. Read at least one character in. After one
// character has been read, get any more available characters and
// return. The first call to GetChar may block. If we do wait, a read
// data structure will be allocated from the heap and its pointer will
// be stored in the wait block. The RawReadData will be copied into
// the structure. The data is freed when the read is completed.
// Arguments:
// - inputBuffer - input buffer to read data from
// - buffer - on output, the amount of data read, in bytes
// - bytesRead - number of bytes read and placed into buffer
// - readHandleState - input read handle data associated with this read operation
// - unicode - true if read should be unicode, false otherwise
// - waiter  - if a wait is necessary, on output this will contain
// the associated wait object
// Return Value:
// - CONSOLE_STATUS_WAIT if a wait is necessary. ppWaiter will be
// populated.
// - STATUS_SUCCESS on success
// - Other NTSTATUS codes as necessary
[[nodiscard]] NTSTATUS ReadCharacterInput(InputBuffer& inputBuffer,
                                          std::span<char> buffer,
                                          size_t& bytesRead,
                                          INPUT_READ_HANDLE_DATA& readHandleState,
                                          const bool unicode)
try
{
    UNREFERENCED_PARAMETER(readHandleState);

    bytesRead = 0;

    const auto charSize = unicode ? sizeof(wchar_t) : sizeof(char);
    std::span writer{ buffer };

    if (writer.size() < charSize)
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    inputBuffer.ConsumeCached(unicode, writer);

    auto noDataReadYet = writer.size() == buffer.size();
    auto status = STATUS_SUCCESS;

    while (writer.size() >= charSize)
    {
        wchar_t wch;
        // We don't need to wait for input if `ConsumeCached` read something already, which is
        // indicated by the writer having been advanced (= it's shorter than the original buffer).
        status = GetChar(&inputBuffer, &wch, noDataReadYet, nullptr, nullptr, nullptr);
        if (FAILED_NTSTATUS(status))
        {
            break;
        }

        std::wstring_view wchView{ &wch, 1 };
        inputBuffer.Consume(unicode, wchView, writer);

        noDataReadYet = false;
    }

    bytesRead = buffer.size() - writer.size();
    // Once we read some data off the InputBuffer it can't be read again, so we
    // need to make sure to return a success status to the client in that case.
    return noDataReadYet ? status : STATUS_SUCCESS;
}
NT_CATCH_RETURN()

// Routine Description:
// - This routine reads in characters for stream input and does the
// required processing based on the input mode (line, char, echo).
// - This routine returns UNICODE characters.
// Arguments:
// - inputBuffer - Pointer to input buffer to read from.
// - processData - process handle of process making read request
// - buffer - buffer to place read char data into
// - bytesRead - the length of data placed in buffer. Measured in bytes.
// - controlKeyState - set by a cooked read
// - initialData - text of initial data found in the read message
// - ctrlWakeupMask - used by COOKED_READ_DATA
// - readHandleState - read handle data associated with this read
// - exeName- name of the exe requesting the read
// - unicode - true for a unicode read, false for ascii
// - waiter - If a wait is necessary this will contain the wait
// object on output
// Return Value:
// - STATUS_BUFFER_TOO_SMALL if pdwNumBytes is too small to store char
// data.
// - CONSOLE_STATUS_WAIT if a wait is necessary. ppWaiter will be
// populated.
// - STATUS_SUCCESS on success
// - Other NSTATUS codes as necessary
[[nodiscard]] NTSTATUS DoReadConsole(InputBuffer& inputBuffer,
                                     const HANDLE processData,
                                     std::span<char> buffer,
                                     size_t& bytesRead,
                                     ULONG& controlKeyState,
                                     const std::wstring_view initialData,
                                     const DWORD ctrlWakeupMask,
                                     INPUT_READ_HANDLE_DATA& readHandleState,
                                     const std::wstring_view exeName,
                                     const bool unicode,
                                     std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        waiter.reset();

        bytesRead = 0;

        if (buffer.size() < 1)
        {
            return STATUS_BUFFER_TOO_SMALL;
        }

        if (readHandleState.IsInputPending())
        {
            return _ReadPendingInput(inputBuffer,
                                     buffer,
                                     bytesRead,
                                     readHandleState,
                                     unicode);
        }
        else if (WI_IsFlagSet(inputBuffer.InputMode, ENABLE_LINE_INPUT))
        {
            return NTSTATUS_FROM_HRESULT(_ReadLineInput(inputBuffer,
                                                        processData,
                                                        buffer,
                                                        bytesRead,
                                                        controlKeyState,
                                                        initialData,
                                                        ctrlWakeupMask,
                                                        readHandleState,
                                                        exeName,
                                                        unicode,
                                                        waiter));
        }
        else
        {
            const auto status = ReadCharacterInput(inputBuffer,
                                                   buffer,
                                                   bytesRead,
                                                   readHandleState,
                                                   unicode);
            if (status == CONSOLE_STATUS_WAIT)
            {
                waiter = std::make_unique<RAW_READ_DATA>(&inputBuffer, &readHandleState, gsl::narrow<ULONG>(buffer.size()), reinterpret_cast<wchar_t*>(buffer.data()));
            }
            return status;
        }
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT ApiRoutines::ReadConsoleImpl(IConsoleInputObject& context,
                                                   std::span<char> buffer,
                                                   size_t& written,
                                                   std::unique_ptr<IWaitRoutine>& waiter,
                                                   const std::wstring_view initialData,
                                                   const std::wstring_view exeName,
                                                   INPUT_READ_HANDLE_DATA& readHandleState,
                                                   const bool IsUnicode,
                                                   const HANDLE clientHandle,
                                                   const DWORD controlWakeupMask,
                                                   DWORD& controlKeyState) noexcept
{
    return HRESULT_FROM_NT(DoReadConsole(context,
                                         clientHandle,
                                         buffer,
                                         written,
                                         controlKeyState,
                                         initialData,
                                         controlWakeupMask,
                                         readHandleState,
                                         exeName,
                                         IsUnicode,
                                         waiter));
}

void UnblockWriteConsole(const DWORD dwReason)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.Flags &= ~dwReason;

    if (WI_AreAllFlagsClear(gci.Flags, (CONSOLE_SUSPENDED | CONSOLE_SELECTING | CONSOLE_SCROLLBAR_TRACKING)))
    {
        // There is no longer any reason to suspend output, so unblock it.
        gci.OutputQueue.NotifyWaiters(true);
    }
}
