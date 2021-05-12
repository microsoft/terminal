/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- readDataCooked.hpp

Abstract:
- This file defines the read data structure for reading the command line.
- Cooked reads specifically refer to when the console host acts as a command line on behalf
  of another console application (e.g. aliases, command history, completion, line manipulation, etc.)
- The data struct will help store context across multiple calls or in the case of a wait condition.
- Wait conditions happen frequently for cooked reads because they're virtually always waiting for
  the user to finish "manipulating" the edit line before hitting enter and submitting the final
  result to the client application.
- A cooked read is also limited specifically to string/textual information. Only keyboard-type input applies.
- This can be triggered via ReadConsole A/W and ReadFile A/W calls.

Author:
- Austin Diviness (AustDi) 1-Mar-2017
- Michael Niksa (MiNiksa) 1-Mar-2017

Revision History:
- Pulled from original authoring by Therese Stowell (ThereseS, 1990)
- Separated from cmdline.h/cmdline.cpp (AustDi, 2017)
--*/

#pragma once

#include "readData.hpp"
#include "history.h"

class COOKED_READ_DATA final : public ReadData
{
public:
    COOKED_READ_DATA(_In_ InputBuffer* const pInputBuffer,
                     _In_ INPUT_READ_HANDLE_DATA* const pInputReadHandleData,
                     SCREEN_INFORMATION& screenInfo,
                     _In_ size_t UserBufferSize,
                     _In_ PWCHAR UserBuffer,
                     _In_ ULONG CtrlWakeupMask,
                     _In_ CommandHistory* CommandHistory,
                     const std::wstring_view exeName,
                     const std::string_view initialData);

    ~COOKED_READ_DATA() override;
    COOKED_READ_DATA(COOKED_READ_DATA&&) = default;

    bool AtEol() const noexcept;

    void MigrateUserBuffersOnTransitionToBackgroundWait(const void* oldBuffer, void* newBuffer) override;

    bool Notify(const WaitTerminationReason TerminationReason,
                const bool fIsUnicode,
                _Out_ NTSTATUS* const pReplyStatus,
                _Out_ size_t* const pNumBytes,
                _Out_ DWORD* const pControlKeyState,
                _Out_ void* const pOutputData) override;

    gsl::span<wchar_t> SpanAtPointer();
    gsl::span<wchar_t> SpanWholeBuffer();

    size_t Write(const std::wstring_view wstr);

    void ProcessAliases(DWORD& lineCount);

    [[nodiscard]] HRESULT Read(const bool isUnicode,
                               size_t& numBytes,
                               ULONG& controlKeyState) noexcept;

    bool ProcessInput(const wchar_t wch,
                      const DWORD keyState,
                      NTSTATUS& status);

    CommandHistory& History() noexcept;
    bool HasHistory() const noexcept;

    const size_t& VisibleCharCount() const noexcept;
    size_t& VisibleCharCount() noexcept;

    SCREEN_INFORMATION& ScreenInfo() noexcept;

    const COORD& OriginalCursorPosition() const noexcept;
    COORD& OriginalCursorPosition() noexcept;

    COORD& BeforeDialogCursorPosition() noexcept;

    bool IsEchoInput() const noexcept;
    bool IsInsertMode() const noexcept;
    void SetInsertMode(const bool mode) noexcept;
    bool IsUnicode() const noexcept;

    size_t UserBufferSize() const noexcept;

    wchar_t* BufferStartPtr() noexcept;
    wchar_t* BufferCurrentPtr() noexcept;
    void SetBufferCurrentPtr(wchar_t* ptr) noexcept;

    const size_t& BytesRead() const noexcept;
    size_t& BytesRead() noexcept;

    const size_t& InsertionPoint() const noexcept;
    size_t& InsertionPoint() noexcept;

    void SetReportedByteCount(const size_t count) noexcept;

    void Erase() noexcept;
    size_t SavePromptToUserBuffer(const size_t cch);
    void SavePendingInput(const size_t cch, const bool multiline);

#if UNIT_TESTING
    friend class CommandLineTests;
    friend class CopyToCharPopupTests;
    friend class CommandNumberPopupTests;
    friend class CommandListPopupTests;
    friend class PopupTestHelper;
#endif

private:
    size_t _bufferSize; // size in bytes
    size_t _bytesRead;

    // insertion position into the buffer (where the conceptual prompt cursor is)
    size_t _currentPosition; // char position, not byte position

    wchar_t* _bufPtr; // current position to insert chars at

    // should be const. the first char of the buffer
    wchar_t* _backupLimit;

    size_t _userBufferSize; // doubled size in ansi case
    wchar_t* _userBuffer;

    size_t* _pdwNumBytes;

    std::unique_ptr<byte[]> _buffer;
    std::wstring _exeName;
    std::unique_ptr<ConsoleHandleData> _tempHandle;

    // TODO MSFT:11285829 make this something other than a deletable pointer
    // non-ownership pointer
    CommandHistory* _commandHistory;

    ULONG _controlKeyState;
    ULONG _ctrlWakeupMask;
    size_t _visibleCharCount; // TODO MSFT:11285829 is this cells or glyphs? ie. is a wide char counted as 1 or 2?
    SCREEN_INFORMATION& _screenInfo;

    // Note that cookedReadData's _originalCursorPosition is the position before ANY text was entered on the edit line.
    COORD _originalCursorPosition;
    COORD _beforeDialogCursorPosition; // Currently only used for F9 (ProcessCommandNumberInput) since it's the only pop-up to move the cursor when it starts.

    const bool _echoInput;
    const bool _lineInput;
    const bool _processedInput;
    bool _insertMode;
    bool _unicode;

    [[nodiscard]] NTSTATUS _readCharInputLoop(const bool isUnicode, size_t& numBytes) noexcept;

    [[nodiscard]] NTSTATUS _handlePostCharInputLoop(const bool isUnicode, size_t& numBytes, ULONG& controlKeyState) noexcept;
};
