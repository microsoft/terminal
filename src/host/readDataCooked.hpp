// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "readData.hpp"
#include "history.h"

class COOKED_READ_DATA final : public ReadData
{
public:
    COOKED_READ_DATA(_In_ InputBuffer* pInputBuffer,
                     _In_ INPUT_READ_HANDLE_DATA* pInputReadHandleData,
                     SCREEN_INFORMATION& screenInfo,
                     _In_ size_t UserBufferSize,
                     _In_ char* UserBuffer,
                     _In_ ULONG CtrlWakeupMask,
                     _In_ std::wstring_view exeName,
                     _In_ std::wstring_view initialData,
                     _In_ ConsoleProcessHandle* pClientProcess);

    void MigrateUserBuffersOnTransitionToBackgroundWait(const void* oldBuffer, void* newBuffer) noexcept override;

    bool Notify(WaitTerminationReason TerminationReason,
                bool fIsUnicode,
                _Out_ NTSTATUS* pReplyStatus,
                _Out_ size_t* pNumBytes,
                _Out_ DWORD* pControlKeyState,
                _Out_ void* pOutputData) noexcept override;

    bool Read(bool isUnicode, size_t& numBytes, ULONG& controlKeyState);

    void EraseBeforeResize();
    void RedrawAfterResize();

    void SetInsertMode(bool insertMode) noexcept;
    bool IsEmpty() const noexcept;
    bool PresentingPopup() const noexcept;
    til::point_span GetBoundaries() const noexcept;

private:
    static constexpr uint8_t CommandNumberMaxInputLength = 5;

    enum class PopupKind
    {
        // Copies text from the previous command between the current cursor position and the first instance
        // of a given char (but not including it) into the current prompt line at the current cursor position.
        // Basically, F3 and this prompt have identical behavior, but the prompt searches for a terminating character.
        //
        // Let's say your last command was:
        //   echo hello
        // And you type the following with the cursor at "^":
        //   echo abcd efgh
        //       ^
        // Then this command, given the char "o" will turn it into
        //   echo hell efgh
        CopyToChar,
        // Erases text between the current cursor position and the first instance of a given char (but not including it).
        // It's unknown to me why this is was historically called "copy from char" as it conhost never copied anything.
        CopyFromChar,
        // Let's you choose to replace the current prompt with one from the command history by index.
        CommandNumber,
        // Let's you choose to replace the current prompt with one from the command history via a
        // visual select dialog. Among all the popups this one is the most widely used one by far.
        CommandList,
    };

    struct Popup
    {
        PopupKind kind;

        // The inner rectangle of the popup, excluding the border that we draw.
        // In absolute TextBuffer coordinates.
        til::rect contentRect;
        // The area we've backed up and need to restore when we dismiss the popup.
        // It'll practically always be 1 larger than contentRect in all 4 directions.
        Microsoft::Console::Types::Viewport backupRect;
        // The backed up buffer contents. Uses CHAR_INFO for convenience.
        std::vector<CHAR_INFO> backup;

        // Using a std::variant would be preferable in modern C++ but is practically equally annoying to use.
        union
        {
            // Used by PopupKind::CommandNumber
            struct
            {
                // Keep 1 char space for the trailing \0 char.
                std::array<wchar_t, CommandNumberMaxInputLength + 1> buffer;
                uint8_t bufferSize;
            } commandNumber;

            // Used by PopupKind::CommandList
            struct
            {
                // Command history index of the first row we draw in the popup.
                CommandHistory::Index top;
                // Command history index of the currently selected row.
                CommandHistory::Index selected;
                // Tracks the part of the popup that has previously been drawn and needs to be redrawn in the next paint.
                // This becomes relevant when the length of the history changes while the popup is open (= when deleting entries).
                til::CoordType dirtyHeight;
            } commandList;
        };
    };

    static size_t _wordPrev(const std::wstring_view& chars, size_t position);
    static size_t _wordNext(const std::wstring_view& chars, size_t position);

    const std::wstring_view& _newlineSuffix() const noexcept;
    bool _readCharInputLoop();
    bool _handleChar(wchar_t wch, DWORD modifiers);
    void _handleVkey(uint16_t vkey, DWORD modifiers);
    void _handlePostCharInputLoop(bool isUnicode, size_t& numBytes, ULONG& controlKeyState);
    void _markAsDirty();
    void _flushBuffer();
    void _erase(ptrdiff_t distance) const;
    ptrdiff_t _writeChars(const std::wstring_view& text) const;
    til::point _offsetPosition(til::point pos, ptrdiff_t distance) const;
    void _unwindCursorPosition(ptrdiff_t distance) const;
    void _replaceBuffer(const std::wstring_view& str);

    void _popupPush(PopupKind kind);
    void _popupsDone();
    void _popupHandleCopyToCharInput(Popup& popup, wchar_t wch, uint16_t vkey, DWORD modifiers);
    void _popupHandleCopyFromCharInput(Popup& popup, wchar_t wch, uint16_t vkey, DWORD modifiers);
    void _popupHandleCommandNumberInput(Popup& popup, wchar_t wch, uint16_t vkey, DWORD modifiers);
    bool _popupHandleCommandListInput(Popup& popup, wchar_t wch, uint16_t vkey, DWORD modifiers);
    bool _popupHandleInput(wchar_t wch, uint16_t vkey, DWORD keyState);
    void _popupDrawPrompt(const Popup& popup, UINT id) const;
    void _popupDrawCommandList(Popup& popup) const;

    SCREEN_INFORMATION& _screenInfo;
    std::span<char> _userBuffer;
    std::wstring _exeName;
    ConsoleProcessHandle* _processHandle = nullptr;
    CommandHistory* _history = nullptr;
    ULONG _ctrlWakeupMask = 0;
    ULONG _controlKeyState = 0;
    std::unique_ptr<ConsoleHandleData> _tempHandle;

    std::wstring _buffer;
    size_t _bufferCursor = 0;
    ptrdiff_t _distanceCursor = 0;
    ptrdiff_t _distanceEnd = 0;
    bool _bufferDirty = false;
    bool _insertMode = false;

    std::vector<Popup> _popups;
};
