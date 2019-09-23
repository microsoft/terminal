// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "ColorsPage.h"
#include "ColorControl.h"

static BYTE ColorArray[4];
static int iColor;

// Routine Description:
// - Window proc for the color buttons
[[nodiscard]] LRESULT CALLBACK ColorTableControlProc(HWND hColor, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    int ColorId;
    RECT rColor;
    RECT rTemp;
    HDC hdc;
    HWND hWnd;
    HWND hDlg;

    ColorId = GetWindowLong(hColor, GWL_ID);
    hDlg = GetParent(hColor);

    switch (wMsg)
    {
    case WM_SETFOCUS:
        if (ColorArray[iColor] != (BYTE)(ColorId - IDD_COLOR_1))
        {
            hWnd = GetDlgItem(hDlg, ColorArray[iColor] + IDD_COLOR_1);
            SetFocus(hWnd);
        }
        __fallthrough;
    case WM_KILLFOCUS:
        hdc = GetDC(hDlg);
        hWnd = GetDlgItem(hDlg, IDD_COLOR_1);
        GetWindowRect(hWnd, &rColor);
        hWnd = GetDlgItem(hDlg, IDD_COLOR_16);
        GetWindowRect(hWnd, &rTemp);
        rColor.right = rTemp.right;
        ScreenToClient(hDlg, (LPPOINT)&rColor.left);
        ScreenToClient(hDlg, (LPPOINT)&rColor.right);
        InflateRect(&rColor, 2, 2);
        DrawFocusRect(hdc, &rColor);
        ReleaseDC(hDlg, hdc);
        break;
    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_UP:
        case VK_LEFT:
            if (ColorId > IDD_COLOR_1)
            {
                SendMessage(hDlg, CM_SETCOLOR, ColorId - 1 - IDD_COLOR_1, (LPARAM)hColor);
            }
            break;
        case VK_DOWN:
        case VK_RIGHT:
            if (ColorId < IDD_COLOR_16)
            {
                SendMessage(hDlg, CM_SETCOLOR, ColorId + 1 - IDD_COLOR_1, (LPARAM)hColor);
            }
            break;
        case VK_TAB:
            hWnd = GetDlgItem(hDlg, IDD_COLOR_1);
            hWnd = GetNextDlgTabItem(hDlg, hWnd, GetKeyState(VK_SHIFT) < 0);
            SetFocus(hWnd);
            break;
        default:
            return DefWindowProc(hColor, wMsg, wParam, lParam);
        }
        break;
    case WM_RBUTTONDOWN:
    case WM_LBUTTONDOWN:
        SendMessage(hDlg, CM_SETCOLOR, ColorId - IDD_COLOR_1, (LPARAM)hColor);
        break;
    case WM_PAINT:

        BeginPaint(hColor, &ps);
        GetClientRect(hColor, &rColor);

        // are we the selected color for the current object?
        if (ColorArray[iColor] == (BYTE)(ColorId - IDD_COLOR_1))
        {
            // highlight the selected color
            FrameRect(ps.hdc, &rColor, (HBRUSH)GetStockObject(BLACK_BRUSH));
            InflateRect(&rColor, -1, -1);
            FrameRect(ps.hdc, &rColor, (HBRUSH)GetStockObject(BLACK_BRUSH));
        }

        SimpleColorDoPaint(hColor, ps, ColorId);
        EndPaint(hColor, &ps);
        break;
    default:
        return SimpleColorControlProc(hColor, wMsg, wParam, lParam);
        break;
    }
    return TRUE;
}

bool InitColorsDialog(HWND hDlg)
{
    ColorArray[IDD_COLOR_SCREEN_TEXT - IDD_COLOR_SCREEN_TEXT] =
        LOBYTE(gpStateInfo->ScreenAttributes) & 0x0F;
    ColorArray[IDD_COLOR_SCREEN_BKGND - IDD_COLOR_SCREEN_TEXT] =
        LOBYTE(gpStateInfo->ScreenAttributes >> 4);
    ColorArray[IDD_COLOR_POPUP_TEXT - IDD_COLOR_SCREEN_TEXT] =
        LOBYTE(gpStateInfo->PopupAttributes) & 0x0F;
    ColorArray[IDD_COLOR_POPUP_BKGND - IDD_COLOR_SCREEN_TEXT] =
        LOBYTE(gpStateInfo->PopupAttributes >> 4);
    CheckRadioButton(hDlg, IDD_COLOR_SCREEN_TEXT, IDD_COLOR_POPUP_BKGND, IDD_COLOR_SCREEN_BKGND);
    iColor = IDD_COLOR_SCREEN_BKGND - IDD_COLOR_SCREEN_TEXT;

    // initialize size of edit controls

    SendDlgItemMessage(hDlg, IDD_COLOR_RED, EM_LIMITTEXT, 3, 0);
    SendDlgItemMessage(hDlg, IDD_COLOR_GREEN, EM_LIMITTEXT, 3, 0);
    SendDlgItemMessage(hDlg, IDD_COLOR_BLUE, EM_LIMITTEXT, 3, 0);

    // initialize arrow controls

    SendDlgItemMessage(hDlg, IDD_COLOR_REDSCROLL, UDM_SETRANGE, 0, MAKELONG(255, 0));
    SendDlgItemMessage(hDlg, IDD_COLOR_REDSCROLL, UDM_SETPOS, 0, MAKELONG(GetRValue(AttrToRGB(ColorArray[iColor])), 0));
    SendDlgItemMessage(hDlg, IDD_COLOR_GREENSCROLL, UDM_SETRANGE, 0, MAKELONG(255, 0));
    SendDlgItemMessage(hDlg, IDD_COLOR_GREENSCROLL, UDM_SETPOS, 0, MAKELONG(GetGValue(AttrToRGB(ColorArray[iColor])), 0));
    SendDlgItemMessage(hDlg, IDD_COLOR_BLUESCROLL, UDM_SETRANGE, 0, MAKELONG(255, 0));
    SendDlgItemMessage(hDlg, IDD_COLOR_BLUESCROLL, UDM_SETPOS, 0, MAKELONG(GetBValue(AttrToRGB(ColorArray[iColor])), 0));

    CreateAndAssociateToolTipToControl(IDD_TRANSPARENCY, hDlg, IDS_TOOLTIP_OPACITY);

    SendMessage(GetDlgItem(hDlg, IDD_TRANSPARENCY), TBM_SETRANGE, FALSE, (LPARAM)MAKELONG(TRANSPARENCY_RANGE_MIN, BYTE_MAX));
    ToggleV2ColorControls(hDlg);

    return TRUE;
}

// Routine Description:
// - Dialog proc for the color selection dialog box.
//
INT_PTR WINAPI ColorDlgProc(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
    UINT Value;
    UINT Red;
    UINT Green;
    UINT Blue;
    UINT Item;
    HWND hWnd;
    HWND hWndOld;
    BOOL bOK;
    static bool fHaveInitialized = false;

    switch (wMsg)
    {
    case WM_INITDIALOG:
    {
        fHaveInitialized = true;
        return InitColorsDialog(hDlg);
    }

    case WM_COMMAND:
    {
        if (!fHaveInitialized)
        {
            return FALSE;
        }

        Item = LOWORD(wParam);
        switch (Item)
        {
        case IDD_COLOR_SCREEN_TEXT:
        case IDD_COLOR_SCREEN_BKGND:
        case IDD_COLOR_POPUP_TEXT:
        case IDD_COLOR_POPUP_BKGND:
            hWndOld = GetDlgItem(hDlg, ColorArray[iColor] + IDD_COLOR_1);

            iColor = Item - IDD_COLOR_SCREEN_TEXT;

            // repaint new color
            hWnd = GetDlgItem(hDlg, ColorArray[iColor] + IDD_COLOR_1);
            InvalidateRect(hWnd, NULL, TRUE);

            // repaint old color
            if (hWndOld != hWnd)
            {
                InvalidateRect(hWndOld, NULL, TRUE);
            }

            return TRUE;

        case IDD_COLOR_RED:
        case IDD_COLOR_GREEN:
        case IDD_COLOR_BLUE:
        {
            switch (HIWORD(wParam))
            {
            case EN_UPDATE:
                if (!CheckNum(hDlg, Item))
                {
                    Undo((HWND)lParam);
                }
                break;

            case EN_CHANGE:
                /*
                             * Update the state info structure
                             */
                Value = GetDlgItemInt(hDlg, Item, &bOK, TRUE);
                if (bOK)
                {
                    if (Value > 255)
                    {
                        UpdateItem(hDlg, Item, 255);
                        Value = 255;
                    }
                    if (Item == IDD_COLOR_RED)
                    {
                        Red = Value;
                    }
                    else
                    {
                        Red = GetRValue(AttrToRGB(ColorArray[iColor]));
                    }
                    if (Item == IDD_COLOR_GREEN)
                    {
                        Green = Value;
                    }
                    else
                    {
                        Green = GetGValue(AttrToRGB(ColorArray[iColor]));
                    }
                    if (Item == IDD_COLOR_BLUE)
                    {
                        Blue = Value;
                    }
                    else
                    {
                        Blue = GetBValue(AttrToRGB(ColorArray[iColor]));
                    }
                    UpdateStateInfo(hDlg, ColorArray[iColor] + IDD_COLOR_1, RGB(Red, Green, Blue));
                    UpdateApplyButton(hDlg);
                }
                __fallthrough;

            case EN_KILLFOCUS:
                /*
                             * Update the preview windows with the new value
                             */
                hWnd = GetDlgItem(hDlg, IDD_COLOR_SCREEN_COLORS);
                InvalidateRect(hWnd, NULL, FALSE);
                hWnd = GetDlgItem(hDlg, IDD_COLOR_POPUP_COLORS);
                InvalidateRect(hWnd, NULL, FALSE);
                hWnd = GetDlgItem(hDlg, ColorArray[iColor] + IDD_COLOR_1);
                InvalidateRect(hWnd, NULL, FALSE);
                break;
            }
            return TRUE;
        }
        }
        break;
    }

    case WM_NOTIFY:
    {
        const PSHNOTIFY* const pshn = (LPPSHNOTIFY)lParam;
        switch (pshn->hdr.code)
        {
        case PSN_APPLY:
            /*
                     * Write out the state values and exit.
                     */

            // Ignore opacity in v1 console
            if (g_fForceV2)
            {
                gpStateInfo->bWindowTransparency = (BYTE)SendDlgItemMessage(hDlg, IDD_TRANSPARENCY, TBM_GETPOS, 0, 0);
            }

            EndDlgPage(hDlg, !pshn->lParam);
            return TRUE;

        case PSN_RESET:
            // Ignore opacity in v1 console
            if (g_fForceV2 && g_bPreviewOpacity != gpStateInfo->bWindowTransparency)
            {
                SetLayeredWindowAttributes(gpStateInfo->hWnd, 0, gpStateInfo->bWindowTransparency, LWA_ALPHA);
            }

            return 0;

        case PSN_SETACTIVE:
            ToggleV2ColorControls(hDlg);
            return 0;

        case PSN_KILLACTIVE:
            /*
                     * Fake the dialog proc into thinking the edit control just
                     * lost focus so it'll update properly
                     */
            Item = GetDlgCtrlID(GetFocus());
            if (Item)
            {
                SendMessage(hDlg, WM_COMMAND, MAKELONG(Item, EN_KILLFOCUS), 0);
            }
            return TRUE;
        }
        break;
    }

    case WM_VSCROLL:
        /*
             * Fake the dialog proc into thinking the edit control just
             * lost focus so it'll update properly
             */
        Item = GetDlgCtrlID((HWND)lParam) - 1;
        SendMessage(hDlg, WM_COMMAND, MAKELONG(Item, EN_KILLFOCUS), 0);
        return TRUE;

    case WM_HSCROLL:
        if (IDD_TRANSPARENCY == GetDlgCtrlID((HWND)lParam))
        {
            switch (LOWORD(wParam))
            {
            //When moving slider with the mouse
            case TB_THUMBPOSITION:
            case TB_THUMBTRACK:
                g_bPreviewOpacity = (BYTE)HIWORD(wParam);
                break;

                //moving via keyboard
            default:
                g_bPreviewOpacity = (BYTE)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
            }

            PreviewOpacity(hDlg, g_bPreviewOpacity);
            UpdateApplyButton(hDlg);

            return TRUE;
        }
        break;

    case CM_SETCOLOR:
        UpdateStateInfo(hDlg, iColor + IDD_COLOR_SCREEN_TEXT, (UINT)wParam);
        UpdateApplyButton(hDlg);

        hWndOld = GetDlgItem(hDlg, ColorArray[iColor] + IDD_COLOR_1);

        ColorArray[iColor] = (BYTE)wParam;

        /* Force the preview window to repaint */

        if (iColor < (IDD_COLOR_POPUP_TEXT - IDD_COLOR_SCREEN_TEXT))
        {
            hWnd = GetDlgItem(hDlg, IDD_COLOR_SCREEN_COLORS);
        }
        else
        {
            hWnd = GetDlgItem(hDlg, IDD_COLOR_POPUP_COLORS);
        }
        InvalidateRect(hWnd, NULL, TRUE);

        // repaint new color
        hWnd = GetDlgItem(hDlg, ColorArray[iColor] + IDD_COLOR_1);
        InvalidateRect(hWnd, NULL, TRUE);
        SetFocus(hWnd);

        UpdateItem(hDlg, IDD_COLOR_RED, GetRValue(AttrToRGB(ColorArray[iColor])));
        UpdateItem(hDlg, IDD_COLOR_GREEN, GetGValue(AttrToRGB(ColorArray[iColor])));
        UpdateItem(hDlg, IDD_COLOR_BLUE, GetBValue(AttrToRGB(ColorArray[iColor])));

        // repaint old color
        if (hWndOld != hWnd)
        {
            InvalidateRect(hWndOld, NULL, TRUE);
        }
        return TRUE;

    default:
        break;
    }

    return FALSE;
}

// enables or disables color page dialog controls depending on whether V2 is enabled or not
void ToggleV2ColorControls(__in const HWND hDlg)
{
    EnableWindow(GetDlgItem(hDlg, IDD_TRANSPARENCY), g_fForceV2);
    SetOpacitySlider(hDlg);

    EnableWindow(GetDlgItem(hDlg, IDD_OPACITY_GROUPBOX), g_fForceV2);
    EnableWindow(GetDlgItem(hDlg, IDD_OPACITY_LOW_LABEL), g_fForceV2);
    EnableWindow(GetDlgItem(hDlg, IDD_OPACITY_HIGH_LABEL), g_fForceV2);
    EnableWindow(GetDlgItem(hDlg, IDD_OPACITY_VALUE), g_fForceV2);
}

void PreviewOpacity(HWND hDlg, BYTE bOpacity)
{
    // Ignore opacity in v1 console
    if (g_fForceV2)
    {
        WCHAR wszOpacityValue[4];
        HWND hWndConsole = gpStateInfo->hWnd;

        StringCchPrintf(wszOpacityValue, ARRAYSIZE(wszOpacityValue), L"%d", (int)((float)bOpacity / BYTE_MAX * 100));
        SetDlgItemText(hDlg, IDD_OPACITY_VALUE, wszOpacityValue);

        if (hWndConsole)
        {
            // TODO: CONSIDER:What happens when this code is eventually ported directly into the shell. Which "hWnd" does this apply to then? Desktop?
            // Hopefully it is simply null. -RSE   http://osgvsowi/2136073
            SetLayeredWindowAttributes(hWndConsole, 0, bOpacity, LWA_ALPHA);
        }
    }
}

void SetOpacitySlider(__in HWND hDlg)
{
    if (g_fForceV2)
    {
        if (0 == g_bPreviewOpacity) //if no preview opacity has been set yet...
        {
            g_bPreviewOpacity = (gpStateInfo->bWindowTransparency >= TRANSPARENCY_RANGE_MIN) ? gpStateInfo->bWindowTransparency : BYTE_MAX;
        }
    }
    else
    {
        g_bPreviewOpacity = BYTE_MAX; //always fully opaque in V1
    }

    SendMessage(GetDlgItem(hDlg, IDD_TRANSPARENCY), TBM_SETPOS, TRUE, (LPARAM)(g_bPreviewOpacity));
    PreviewOpacity(hDlg, g_bPreviewOpacity);
}
