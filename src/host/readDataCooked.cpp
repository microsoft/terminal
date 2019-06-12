// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "readDataCooked.hpp"
#include "dbcs.h"
#include "stream.h"
#include "misc.h"
#include "_stream.h"
#include "inputBuffer.hpp"
#include "cmdline.h"
#include "../types/inc/GlyphWidth.hpp"
#include "../types/inc/convert.hpp"

#include "..\interactivity\inc\ServiceLocator.hpp"

#define LINE_INPUT_BUFFER_SIZE (256 * sizeof(WCHAR))

using Microsoft::Console::Interactivity::ServiceLocator;

// Routine Description:
// - Constructs cooked read data class to hold context across key presses while a user is modifying their 'input line'.
// Arguments:
// - pInputBuffer - Buffer that data will be read from.
// - pInputReadHandleData - Context stored across calls from the same input handle to return partial data appropriately.
// - screenInfo - Output buffer that will be used for 'echoing' the line back to the user so they can see/manipulate it
// - BufferSize -
// - BytesRead -
// - CurrentPosition -
// - BufPtr -
// - BackupLimit -
// - UserBufferSize - The byte count of the buffer presented by the client
// - UserBuffer - The buffer that was presented by the client for filling with input data on read conclusion/return from server/host.
// - OriginalCursorPosition -
// - NumberOfVisibleChars
// - CtrlWakeupMask - Special client parameter to interrupt editing, end the wait, and return control to the client application
// - CommandHistory -
// - Echo -
// - InsertMode -
// - Processed -
// - Line -
// - pTempHandle - A handle to the output buffer to prevent it from being destroyed while we're using it to present 'edit line' text.
// - initialData - any text data that should be prepopulated into the buffer
// Return Value:
// - THROW: Throws E_INVALIDARG for invalid pointers.
COOKED_READ_DATA::COOKED_READ_DATA(_In_ InputBuffer* const pInputBuffer,
                                   _In_ INPUT_READ_HANDLE_DATA* const pInputReadHandleData,
                                   SCREEN_INFORMATION& screenInfo,
                                   _In_ size_t UserBufferSize,
                                   _In_ PWCHAR UserBuffer,
                                   _In_ ULONG CtrlWakeupMask,
                                   _In_ CommandHistory* CommandHistory,
                                   const std::wstring_view exeName,
                                   const std::string_view initialData) :
    ReadData(pInputBuffer, pInputReadHandleData),
    _screenInfo{ screenInfo },
    _bytesRead{ 0 },
    _currentPosition{ 0 },
    _userBufferSize{ UserBufferSize },
    _userBuffer{ UserBuffer },
    _tempHandle{ nullptr },
    _exeName{ exeName },
    _pdwNumBytes{ nullptr },

    _commandHistory{ CommandHistory },
    _controlKeyState{ 0 },
    _ctrlWakeupMask{ CtrlWakeupMask },
    _visibleCharCount{ 0 },
    _originalCursorPosition{ -1, -1 },
    _beforeDialogCursorPosition{ 0, 0 },

    _echoInput{ WI_IsFlagSet(pInputBuffer->InputMode, ENABLE_ECHO_INPUT) },
    _lineInput{ WI_IsFlagSet(pInputBuffer->InputMode, ENABLE_LINE_INPUT) },
    _processedInput{ WI_IsFlagSet(pInputBuffer->InputMode, ENABLE_PROCESSED_INPUT) },
    _insertMode{ ServiceLocator::LocateGlobals().getConsoleInformation().GetInsertMode() },
    _unicode{ false }
{
#ifndef UNIT_TESTING
    THROW_IF_FAILED(screenInfo.GetMainBuffer().AllocateIoHandle(ConsoleHandleData::HandleType::Output,
                                                                GENERIC_WRITE,
                                                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                                _tempHandle));
#endif

    // to emulate OS/2 KbdStringIn, we read into our own big buffer
    // (256 bytes) until the user types enter.  then return as many
    // chars as will fit in the user's buffer.
    _bufferSize = std::max(UserBufferSize, LINE_INPUT_BUFFER_SIZE);
    _buffer = std::make_unique<byte[]>(_bufferSize);
    _backupLimit = reinterpret_cast<wchar_t*>(_buffer.get());
    _bufPtr = reinterpret_cast<wchar_t*>(_buffer.get());

    // Initialize the user's buffer to spaces. This is done so that
    // moving in the buffer via cursor doesn't do strange things.
    std::fill_n(_bufPtr, _bufferSize / sizeof(wchar_t), UNICODE_SPACE);

    if (!initialData.empty())
    {
        memcpy_s(_bufPtr, _bufferSize, initialData.data(), initialData.size());

        _bytesRead += initialData.size();

        const size_t cchInitialData = initialData.size() / sizeof(wchar_t);
        VisibleCharCount() = cchInitialData;
        _bufPtr += cchInitialData;
        _currentPosition = cchInitialData;

        OriginalCursorPosition() = screenInfo.GetTextBuffer().GetCursor().GetPosition();
        OriginalCursorPosition().X -= (SHORT)_currentPosition;

        const SHORT sScreenBufferSizeX = screenInfo.GetBufferSize().Width();
        while (OriginalCursorPosition().X < 0)
        {
            OriginalCursorPosition().X += sScreenBufferSizeX;
            OriginalCursorPosition().Y -= 1;
        }
    }

    // TODO MSFT:11285829 find a better way to manage the lifetime of this object in relation to gci
}

// Routine Description:
// - Destructs a read data class.
// - Decrements count of readers waiting on the given handle.
COOKED_READ_DATA::~COOKED_READ_DATA()
{
    CommandLine::Instance().EndAllPopups();
}

gsl::span<wchar_t> COOKED_READ_DATA::SpanWholeBuffer()
{
    return gsl::make_span(_backupLimit, (_bufferSize / sizeof(wchar_t)));
}

gsl::span<wchar_t> COOKED_READ_DATA::SpanAtPointer()
{
    auto wholeSpan = SpanWholeBuffer();
    return wholeSpan.subspan(_bufPtr - _backupLimit);
}

bool COOKED_READ_DATA::HasHistory() const noexcept
{
    return _commandHistory != nullptr;
}

CommandHistory& COOKED_READ_DATA::History() noexcept
{
    return *_commandHistory;
}

const size_t& COOKED_READ_DATA::VisibleCharCount() const noexcept
{
    return _visibleCharCount;
}

size_t& COOKED_READ_DATA::VisibleCharCount() noexcept
{
    return _visibleCharCount;
}

SCREEN_INFORMATION& COOKED_READ_DATA::ScreenInfo() noexcept
{
    return _screenInfo;
}

const COORD& COOKED_READ_DATA::OriginalCursorPosition() const noexcept
{
    return _originalCursorPosition;
}

COORD& COOKED_READ_DATA::OriginalCursorPosition() noexcept
{
    return _originalCursorPosition;
}

COORD& COOKED_READ_DATA::BeforeDialogCursorPosition() noexcept
{
    return _beforeDialogCursorPosition;
}

bool COOKED_READ_DATA::IsEchoInput() const noexcept
{
    return _echoInput;
}

bool COOKED_READ_DATA::IsInsertMode() const noexcept
{
    return _insertMode;
}

void COOKED_READ_DATA::SetInsertMode(const bool mode) noexcept
{
    _insertMode = mode;
}

bool COOKED_READ_DATA::IsUnicode() const noexcept
{
    return _unicode;
}

// Routine Description:
// - gets the size of the user buffer
// Return Value:
// - the size of the user buffer in bytes
size_t COOKED_READ_DATA::UserBufferSize() const noexcept
{
    return _userBufferSize;
}

// Routine Description:
// - gets a pointer to the beginning of the prompt storage
// Return Value:
// - pointer to the first char in the internal prompt storage array
wchar_t* COOKED_READ_DATA::BufferStartPtr() noexcept
{
    return _backupLimit;
}

// Routine Description:
// - gets a pointer to where the next char will be inserted into the prompt storage
// Return Value:
// - pointer to the current insertion point of the prompt storage array
wchar_t* COOKED_READ_DATA::BufferCurrentPtr() noexcept
{
    return _bufPtr;
}

// Routine Description:
// - Set the location of the next char insert into the prompt storage to be at
// ptr. ptr must point into a valid portion of the internal prompt storage array
// Arguments:
// - ptr - the new char insertion location
void COOKED_READ_DATA::SetBufferCurrentPtr(wchar_t* ptr) noexcept
{
    _bufPtr = ptr;
}

// Routine Description:
// - gets the number of bytes read so far into the prompt buffer
// Return Value:
// - the number of bytes read
const size_t& COOKED_READ_DATA::BytesRead() const noexcept
{
    return _bytesRead;
}

// Routine Description:
// - gets the number of bytes read so far into the prompt buffer
// Return Value:
// - the number of bytes read
size_t& COOKED_READ_DATA::BytesRead() noexcept
{
    return _bytesRead;
}

// Routine Description:
// - gets the index for the current insertion point of the prompt
// Return Value:
// - the index of the current insertion point
const size_t& COOKED_READ_DATA::InsertionPoint() const noexcept
{
    return _currentPosition;
}

// Routine Description:
// - gets the index for the current insertion point of the prompt
// Return Value:
// - the index of the current insertion point
size_t& COOKED_READ_DATA::InsertionPoint() noexcept
{
    return _currentPosition;
}

// Routine Description:
// - sets the number of bytes that will be reported when this read block completes its read
// Arguments:
// - count - the number of bytes to report
void COOKED_READ_DATA::SetReportedByteCount(const size_t count) noexcept
{
    FAIL_FAST_IF_NULL(_pdwNumBytes);
    *_pdwNumBytes = count;
}

// Routine Description:
// - resets the prompt to be as if it was erased
void COOKED_READ_DATA::Erase() noexcept
{
    _bufPtr = _backupLimit;
    _bytesRead = 0;
    _currentPosition = 0;
    _visibleCharCount = 0;
}

// Routine Description:
// - This routine is called to complete a cooked read that blocked in ReadInputBuffer.
// - The context of the read was saved in the CookedReadData structure.
// - This routine is called when events have been written to the input buffer.
// - It is called in the context of the writing thread.
// - It may be called more than once.
// Arguments:
// - TerminationReason - if this routine is called because a ctrl-c or ctrl-break was seen, this argument
//                      contains CtrlC or CtrlBreak. If the owning thread is exiting, it will have ThreadDying. Otherwise 0.
// - fIsUnicode - Whether to convert the final data to A (using Console Input CP) at the end or treat everything as Unicode (UCS-2)
// - pReplyStatus - The status code to return to the client application that originally called the API (before it was queued to wait)
// - pNumBytes - The number of bytes of data that the server/driver will need to transmit back to the client process
// - pControlKeyState - For certain types of reads, this specifies which modifier keys were held.
// - pOutputData - not used
// Return Value:
// - true if the wait is done and result buffer/status code can be sent back to the client.
// - false if we need to continue to wait until more data is available.
bool COOKED_READ_DATA::Notify(const WaitTerminationReason TerminationReason,
                              const bool fIsUnicode,
                              _Out_ NTSTATUS* const pReplyStatus,
                              _Out_ size_t* const pNumBytes,
                              _Out_ DWORD* const pControlKeyState,
                              _Out_ void* const /*pOutputData*/)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    // this routine should be called by a thread owning the same
    // lock on the same console as we're reading from.
    FAIL_FAST_IF(!gci.IsConsoleLocked());

    *pNumBytes = 0;
    *pControlKeyState = 0;

    *pReplyStatus = STATUS_SUCCESS;

    FAIL_FAST_IF(_pInputReadHandleData->IsInputPending());

    // this routine should be called by a thread owning the same lock on the same console as we're reading from.
    FAIL_FAST_IF(_pInputReadHandleData->GetReadCount() == 0);

    // if ctrl-c or ctrl-break was seen, terminate read.
    if (WI_IsAnyFlagSet(TerminationReason, (WaitTerminationReason::CtrlC | WaitTerminationReason::CtrlBreak)))
    {
        *pReplyStatus = STATUS_ALERTED;
        gci.SetCookedReadData(nullptr);
        return true;
    }

    // See if we were called because the thread that owns this wait block is exiting.
    if (WI_IsFlagSet(TerminationReason, WaitTerminationReason::ThreadDying))
    {
        *pReplyStatus = STATUS_THREAD_IS_TERMINATING;
        gci.SetCookedReadData(nullptr);
        return true;
    }

    // We must see if we were woken up because the handle is being closed. If
    // so, we decrement the read count. If it goes to zero, we wake up the
    // close thread. Otherwise, we wake up any other thread waiting for data.

    if (WI_IsFlagSet(TerminationReason, WaitTerminationReason::HandleClosing))
    {
        *pReplyStatus = STATUS_ALERTED;
        gci.SetCookedReadData(nullptr);
        return true;
    }

    // If we get to here, this routine was called either by the input thread
    // or a write routine. Both of these callers grab the current console
    // lock.

    // MSFT:13994975 This is REALLY weird.
    // When we're doing cooked reading for popups, we come through this method
    //   twice. Once when we press F7 to bring up the popup, then again when we
    //   press enter to input the selected command.
    // The first time, there is no popup, and we go to CookedRead. We pass into
    //   CookedRead `pNumBytes`, which is passed to us as the address of the
    //   stack variable dwNumBytes, in ConsoleWaitBlock::Notify.
    // CookedRead sets this->_pdwNumBytes to that value, and starts the popup,
    //   which returns all the way up, and pops the ConsoleWaitBlock::Notify
    //   stack frame containing the address we're pointing at.
    // Then on the second time  through this function, we hit this if block,
    //   because there is a popup to get input from.
    // However, pNumBytes is now the address of a different stack frame, and not
    //   necessarily the same as before (presumably not at all). The
    //   Callback would try and write the number of bytes read to the
    //   value in _pdwNumBytes, and then we'd return up to ConsoleWaitBlock::Notify,
    //   who's dwNumBytes had nothing in it.
    // To fix this, when we hit this with a popup, we're going to make sure to
    //   refresh the value of _pdwNumBytes to the current address we want to put
    //   the out value into.
    // It's still really weird, but limits the potential fallout of changing a
    //   piece of old spaghetti code.
    if (_commandHistory)
    {
        if (CommandLine::Instance().HasPopup())
        {
            // (see above comment, MSFT:13994975)
            // Make sure that the popup writes the dwNumBytes to the right place
            if (pNumBytes)
            {
                _pdwNumBytes = pNumBytes;
            }

            auto& popup = CommandLine::Instance().GetPopup();
            *pReplyStatus = popup.Process(*this);
            if (*pReplyStatus == CONSOLE_STATUS_READ_COMPLETE ||
                (*pReplyStatus != CONSOLE_STATUS_WAIT && *pReplyStatus != CONSOLE_STATUS_WAIT_NO_BLOCK))
            {
                *pReplyStatus = S_OK;
                gci.SetCookedReadData(nullptr);
                return true;
            }
            return false;
        }
    }

    *pReplyStatus = Read(fIsUnicode, *pNumBytes, *pControlKeyState);
    if (*pReplyStatus != CONSOLE_STATUS_WAIT)
    {
        gci.SetCookedReadData(nullptr);
        return true;
    }
    else
    {
        return false;
    }
}

bool COOKED_READ_DATA::AtEol() const noexcept
{
    return _bytesRead == (_currentPosition * 2);
}

// Routine Description:
// - Method that actually retrieves a character/input record from the buffer (key press form)
//   and determines the next action based on the various possible cooked read modes.
// - Mode options include the F-keys popup menus, keyboard manipulation of the edit line, etc.
// - This method also does the actual copying of the final manipulated data into the return buffer.
// Arguments:
// - isUnicode - Treat as UCS-2 unicode or use Input CP to convert when done.
// - numBytes - On in, the number of bytes available in the client
// buffer. On out, the number of bytes consumed in the client buffer.
// - controlKeyState - For some types of reads, this is the modifier key state with the last button press.
[[nodiscard]] HRESULT COOKED_READ_DATA::Read(const bool isUnicode,
                                             size_t& numBytes,
                                             ULONG& controlKeyState) noexcept
{
    controlKeyState = 0;

    NTSTATUS Status = _readCharInputLoop(isUnicode, numBytes);

    // if the read was completed (status != wait), free the cooked read
    // data.  also, close the temporary output handle that was opened to
    // echo the characters read.
    if (Status != CONSOLE_STATUS_WAIT)
    {
        Status = _handlePostCharInputLoop(isUnicode, numBytes, controlKeyState);
    }

    return Status;
}

void COOKED_READ_DATA::ProcessAliases(DWORD& lineCount)
{
    Alias::s_MatchAndCopyAliasLegacy(_backupLimit,
                                     _bytesRead,
                                     _backupLimit,
                                     _bufferSize,
                                     _bytesRead,
                                     _exeName,
                                     lineCount);
}

// Routine Description:
// - This method handles the various actions that occur on the edit line like pressing keys left/right/up/down, paging, and
//   the final ENTER key press that will end the wait and finally return the data.
// Arguments:
// - pCookedReadData - Pointer to cooked read data information (edit line, client buffer, etc.)
// - wch - The most recently pressed/retrieved character from the input buffer (keystroke)
// - keyState - Modifier keys/state information with the pressed key/character
// - status - The return code to pass to the client
// Return Value:
// - true if read is completed. false if we need to keep waiting and be called again with the user's next keystroke.
bool COOKED_READ_DATA::ProcessInput(const wchar_t wchOrig,
                                    const DWORD keyState,
                                    NTSTATUS& status)
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    size_t NumSpaces = 0;
    SHORT ScrollY = 0;
    size_t NumToWrite;
    WCHAR wch = wchOrig;
    bool fStartFromDelim;

    status = STATUS_SUCCESS;
    if (_bytesRead >= (_bufferSize - (2 * sizeof(WCHAR))) && wch != UNICODE_CARRIAGERETURN && wch != UNICODE_BACKSPACE)
    {
        return false;
    }

    if (_ctrlWakeupMask != 0 && wch < L' ' && (_ctrlWakeupMask & (1 << wch)))
    {
        *_bufPtr = wch;
        _bytesRead += sizeof(WCHAR);
        _bufPtr += 1;
        _currentPosition += 1;
        _controlKeyState = keyState;
        return true;
    }

    if (wch == EXTKEY_ERASE_PREV_WORD)
    {
        wch = UNICODE_BACKSPACE;
    }

    if (AtEol())
    {
        // If at end of line, processing is relatively simple. Just store the character and write it to the screen.
        if (wch == UNICODE_BACKSPACE2)
        {
            wch = UNICODE_BACKSPACE;
        }

        if (wch != UNICODE_BACKSPACE || _bufPtr != _backupLimit)
        {
            fStartFromDelim = IsWordDelim(_bufPtr[-1]);

            bool loop = true;
            while (loop)
            {
                loop = false;
                if (_echoInput)
                {
                    NumToWrite = sizeof(WCHAR);
                    status = WriteCharsLegacy(_screenInfo,
                                              _backupLimit,
                                              _bufPtr,
                                              &wch,
                                              &NumToWrite,
                                              &NumSpaces,
                                              _originalCursorPosition.X,
                                              WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                              &ScrollY);
                    if (NT_SUCCESS(status))
                    {
                        _originalCursorPosition.Y += ScrollY;
                    }
                    else
                    {
                        RIPMSG1(RIP_WARNING, "WriteCharsLegacy failed %x", status);
                    }
                }

                _visibleCharCount += NumSpaces;
                if (wch == UNICODE_BACKSPACE && _processedInput)
                {
                    _bytesRead -= sizeof(WCHAR);
                    // clang-format off
#pragma prefast(suppress: __WARNING_POTENTIAL_BUFFER_OVERFLOW_HIGH_PRIORITY, "This access is fine")
                    // clang-format on
                    *_bufPtr = (WCHAR)' ';
                    _bufPtr -= 1;
                    _currentPosition -= 1;

                    // Repeat until it hits the word boundary
                    if (wchOrig == EXTKEY_ERASE_PREV_WORD &&
                        _bufPtr != _backupLimit &&
                        fStartFromDelim ^ !IsWordDelim(_bufPtr[-1]))
                    {
                        loop = true;
                    }
                }
                else
                {
                    *_bufPtr = wch;
                    _bytesRead += sizeof(WCHAR);
                    _bufPtr += 1;
                    _currentPosition += 1;
                }
            }
        }
    }
    else
    {
        bool CallWrite = true;
        const SHORT sScreenBufferSizeX = _screenInfo.GetBufferSize().Width();

        // processing in the middle of the line is more complex:

        // calculate new cursor position
        // store new char
        // clear the current command line from the screen
        // write the new command line to the screen
        // update the cursor position

        if (wch == UNICODE_BACKSPACE && _processedInput)
        {
            // for backspace, use writechars to calculate the new cursor position.
            // this call also sets the cursor to the right position for the
            // second call to writechars.

            if (_bufPtr != _backupLimit)
            {
                fStartFromDelim = IsWordDelim(_bufPtr[-1]);

                bool loop = true;
                while (loop)
                {
                    loop = false;
                    // we call writechar here so that cursor position gets updated
                    // correctly.  we also call it later if we're not at eol so
                    // that the remainder of the string can be updated correctly.

                    if (_echoInput)
                    {
                        NumToWrite = sizeof(WCHAR);
                        status = WriteCharsLegacy(_screenInfo,
                                                  _backupLimit,
                                                  _bufPtr,
                                                  &wch,
                                                  &NumToWrite,
                                                  nullptr,
                                                  _originalCursorPosition.X,
                                                  WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                                  nullptr);
                        if (!NT_SUCCESS(status))
                        {
                            RIPMSG1(RIP_WARNING, "WriteCharsLegacy failed %x", status);
                        }
                    }
                    _bytesRead -= sizeof(WCHAR);
                    _bufPtr -= 1;
                    _currentPosition -= 1;
                    memmove(_bufPtr,
                            _bufPtr + 1,
                            _bytesRead - (_currentPosition * sizeof(WCHAR)));
                    {
                        PWCHAR buf = (PWCHAR)((PBYTE)_backupLimit + _bytesRead);
                        *buf = (WCHAR)' ';
                    }
                    NumSpaces = 0;

                    // Repeat until it hits the word boundary
                    if (wchOrig == EXTKEY_ERASE_PREV_WORD &&
                        _bufPtr != _backupLimit &&
                        fStartFromDelim ^ !IsWordDelim(_bufPtr[-1]))
                    {
                        loop = true;
                    }
                }
            }
            else
            {
                CallWrite = false;
            }
        }
        else
        {
            // store the char
            if (wch == UNICODE_CARRIAGERETURN)
            {
                _bufPtr = (PWCHAR)((PBYTE)_backupLimit + _bytesRead);
                *_bufPtr = wch;
                _bufPtr += 1;
                _bytesRead += sizeof(WCHAR);
                _currentPosition += 1;
            }
            else
            {
                bool fBisect = false;

                if (_echoInput)
                {
                    if (CheckBisectProcessW(_screenInfo,
                                            _backupLimit,
                                            _currentPosition + 1,
                                            sScreenBufferSizeX - _originalCursorPosition.X,
                                            _originalCursorPosition.X,
                                            TRUE))
                    {
                        fBisect = true;
                    }
                }

                if (_insertMode)
                {
                    memmove(_bufPtr + 1,
                            _bufPtr,
                            _bytesRead - (_currentPosition * sizeof(WCHAR)));
                    _bytesRead += sizeof(WCHAR);
                }
                *_bufPtr = wch;
                _bufPtr += 1;
                _currentPosition += 1;

                // calculate new cursor position
                if (_echoInput)
                {
                    NumSpaces = RetrieveNumberOfSpaces(_originalCursorPosition.X,
                                                       _backupLimit,
                                                       _currentPosition - 1);
                    if (NumSpaces > 0 && fBisect)
                        NumSpaces--;
                }
            }
        }

        if (_echoInput && CallWrite)
        {
            COORD CursorPosition;

            // save cursor position
            CursorPosition = _screenInfo.GetTextBuffer().GetCursor().GetPosition();
            CursorPosition.X = (SHORT)(CursorPosition.X + NumSpaces);

            // clear the current command line from the screen
            // clang-format off
#pragma prefast(suppress: __WARNING_BUFFER_OVERFLOW, "Not sure why prefast doesn't like this call.")
            // clang-format on
            DeleteCommandLine(*this, FALSE);

            // write the new command line to the screen
            NumToWrite = _bytesRead;

            DWORD dwFlags = WC_DESTRUCTIVE_BACKSPACE | WC_ECHO;
            if (wch == UNICODE_CARRIAGERETURN)
            {
                dwFlags |= WC_KEEP_CURSOR_VISIBLE;
            }
            status = WriteCharsLegacy(_screenInfo,
                                      _backupLimit,
                                      _backupLimit,
                                      _backupLimit,
                                      &NumToWrite,
                                      &_visibleCharCount,
                                      _originalCursorPosition.X,
                                      dwFlags,
                                      &ScrollY);
            if (!NT_SUCCESS(status))
            {
                RIPMSG1(RIP_WARNING, "WriteCharsLegacy failed 0x%x", status);
                _bytesRead = 0;
                return true;
            }

            // update cursor position
            if (wch != UNICODE_CARRIAGERETURN)
            {
                if (CheckBisectProcessW(_screenInfo,
                                        _backupLimit,
                                        _currentPosition + 1,
                                        sScreenBufferSizeX - _originalCursorPosition.X,
                                        _originalCursorPosition.X,
                                        TRUE))
                {
                    if (CursorPosition.X == (sScreenBufferSizeX - 1))
                    {
                        CursorPosition.X++;
                    }
                }

                // adjust cursor position for WriteChars
                _originalCursorPosition.Y += ScrollY;
                CursorPosition.Y += ScrollY;
                status = AdjustCursorPosition(_screenInfo, CursorPosition, TRUE, nullptr);
                if (!NT_SUCCESS(status))
                {
                    _bytesRead = 0;
                    return true;
                }
            }
        }
    }

    // in cooked mode, enter (carriage return) is converted to
    // carriage return linefeed (0xda).  carriage return is always
    // stored at the end of the buffer.
    if (wch == UNICODE_CARRIAGERETURN)
    {
        if (_processedInput)
        {
            if (_bytesRead < _bufferSize)
            {
                *_bufPtr = UNICODE_LINEFEED;
                if (_echoInput)
                {
                    NumToWrite = sizeof(WCHAR);
                    status = WriteCharsLegacy(_screenInfo,
                                              _backupLimit,
                                              _bufPtr,
                                              _bufPtr,
                                              &NumToWrite,
                                              nullptr,
                                              _originalCursorPosition.X,
                                              WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                              nullptr);
                    if (!NT_SUCCESS(status))
                    {
                        RIPMSG1(RIP_WARNING, "WriteCharsLegacy failed 0x%x", status);
                    }
                }
                _bytesRead += sizeof(WCHAR);
                _bufPtr++;
                _currentPosition += 1;
            }
        }
        // reset the cursor back to 25% if necessary
        if (_lineInput)
        {
            if (_insertMode != gci.GetInsertMode())
            {
                // Make cursor small.
                LOG_IF_FAILED(CommandLine::Instance().ProcessCommandLine(*this, VK_INSERT, 0));
            }

            status = STATUS_SUCCESS;
            return true;
        }
    }

    return false;
}

// Routine Description:
// - Writes string to current position in prompt line. can overwrite text to the right of the cursor.
// Arguments:
// - wstr - the string to write
// Return Value:
// - The number of chars written
size_t COOKED_READ_DATA::Write(const std::wstring_view wstr)
{
    auto end = wstr.end();
    const size_t charsRemaining = (_bufferSize / sizeof(wchar_t)) - (_bufPtr - _backupLimit);
    if (wstr.size() > charsRemaining)
    {
        end = std::next(wstr.begin(), charsRemaining);
    }

    std::copy(wstr.begin(), end, _bufPtr);
    const size_t charsInserted = end - wstr.begin();
    size_t bytesInserted = charsInserted * sizeof(wchar_t);
    _currentPosition += charsInserted;
    _bytesRead += bytesInserted;

    if (IsEchoInput())
    {
        size_t NumSpaces = 0;
        SHORT ScrollY = 0;

        FAIL_FAST_IF_NTSTATUS_FAILED(WriteCharsLegacy(ScreenInfo(),
                                                      _backupLimit,
                                                      _bufPtr,
                                                      _bufPtr,
                                                      &bytesInserted,
                                                      &NumSpaces,
                                                      OriginalCursorPosition().X,
                                                      WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                                      &ScrollY));
        OriginalCursorPosition().Y += ScrollY;
        VisibleCharCount() += NumSpaces;
    }
    _bufPtr += charsInserted;

    return charsInserted;
}

// Routine Description:
// - saves data in the prompt buffer to the outgoing user buffer
// Arguments:
// - cch - the number of chars to write to the user buffer
// Return Value:
// - the number of bytes written to the user buffer
size_t COOKED_READ_DATA::SavePromptToUserBuffer(const size_t cch)
{
    size_t bytesToWrite = 0;
    const HRESULT hr = SizeTMult(cch, sizeof(wchar_t), &bytesToWrite);
    if (FAILED(hr))
    {
        return 0;
    }

    memmove(_userBuffer, _backupLimit, bytesToWrite);

    if (!IsUnicode())
    {
        try
        {
            const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
            const std::wstring wstr = ConvertToW(gci.CP, { reinterpret_cast<char*>(_userBuffer), cch });
            const size_t copyAmount = std::min(wstr.size(), _userBufferSize / sizeof(wchar_t));
            std::copy_n(wstr.begin(), copyAmount, _userBuffer);
            return copyAmount * sizeof(wchar_t);
        }
        CATCH_LOG();
    }
    return bytesToWrite;
}

// Routine Description:
// - saves data in the prompt buffer as pending input
// Arguments:
// - index - the index of what wchar to start the saving
// - multiline - whether the pending input should be saved as multiline or not
void COOKED_READ_DATA::SavePendingInput(const size_t index, const bool multiline)
{
    INPUT_READ_HANDLE_DATA& inputReadHandleData = *GetInputReadHandleData();
    const std::wstring_view pending{ _backupLimit + index,
                                     BytesRead() / sizeof(wchar_t) - index };
    if (multiline)
    {
        inputReadHandleData.SaveMultilinePendingInput(pending);
    }
    else
    {
        inputReadHandleData.SavePendingInput(pending);
    }
}

// Routine Description:
// - saves data in the prompt buffer as pending input
// Arguments:
// - isUnicode - Treat as UCS-2 unicode or use Input CP to convert when done.
// - numBytes - On in, the number of bytes available in the client
// buffer. On out, the number of bytes consumed in the client buffer.
// Return Value:
// - Status code that indicates success, wait, etc.
[[nodiscard]] NTSTATUS COOKED_READ_DATA::_readCharInputLoop(const bool isUnicode, size_t& numBytes) noexcept
{
    NTSTATUS Status = STATUS_SUCCESS;

    while (_bytesRead < _bufferSize)
    {
        wchar_t wch = UNICODE_NULL;
        bool commandLineEditingKeys = false;
        DWORD keyState = 0;

        // This call to GetChar may block.
        Status = GetChar(_pInputBuffer,
                         &wch,
                         true,
                         &commandLineEditingKeys,
                         nullptr,
                         &keyState);
        if (!NT_SUCCESS(Status))
        {
            if (Status != CONSOLE_STATUS_WAIT)
            {
                _bytesRead = 0;
            }
            break;
        }

        // we should probably set these up in GetChars, but we set them
        // up here because the debugger is multi-threaded and calls
        // read before outputting the prompt.

        if (_originalCursorPosition.X == -1)
        {
            _originalCursorPosition = _screenInfo.GetTextBuffer().GetCursor().GetPosition();
        }

        if (commandLineEditingKeys)
        {
            // TODO: this is super weird for command line popups only
            _unicode = isUnicode;

            _pdwNumBytes = &numBytes;

            Status = CommandLine::Instance().ProcessCommandLine(*this, wch, keyState);
            if (Status == CONSOLE_STATUS_READ_COMPLETE || Status == CONSOLE_STATUS_WAIT)
            {
                break;
            }
            if (!NT_SUCCESS(Status))
            {
                if (Status == CONSOLE_STATUS_WAIT_NO_BLOCK)
                {
                    Status = CONSOLE_STATUS_WAIT;
                }
                else
                {
                    _bytesRead = 0;
                }
                break;
            }
        }
        else
        {
            if (ProcessInput(wch, keyState, Status))
            {
                CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
                gci.Flags |= CONSOLE_IGNORE_NEXT_KEYUP;
                break;
            }
        }
    }
    return Status;
}

// Routine Description:
// - handles any tasks that need to be completed after the read input loop finishes
// Arguments:
// - isUnicode - Treat as UCS-2 unicode or use Input CP to convert when done.
// - numBytes - On in, the number of bytes available in the client
// buffer. On out, the number of bytes consumed in the client buffer.
// - controlKeyState - For some types of reads, this is the modifier key state with the last button press.
// Return Value:
// - Status code that indicates success, out of memory, etc.
[[nodiscard]] NTSTATUS COOKED_READ_DATA::_handlePostCharInputLoop(const bool isUnicode, size_t& numBytes, ULONG& controlKeyState) noexcept
{
    DWORD LineCount = 1;

    if (_echoInput)
    {
        // Figure out where real string ends (at carriage return or end of buffer).
        PWCHAR StringPtr = _backupLimit;
        size_t StringLength = _bytesRead;
        bool FoundCR = false;
        for (size_t i = 0; i < (_bytesRead / sizeof(WCHAR)); i++)
        {
            if (*StringPtr++ == UNICODE_CARRIAGERETURN)
            {
                StringLength = i * sizeof(WCHAR);
                FoundCR = true;
                break;
            }
        }

        if (FoundCR)
        {
            if (_commandHistory)
            {
                // add to command line recall list if we have a history list.
                CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
                LOG_IF_FAILED(_commandHistory->Add({ _backupLimit, StringLength / sizeof(wchar_t) },
                                                   WI_IsFlagSet(gci.Flags, CONSOLE_HISTORY_NODUP)));
            }

            // check for alias
            ProcessAliases(LineCount);
        }
    }

    bool fAddDbcsLead = false;
    size_t NumBytes = 0;
    // at this point, a->NumBytes contains the number of bytes in
    // the UNICODE string read.  UserBufferSize contains the converted
    // size of the app's buffer.
    if (_bytesRead > _userBufferSize || LineCount > 1)
    {
        if (LineCount > 1)
        {
            PWSTR Tmp;
            if (!isUnicode)
            {
                if (_pInputBuffer->IsReadPartialByteSequenceAvailable())
                {
                    fAddDbcsLead = true;
                    std::unique_ptr<IInputEvent> event = GetInputBuffer()->FetchReadPartialByteSequence(false);
                    const KeyEvent* const pKeyEvent = static_cast<const KeyEvent* const>(event.get());
                    *_userBuffer = static_cast<char>(pKeyEvent->GetCharData());
                    _userBuffer++;
                    _userBufferSize -= sizeof(wchar_t);
                }

                NumBytes = 0;
                for (Tmp = _backupLimit;
                     *Tmp != UNICODE_LINEFEED && _userBufferSize / sizeof(WCHAR) > NumBytes;
                     Tmp++)
                {
                    NumBytes += IsGlyphFullWidth(*Tmp) ? 2 : 1;
                }
            }

            // clang-format off
#pragma prefast(suppress: __WARNING_BUFFER_OVERFLOW, "LineCount > 1 means there's a UNICODE_LINEFEED")
            // clang-format on
            for (Tmp = _backupLimit; *Tmp != UNICODE_LINEFEED; Tmp++)
            {
                FAIL_FAST_IF(!(Tmp < (_backupLimit + _bytesRead)));
            }

            numBytes = (ULONG)(Tmp - _backupLimit + 1) * sizeof(*Tmp);
        }
        else
        {
            if (!isUnicode)
            {
                PWSTR Tmp;

                if (_pInputBuffer->IsReadPartialByteSequenceAvailable())
                {
                    fAddDbcsLead = true;
                    std::unique_ptr<IInputEvent> event = GetInputBuffer()->FetchReadPartialByteSequence(false);
                    const KeyEvent* const pKeyEvent = static_cast<const KeyEvent* const>(event.get());
                    *_userBuffer = static_cast<char>(pKeyEvent->GetCharData());
                    _userBuffer++;
                    _userBufferSize -= sizeof(wchar_t);
                }
                NumBytes = 0;
                size_t NumToWrite = _bytesRead;
                for (Tmp = _backupLimit;
                     NumToWrite && _userBufferSize / sizeof(WCHAR) > NumBytes;
                     Tmp++, NumToWrite -= sizeof(WCHAR))
                {
                    NumBytes += IsGlyphFullWidth(*Tmp) ? 2 : 1;
                }
            }
            numBytes = _userBufferSize;
        }

        __analysis_assume(numBytes <= _userBufferSize);
        memmove(_userBuffer, _backupLimit, numBytes);

        INPUT_READ_HANDLE_DATA* const pInputReadHandleData = GetInputReadHandleData();
        const std::wstring_view pending{ _backupLimit + (numBytes / sizeof(wchar_t)), (_bytesRead - numBytes) / sizeof(wchar_t) };
        if (LineCount > 1)
        {
            pInputReadHandleData->SaveMultilinePendingInput(pending);
        }
        else
        {
            pInputReadHandleData->SavePendingInput(pending);
        }
    }
    else
    {
        if (!isUnicode)
        {
            PWSTR Tmp;

            if (_pInputBuffer->IsReadPartialByteSequenceAvailable())
            {
                fAddDbcsLead = true;
                std::unique_ptr<IInputEvent> event = GetInputBuffer()->FetchReadPartialByteSequence(false);
                const KeyEvent* const pKeyEvent = static_cast<const KeyEvent* const>(event.get());
                *_userBuffer = static_cast<char>(pKeyEvent->GetCharData());
                _userBuffer++;
                _userBufferSize -= sizeof(wchar_t);

                if (_userBufferSize == 0)
                {
                    numBytes = 1;
                    return STATUS_SUCCESS;
                }
            }
            NumBytes = 0;
            size_t NumToWrite = _bytesRead;
            for (Tmp = _backupLimit;
                 NumToWrite && _userBufferSize / sizeof(WCHAR) > NumBytes;
                 Tmp++, NumToWrite -= sizeof(WCHAR))
            {
                NumBytes += IsGlyphFullWidth(*Tmp) ? 2 : 1;
            }
        }

        numBytes = _bytesRead;

        if (numBytes > _userBufferSize)
        {
            return STATUS_BUFFER_OVERFLOW;
        }

        memmove(_userBuffer, _backupLimit, numBytes);
    }
    controlKeyState = _controlKeyState;

    if (!isUnicode)
    {
        // if ansi, translate string.
        std::unique_ptr<char[]> tempBuffer;
        try
        {
            tempBuffer = std::make_unique<char[]>(NumBytes);
        }
        catch (...)
        {
            return STATUS_NO_MEMORY;
        }

        std::unique_ptr<IInputEvent> partialEvent;
        numBytes = TranslateUnicodeToOem(_userBuffer,
                                         gsl::narrow<ULONG>(numBytes / sizeof(wchar_t)),
                                         tempBuffer.get(),
                                         gsl::narrow<ULONG>(NumBytes),
                                         partialEvent);

        if (partialEvent.get())
        {
            GetInputBuffer()->StoreReadPartialByteSequence(std::move(partialEvent));
        }

        if (numBytes > _userBufferSize)
        {
            return STATUS_BUFFER_OVERFLOW;
        }

        memmove(_userBuffer, tempBuffer.get(), numBytes);
        if (fAddDbcsLead)
        {
            numBytes++;
        }
    }
    return STATUS_SUCCESS;
}
