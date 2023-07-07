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
#include "VtIo.hpp"
#include "CursorBlinker.hpp"

#include "../server/ProcessList.h"
#include "../server/WaitQueue.h"

#include "../host/RenderData.hpp"
#include "../audio/midi/MidiAudio.hpp"

#include <til/ticket_lock.h>

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
    public Microsoft::Console::IIoProvider
{
public:
    CONSOLE_INFORMATION() = default;
    CONSOLE_INFORMATION(const CONSOLE_INFORMATION& c) = delete;
    CONSOLE_INFORMATION& operator=(const CONSOLE_INFORMATION& c) = delete;

    ConsoleProcessList ProcessHandleList;
    InputBuffer* pInputBuffer = nullptr;

    SCREEN_INFORMATION* ScreenBuffers = nullptr; // singly linked list
    ConsoleWaitQueue OutputQueue;

    DWORD Flags = 0;

    std::atomic<WORD> PopupCount = 0;

    // the following fields are used for ansi-unicode translation
    UINT CP = 0;
    UINT OutputCP = 0;

    ULONG CtrlFlags = 0; // indicates outstanding ctrl requests
    ULONG LimitingProcessId = 0;

    CPINFO CPInfo = {};
    CPINFO OutputCPInfo = {};

    ConsoleImeInfo ConsoleIme;

    void LockConsole() noexcept;
    void UnlockConsole() noexcept;
    bool IsConsoleLocked() const noexcept;
    ULONG GetCSRecursionCount() const noexcept;

    Microsoft::Console::VirtualTerminal::VtIo* GetVtIo();

    SCREEN_INFORMATION& GetActiveOutputBuffer() override;
    const SCREEN_INFORMATION& GetActiveOutputBuffer() const override;
    void SetActiveOutputBuffer(SCREEN_INFORMATION& screenBuffer);
    bool HasActiveOutputBuffer() const;

    InputBuffer* const GetActiveInputBuffer() const override;

    bool IsInVtIoMode() const;
    bool HasPendingCookedRead() const noexcept;
    const COOKED_READ_DATA& CookedReadData() const noexcept;
    COOKED_READ_DATA& CookedReadData() noexcept;
    void SetCookedReadData(COOKED_READ_DATA* readData) noexcept;

    bool GetBracketedPasteMode() const noexcept;
    void SetBracketedPasteMode(const bool enabled) noexcept;

    void SetTitle(const std::wstring_view newTitle);
    void SetTitlePrefix(const std::wstring_view newTitlePrefix);
    void SetOriginalTitle(const std::wstring_view originalTitle);
    void SetLinkTitle(const std::wstring_view linkTitle);
    const std::wstring_view GetTitle() const noexcept;
    const std::wstring_view GetOriginalTitle() const noexcept;
    const std::wstring_view GetLinkTitle() const noexcept;
    const std::wstring_view GetTitleAndPrefix() const;

    [[nodiscard]] static NTSTATUS AllocateConsole(const std::wstring_view title);
    // MSFT:16886775 : get rid of friends
    friend void SetActiveScreenBuffer(_Inout_ SCREEN_INFORMATION& screenInfo);
    friend class SCREEN_INFORMATION;
    friend class CommonState;
    Microsoft::Console::CursorBlinker& GetCursorBlinker() noexcept;

    MidiAudio& GetMidiAudio();

    CHAR_INFO AsCharInfo(const OutputCellView& cell) const noexcept;

    RenderData renderData;

private:
    til::recursive_ticket_lock _lock;

    std::wstring _Title;
    std::wstring _Prefix; // Eg Select, Mark - things that we manually prepend to the title.
    std::wstring _TitleAndPrefix;
    std::wstring _OriginalTitle;
    std::wstring _LinkTitle; // Path to .lnk file
    SCREEN_INFORMATION* pCurrentScreenBuffer = nullptr;
    COOKED_READ_DATA* _cookedReadData = nullptr; // non-ownership pointer
    bool _bracketedPasteMode = false;

    Microsoft::Console::VirtualTerminal::VtIo _vtIo;
    Microsoft::Console::CursorBlinker _blinker;
    MidiAudio _midiAudio;
};

#define ConsoleLocked() (ServiceLocator::LocateGlobals()->getConsoleInformation()->ConsoleLock.OwningThread == NtCurrentTeb()->ClientId.UniqueThread)

#define CONSOLE_STATUS_WAIT 0xC0030001
#define CONSOLE_STATUS_READ_COMPLETE 0xC0030002
#define CONSOLE_STATUS_WAIT_NO_BLOCK 0xC0030003

#include "../server/ObjectHandle.h"

void SetActiveScreenBuffer(SCREEN_INFORMATION& screenInfo);
