// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "AccessibilityNotifier.hpp"

#include "..\inc\ServiceLocator.hpp"
#include "ConsoleControl.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity::Win32;

void AccessibilityNotifier::NotifyConsoleCaretEvent(_In_ RECT rectangle)
{
    IConsoleWindow* const pWindow = ServiceLocator::LocateConsoleWindow();
    if (pWindow != nullptr)
    {
        CONSOLE_CARET_INFO caretInfo;
        caretInfo.hwnd = pWindow->GetWindowHandle();
        caretInfo.rc = rectangle;

        LOG_IF_FAILED(ServiceLocator::LocateConsoleControl<ConsoleControl>()->Control(ConsoleControl::ControlType::ConsoleSetCaretInfo,
                                                                                      &caretInfo,
                                                                                      sizeof(caretInfo)));
    }
}

void AccessibilityNotifier::NotifyConsoleCaretEvent(_In_ ConsoleCaretEventFlags flags, _In_ LONG position)
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
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
    static COORD previousCursorLocation = { 0, 0 };
    IConsoleWindow* const pWindow = ServiceLocator::LocateConsoleWindow();

    if (pWindow != nullptr)
    {
        NotifyWinEvent(EVENT_CONSOLE_CARET,
                       pWindow->GetWindowHandle(),
                       dwFlags,
                       position);

        const auto& screenInfo = gci.GetActiveOutputBuffer();
        const Cursor& cursor = screenInfo.GetTextBuffer().GetCursor();
        const COORD currentCursorPosition = cursor.GetPosition();
        if (currentCursorPosition != previousCursorLocation)
        {
            LOG_IF_FAILED(pWindow->SignalUia(UIA_Text_TextSelectionChangedEventId));
        }
        previousCursorLocation = currentCursorPosition;
    }
}

void AccessibilityNotifier::NotifyConsoleUpdateScrollEvent(_In_ LONG x, _In_ LONG y)
{
    IConsoleWindow* pWindow = ServiceLocator::LocateConsoleWindow();
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
    IConsoleWindow* pWindow = ServiceLocator::LocateConsoleWindow();
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
    IConsoleWindow* pWindow = ServiceLocator::LocateConsoleWindow();
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
    IConsoleWindow* pWindow = ServiceLocator::LocateConsoleWindow();
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
    IConsoleWindow* pWindow = ServiceLocator::LocateConsoleWindow();
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
    IConsoleWindow* pWindow = ServiceLocator::LocateConsoleWindow();
    if (pWindow)
    {
        NotifyWinEvent(EVENT_CONSOLE_END_APPLICATION,
                       pWindow->GetWindowHandle(),
                       processId,
                       0);
    }
}
