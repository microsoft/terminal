// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "ColorControl.h"

COLORREF GetColorForItem(const int itemId)
{
    if (itemId >= IDD_COLOR_1 && itemId <= IDD_COLOR_16)
    {
        return AttrToRGB(itemId - IDD_COLOR_1);
    }
    else if (itemId == IDD_TERMINAL_FGCOLOR)
    {
        return g_fakeForegroundColor;
    }
    else if (itemId == IDD_TERMINAL_BGCOLOR)
    {
        return g_fakeBackgroundColor;
    }
    else if (itemId == IDD_TERMINAL_CURSOR_COLOR)
    {
        return g_fakeCursorColor;
    }
    return RGB(0xff, 0x0, 0xff);
}

void SimpleColorDoPaint(const HWND hColor, PAINTSTRUCT& ps, const int ColorId)
{
    RECT rColor;
    COLORREF rgbBrush;
    HBRUSH hbr;

    GetClientRect(hColor, &rColor);
    rgbBrush = GetNearestColor(ps.hdc, GetColorForItem(ColorId));
    if ((hbr = CreateSolidBrush(rgbBrush)) != NULL)
    {
        InflateRect(&rColor, -1, -1);
        FillRect(ps.hdc, &rColor, hbr);
        DeleteObject(hbr);
    }
}

// Routine Description:
// - Window proc for the color buttons
[[nodiscard]] LRESULT CALLBACK SimpleColorControlProc(const HWND hColor, const UINT wMsg, const WPARAM wParam, const LPARAM lParam)
{
    PAINTSTRUCT ps;
    int ColorId;
    HWND hDlg;

    ColorId = GetWindowLong(hColor, GWL_ID);
    hDlg = GetParent(hColor);

    switch (wMsg)
    {
    case WM_GETDLGCODE:
        return DLGC_WANTARROWS | DLGC_WANTTAB;
        break;
    case WM_PAINT:
        BeginPaint(hColor, &ps);
        SimpleColorDoPaint(hColor, ps, ColorId);
        EndPaint(hColor, &ps);
        break;
    default:
        return DefWindowProc(hColor, wMsg, wParam, lParam);
        break;
    }
    return TRUE;
}
