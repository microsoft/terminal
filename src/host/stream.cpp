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
        if (!cookedReadData->Read(unicode, bytesRead, controlKeyState))
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

    const InputBuffer::ReadDescriptor readDesc{
        .wide = unicode,
    };
    bytesRead = inputBuffer.Read(readDesc, buffer.data(), buffer.size());
    return bytesRead == 0 ? CONSOLE_STATUS_WAIT : STATUS_SUCCESS;
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
