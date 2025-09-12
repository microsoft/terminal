// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "input.h"

#include "dbcs.h"
#include "stream.h"

#include "../terminal/input/terminalInput.hpp"

#include "../interactivity/inc/ServiceLocator.hpp"

#pragma hdrstop

#define KEY_ENHANCED 0x01000000

using Microsoft::Console::Interactivity::ServiceLocator;

bool IsInProcessedInputMode()
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return (gci.pInputBuffer->InputMode & ENABLE_PROCESSED_INPUT) != 0;
}

bool IsInVirtualTerminalInputMode()
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
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
    default:
        return FALSE;
    }
}

ULONG GetControlKeyState(const LPARAM lParam)
{
    ULONG ControlKeyState = 0;

    if (OneCoreSafeGetKeyState(VK_LMENU) & KEY_PRESSED)
    {
        ControlKeyState |= LEFT_ALT_PRESSED;
    }
    if (OneCoreSafeGetKeyState(VK_RMENU) & KEY_PRESSED)
    {
        ControlKeyState |= RIGHT_ALT_PRESSED;
    }
    if (OneCoreSafeGetKeyState(VK_LCONTROL) & KEY_PRESSED)
    {
        ControlKeyState |= LEFT_CTRL_PRESSED;
    }
    if (OneCoreSafeGetKeyState(VK_RCONTROL) & KEY_PRESSED)
    {
        ControlKeyState |= RIGHT_CTRL_PRESSED;
    }
    if (OneCoreSafeGetKeyState(VK_SHIFT) & KEY_PRESSED)
    {
        ControlKeyState |= SHIFT_PRESSED;
    }
    if (OneCoreSafeGetKeyState(VK_NUMLOCK) & KEY_TOGGLED)
    {
        ControlKeyState |= NUMLOCK_ON;
    }
    if (OneCoreSafeGetKeyState(VK_SCROLL) & KEY_TOGGLED)
    {
        ControlKeyState |= SCROLLLOCK_ON;
    }
    if (OneCoreSafeGetKeyState(VK_CAPITAL) & KEY_TOGGLED)
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
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return !gci.GetCtrlKeyShortcutsDisabled() && IsInProcessedInputMode();
}

// Routine Description:
// - handles key events without reference to Win32 elements.
void HandleGenericKeyEvent(INPUT_RECORD event, const bool generateBreak)
{
    auto& keyEvent = event.Event.KeyEvent;
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto ContinueProcessing = true;

    if (WI_IsAnyFlagSet(keyEvent.dwControlKeyState, CTRL_PRESSED) &&
        WI_AreAllFlagsClear(keyEvent.dwControlKeyState, ALT_PRESSED) &&
        keyEvent.bKeyDown)
    {
        // check for ctrl-c, if in line input mode.
        if (keyEvent.wVirtualKeyCode == 'C' && IsInProcessedInputMode())
        {
            HandleCtrlEvent(CTRL_C_EVENT);
            if (!gci.HasPendingPopup())
            {
                gci.pInputBuffer->TerminateRead(WaitTerminationReason::CtrlC);
            }

            if (!(WI_IsFlagSet(gci.Flags, CONSOLE_SUSPENDED)))
            {
                ContinueProcessing = false;
            }
        }

        // Check for ctrl-break.
        else if (keyEvent.wVirtualKeyCode == VK_CANCEL)
        {
            gci.pInputBuffer->Flush();
            HandleCtrlEvent(CTRL_BREAK_EVENT);
            if (!gci.HasPendingPopup())
            {
                gci.pInputBuffer->TerminateRead(WaitTerminationReason::CtrlBreak);
            }

            if (!(WI_IsFlagSet(gci.Flags, CONSOLE_SUSPENDED)))
            {
                ContinueProcessing = false;
            }
        }

        // don't write ctrl-esc to the input buffer
        else if (keyEvent.wVirtualKeyCode == VK_ESCAPE)
        {
            ContinueProcessing = false;
        }
    }
    else if (WI_IsAnyFlagSet(keyEvent.dwControlKeyState, ALT_PRESSED) &&
             keyEvent.bKeyDown &&
             keyEvent.wVirtualKeyCode == VK_ESCAPE)
    {
        ContinueProcessing = false;
    }

    if (ContinueProcessing)
    {
        gci.pInputBuffer->Write(event);
        if (generateBreak)
        {
            keyEvent.bKeyDown = false;
            gci.pInputBuffer->Write(event);
        }

        if (gci.HasActiveOutputBuffer())
        {
            gci.GetActiveOutputBuffer().SnapOnInput(keyEvent.wVirtualKeyCode);
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

    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    try
    {
        gci.pInputBuffer->WriteFocusEvent(fSetFocus);
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
    }
}

void HandleMenuEvent(const DWORD wParam)
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    size_t EventsWritten = 0;
    try
    {
        EventsWritten = gci.pInputBuffer->Write(SynthesizeMenuEvent(wParam));
        if (EventsWritten != 1)
        {
            LOG_HR_MSG(E_FAIL, "PutInputInBuffer: EventsWritten != 1, 1 expected");
        }
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
    }
}

void HandleCtrlEvent(const DWORD EventType)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
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
        LOG_HR_MSG(E_INVALIDARG, "Invalid EventType: 0x%x", EventType);
    }
}

static void CALLBACK midiSkipTimerCallback(HWND, UINT, UINT_PTR idEvent, DWORD) noexcept
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& midiAudio = gci.GetMidiAudio();

    KillTimer(nullptr, idEvent);
    midiAudio.EndSkip();
}

static void beginMidiSkip() noexcept
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& midiAudio = gci.GetMidiAudio();

    midiAudio.BeginSkip();
    SetTimer(nullptr, 0, 1000, midiSkipTimerCallback);
}

void ProcessCtrlEvents()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    if (gci.CtrlFlags == 0)
    {
        gci.UnlockConsole();
        return;
    }

    beginMidiSkip();

    // Make our own copy of the console process handle list.
    const auto LimitingProcessId = gci.LimitingProcessId;
    gci.LimitingProcessId = 0;

    std::vector<ConsoleProcessTerminationRecord> termRecords;
    const auto hr = gci.ProcessHandleList
                        .GetTerminationRecordsByGroupId(LimitingProcessId,
                                                        WI_IsFlagSet(gci.CtrlFlags,
                                                                     CONSOLE_CTRL_CLOSE_FLAG),
                                                        termRecords);

    if (FAILED(hr) || termRecords.empty())
    {
        gci.UnlockConsole();
        return;
    }

    // Copy ctrl flags.
    const auto CtrlFlags = std::exchange(gci.CtrlFlags, 0);

    gci.UnlockConsole();

    // the ctrl flags could be a combination of the following
    // values:
    //
    //        CONSOLE_CTRL_C_FLAG
    //        CONSOLE_CTRL_BREAK_FLAG
    //        CONSOLE_CTRL_CLOSE_FLAG
    //        CONSOLE_CTRL_LOGOFF_FLAG
    //        CONSOLE_CTRL_SHUTDOWN_FLAG

    auto EventType = (DWORD)-1;
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

    default:
        return;
    }

    const auto ctrl = ServiceLocator::LocateConsoleControl();

    for (const auto& r : termRecords)
    {
        // Older versions of Windows would do various things if the EndTask() call failed:
        // * XP: Pops up a "Windows can't end this program" dialog for every already-dead process.
        // * Vista - Win 7: Simply skips over already-dead processes.
        // * Win 8 - Win 11 26100: Aborts on an already-dead process (you have to WM_CLOSE conhost multiple times).
        //
        // That last period had the following comment:
        //   Status will be non-successful if a process attached to this console
        //   vetoes shutdown. In that case, we don't want to try to kill any more
        //   processes, but we do need to make sure we continue looping so we
        //   can close any remaining process handles. The exception is if the
        //   process is inaccessible, such that we can't even open a handle for
        //   query. In this case, use best effort to send the close event but
        //   ignore any errors.
        //
        // The corresponding logic worked like this:
        //   if (FAILED(EndTask(...)) && r.hProcess) {
        //       break;
        //   }
        //
        // That logic was removed around the Windows 11 26100 time frame, because CSRSS
        // (which handles EndTask) now waits 5s and then force-kills the process for us.
        // Going back to the Win 7 behavior then should make shutdown a lot more robust.
        // The bad news is that EndTask() returns STATUS_UNSUCCESSFUL no matter whether
        // the process was already dead, or if the request actually failed for some reason.
        // Hopefully there aren't any regressions, but we can't know without trying.
        ctrl->EndTask(r.dwProcessID, EventType, CtrlFlags);
    }
}
