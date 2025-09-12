/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ConsoleControl.hpp

Abstract:
- This module is used for abstracting calls to private user32 DLL APIs to break the build system dependency.

Author(s):
- Michael Niksa (MiNiksa) July-2016
--*/
#pragma once

#include "../inc/IConsoleControl.hpp"

// Uncomment to build publicly targeted scenarios.
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
        // IConsoleControl Members
        void Control(ControlType command, PVOID ptr, DWORD len) noexcept override;
        void NotifyWinEvent(DWORD event, HWND hwnd, LONG idObject, LONG idChild) noexcept override;
        void NotifyConsoleApplication(_In_ DWORD dwProcessId) noexcept override;
        void SetForeground(_In_ HANDLE hProcess, _In_ BOOL fForeground) noexcept override;
        void EndTask(_In_ DWORD dwProcessId, _In_ DWORD dwEventType, _In_ ULONG ulCtrlFlags) noexcept override;
        void SetWindowOwner(HWND hwnd, DWORD processId, DWORD threadId) noexcept override;

        BOOL EnterReaderModeHelper(_In_ HWND hwnd);

        BOOL TranslateMessageEx(const MSG* pmsg,
                                _In_ UINT flags);

#ifdef CON_USERPRIVAPI_INDIRECT
        ConsoleControl();

    private:
        using PfnConsoleControl = NTSTATUS(WINAPI*)(ControlType, PVOID, DWORD);
        using PfnEnterReaderModeHelper = BOOL(WINAPI*)(HWND);
        using PfnTranslateMessageEx = BOOL(WINAPI*)(const MSG*, UINT);
        PfnConsoleControl _consoleControl = nullptr;
        PfnEnterReaderModeHelper _enterReaderModeHelper = nullptr;
        PfnTranslateMessageEx _translateMessageEx = nullptr;
#endif
    };
}
