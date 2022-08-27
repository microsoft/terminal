// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "AccessibilityNotifier.hpp"

#include "../inc/ServiceLocator.hpp"
#include "ConsoleControl.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity::Win32;

void AccessibilityNotifier::NotifyConsoleCaretEvent(_In_ const til::rect& rectangle)
{
    const auto pWindow = ServiceLocator::LocateConsoleWindow();
    if (pWindow != nullptr)
    {
        CONSOLE_CARET_INFO caretInfo;
        caretInfo.hwnd = pWindow->GetWindowHandle();
        caretInfo.rc = rectangle.to_win32_rect();

        LOG_IF_FAILED(ServiceLocator::LocateConsoleControl<ConsoleControl>()->Control(ConsoleControl::ControlType::ConsoleSetCaretInfo,
                                                                                      &caretInfo,
                                                                                      sizeof(caretInfo)));
    }
}

void AccessibilityNotifier::NotifyConsoleCaretEvent(_In_ ConsoleCaretEventFlags flags, _In_ LONG position)
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    DWORD dwFlags = 0;

    if (flags == ConsoleCaretEventFlags::CaretSelection)
    {
        dwFlags = CONSOLE_CARET_SELECTION;
    }
    else if (flags == ConsoleCaretEventFlags::CaretVisible)
    {
        dwFlags = CONSOLE_CARET_VISIBLE;
    }

    // UIA event notification
    static til::point previousCursorLocation;
    const auto pWindow = ServiceLocator::LocateConsoleWindow();

    if (pWindow != nullptr)
    {
        NotifyWinEvent(EVENT_CONSOLE_CARET,
                       pWindow->GetWindowHandle(),
                       dwFlags,
                       position);

        const auto& screenInfo = gci.GetActiveOutputBuffer();
        const auto& cursor = screenInfo.GetTextBuffer().GetCursor();
        const auto currentCursorPosition = cursor.GetPosition();
        if (currentCursorPosition != previousCursorLocation)
        {
            LOG_IF_FAILED(pWindow->SignalUia(UIA_Text_TextSelectionChangedEventId));
        }
        previousCursorLocation = currentCursorPosition;
    }
}

void AccessibilityNotifier::NotifyConsoleUpdateScrollEvent(_In_ LONG x, _In_ LONG y)
{
    auto pWindow = ServiceLocator::LocateConsoleWindow();
    if (pWindow)
    {
        NotifyWinEvent(EVENT_CONSOLE_UPDATE_SCROLL,
                       pWindow->GetWindowHandle(),
                       x,
                       y);
    }
}

void AccessibilityNotifier::NotifyConsoleUpdateSimpleEvent(_In_ LONG start, _In_ LONG charAndAttribute)
{
    auto pWindow = ServiceLocator::LocateConsoleWindow();
    if (pWindow)
    {
        NotifyWinEvent(EVENT_CONSOLE_UPDATE_SIMPLE,
                       pWindow->GetWindowHandle(),
                       start,
                       charAndAttribute);
    }
}

void AccessibilityNotifier::NotifyConsoleUpdateRegionEvent(_In_ LONG startXY, _In_ LONG endXY)
{
    auto pWindow = ServiceLocator::LocateConsoleWindow();
    if (pWindow)
    {
        NotifyWinEvent(EVENT_CONSOLE_UPDATE_REGION,
                       pWindow->GetWindowHandle(),
                       startXY,
                       endXY);
    }
}

void AccessibilityNotifier::NotifyConsoleLayoutEvent()
{
    auto pWindow = ServiceLocator::LocateConsoleWindow();
    if (pWindow)
    {
        NotifyWinEvent(EVENT_CONSOLE_LAYOUT,
                       pWindow->GetWindowHandle(),
                       0,
                       0);
    }
}

void AccessibilityNotifier::NotifyConsoleStartApplicationEvent(_In_ DWORD processId)
{
    auto pWindow = ServiceLocator::LocateConsoleWindow();
    if (pWindow)
    {
        NotifyWinEvent(EVENT_CONSOLE_START_APPLICATION,
                       pWindow->GetWindowHandle(),
                       processId,
                       0);
    }
}

void AccessibilityNotifier::NotifyConsoleEndApplicationEvent(_In_ DWORD processId)
{
    auto pWindow = ServiceLocator::LocateConsoleWindow();
    if (pWindow)
    {
        NotifyWinEvent(EVENT_CONSOLE_END_APPLICATION,
                       pWindow->GetWindowHandle(),
                       processId,
                       0);
    }
}
