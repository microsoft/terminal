// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "../host/scrolling.hpp"
#include "../interactivity/inc/ServiceLocator.hpp"
#pragma hdrstop

using namespace Microsoft::Console;
using namespace Microsoft::Console::Interactivity;

CursorBlinker::CursorBlinker() :
    _hCaretBlinkTimer(INVALID_HANDLE_VALUE),
    _hCaretBlinkTimerQueue(THROW_LAST_ERROR_IF_NULL(CreateTimerQueue())),
    _uCaretBlinkTime(INFINITE) // default to no blink
{
}

CursorBlinker::~CursorBlinker()
{
    if (_hCaretBlinkTimerQueue)
    {
        DeleteTimerQueueEx(_hCaretBlinkTimerQueue, INVALID_HANDLE_VALUE);
    }
}

void CursorBlinker::UpdateSystemMetrics()
{
    // This can be -1 in a TS session
    _uCaretBlinkTime = ServiceLocator::LocateSystemConfigurationProvider()->GetCaretBlinkTime();
}

void CursorBlinker::SettingsChanged()
{
    DWORD const dwCaretBlinkTime = ServiceLocator::LocateSystemConfigurationProvider()->GetCaretBlinkTime();

    if (dwCaretBlinkTime != _uCaretBlinkTime)
    {
        KillCaretTimer();
        _uCaretBlinkTime = dwCaretBlinkTime;
        SetCaretTimer();
    }
}

void CursorBlinker::FocusEnd()
{
    KillCaretTimer();
}

void CursorBlinker::FocusStart()
{
    SetCaretTimer();
}

// Routine Description:
// - This routine is called when the timer in the console with the focus goes off.  It blinks the cursor.
// Arguments:
// - ScreenInfo - reference to screen info structure.
// Return Value:
// - <none>
void CursorBlinker::TimerRoutine(SCREEN_INFORMATION& ScreenInfo)
{
    Cursor& cursor = ScreenInfo.GetTextBuffer().GetCursor();
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto* const _pAccessibilityNotifier = ServiceLocator::LocateAccessibilityNotifier();

    if (!WI_IsFlagSet(gci.Flags, CONSOLE_HAS_FOCUS))
    {
        goto DoScroll;
    }

    // Update the cursor pos in USER so accessibility will work.
    if (cursor.HasMoved())
    {
        const auto position = cursor.GetPosition();
        const auto viewport = ScreenInfo.GetViewport();
        const auto fontSize = ScreenInfo.GetScreenFontSize();
        cursor.SetHasMoved(false);

        RECT rc;
        rc.left = (position.X - viewport.Left()) * fontSize.X;
        rc.top = (position.Y - viewport.Top()) * fontSize.Y;
        rc.right = rc.left + fontSize.X;
        rc.bottom = rc.top + fontSize.Y;

        _pAccessibilityNotifier->NotifyConsoleCaretEvent(rc);

        // Send accessibility information
        {
            IAccessibilityNotifier::ConsoleCaretEventFlags flags = IAccessibilityNotifier::ConsoleCaretEventFlags::CaretInvisible;

            // Flags is expected to be 2, 1, or 0. 2 in selecting (whether or not visible), 1 if just visible, 0 if invisible/noselect.
            if (WI_IsFlagSet(gci.Flags, CONSOLE_SELECTING))
            {
                flags = IAccessibilityNotifier::ConsoleCaretEventFlags::CaretSelection;
            }
            else if (cursor.IsVisible())
            {
                flags = IAccessibilityNotifier::ConsoleCaretEventFlags::CaretVisible;
            }

            _pAccessibilityNotifier->NotifyConsoleCaretEvent(flags, MAKELONG(position.X, position.Y));
        }
    }

    // If the DelayCursor flag has been set, wait one more tick before toggle.
    // This is used to guarantee the cursor is on for a finite period of time
    // after a move and off for a finite period of time after a WriteString.
    if (cursor.GetDelay())
    {
        cursor.SetDelay(false);
        goto DoScroll;
    }

    // Don't blink the cursor for remote sessions.
    if ((!ServiceLocator::LocateSystemConfigurationProvider()->IsCaretBlinkingEnabled() ||
         _uCaretBlinkTime == -1 ||
         (!cursor.IsBlinkingAllowed())) &&
        cursor.IsOn())
    {
        goto DoScroll;
    }

    // Blink only if the cursor isn't turned off via the API
    if (cursor.IsVisible())
    {
        cursor.SetIsOn(!cursor.IsOn());
    }

DoScroll:
    Scrolling::s_ScrollIfNecessary(ScreenInfo);
}

void CALLBACK CursorTimerRoutineWrapper(_In_ PVOID /* lpParam */, _In_ BOOLEAN /* TimerOrWaitFired */)
{
    // Suppose the following sequence of events takes place:
    //
    // 1. The user resizes the console;
    // 2. The console acquires the console lock;
    // 3. The current SCREEN_INFORMATION instance is deleted;
    // 4. This causes the current Cursor instance to be deleted, too;
    // 5. The Cursor's destructor is called;
    // => Somewhere between 1 and 5, the timer fires:
    //    Timer queue timer callbacks execute asynchronously with respect to
    //    the UI thread under which the numbered steps are taking place.
    //    Because the callback touches console state, it needs to acquire the
    //    console lock. But what if the timer callback fires at just the right
    //    time such that 2 has already acquired the lock?
    // 6. The Cursor's destructor deletes the timer queue and thereby destroys
    //    the timer queue timer used for blinking. However, because this
    //    timer's callback modifies console state, it is prudent to not
    //    continue the destruction if the callback has already started but has
    //    not yet finished. Therefore, the destructor waits for the callback to
    //    finish executing.
    // => Meanwhile, the callback just happens to be stuck waiting for the
    //    console lock acquired in step 2. Since the destructor is waiting on
    //    the callback to complete, and the callback is waiting on the lock,
    //    which will only be released way after the Cursor instance is deleted,
    //    the console has now deadlocked.
    //
    // As a solution, skip the blink if the console lock is already being held.
    // Note that critical sections to not have a waitable synchronization
    // object unless there readily is contention on it. As a result, if we
    // wanted to wait until the lock became available under the condition of
    // not being destroyed, things get too complicated.
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    if (gci.TryLockConsole() != false)
    {
        // Cursor& cursor = gci.GetActiveOutputBuffer().GetTextBuffer().GetCursor();
        gci.GetCursorBlinker().TimerRoutine(gci.GetActiveOutputBuffer());

        // This was originally just UnlockConsole, not CONSOLE_INFORMATION::UnlockConsole
        // Is there a reason it would need to be the global version?
        gci.UnlockConsole();
    }
}

// Routine Description:
// - If guCaretBlinkTime is -1, we don't want to blink the caret. However, we
//   need to make sure it gets drawn, so we'll set a short timer. When that
//   goes off, we'll hit CursorTimerRoutine, and it'll do the right thing if
//   guCaretBlinkTime is -1.
void CursorBlinker::SetCaretTimer()
{
    static const DWORD dwDefTimeout = 0x212;

    KillCaretTimer();

    if (_hCaretBlinkTimer == INVALID_HANDLE_VALUE)
    {
        bool bRet = true;
        DWORD dwEffectivePeriod = _uCaretBlinkTime == -1 ? dwDefTimeout : _uCaretBlinkTime;

        bRet = CreateTimerQueueTimer(&_hCaretBlinkTimer,
                                     _hCaretBlinkTimerQueue,
                                     CursorTimerRoutineWrapper,
                                     this,
                                     dwEffectivePeriod,
                                     dwEffectivePeriod,
                                     0);

        LOG_LAST_ERROR_IF(!bRet);
    }
}

void CursorBlinker::KillCaretTimer()
{
    if (_hCaretBlinkTimer != INVALID_HANDLE_VALUE)
    {
        bool bRet = true;

        bRet = DeleteTimerQueueTimer(_hCaretBlinkTimerQueue,
                                     _hCaretBlinkTimer,
                                     NULL);

        // According to https://msdn.microsoft.com/en-us/library/windows/desktop/ms682569(v=vs.85).aspx
        // A failure to delete the timer with the LastError being ERROR_IO_PENDING means that the timer is
        // currently in use and will get cleaned up when released. Delete should not be called again.
        // We treat that case as a success.
        if (bRet == false && GetLastError() != ERROR_IO_PENDING)
        {
            LOG_LAST_ERROR();
        }
        else
        {
            _hCaretBlinkTimer = INVALID_HANDLE_VALUE;
        }
    }
}
