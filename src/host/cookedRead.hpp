
#pragma once

#include "readData.hpp"
#include "screenInfo.hpp"
#include "history.h"

class CookedRead final : public ReadData
{
public:

    CookedRead(InputBuffer* const pInputBuffer,
               INPUT_READ_HANDLE_DATA* const pInputReadHandleData,
               SCREEN_INFORMATION& screenInfo,
               CommandHistory* const pCommandHistory,
               wchar_t* userBuffer,
               const size_t cchUserBuffer,
               const ULONG ctrlWakeupMask
        );

    bool Notify(const WaitTerminationReason TerminationReason,
                const bool fIsUnicode,
                _Out_ NTSTATUS* const pReplyStatus,
                _Out_ size_t* const pNumBytes,
                _Out_ DWORD* const pControlKeyState,
                _Out_ void* const pOutputData);

    [[nodiscard]]
    NTSTATUS Read(const bool isUnicode,
                  size_t& numBytes,
                  ULONG& controlKeyState) noexcept;

    void Erase();


    void SetInsertMode(const bool mode) noexcept;

    COORD PromptStartLocation() const noexcept;
    size_t VisibleCharCount() const;

private:
    enum class ReadState
    {
        Ready,
        Wait,
        Error,
        GotChar,
        Complete
    };


    bool _isTailSurrogatePair() const;
    bool _isCtrlWakeupMaskTriggered(const wchar_t wch) const noexcept;

    bool _insertMode;


    wchar_t* _userBuffer;
    size_t _cchUserBuffer;
    SCREEN_INFORMATION& _screenInfo;
    std::wstring _prompt;
    COORD _promptStartLocation;
    CommandHistory* const _pCommandHistory; // non-ownership pointer
    const ULONG _ctrlWakeupMask;
    ReadState _state;
    NTSTATUS _status;


    void _readChar();
    void _process();
    void _error();
    void _wait();
    void _complete(size_t& numBytes);
};
