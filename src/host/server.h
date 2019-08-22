/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- server.h

Abstract:
- This module contains the internal structures and definitions used by the console server.

Author:
- Therese Stowell (ThereseS) 12-Nov-1990

Revision History:
--*/

#pragma once

#include "IIoProvider.hpp"

#include "settings.hpp"

#include "conimeinfo.h"
#include "..\terminal\adapter\MouseInput.hpp"
#include "VtIo.hpp"
#include "CursorBlinker.hpp"

#include "..\server\ProcessList.h"
#include "..\server\WaitQueue.h"

#include "..\host\RenderData.hpp"

#include "..\inc\IDefaultColorProvider.hpp"

// clang-format off
// Flags flags
#define CONSOLE_IS_ICONIC               0x00000001
#define CONSOLE_OUTPUT_SUSPENDED        0x00000002
#define CONSOLE_HAS_FOCUS               0x00000004
#define CONSOLE_IGNORE_NEXT_MOUSE_INPUT 0x00000008
#define CONSOLE_SELECTING               0x00000010
#define CONSOLE_SCROLLING               0x00000020
// unused (CONSOLE_DISABLE_CLOSE)       0x00000040
// unused (CONSOLE_USE_POLY_TEXT)       0x00000080

// Removed Oct 2017 - added a headless mode, which revealed that the consumption
//      of this flag was redundant.
// unused (CONSOLE_NO_WINDOW)               0x00000100

// unused (CONSOLE_VDM_REGISTERED)      0x00000200
#define CONSOLE_UPDATING_SCROLL_BARS    0x00000400
#define CONSOLE_QUICK_EDIT_MODE         0x00000800
#define CONSOLE_CONNECTED_TO_EMULATOR   0x00002000
// unused (CONSOLE_FULLSCREEN_NOPAINT)  0x00004000
#define CONSOLE_QUIT_POSTED             0x00008000
#define CONSOLE_AUTO_POSITION           0x00010000
#define CONSOLE_IGNORE_NEXT_KEYUP       0x00020000
// unused (CONSOLE_WOW_REGISTERED)      0x00040000
#define CONSOLE_HISTORY_NODUP           0x00100000
#define CONSOLE_SCROLLBAR_TRACKING      0x00200000
#define CONSOLE_SETTING_WINDOW_SIZE     0x00800000
// unused (CONSOLE_VDM_HIDDEN_WINDOW)   0x01000000
// unused (CONSOLE_OS2_REGISTERED)      0x02000000
// unused (CONSOLE_OS2_OEM_FORMAT)      0x04000000
// unused (CONSOLE_JUST_VDM_UNREGISTERED)  0x08000000
// unused (CONSOLE_FULLSCREEN_INITIALIZED) 0x10000000
#define CONSOLE_USE_PRIVATE_FLAGS       0x20000000
// unused (CONSOLE_TSF_ACTIVATED)       0x40000000
#define CONSOLE_INITIALIZED             0x80000000

#define CONSOLE_SUSPENDED (CONSOLE_OUTPUT_SUSPENDED)
// clang-format on

class COOKED_READ_DATA;
class CommandHistory;

class CONSOLE_INFORMATION :
    public Settings,
    public Microsoft::Console::IIoProvider,
    public Microsoft::Console::IDefaultColorProvider
{
public:
    CONSOLE_INFORMATION();
    ~CONSOLE_INFORMATION();
    CONSOLE_INFORMATION(const CONSOLE_INFORMATION& c) = delete;
    CONSOLE_INFORMATION& operator=(const CONSOLE_INFORMATION& c) = delete;

    ConsoleProcessList ProcessHandleList;
    InputBuffer* pInputBuffer;

    SCREEN_INFORMATION* ScreenBuffers; // singly linked list
    ConsoleWaitQueue OutputQueue;

    DWORD Flags;

    std::atomic<WORD> PopupCount;

    // the following fields are used for ansi-unicode translation
    UINT CP;
    UINT OutputCP;

    ULONG CtrlFlags; // indicates outstanding ctrl requests
    ULONG LimitingProcessId;

    CPINFO CPInfo;
    CPINFO OutputCPInfo;

    ConsoleImeInfo ConsoleIme;

    Microsoft::Console::VirtualTerminal::MouseInput terminalMouseInput;

    void LockConsole();
    bool TryLockConsole();
    void UnlockConsole();
    bool IsConsoleLocked() const;
    ULONG GetCSRecursionCount();

    Microsoft::Console::VirtualTerminal::VtIo* GetVtIo();

    static void HandleTerminalKeyEventCallback(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& events);

    SCREEN_INFORMATION& GetActiveOutputBuffer() override;
    const SCREEN_INFORMATION& GetActiveOutputBuffer() const override;
    bool HasActiveOutputBuffer() const;

    InputBuffer* const GetActiveInputBuffer() const;

    bool IsInVtIoMode() const;
    bool HasPendingCookedRead() const noexcept;
    const COOKED_READ_DATA& CookedReadData() const noexcept;
    COOKED_READ_DATA& CookedReadData() noexcept;
    void SetCookedReadData(COOKED_READ_DATA* readData) noexcept;

    COLORREF GetDefaultForeground() const noexcept;
    COLORREF GetDefaultBackground() const noexcept;

    void SetTitle(const std::wstring_view newTitle);
    void SetTitlePrefix(const std::wstring& newTitlePrefix);
    void SetOriginalTitle(const std::wstring& originalTitle);
    void SetLinkTitle(const std::wstring& linkTitle);
    const std::wstring& GetTitle() const noexcept;
    const std::wstring& GetOriginalTitle() const noexcept;
    const std::wstring& GetLinkTitle() const noexcept;
    const std::wstring GetTitleAndPrefix() const;

    [[nodiscard]] static NTSTATUS AllocateConsole(const std::wstring_view title);
    // MSFT:16886775 : get rid of friends
    friend void SetActiveScreenBuffer(_Inout_ SCREEN_INFORMATION& screenInfo);
    friend class SCREEN_INFORMATION;
    friend class CommonState;
    Microsoft::Console::CursorBlinker& GetCursorBlinker() noexcept;

    CHAR_INFO AsCharInfo(const OutputCellView& cell) const noexcept;

    RenderData renderData;

private:
    CRITICAL_SECTION _csConsoleLock; // serialize input and output using this
    std::wstring _Title;
    std::wstring _TitlePrefix; // Eg Select, Mark - things that we manually prepend to the title.
    std::wstring _OriginalTitle;
    std::wstring _LinkTitle; // Path to .lnk file
    SCREEN_INFORMATION* pCurrentScreenBuffer;
    COOKED_READ_DATA* _cookedReadData; // non-ownership pointer

    Microsoft::Console::VirtualTerminal::VtIo _vtIo;
    Microsoft::Console::CursorBlinker _blinker;
};

#define ConsoleLocked() (ServiceLocator::LocateGlobals()->getConsoleInformation()->ConsoleLock.OwningThread == NtCurrentTeb()->ClientId.UniqueThread)

#define CONSOLE_STATUS_WAIT 0xC0030001
#define CONSOLE_STATUS_READ_COMPLETE 0xC0030002
#define CONSOLE_STATUS_WAIT_NO_BLOCK 0xC0030003

#include "..\server\ObjectHandle.h"

void SetActiveScreenBuffer(SCREEN_INFORMATION& screenInfo);
