/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- userdpiapi.hpp

Abstract:
- This module is used for abstracting calls to private user32 DLL APIs to break the build system dependency.

Author(s):
- Michael Niksa (MiNiksa) July-2016
--*/
#pragma once

#include "..\inc\IConsoleControl.hpp"

// Uncomment to build publically targeted scenarios.
//#define CON_USERPRIVAPI_INDIRECT

// Used by TranslateMessageEx to purposefully return false to certain WM_KEYDOWN/WM_CHAR messages
#define TM_POSTCHARBREAKS 0x0002

// Used by window structures to place our special frozen-console painting data
#define GWL_CONSOLE_WNDALLOC (3 * sizeof(DWORD))

// Used for pre-resize querying of the new scaled size of a window when the DPI is about to change.
#define WM_GETDPISCALEDSIZE 0x02E4

namespace Microsoft::Console::Interactivity::Win32
{
    class ConsoleControl final : public IConsoleControl
    {
    public:
        enum ControlType
        {
            ConsoleSetVDMCursorBounds,
            ConsoleNotifyConsoleApplication,
            ConsoleFullscreenSwitch,
            ConsoleSetCaretInfo,
            ConsoleSetReserveKeys,
            ConsoleSetForeground,
            ConsoleSetWindowOwner,
            ConsoleEndTask,
        };

        // IConsoleControl Members
        [[nodiscard]] NTSTATUS NotifyConsoleApplication(_In_ DWORD dwProcessId);
        [[nodiscard]] NTSTATUS SetForeground(_In_ HANDLE hProcess, _In_ BOOL fForeground);
        [[nodiscard]] NTSTATUS EndTask(_In_ HANDLE hProcessId, _In_ DWORD dwEventType, _In_ ULONG ulCtrlFlags);

        // Public Members
        [[nodiscard]] NTSTATUS Control(_In_ ConsoleControl::ControlType ConsoleCommand,
                                       _In_reads_bytes_(ConsoleInformationLength) PVOID ConsoleInformation,
                                       _In_ DWORD ConsoleInformationLength);

        BOOL EnterReaderModeHelper(_In_ HWND hwnd);

        BOOL TranslateMessageEx(const MSG* pmsg,
                                _In_ UINT flags);

#ifdef CON_USERPRIVAPI_INDIRECT
        ConsoleControl();
        ~ConsoleControl();

    private:
        HMODULE _hUser32;
#endif
    };
}
