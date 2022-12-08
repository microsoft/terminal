// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "scrolling.hpp"

#include "selection.hpp"

#include "../interactivity/inc/ServiceLocator.hpp"

using Microsoft::Console::VirtualTerminal::StateMachine;
using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::Types;

til::CoordType Scrolling::s_ucWheelScrollLines = 0;
til::CoordType Scrolling::s_ucWheelScrollChars = 0;

void Scrolling::s_UpdateSystemMetrics()
{
    s_ucWheelScrollLines = ::base::saturated_cast<decltype(s_ucWheelScrollLines)>(ServiceLocator::LocateSystemConfigurationProvider()->GetNumberOfWheelScrollLines());
    s_ucWheelScrollChars = ::base::saturated_cast<decltype(s_ucWheelScrollChars)>(ServiceLocator::LocateSystemConfigurationProvider()->GetNumberOfWheelScrollCharacters());
}

bool Scrolling::s_IsInScrollMode()
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return WI_IsFlagSet(gci.Flags, CONSOLE_SCROLLING);
}

void Scrolling::s_DoScroll()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto pWindow = ServiceLocator::LocateConsoleWindow();
    if (!s_IsInScrollMode())
    {
        // clear any selection we may have -- can't scroll and select at the same time
        Selection::Instance().ClearSelection();

        WI_SetFlag(gci.Flags, CONSOLE_SCROLLING);

        if (pWindow != nullptr)
        {
            pWindow->UpdateWindowText();
        }
    }
}

void Scrolling::s_ClearScroll()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto pWindow = ServiceLocator::LocateConsoleWindow();
    WI_ClearFlag(gci.Flags, CONSOLE_SCROLLING);
    if (pWindow != nullptr)
    {
        pWindow->UpdateWindowText();
    }
}

void Scrolling::s_ScrollIfNecessary(const SCREEN_INFORMATION& ScreenInfo)
{
    auto pWindow = ServiceLocator::LocateConsoleWindow();
    FAIL_FAST_IF_NULL(pWindow);

    const auto pSelection = &Selection::Instance();

    if (pSelection->IsInSelectingState() && pSelection->IsMouseButtonDown())
    {
        til::point CursorPos;
        if (!pWindow->GetCursorPosition(&CursorPos))
        {
            return;
        }

        til::rect ClientRect;
        if (!pWindow->GetClientRectangle(&ClientRect))
        {
            return;
        }

        pWindow->MapRect(&ClientRect);
        if (!(s_IsPointInRectangle(&ClientRect, CursorPos)))
        {
            pWindow->ConvertScreenToClient(&CursorPos);

            til::point MousePosition;
            MousePosition.x = CursorPos.x;
            MousePosition.y = CursorPos.y;

            auto coordFontSize = ScreenInfo.GetScreenFontSize();

            MousePosition.x /= coordFontSize.width;
            MousePosition.y /= coordFontSize.height;

            MousePosition.x += ScreenInfo.GetViewport().Left();
            MousePosition.y += ScreenInfo.GetViewport().Top();

            pSelection->ExtendSelection(MousePosition);
        }
    }
}

void Scrolling::s_HandleMouseWheel(_In_ bool isMouseWheel,
                                   _In_ bool isMouseHWheel,
                                   _In_ short wheelDelta,
                                   _In_ bool hasShift,
                                   SCREEN_INFORMATION& ScreenInfo)
{
    auto NewOrigin = ScreenInfo.GetViewport().Origin();

    // s_ucWheelScrollLines == 0 means that it is turned off.
    if (isMouseWheel && s_ucWheelScrollLines > 0)
    {
        // Rounding could cause this to be zero if gucWSL is bigger than 240 or so.
        const auto ulActualDelta = std::max(WHEEL_DELTA / s_ucWheelScrollLines, 1);

        // If we change direction we need to throw away any remainder we may have in the other direction.
        if ((ScreenInfo.WheelDelta > 0) == (wheelDelta > 0))
        {
            ScreenInfo.WheelDelta += wheelDelta;
        }
        else
        {
            ScreenInfo.WheelDelta = wheelDelta;
        }

        if (abs(ScreenInfo.WheelDelta) >= ulActualDelta)
        {
            /*
            * By default, SHIFT + WM_MOUSEWHEEL will scroll 1/2 the
            * screen size. A ScrollScale of 1 indicates 1/2 the screen
            * size. This value can be modified in the registry.
            */
            til::CoordType delta = 1;
            if (hasShift)
            {
                delta = std::max((ScreenInfo.GetViewport().Height() * ::base::saturated_cast<til::CoordType>(ScreenInfo.ScrollScale)) / 2, 1);

                // Account for scroll direction changes by adjusting delta if there was a direction change.
                delta *= (ScreenInfo.WheelDelta < 0 ? -1 : 1);
                ScreenInfo.WheelDelta %= delta;
            }
            else
            {
                delta *= (ScreenInfo.WheelDelta / ulActualDelta);
                ScreenInfo.WheelDelta %= ulActualDelta;
            }

            NewOrigin.y -= delta;
            const auto coordBufferSize = ScreenInfo.GetBufferSize().Dimensions();
            if (NewOrigin.y < 0)
            {
                NewOrigin.y = 0;
            }
            else if (NewOrigin.y + ScreenInfo.GetViewport().Height() > coordBufferSize.height)
            {
                NewOrigin.y = coordBufferSize.height - ScreenInfo.GetViewport().Height();
            }
            LOG_IF_FAILED(ScreenInfo.SetViewportOrigin(true, NewOrigin, false));
        }
    }
    else if (isMouseHWheel && s_ucWheelScrollChars > 0)
    {
        const auto ulActualDelta = std::max(WHEEL_DELTA / s_ucWheelScrollChars, 1);

        if ((ScreenInfo.HWheelDelta > 0) == (wheelDelta > 0))
        {
            ScreenInfo.HWheelDelta += wheelDelta;
        }
        else
        {
            ScreenInfo.HWheelDelta = wheelDelta;
        }

        if (abs(ScreenInfo.HWheelDelta) >= ulActualDelta)
        {
            til::CoordType delta = 1;

            if (hasShift)
            {
                delta = std::max(ScreenInfo.GetViewport().RightInclusive(), 1);
            }

            delta *= (ScreenInfo.HWheelDelta / ulActualDelta);
            ScreenInfo.HWheelDelta %= ulActualDelta;

            NewOrigin.x += delta;
            const auto coordBufferSize = ScreenInfo.GetBufferSize().Dimensions();
            if (NewOrigin.x < 0)
            {
                NewOrigin.x = 0;
            }
            else if (NewOrigin.x + ScreenInfo.GetViewport().Width() > coordBufferSize.width)
            {
                NewOrigin.x = coordBufferSize.width - ScreenInfo.GetViewport().Width();
            }

            LOG_IF_FAILED(ScreenInfo.SetViewportOrigin(true, NewOrigin, false));
        }
    }
}

bool Scrolling::s_HandleKeyScrollingEvent(const INPUT_KEY_INFO* const pKeyInfo)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto pWindow = ServiceLocator::LocateConsoleWindow();
    FAIL_FAST_IF_NULL(pWindow);

    const auto VirtualKeyCode = pKeyInfo->GetVirtualKey();
    const auto fIsCtrlPressed = pKeyInfo->IsCtrlPressed();
    const auto fIsEditLineEmpty = CommandLine::IsEditLineEmpty();

    // If escape, enter or ctrl-c, cancel scroll.
    if (VirtualKeyCode == VK_ESCAPE ||
        VirtualKeyCode == VK_RETURN ||
        (VirtualKeyCode == 'C' && fIsCtrlPressed))
    {
        Scrolling::s_ClearScroll();
    }
    else
    {
        WORD ScrollCommand;
        auto Horizontal = FALSE;
        switch (VirtualKeyCode)
        {
        case VK_UP:
        {
            ScrollCommand = SB_LINEUP;
            break;
        }
        case VK_DOWN:
        {
            ScrollCommand = SB_LINEDOWN;
            break;
        }
        case VK_LEFT:
        {
            ScrollCommand = SB_LINEUP;
            Horizontal = TRUE;
            break;
        }
        case VK_RIGHT:
        {
            ScrollCommand = SB_LINEDOWN;
            Horizontal = TRUE;
            break;
        }
        case VK_NEXT:
        {
            ScrollCommand = SB_PAGEDOWN;
            break;
        }
        case VK_PRIOR:
        {
            ScrollCommand = SB_PAGEUP;
            break;
        }
        case VK_END:
        {
            if (fIsCtrlPressed)
            {
                if (fIsEditLineEmpty)
                {
                    // Ctrl-End when edit line is empty will scroll to last line in the buffer.
                    gci.GetActiveOutputBuffer().MakeCurrentCursorVisible();
                    return true;
                }
                else
                {
                    // If edit line is non-empty, we won't handle this so it can modify the edit line (trim characters to end of line from cursor pos).
                    return false;
                }
            }
            else
            {
                ScrollCommand = SB_PAGEDOWN;
                Horizontal = TRUE;
            }
            break;
        }
        case VK_HOME:
        {
            if (fIsCtrlPressed)
            {
                if (fIsEditLineEmpty)
                {
                    // Ctrl-Home when edit line is empty will scroll to top of buffer.
                    ScrollCommand = SB_TOP;
                }
                else
                {
                    // If edit line is non-empty, we won't handle this so it can modify the edit line (trim characters to beginning of line from cursor pos).
                    return false;
                }
            }
            else
            {
                ScrollCommand = SB_PAGEUP;
                Horizontal = TRUE;
            }
            break;
        }
        case VK_SHIFT:
        case VK_CONTROL:
        case VK_MENU:
        {
            return true;
        }
        default:
        {
            pWindow->SendNotifyBeep();
            return true;
        }
        }

        if (Horizontal)
        {
            pWindow->HorizontalScroll(ScrollCommand, 0);
        }
        else
        {
            pWindow->VerticalScroll(ScrollCommand, 0);
        }
    }

    return true;
}

BOOL Scrolling::s_IsPointInRectangle(const til::rect* prc, const til::point pt)
{
    return ((pt.x >= prc->left) && (pt.x < prc->right) &&
            (pt.y >= prc->top) && (pt.y < prc->bottom));
}
