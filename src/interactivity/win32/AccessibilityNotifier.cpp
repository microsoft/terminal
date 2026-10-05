// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "AccessibilityNotifier.hpp"

#include "../inc/ServiceLocator.hpp"
#include "ConsoleControl.hpp"
#include "resource.h"
#include "window.hpp"

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

// Routine Description:
// - Loads a string resource and returns it. Returns an empty string on failure
static std::wstring _loadString(const UINT id)
{
    WCHAR buffer[70];
    const auto length = LoadStringW(Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().hInstance, id, buffer, ARRAYSIZE(buffer));
    return { &buffer[0], gsl::narrow_cast<size_t>(std::max(length, 0)) };
}

// Routine Description:
// - Announces the state of the Find dialog's search results to screen readers
// Arguments:
// - index: the 0-based index of the current match
// - count: the total number of matches
void AccessibilityNotifier::AnnounceSearchResults(_In_ const ptrdiff_t index, _In_ const size_t count)
try
{
    const auto pWindow = ServiceLocator::LocateConsoleWindow<Window>();
    if (!pWindow)
    {
        return;
    }

    std::wstring announcement;

    if (count == 0)
    {
        // No results found
        announcement = _loadString(ID_CONSOLE_MSGFINDNORESULT);
    }
    else
    {
        // Results found. Announce as the 1-based index of the total ("2 of 5")
        const auto format = _loadString(ID_CONSOLE_MSGFINDRESULT);
        const auto position = std::clamp<size_t>(gsl::narrow_cast<size_t>(std::max<ptrdiff_t>(index, 0)) + 1, 1, count);

        // The resource uses positional inserts (%1, %2) so that translations can reorder them.
        const DWORD_PTR args[]{
            gsl::narrow_cast<DWORD_PTR>(position),
            gsl::narrow_cast<DWORD_PTR>(count),
        };
        wil::unique_hlocal_string formatted;
        const auto length = FormatMessageW(
            FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_ARGUMENT_ARRAY,
            format.c_str(),
            0,
            0,
            reinterpret_cast<LPWSTR>(formatted.addressof()),
            0,
            reinterpret_cast<va_list*>(const_cast<DWORD_PTR*>(&args[0])));
        if (length == 0)
        {
            LOG_LAST_ERROR();
            return;
        }
        announcement.assign(formatted.get(), length);
    }

    if (announcement.empty())
    {
        return;
    }

    LOG_IF_FAILED(pWindow->SignalUiaAnnouncement(announcement));
}
CATCH_LOG()
