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
    static constexpr size_t npos = static_cast<size_t>(-1);

    enum class State : uint8_t
    {
        Accumulating = 0,
        DoneWithWakeupMask,
        DoneWithCarriageReturn,
    };

    // A helper struct to ensure we keep track of _dirtyBeg while the
    // underlying _buffer is being modified by COOKED_READ_DATA.
    struct BufferState
    {
        const std::wstring& Get() const noexcept;
        std::wstring Extract() noexcept
        {
            return std::move(_buffer);
        }
        void Replace(size_t offset, size_t remove, const wchar_t* input, size_t count);
        void Replace(const std::wstring_view& str);

        size_t GetCursorPosition() const noexcept;
        void SetCursorPosition(size_t pos) noexcept;

        bool IsClean() const noexcept;
        void MarkEverythingDirty() noexcept;
        void MarkAsClean() noexcept;

        std::wstring_view GetUnmodifiedTextBeforeCursor() const noexcept;
        std::wstring_view GetUnmodifiedTextAfterCursor() const noexcept;
        std::wstring_view GetModifiedTextBeforeCursor() const noexcept;
        std::wstring_view GetModifiedTextAfterCursor() const noexcept;

    private:
        std::wstring_view _slice(size_t from, size_t to) const noexcept;

        std::wstring _buffer;
        size_t _dirtyBeg = npos;
        size_t _cursor = 0;
    };

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

    void _readCharInputLoop();
    void _handleChar(wchar_t wch, DWORD modifiers);
    void _handleVkey(uint16_t vkey, DWORD modifiers);
    void _handlePostCharInputLoop(bool isUnicode, size_t& numBytes, ULONG& controlKeyState);
    void _transitionState(State state) noexcept;
    void _flushBuffer();
    void _erase(ptrdiff_t distance) const;
    ptrdiff_t _measureChars(const std::wstring_view& text, ptrdiff_t cursorOffset) const;
    ptrdiff_t _writeChars(const std::wstring_view& text) const;
    ptrdiff_t _writeCharsImpl(const std::wstring_view& text, bool measureOnly, ptrdiff_t cursorOffset) const;
    ptrdiff_t _measureCharsUnprocessed(const std::wstring_view& text, ptrdiff_t cursorOffset) const;
    ptrdiff_t _writeCharsUnprocessed(const std::wstring_view& text) const;
    til::point _offsetPosition(til::point pos, ptrdiff_t distance) const;
    void _offsetCursorPosition(ptrdiff_t distance) const;
    void _offsetCursorPositionAlways(ptrdiff_t distance) const;
    til::CoordType _getColumnAtRelativeCursorPosition(ptrdiff_t distance) const;

    void _popupPush(PopupKind kind);
    void _popupsDone();
    void _popupHandleCopyToCharInput(Popup& popup, wchar_t wch, uint16_t vkey, DWORD modifiers);
    void _popupHandleCopyFromCharInput(Popup& popup, wchar_t wch, uint16_t vkey, DWORD modifiers);
    void _popupHandleCommandNumberInput(Popup& popup, wchar_t wch, uint16_t vkey, DWORD modifiers);
    void _popupHandleCommandListInput(Popup& popup, wchar_t wch, uint16_t vkey, DWORD modifiers);
    void _popupHandleInput(wchar_t wch, uint16_t vkey, DWORD keyState);
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

    BufferState _buffer;
    // _distanceCursor is the distance between the start of the prompt and the
    // current cursor location in columns (including wide glyph padding columns).
    ptrdiff_t _distanceCursor = 0;
    // _distanceEnd is the distance between the start of the prompt and its last
    // glyph at the end in columns (including wide glyph padding columns).
    ptrdiff_t _distanceEnd = 0;
    bool _insertMode = false;
    State _state = State::Accumulating;

    std::vector<Popup> _popups;
};
