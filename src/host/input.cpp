// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "input.h"

#include "dbcs.h"
#include "stream.h"

#include "..\terminal\input\terminalInput.hpp"

#include "..\interactivity\inc\ServiceLocator.hpp"

#pragma hdrstop

#define KEY_ENHANCED 0x01000000

using Microsoft::Console::Interactivity::ServiceLocator;

bool IsInProcessedInputMode()
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return (gci.pInputBuffer->InputMode & ENABLE_PROCESSED_INPUT) != 0;
}

bool IsInVirtualTerminalInputMode()
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return WI_IsFlagSet(gci.pInputBuffer->InputMode, ENABLE_VIRTUAL_TERMINAL_INPUT);
}

BOOL IsSystemKey(const WORD wVirtualKeyCode)
{
    switch (wVirtualKeyCode)
    {
    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU:
    case VK_PAUSE:
    case VK_CAPITAL:
    case VK_LWIN:
    case VK_RWIN:
    case VK_NUMLOCK:
    case VK_SCROLL:
        return TRUE;
    }
    return FALSE;
}

ULONG GetControlKeyState(const LPARAM lParam)
{
    ULONG ControlKeyState = 0;

    if (ServiceLocator::LocateInputServices()->GetKeyState(VK_LMENU) & KEY_PRESSED)
    {
        ControlKeyState |= LEFT_ALT_PRESSED;
    }
    if (ServiceLocator::LocateInputServices()->GetKeyState(VK_RMENU) & KEY_PRESSED)
    {
        ControlKeyState |= RIGHT_ALT_PRESSED;
    }
    if (ServiceLocator::LocateInputServices()->GetKeyState(VK_LCONTROL) & KEY_PRESSED)
    {
        ControlKeyState |= LEFT_CTRL_PRESSED;
    }
    if (ServiceLocator::LocateInputServices()->GetKeyState(VK_RCONTROL) & KEY_PRESSED)
    {
        ControlKeyState |= RIGHT_CTRL_PRESSED;
    }
    if (ServiceLocator::LocateInputServices()->GetKeyState(VK_SHIFT) & KEY_PRESSED)
    {
        ControlKeyState |= SHIFT_PRESSED;
    }
    if (ServiceLocator::LocateInputServices()->GetKeyState(VK_NUMLOCK) & KEY_TOGGLED)
    {
        ControlKeyState |= NUMLOCK_ON;
    }
    if (ServiceLocator::LocateInputServices()->GetKeyState(VK_SCROLL) & KEY_TOGGLED)
    {
        ControlKeyState |= SCROLLLOCK_ON;
    }
    if (ServiceLocator::LocateInputServices()->GetKeyState(VK_CAPITAL) & KEY_TOGGLED)
    {
        ControlKeyState |= CAPSLOCK_ON;
    }
    if (lParam & KEY_ENHANCED)
    {
        ControlKeyState |= ENHANCED_KEY;
    }

    ControlKeyState |= (lParam & ALTNUMPAD_BIT);

    return ControlKeyState;
}

// Routine Description:
// - returns true if we're in a mode amenable to us taking over keyboard shortcuts
bool ShouldTakeOverKeyboardShortcuts()
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return !gci.GetCtrlKeyShortcutsDisabled() && IsInProcessedInputMode();
}

// Routine Description:
// - handles key events without reference to Win32 elements.
void HandleGenericKeyEvent(_In_ KeyEvent keyEvent, const bool generateBreak)
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    bool ContinueProcessing = true;

    if (keyEvent.IsCtrlPressed() &&
        !keyEvent.IsAltPressed() &&
        keyEvent.IsKeyDown())
    {
        // check for ctrl-c, if in line input mode.
        if (keyEvent.GetVirtualKeyCode() == 'C' && IsInProcessedInputMode())
        {
            HandleCtrlEvent(CTRL_C_EVENT);
            if (gci.PopupCount == 0)
            {
                gci.pInputBuffer->TerminateRead(WaitTerminationReason::CtrlC);
            }

            if (!(WI_IsFlagSet(gci.Flags, CONSOLE_SUSPENDED)))
            {
                ContinueProcessing = false;
            }
        }

        // Check for ctrl-break.
        else if (keyEvent.GetVirtualKeyCode() == VK_CANCEL)
        {
            gci.pInputBuffer->Flush();
            HandleCtrlEvent(CTRL_BREAK_EVENT);
            if (gci.PopupCount == 0)
            {
                gci.pInputBuffer->TerminateRead(WaitTerminationReason::CtrlBreak);
            }

            if (!(WI_IsFlagSet(gci.Flags, CONSOLE_SUSPENDED)))
            {
                ContinueProcessing = false;
            }
        }

        // don't write ctrl-esc to the input buffer
        else if (keyEvent.GetVirtualKeyCode() == VK_ESCAPE)
        {
            ContinueProcessing = false;
        }
    }
    else if (keyEvent.IsAltPressed() &&
             keyEvent.IsKeyDown() &&
             keyEvent.GetVirtualKeyCode() == VK_ESCAPE)
    {
        ContinueProcessing = false;
    }

    if (ContinueProcessing)
    {
        size_t EventsWritten = 0;
        try
        {
            EventsWritten = gci.pInputBuffer->Write(std::make_unique<KeyEvent>(keyEvent));
            if (EventsWritten && generateBreak)
            {
                keyEvent.SetKeyDown(false);
                EventsWritten = gci.pInputBuffer->Write(std::make_unique<KeyEvent>(keyEvent));
            }
        }
        catch (...)
        {
            LOG_HR(wil::ResultFromCaughtException());
        }
    }
}

#ifdef DBG
// set to true with a debugger to temporarily disable focus events getting written to the InputBuffer
volatile bool DisableFocusEvents = false;
#endif

void HandleFocusEvent(const BOOL fSetFocus)
{
#ifdef DBG
    if (DisableFocusEvents)
    {
        return;
    }
#endif

    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    try
    {
        const size_t EventsWritten = gci.pInputBuffer->Write(std::make_unique<FocusEvent>(!!fSetFocus));
        FAIL_FAST_IF(EventsWritten != 1);
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
    }
}

void HandleMenuEvent(const DWORD wParam)
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    size_t EventsWritten = 0;
    try
    {
        EventsWritten = gci.pInputBuffer->Write(std::make_unique<MenuEvent>(wParam));
        if (EventsWritten != 1)
        {
            RIPMSG0(RIP_WARNING, "PutInputInBuffer: EventsWritten != 1, 1 expected");
        }
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
    }
}

void HandleCtrlEvent(const DWORD EventType)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    switch (EventType)
    {
    case CTRL_C_EVENT:
        gci.CtrlFlags |= CONSOLE_CTRL_C_FLAG;
        break;
    case CTRL_BREAK_EVENT:
        gci.CtrlFlags |= CONSOLE_CTRL_BREAK_FLAG;
        break;
    case CTRL_CLOSE_EVENT:
        gci.CtrlFlags |= CONSOLE_CTRL_CLOSE_FLAG;
        break;
    default:
        RIPMSG1(RIP_ERROR, "Invalid EventType: 0x%x", EventType);
    }
}

void ProcessCtrlEvents()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    if (gci.CtrlFlags == 0)
    {
        gci.UnlockConsole();
        return;
    }

    // Make our own copy of the console process handle list.
    DWORD const LimitingProcessId = gci.LimitingProcessId;
    gci.LimitingProcessId = 0;

    ConsoleProcessTerminationRecord* rgProcessHandleList;
    size_t cProcessHandleList;

    HRESULT hr = gci.ProcessHandleList
                     .GetTerminationRecordsByGroupId(LimitingProcessId,
                                                     WI_IsFlagSet(gci.CtrlFlags,
                                                                  CONSOLE_CTRL_CLOSE_FLAG),
                                                     &rgProcessHandleList,
                                                     &cProcessHandleList);

    if (FAILED(hr) || cProcessHandleList == 0)
    {
        gci.UnlockConsole();
        return;
    }

    // Copy ctrl flags.
    ULONG CtrlFlags = gci.CtrlFlags;
    FAIL_FAST_IF(!(!((CtrlFlags & (CONSOLE_CTRL_CLOSE_FLAG | CONSOLE_CTRL_BREAK_FLAG | CONSOLE_CTRL_C_FLAG)) && (CtrlFlags & (CONSOLE_CTRL_LOGOFF_FLAG | CONSOLE_CTRL_SHUTDOWN_FLAG)))));

    gci.CtrlFlags = 0;

    gci.UnlockConsole();

    // the ctrl flags could be a combination of the following
    // values:
    //
    //        CONSOLE_CTRL_C_FLAG
    //        CONSOLE_CTRL_BREAK_FLAG
    //        CONSOLE_CTRL_CLOSE_FLAG
    //        CONSOLE_CTRL_LOGOFF_FLAG
    //        CONSOLE_CTRL_SHUTDOWN_FLAG

    DWORD EventType = (DWORD)-1;
    switch (CtrlFlags & (CONSOLE_CTRL_CLOSE_FLAG | CONSOLE_CTRL_BREAK_FLAG | CONSOLE_CTRL_C_FLAG | CONSOLE_CTRL_LOGOFF_FLAG | CONSOLE_CTRL_SHUTDOWN_FLAG))
    {
    case CONSOLE_CTRL_CLOSE_FLAG:
        EventType = CTRL_CLOSE_EVENT;
        break;

    case CONSOLE_CTRL_BREAK_FLAG:
        EventType = CTRL_BREAK_EVENT;
        break;

    case CONSOLE_CTRL_C_FLAG:
        EventType = CTRL_C_EVENT;
        break;

    case CONSOLE_CTRL_LOGOFF_FLAG:
        EventType = CTRL_LOGOFF_EVENT;
        break;

    case CONSOLE_CTRL_SHUTDOWN_FLAG:
        EventType = CTRL_SHUTDOWN_EVENT;
        break;
    }

    NTSTATUS Status = STATUS_SUCCESS;
    for (size_t i = 0; i < cProcessHandleList; i++)
    {
        /*
         * Status will be non-successful if a process attached to this console
         * vetos shutdown. In that case, we don't want to try to kill any more
         * processes, but we do need to make sure we continue looping so we
         * can close any remaining process handles. The exception is if the
         * process is inaccessible, such that we can't even open a handle for
         * query. In this case, use best effort to send the close event but
         * ignore any errors.
         */
        if (NT_SUCCESS(Status))
        {
            Status = ServiceLocator::LocateConsoleControl()
                         ->EndTask((HANDLE)rgProcessHandleList[i].dwProcessID,
                                   EventType,
                                   CtrlFlags);
            if (rgProcessHandleList[i].hProcess == nullptr)
            {
                Status = STATUS_SUCCESS;
            }
        }

        if (rgProcessHandleList[i].hProcess != nullptr)
        {
            CloseHandle(rgProcessHandleList[i].hProcess);
        }
    }

    delete[] rgProcessHandleList;
}
