/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- conwinuserrefs.h

Abstract:
- Contains private definitions from WinUserK.h that we'll need to publish.
--*/

#pragma once

#pragma region WinUserK.h(private internal)

extern "C" {
/* WinUserK */
/*
    * Console window startup optimization.
    */

typedef enum _CONSOLECONTROL
{
    Reserved1,
    ConsoleNotifyConsoleApplication,
    Reserved2,
    ConsoleSetCaretInfo,
    Reserved3,
    ConsoleSetForeground,
    ConsoleSetWindowOwner,
    ConsoleEndTask,
} CONSOLECONTROL;

//
// CtrlFlags definitions
//
#define CONSOLE_CTRL_C_FLAG 0x00000001
#define CONSOLE_CTRL_BREAK_FLAG 0x00000002
#define CONSOLE_CTRL_CLOSE_FLAG 0x00000004

#define CONSOLE_CTRL_LOGOFF_FLAG 0x00000010
#define CONSOLE_CTRL_SHUTDOWN_FLAG 0x00000020

typedef struct _CONSOLEENDTASK
{
    HANDLE ProcessId;
    HWND hwnd;
    ULONG ConsoleEventCode;
    ULONG ConsoleFlags;
} CONSOLEENDTASK, *PCONSOLEENDTASK;

typedef struct _CONSOLEWINDOWOWNER
{
    HWND hwnd;
    ULONG ProcessId;
    ULONG ThreadId;
} CONSOLEWINDOWOWNER, *PCONSOLEWINDOWOWNER;

typedef struct _CONSOLESETFOREGROUND
{
    HANDLE hProcess;
    BOOL bForeground;
} CONSOLESETFOREGROUND, *PCONSOLESETFOREGROUND;

/*
    * Console window startup optimization.
    */
#define CPI_NEWPROCESSWINDOW 0x0001

typedef struct _CONSOLE_PROCESS_INFO
{
    IN DWORD dwProcessID;
    IN DWORD dwFlags;
} CONSOLE_PROCESS_INFO, *PCONSOLE_PROCESS_INFO;

typedef struct _CONSOLE_CARET_INFO
{
    IN HWND hwnd;
    IN RECT rc;
} CONSOLE_CARET_INFO, *PCONSOLE_CARET_INFO;

NTSTATUS ConsoleControl(
    __in CONSOLECONTROL Command,
    __in_bcount_opt(ConsoleInformationLength) PVOID ConsoleInformation,
    __in DWORD ConsoleInformationLength);

/* END WinUserK */
};
#pragma endregion
