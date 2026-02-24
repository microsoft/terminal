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

    const SCREEN_INFORMATION* GetScreenBuffer() const noexcept override;
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
    til::point_span GetBoundaries() noexcept;

private:
    static constexpr size_t CommandNumberMaxInputLength = 5;
    static constexpr size_t npos = static_cast<size_t>(-1);

    enum class State : uint8_t
    {
        Accumulating = 0,
        DoneWithWakeupMask,
        DoneWithCarriageReturn,
    };

    enum class PopupKind
    {
        // The F2 popup:
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
        // The F4 popup:
        // Erases text between the current cursor position and the first instance of a given char (but not including it).
        // It's unknown to me why this is was historically called "copy from char" as it conhost never copied anything.
        CopyFromChar,
        // The F9 popup:
        // Let's you choose to replace the current prompt with one from the command history by index.
        CommandNumber,
        // The F7 popup:
        // Let's you choose to replace the current prompt with one from the command history via a
        // visual select dialog. Among all the popups this one is the most widely used one by far.
        CommandList,
    };

    struct Popup
    {
        PopupKind kind;

        // Using a std::variant would be preferable in modern C++ but is practically equally annoying to use.
        union
        {
            // Used by PopupKind::CommandNumber
            struct
            {
                // Keep 1 char space for the trailing \0 char.
                std::array<wchar_t, CommandNumberMaxInputLength + 1> buffer;
                size_t bufferSize;
            } commandNumber;

            // Used by PopupKind::CommandList
            struct
            {
                // The previous height of the popup.
                til::CoordType height;
                // Command history index of the first row we draw in the popup.
                // A value of -1 means it hasn't been initialized yet.
                CommandHistory::Index top;
                // Command history index of the currently selected row.
                CommandHistory::Index selected;
            } commandList;
        };
    };

    struct LayoutResult
    {
        size_t offset;
        til::CoordType column = 0;
    };

    struct Line
    {
        std::wstring text;
        size_t dirtyBegOffset = 0;
        til::CoordType dirtyBegColumn = 0;
        til::CoordType columns = 0;
    };

    static size_t _wordPrev(const std::wstring_view& chars, size_t position);
    static size_t _wordNext(const std::wstring_view& chars, size_t position);

    void _readCharInputLoop();
    void _handleChar(wchar_t wch, DWORD modifiers);
    void _handleVkey(uint16_t vkey, DWORD modifiers);
    void _handlePostCharInputLoop(bool isUnicode, size_t& numBytes, ULONG& controlKeyState);
    void _transitionState(State state) noexcept;
    til::point _getViewportCursorPosition() const noexcept;
    til::point _getOriginInViewport() noexcept;
    void _replace(size_t offset, size_t remove, const wchar_t* input, size_t count);
    void _replace(const std::wstring_view& str);
    std::wstring_view _slice(size_t from, size_t to) const noexcept;
    void _setCursorPosition(size_t position) noexcept;
    void _redisplay();
    LayoutResult _layoutLine(std::wstring& output, const std::wstring_view& input, size_t inputOffset, til::CoordType columnBegin, til::CoordType columnLimit) const;
    static void _appendCUP(std::wstring& output, til::point pos);
    void _appendPopupAttr(std::wstring& output) const;

    void _popupPush(PopupKind kind);
    void _popupsDone();
    void _popupHandleCopyToCharInput(Popup& popup, wchar_t wch, uint16_t vkey, DWORD modifiers);
    void _popupHandleCopyFromCharInput(Popup& popup, wchar_t wch, uint16_t vkey, DWORD modifiers);
    void _popupHandleCommandNumberInput(Popup& popup, wchar_t wch, uint16_t vkey, DWORD modifiers);
    void _popupHandleCommandListInput(Popup& popup, wchar_t wch, uint16_t vkey, DWORD modifiers);
    void _popupHandleInput(wchar_t wch, uint16_t vkey, DWORD keyState);
    void _popupDrawPrompt(std::vector<Line>& lines, const til::CoordType width, UINT id, const std::wstring_view& prefix, const std::wstring_view& suffix) const;
    void _popupDrawCommandList(std::vector<Line>& lines, til::size size, Popup& popup) const;

    SCREEN_INFORMATION& _screenInfo;
    std::span<char> _userBuffer;
    std::wstring _exeName;
    ConsoleProcessHandle* _processHandle = nullptr;
    CommandHistory* _history = nullptr;
    ULONG _ctrlWakeupMask = 0;
    ULONG _controlKeyState = 0;
    std::unique_ptr<ConsoleHandleData> _tempHandle;

    std::wstring _buffer;
    size_t _bufferDirtyBeg = npos;
    size_t _bufferCursor = 0;
    State _state = State::Accumulating;
    bool _insertMode = false;
    bool _dirty = false;
    bool _redrawPending = false;
    bool _clearPending = false;

    std::optional<til::point> _originInViewport;
    // This value is in the pager coordinate space. (0,0) is the first character of the
    // first line, independent on where the prompt actually appears on the screen.
    // The coordinate is "end exclusive", so the last character is 1 in front of it.
    til::point _pagerPromptEnd;
    // The scroll position of the pager.
    til::CoordType _pagerContentTop = 0;
    // Contains the viewport height for which it previously was drawn for.
    til::CoordType _pagerHeight = 0;

    std::vector<Popup> _popups;
    bool _popupOpened = false;
};
