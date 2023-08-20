// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "../host/scrolling.hpp"
#include "../interactivity/inc/ServiceLocator.hpp"
#pragma hdrstop

using namespace Microsoft::Console;
using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::Render;

static void CALLBACK CursorTimerRoutineWrapper(_Inout_ PTP_CALLBACK_INSTANCE /*Instance*/, _Inout_opt_ PVOID /*Context*/, _Inout_ PTP_TIMER /*Timer*/)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    // There's a slight race condition here.
    // CreateThreadpoolTimer callbacks may be scheduled even after they were canceled.
    // But I'm not too concerned that this will lead to issues at the time of writing,
    // as CursorBlinker is allocated as a static variable through the Globals class.
    // It'd be nice to fix this, but realistically it'll likely not lead to issues.
    gci.LockConsole();
    gci.GetCursorBlinker().TimerRoutine(gci.GetActiveOutputBuffer());
    gci.UnlockConsole();
}

CursorBlinker::CursorBlinker() :
    _timer(THROW_LAST_ERROR_IF_NULL(CreateThreadpoolTimer(&CursorTimerRoutineWrapper, nullptr, nullptr))),
    _uCaretBlinkTime(INFINITE) // default to no blink
{
}

CursorBlinker::~CursorBlinker()
{
    KillCaretTimer();
}

void CursorBlinker::UpdateSystemMetrics() noexcept
{
    // This can be -1 in a TS session
    _uCaretBlinkTime = ServiceLocator::LocateSystemConfigurationProvider()->GetCaretBlinkTime();

    // If animations are disabled, or the blink rate is infinite, blinking is not allowed.
    auto animationsEnabled = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animationsEnabled, 0);
    auto& renderSettings = ServiceLocator::LocateGlobals().getConsoleInformation().GetRenderSettings();
    renderSettings.SetRenderMode(RenderSettings::Mode::BlinkAllowed, animationsEnabled && _uCaretBlinkTime != INFINITE);
}

void CursorBlinker::SettingsChanged() noexcept
{
    const auto dwCaretBlinkTime = ServiceLocator::LocateSystemConfigurationProvider()->GetCaretBlinkTime();

    if (dwCaretBlinkTime != _uCaretBlinkTime)
    {
        KillCaretTimer();
        _uCaretBlinkTime = dwCaretBlinkTime;
        SetCaretTimer();
    }
}

void CursorBlinker::FocusEnd() const noexcept
{
    KillCaretTimer();
}

void CursorBlinker::FocusStart() const noexcept
{
    SetCaretTimer();
}

// Routine Description:
// - This routine is called when the timer in the console with the focus goes off.
//   It blinks the cursor and also toggles the rendition of any blinking attributes.
// Arguments:
// - ScreenInfo - reference to screen info structure.
// Return Value:
// - <none>
void CursorBlinker::TimerRoutine(SCREEN_INFORMATION& ScreenInfo) const noexcept
{
    auto& buffer = ScreenInfo.GetTextBuffer();
    auto& cursor = buffer.GetCursor();
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto* const pAccessibilityNotifier = ServiceLocator::LocateAccessibilityNotifier();
    const auto inConpty{ gci.IsInVtIoMode() };

    // GH#2988: ConPTY can now be focused, but it doesn't need to do any of this work either.
    if (inConpty || !WI_IsFlagSet(gci.Flags, CONSOLE_HAS_FOCUS))
    {
        goto DoScroll;
    }

    // Update the cursor pos in USER so accessibility will work.
    // Don't do all this work or send events if we don't have a notifier target.
    if (pAccessibilityNotifier && cursor.HasMoved())
    {
        // Convert the buffer position to the equivalent screen coordinates
        // required by the notifier, taking line rendition into account.
        const auto position = buffer.BufferToScreenPosition(cursor.GetPosition());
        const auto viewport = ScreenInfo.GetViewport();
        const auto fontSize = ScreenInfo.GetScreenFontSize();
        cursor.SetHasMoved(false);

        til::rect rc;
        rc.left = (position.x - viewport.Left()) * fontSize.width;
        rc.top = (position.y - viewport.Top()) * fontSize.height;
        rc.right = rc.left + fontSize.width;
        rc.bottom = rc.top + fontSize.height;

        pAccessibilityNotifier->NotifyConsoleCaretEvent(rc);

        // Send accessibility information
        {
            auto flags = IAccessibilityNotifier::ConsoleCaretEventFlags::CaretInvisible;

            // Flags is expected to be 2, 1, or 0. 2 in selecting (whether or not visible), 1 if just visible, 0 if invisible/noselect.
            if (WI_IsFlagSet(gci.Flags, CONSOLE_SELECTING))
            {
                flags = IAccessibilityNotifier::ConsoleCaretEventFlags::CaretSelection;
            }
            else if (cursor.IsVisible())
            {
                flags = IAccessibilityNotifier::ConsoleCaretEventFlags::CaretVisible;
            }

            pAccessibilityNotifier->NotifyConsoleCaretEvent(flags, MAKELONG(position.x, position.y));
        }
    }

    // If the DelayCursor flag has been set, wait one more tick before toggle.
    // This is used to guarantee the cursor is on for a finite period of time
    // after a move and off for a finite period of time after a WriteString.
    if (cursor.GetDelay())
    {
        cursor.SetDelay(false);
        goto DoBlinkingRenditionAndScroll;
    }

    // Don't blink the cursor for remote sessions.
    if ((!ServiceLocator::LocateSystemConfigurationProvider()->IsCaretBlinkingEnabled() ||
         _uCaretBlinkTime == -1 ||
         (!cursor.IsBlinkingAllowed())) &&
        cursor.IsOn())
    {
        goto DoBlinkingRenditionAndScroll;
    }

    // Blink only if the cursor isn't turned off via the API
    if (cursor.IsVisible())
    {
        cursor.SetIsOn(!cursor.IsOn());
    }

DoBlinkingRenditionAndScroll:
    gci.GetRenderSettings().ToggleBlinkRendition(buffer.GetRenderer());

DoScroll:
    Scrolling::s_ScrollIfNecessary(ScreenInfo);
}

// Routine Description:
// - If guCaretBlinkTime is -1, we don't want to blink the caret. However, we
//   need to make sure it gets drawn, so we'll set a short timer. When that
//   goes off, we'll hit CursorTimerRoutine, and it'll do the right thing if
//   guCaretBlinkTime is -1.
void CursorBlinker::SetCaretTimer() const noexcept
{
    using filetime_duration = std::chrono::duration<int64_t, std::ratio<1, 10000000>>;
    static constexpr DWORD dwDefTimeout = 0x212;

    const auto periodInMS = _uCaretBlinkTime == -1 ? dwDefTimeout : _uCaretBlinkTime;
    // The FILETIME struct measures time in 100ns steps. 10000 thus equals 1ms.
    auto periodInFiletime = -static_cast<int64_t>(periodInMS) * 10000;
    SetThreadpoolTimer(_timer.get(), reinterpret_cast<FILETIME*>(&periodInFiletime), periodInMS, 0);
}

void CursorBlinker::KillCaretTimer() const noexcept
{
    SetThreadpoolTimer(_timer.get(), nullptr, 0, 0);
}
