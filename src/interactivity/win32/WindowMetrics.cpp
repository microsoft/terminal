// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "WindowMetrics.hpp"

#include "windowdpiapi.hpp"
#include "..\inc\ServiceLocator.hpp"

#pragma hdrstop

// The following default masks are used in creating windows
#define CONSOLE_WINDOW_FLAGS (WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL)
#define CONSOLE_WINDOW_EX_FLAGS (WS_EX_WINDOWEDGE | WS_EX_ACCEPTFILES | WS_EX_APPWINDOW)

using namespace Microsoft::Console::Interactivity::Win32;

// Routine Description:
// - Gets the minimum possible client rectangle in pixels.
// - Purely based on system metrics. Doesn't compensate for potential scroll bars.
// Arguments:
// - <none>
// Return Value:
// - RECT of client area positions in pixels.
RECT WindowMetrics::GetMinClientRectInPixels()
{
    // prepare rectangle
    RECT rc;
    SetRectEmpty(&rc);

    // set bottom/right dimensions to represent minimum window width/height
    rc.right = GetSystemMetrics(SM_CXMIN);
    rc.bottom = GetSystemMetrics(SM_CYMIN);

    // convert to client rect
    ConvertWindowRectToClientRect(&rc);

    // there is no scroll bar subtraction here as the minimum window dimensions can be expanded wider to hold a scroll bar if necessary

    return rc;
}

// Routine Description:
// - Gets the maximum possible client rectangle in pixels.
// - This leaves space for potential scroll bars to be visible within the window (which are non-client area pixels when rendered)
// - This is a measurement of the inner area of the window, not including the non-client frame area and not including scroll bars.
// Arguments:
// - <none>
// Return Value:
// - RECT of client area positions in pixels.
RECT WindowMetrics::GetMaxClientRectInPixels()
{
    // This will retrieve the outer window rect. We need the client area to calculate characters.
    RECT rc = GetMaxWindowRectInPixels();

    // convert to client rect
    ConvertWindowRectToClientRect(&rc);

    return rc;
}

// Routine Description:
// - Gets the maximum possible window rectangle in pixels. Based on the monitor the window is on or the primary monitor if no window exists yet.
// Arguments:
// - <none>
// Return Value:
// - RECT containing the left, right, top, and bottom positions from the desktop origin in pixels. Measures the outer edges of the potential window.
RECT WindowMetrics::GetMaxWindowRectInPixels()
{
    RECT rc;
    SetRectEmpty(&rc);
    return GetMaxWindowRectInPixels(&rc, nullptr);
}

// Routine Description:
// - Gets the maximum possible window rectangle in pixels. Based on the monitor the window is on or the primary monitor if no window exists yet.
// Arguments:
// - prcSuggested - If we were given a suggested rectangle for where the window is going, we can pass it in here to find out the max size on that monitor.
//                - If this value is zero and we had a valid window handle, we'll use that instead. Otherwise the value of 0 will make us use the primary monitor.
// - pDpiSuggested - The dpi that matches the suggested rect. We will attempt to compute this during the function, but if we fail for some reason,
//                 - the original value passed in will be left untouched.
// Return Value:
// - RECT containing the left, right, top, and bottom positions from the desktop origin in pixels. Measures the outer edges of the potential window.
RECT WindowMetrics::GetMaxWindowRectInPixels(const RECT* const prcSuggested, _Out_opt_ UINT* pDpiSuggested)
{
    // prepare rectangle
    RECT rc = *prcSuggested;

    // use zero rect to compare.
    RECT rcZero;
    SetRectEmpty(&rcZero);

    // First get the monitor pointer from either the active window or the default location (0,0,0,0)
    HMONITOR hMonitor = nullptr;

    // NOTE: We must use the nearest monitor because sometimes the system moves the window around into strange spots while performing snap and Win+D operations.
    // Those operations won't work correctly if we use MONITOR_DEFAULTTOPRIMARY.
    auto pWindow = ServiceLocator::LocateConsoleWindow();
    if (pWindow == nullptr || (TRUE != EqualRect(&rc, &rcZero)))
    {
        // For invalid window handles or when we were passed a non-zero suggestion rectangle, get the monitor from the rect.
        hMonitor = MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);
    }
    else
    {
        // Otherwise, get the monitor from the window handle.
        hMonitor = MonitorFromWindow(pWindow->GetWindowHandle(), MONITOR_DEFAULTTONEAREST);
    }

    // If for whatever reason there is no monitor, we're going to give back whatever we got since we can't figure anything out.
    // We won't adjust the DPI either. That's OK. DPI doesn't make much sense with no display.
    if (nullptr == hMonitor)
    {
        return rc;
    }

    // Now obtain the monitor pixel dimensions
    MONITORINFO MonitorInfo = { 0 };
    MonitorInfo.cbSize = sizeof(MONITORINFO);

    GetMonitorInfoW(hMonitor, &MonitorInfo);

    // We have to make a correction to the work area. If we actually consume the entire work area (by maximizing the window)
    // The window manager will render the borders off-screen.
    // We need to pad the work rectangle with the border dimensions to represent the actual max outer edges of the window rect.
    WINDOWINFO wi = { 0 };
    wi.cbSize = sizeof(WINDOWINFO);
    GetWindowInfo(pWindow ? pWindow->GetWindowHandle() : nullptr, &wi);

    if (pWindow != nullptr && pWindow->IsInFullscreen())
    {
        // In full screen mode, we will consume the whole monitor with no chrome.
        rc = MonitorInfo.rcMonitor;
    }
    else
    {
        // In non-full screen, we want to only use the work area (avoiding the task bar space)
        rc = MonitorInfo.rcWork;
        rc.top -= wi.cyWindowBorders;
        rc.bottom += wi.cyWindowBorders;
        rc.left -= wi.cxWindowBorders;
        rc.right += wi.cxWindowBorders;
    }

    if (pDpiSuggested != nullptr)
    {
        UINT monitorDpiX;
        UINT monitorDpiY;
        if (SUCCEEDED(GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &monitorDpiX, &monitorDpiY)))
        {
            *pDpiSuggested = monitorDpiX;
        }
        else
        {
            *pDpiSuggested = ServiceLocator::LocateGlobals().dpi;
        }
    }

    return rc;
}

// Routine Description:
// - Converts a client rect (inside not including non-client area) into a window rect (the outside edge dimensions)
// - NOTE: This one uses the current global DPI for calculations.
// Arguments:
// - prc - Rectangle to be adjusted from window to client
// - dwStyle - Style flags
// - fMenu - Whether or not a menu is present for this window
// - dwExStyle - Extended Style flags
// Return Value:
// - BOOL if adjustment was successful. (See AdjustWindowRectEx definition for details).
BOOL WindowMetrics::AdjustWindowRectEx(_Inout_ LPRECT prc, const DWORD dwStyle, const BOOL fMenu, const DWORD dwExStyle)
{
    return ServiceLocator::LocateHighDpiApi<WindowDpiApi>()->AdjustWindowRectExForDpi(prc, dwStyle, fMenu, dwExStyle, ServiceLocator::LocateGlobals().dpi);
}

// Routine Description:
// - Converts a client rect (inside not including non-client area) into a window rect (the outside edge dimensions)
// Arguments:
// - prc - Rectangle to be adjusted from window to client
// - dwStyle - Style flags
// - fMenu - Whether or not a menu is present for this window
// - dwExStyle - Extended Style flags
// - iDpi - The DPI to use for scaling.
// Return Value:
// - BOOL if adjustment was successful. (See AdjustWindowRectEx definition for details).
BOOL WindowMetrics::AdjustWindowRectEx(_Inout_ LPRECT prc, const DWORD dwStyle, const BOOL fMenu, const DWORD dwExStyle, const int iDpi)
{
    return ServiceLocator::LocateHighDpiApi<WindowDpiApi>()->AdjustWindowRectExForDpi(prc, dwStyle, fMenu, dwExStyle, iDpi);
}

// Routine Description:
// - Converts a client rect (inside not including non-client area) into a window rect (the outside edge dimensions)
// - This is a helper to call AdjustWindowRectEx.
// - It finds the appropriate window styles for the active window or uses the defaults from our class registration.
// - It does NOT compensate for scrollbars or menus.
// Arguments:
// - prc - Pointer to rectangle to be adjusted from client to window positions.
// Return Value:
// - <none>
void WindowMetrics::ConvertClientRectToWindowRect(_Inout_ RECT* const prc)
{
    ConvertRect(prc, ConvertRectangle::CLIENT_TO_WINDOW);
}

// Routine Description:
// - Converts a window rect (the outside edge dimensions) into a client rect (inside not including non-client area)
// - This is a helper to call UnadjustWindowRectEx.
// - It finds the appropriate window styles for the active window or uses the defaults from our class registration.
// - It does NOT compensate for scrollbars or menus.
// Arguments:
// - prc - Pointer to rectangle to be adjusted from window to client positions.
// Return Value:
// - <none>
void WindowMetrics::ConvertWindowRectToClientRect(_Inout_ RECT* const prc)
{
    ConvertRect(prc, ConvertRectangle::WINDOW_TO_CLIENT);
}

// Routine Description:
// - Converts a window rect (the outside edge dimensions) into a client rect (inside not including non-client area)
// - Effectively the opposite math of "AdjustWindowRectEx"
// - See: http://blogs.msdn.com/b/oldnewthing/archive/2013/10/17/10457292.aspx
// Arguments:
// - prc - Rectangle to be adjusted from window to client
// - dwStyle - Style flags
// - fMenu - Whether or not a menu is present for this window
// - dwExStyle - Extended Style flags
// Return Value:
// - BOOL if adjustment was successful. (See AdjustWindowRectEx definition for details).
BOOL WindowMetrics::UnadjustWindowRectEx(_Inout_ LPRECT prc, const DWORD dwStyle, const BOOL fMenu, const DWORD dwExStyle)
{
    RECT rc;
    SetRectEmpty(&rc);
    BOOL fRc = AdjustWindowRectEx(&rc, dwStyle, fMenu, dwExStyle);
    if (fRc)
    {
        prc->left -= rc.left;
        prc->top -= rc.top;
        prc->right -= rc.right;
        prc->bottom -= rc.bottom;
    }
    return fRc;
}

// Routine Description:
// - To reduce code duplication, this will do the style lookup and forwards/backwards calls for client/window rectangle translation.
// - Only really intended for use by the related static methods.
// Arguments:
// - prc - Pointer to rectangle to be adjusted from client to window positions.
// - crDirection - specifies which conversion to perform
// Return Value:
// - <none>
void WindowMetrics::ConvertRect(_Inout_ RECT* const prc, const ConvertRectangle crDirection)
{
    // collect up current window style (if available) for adjustment
    DWORD dwStyle = 0;
    DWORD dwExStyle = 0;

    Microsoft::Console::Types::IConsoleWindow* pWindow = ServiceLocator::LocateConsoleWindow();
    if (pWindow != nullptr)
    {
        dwStyle = GetWindowStyle(pWindow->GetWindowHandle());
        dwExStyle = GetWindowExStyle(pWindow->GetWindowHandle());
    }
    else
    {
        dwStyle = CONSOLE_WINDOW_FLAGS;
        dwExStyle = CONSOLE_WINDOW_EX_FLAGS;
    }

    switch (crDirection)
    {
    case CLIENT_TO_WINDOW:
    {
        // ask system to adjust our client rectangle into a window rectangle using the given style
        AdjustWindowRectEx(prc, dwStyle, false, dwExStyle);
        break;
    }
    case WINDOW_TO_CLIENT:
    {
        // ask system to adjust our window rectangle into a client rectangle using the given style
        UnadjustWindowRectEx(prc, dwStyle, false, dwExStyle);
        break;
    }
    default:
    {
        FAIL_FAST_HR(E_NOTIMPL); // not implemented
    }
    }
}
