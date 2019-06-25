/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    preview.c

Abstract:

    This module contains the code for console preview window

Author:

    Therese Stowell (thereses) Feb-3-1992 (swiped from Win3.1)

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

/* ----- Equates ----- */
#define PREVIEW_HSCROLL 0x01
#define PREVIEW_VSCROLL 0x02

/* ----- Prototypes ----- */

void AspectPoint(
    RECT* rectPreview,
    POINT* pt);

LONG AspectScale(
    LONG n1,
    LONG n2,
    LONG m);

/* ----- Globals ----- */

POINT NonClientSize;
RECT WindowRect;
DWORD PreviewFlags;

VOID
    UpdatePreviewRect(VOID)

/*++

    Update the global window size and dimensions

--*/

{
    POINT MinSize;
    POINT MaxSize;
    POINT WindowSize;
    PFONT_INFO lpFont;
    HMONITOR hMonitor;
    MONITORINFO mi = { 0 };

    /*
     * Get the font pointer
     */
    lpFont = &FontInfo[g_currentFontIndex];

    /*
     * Get the window size
     */
    MinSize.x = (GetSystemMetrics(SM_CXMIN) - NonClientSize.x) / lpFont->Size.X;
    MinSize.y = (GetSystemMetrics(SM_CYMIN) - NonClientSize.y) / lpFont->Size.Y;
    MaxSize.x = GetSystemMetrics(SM_CXFULLSCREEN) / lpFont->Size.X;
    MaxSize.y = GetSystemMetrics(SM_CYFULLSCREEN) / lpFont->Size.Y;
    WindowSize.x = max(MinSize.x, min(MaxSize.x, gpStateInfo->WindowSize.X));
    WindowSize.y = max(MinSize.y, min(MaxSize.y, gpStateInfo->WindowSize.Y));

    /*
     * Get the window rectangle, making sure it's at least twice the
     * size of the non-client area.
     */
    WindowRect.left = gpStateInfo->WindowPosX;
    WindowRect.top = gpStateInfo->WindowPosY;
    WindowRect.right = WindowSize.x * lpFont->Size.X + NonClientSize.x;
    if (WindowRect.right < NonClientSize.x * 2)
    {
        WindowRect.right = NonClientSize.x * 2;
    }
    WindowRect.right += WindowRect.left;
    WindowRect.bottom = WindowSize.y * lpFont->Size.Y + NonClientSize.y;
    if (WindowRect.bottom < NonClientSize.y * 2)
    {
        WindowRect.bottom = NonClientSize.y * 2;
    }
    WindowRect.bottom += WindowRect.top;

    /*
     * Get information about the monitor we're on
     */
    hMonitor = MonitorFromRect(&WindowRect, MONITOR_DEFAULTTONEAREST);
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(hMonitor, &mi);
    gcxScreen = mi.rcWork.right - mi.rcWork.left;
    gcyScreen = mi.rcWork.bottom - mi.rcWork.top;

    /*
     * Convert window rectangle to monitor relative coordinates
     */
    WindowRect.right -= WindowRect.left;
    WindowRect.left -= mi.rcWork.left;
    WindowRect.bottom -= WindowRect.top;
    WindowRect.top -= mi.rcWork.top;

    /*
     * Update the display flags
     */
    if (WindowSize.x < gpStateInfo->ScreenBufferSize.X)
    {
        PreviewFlags |= PREVIEW_HSCROLL;
    }
    else
    {
        PreviewFlags &= ~PREVIEW_HSCROLL;
    }
    if (WindowSize.y < gpStateInfo->ScreenBufferSize.Y)
    {
        PreviewFlags |= PREVIEW_VSCROLL;
    }
    else
    {
        PreviewFlags &= ~PREVIEW_VSCROLL;
    }
}

VOID InvalidatePreviewRect(HWND hWnd)

/*++

    Invalidate the area covered by the preview "window"

--*/

{
    RECT rectWin;
    RECT rectPreview;

    /*
     * Get the size of the preview "screen"
     */
    GetClientRect(hWnd, &rectPreview);

    /*
     * Get the dimensions of the preview "window" and scale it to the
     * preview "screen"
     */
    rectWin.left = WindowRect.left;
    rectWin.top = WindowRect.top;
    rectWin.right = WindowRect.left + WindowRect.right;
    rectWin.bottom = WindowRect.top + WindowRect.bottom;
    AspectPoint(&rectPreview, (POINT*)&rectWin.left);
    AspectPoint(&rectPreview, (POINT*)&rectWin.right);

    /*
     * Invalidate the area covered by the preview "window"
     */
    InvalidateRect(hWnd, &rectWin, FALSE);
}

VOID PreviewPaint(
    PAINTSTRUCT* pPS,
    HWND hWnd)

/*++

    Paints the font preview.  This is called inside the paint message
    handler for the preview window

--*/

{
    RECT rectWin;
    RECT rectPreview;
    HBRUSH hbrFrame;
    HBRUSH hbrTitle;
    HBRUSH hbrOld;
    HBRUSH hbrClient;
    HBRUSH hbrBorder;
    HBRUSH hbrButton;
    HBRUSH hbrScroll;
    HBRUSH hbrDesktop;
    POINT ptButton;
    POINT ptScroll;
    HDC hDC;
    HBITMAP hBitmap;
    HBITMAP hBitmapOld;
    COLORREF rgbClient;

    /*
     * Get the size of the preview "screen"
     */
    GetClientRect(hWnd, &rectPreview);

    /*
     * Get the dimensions of the preview "window" and scale it to the
     * preview "screen"
     */
    rectWin = WindowRect;
    AspectPoint(&rectPreview, (POINT*)&rectWin.left);
    AspectPoint(&rectPreview, (POINT*)&rectWin.right);

    /*
     * Compute the dimensions of some other window components
     */
    ptButton.x = GetSystemMetrics(SM_CXSIZE);
    ptButton.y = GetSystemMetrics(SM_CYSIZE);
    AspectPoint(&rectPreview, &ptButton);
    ptButton.y *= 2; /* Double the computed size for "looks" */
    ptScroll.x = GetSystemMetrics(SM_CXVSCROLL);
    ptScroll.y = GetSystemMetrics(SM_CYHSCROLL);
    AspectPoint(&rectPreview, &ptScroll);

    /*
     * Create the memory device context
     */
    hDC = CreateCompatibleDC(pPS->hdc);
    hBitmap = CreateCompatibleBitmap(pPS->hdc,
                                     rectPreview.right,
                                     rectPreview.bottom);
    hBitmapOld = (HBITMAP)SelectObject(hDC, hBitmap);

    /*
     * Create the brushes
     */
    hbrBorder = CreateSolidBrush(GetSysColor(COLOR_ACTIVEBORDER));
    hbrTitle = CreateSolidBrush(GetSysColor(COLOR_ACTIVECAPTION));
    hbrFrame = CreateSolidBrush(GetSysColor(COLOR_WINDOWFRAME));
    hbrButton = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    hbrScroll = CreateSolidBrush(GetSysColor(COLOR_SCROLLBAR));
    hbrDesktop = CreateSolidBrush(GetSysColor(COLOR_BACKGROUND));
    rgbClient = GetNearestColor(hDC, ScreenBkColor(gpStateInfo));
    hbrClient = CreateSolidBrush(rgbClient);

    /*
     * Erase the clipping area
     */
    FillRect(hDC, &(pPS->rcPaint), hbrDesktop);

    /*
     * Fill in the whole window with the client brush
     */
    hbrOld = (HBRUSH)SelectObject(hDC, hbrClient);
    PatBlt(hDC, rectWin.left, rectWin.top, rectWin.right - 1, rectWin.bottom - 1, PATCOPY);

    /*
     * Fill in the caption bar
     */
    SelectObject(hDC, hbrTitle);
    PatBlt(hDC, rectWin.left + 3, rectWin.top + 3, rectWin.right - 7, ptButton.y - 2, PATCOPY);

    /*
     * Draw the "buttons"
     */
    SelectObject(hDC, hbrButton);
    PatBlt(hDC, rectWin.left + 3, rectWin.top + 3, ptButton.x, ptButton.y - 2, PATCOPY);
    PatBlt(hDC, rectWin.left + rectWin.right - 4 - ptButton.x, rectWin.top + 3, ptButton.x, ptButton.y - 2, PATCOPY);
    PatBlt(hDC, rectWin.left + rectWin.right - 4 - 2 * ptButton.x - 1, rectWin.top + 3, ptButton.x, ptButton.y - 2, PATCOPY);
    SelectObject(hDC, hbrFrame);
    PatBlt(hDC, rectWin.left + 3 + ptButton.x, rectWin.top + 3, 1, ptButton.y - 2, PATCOPY);
    PatBlt(hDC, rectWin.left + rectWin.right - 4 - ptButton.x - 1, rectWin.top + 3, 1, ptButton.y - 2, PATCOPY);
    PatBlt(hDC, rectWin.left + rectWin.right - 4 - 2 * ptButton.x - 2, rectWin.top + 3, 1, ptButton.y - 2, PATCOPY);

    /*
     * Draw the scrollbars
     */
    SelectObject(hDC, hbrScroll);
    if (PreviewFlags & PREVIEW_HSCROLL)
    {
        PatBlt(hDC, rectWin.left + 3, rectWin.top + rectWin.bottom - 4 - ptScroll.y, rectWin.right - 7, ptScroll.y, PATCOPY);
    }
    if (PreviewFlags & PREVIEW_VSCROLL)
    {
        PatBlt(hDC, rectWin.left + rectWin.right - 4 - ptScroll.x, rectWin.top + 1 + ptButton.y + 1, ptScroll.x, rectWin.bottom - 6 - ptButton.y, PATCOPY);
        if (PreviewFlags & PREVIEW_HSCROLL)
        {
            SelectObject(hDC, hbrFrame);
            PatBlt(hDC, rectWin.left + rectWin.right - 5 - ptScroll.x, rectWin.top + rectWin.bottom - 4 - ptScroll.y, 1, ptScroll.y, PATCOPY);
            PatBlt(hDC, rectWin.left + rectWin.right - 4 - ptScroll.x, rectWin.top + rectWin.bottom - 5 - ptScroll.y, ptScroll.x, 1, PATCOPY);
        }
    }

    /*
     * Draw the interior window frame and caption frame
     */
    SelectObject(hDC, hbrFrame);
    PatBlt(hDC, rectWin.left + 2, rectWin.top + 2, 1, rectWin.bottom - 5, PATCOPY);
    PatBlt(hDC, rectWin.left + 2, rectWin.top + 2, rectWin.right - 5, 1, PATCOPY);
    PatBlt(hDC, rectWin.left + 2, rectWin.top + rectWin.bottom - 4, rectWin.right - 5, 1, PATCOPY);
    PatBlt(hDC, rectWin.left + rectWin.right - 4, rectWin.top + 2, 1, rectWin.bottom - 5, PATCOPY);
    PatBlt(hDC, rectWin.left + 2, rectWin.top + 1 + ptButton.y, rectWin.right - 5, 1, PATCOPY);

    /*
     * Draw the border
     */
    SelectObject(hDC, hbrBorder);
    PatBlt(hDC, rectWin.left + 1, rectWin.top + 1, 1, rectWin.bottom - 3, PATCOPY);
    PatBlt(hDC, rectWin.left + 1, rectWin.top + 1, rectWin.right - 3, 1, PATCOPY);
    PatBlt(hDC, rectWin.left + 1, rectWin.top + rectWin.bottom - 3, rectWin.right - 3, 1, PATCOPY);
    PatBlt(hDC, rectWin.left + rectWin.right - 3, rectWin.top + 1, 1, rectWin.bottom - 3, PATCOPY);

    /*
     * Draw the exterior window frame
     */
    SelectObject(hDC, hbrFrame);
    PatBlt(hDC, rectWin.left, rectWin.top, 1, rectWin.bottom - 1, PATCOPY);
    PatBlt(hDC, rectWin.left, rectWin.top, rectWin.right - 1, 1, PATCOPY);
    PatBlt(hDC, rectWin.left, rectWin.top + rectWin.bottom - 2, rectWin.right - 1, 1, PATCOPY);
    PatBlt(hDC, rectWin.left + rectWin.right - 2, rectWin.top, 1, rectWin.bottom - 1, PATCOPY);

    /*
     * Copy the memory device context to the screen device context
     */
    BitBlt(pPS->hdc, 0, 0, rectPreview.right, rectPreview.bottom, hDC, 0, 0, SRCCOPY);

    /*
     * Clean up everything
     */
    SelectObject(hDC, hbrOld);
    SelectObject(hDC, hBitmapOld);
    DeleteObject(hbrBorder);
    DeleteObject(hbrFrame);
    DeleteObject(hbrTitle);
    DeleteObject(hbrClient);
    DeleteObject(hbrButton);
    DeleteObject(hbrScroll);
    DeleteObject(hbrDesktop);
    DeleteObject(hBitmap);
    DeleteDC(hDC);
}

[[nodiscard]] LRESULT
    CALLBACK
    PreviewWndProc(
        HWND hWnd,
        UINT wMessage,
        WPARAM wParam,
        LPARAM lParam)

/*
 * PreviewWndProc
 *      Handles the preview window
 */

{
    PAINTSTRUCT ps;
    LPCREATESTRUCT lpcs;
    RECT rcWindow;
    int cx;
    int cy;

    switch (wMessage)
    {
    case WM_CREATE:
        /*
         * Figure out space used by non-client area
         */
        SetRect(&rcWindow, 0, 0, 50, 50);
        AdjustWindowRect(&rcWindow, WS_OVERLAPPEDWINDOW, FALSE);
        NonClientSize.x = rcWindow.right - rcWindow.left - 50;
        NonClientSize.y = rcWindow.bottom - rcWindow.top - 50;

        /*
         * Compute the size of the preview "window"
         */
        UpdatePreviewRect();

        /*
         * Scale the window so it has the same aspect ratio as the screen
         */
        lpcs = (LPCREATESTRUCT)lParam;
        cx = lpcs->cx;
        cy = AspectScale(gcyScreen, gcxScreen, cx);
        if (cy > lpcs->cy)
        {
            cy = lpcs->cy;
            cx = AspectScale(gcxScreen, gcyScreen, cy);
        }
        MoveWindow(hWnd, lpcs->x, lpcs->y, cx, cy, TRUE);
        break;

    case WM_PAINT:
        BeginPaint(hWnd, &ps);
        PreviewPaint(&ps, hWnd);
        EndPaint(hWnd, &ps);
        break;

    case CM_PREVIEW_UPDATE:
        InvalidatePreviewRect(hWnd);
        UpdatePreviewRect();

        /*
         * Make sure the preview "screen" has the correct aspect ratio
         */
        GetWindowRect(hWnd, &rcWindow);
        cx = rcWindow.right - rcWindow.left;
        cy = AspectScale(gcyScreen, gcxScreen, cx);
        if (cy != rcWindow.bottom - rcWindow.top)
        {
            SetWindowPos(hWnd, NULL, 0, 0, cx, cy, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
        }

        InvalidatePreviewRect(hWnd);
        break;

    default:
        return DefWindowProc(hWnd, wMessage, wParam, lParam);
    }
    return 0L;
}

/*  AspectScale
 *      Performs the following calculation in LONG arithmetic to avoid
 *      overflow:
 *          return = n1 * m / n2
 *      This can be used to make an aspect ration calculation where n1/n2
 *      is the aspect ratio and m is a known value.  The return value will
 *      be the value that corresponds to m with the correct apsect ratio.
 */

LONG AspectScale(
    LONG n1,
    LONG n2,
    LONG m)
{
    LONG Temp;

    Temp = n1 * m + (n2 >> 1);
    return Temp / n2;
}

/*  AspectPoint
 *      Scales a point to be preview-sized instead of screen-sized.
 */

void AspectPoint(
    RECT* rectPreview,
    POINT* pt)
{
    pt->x = AspectScale(rectPreview->right, gcxScreen, pt->x);
    pt->y = AspectScale(rectPreview->bottom, gcyScreen, pt->y);
}
