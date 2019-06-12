// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

BOOL g_fScreenSizeDlgInitialized = FALSE;
BOOL g_fInScreenSizeSETACTIVE = FALSE;

BOOL GetStateInfo(HWND /*hDlg*/, UINT Item, __out LPINT lpValue)
{
    BOOL bRet = TRUE;
    int Value;

    switch (Item)
    {
    case IDD_SCRBUF_WIDTH:
        Value = gpStateInfo->ScreenBufferSize.X;
        break;
    case IDD_SCRBUF_HEIGHT:
        Value = gpStateInfo->ScreenBufferSize.Y;
        break;
    case IDD_WINDOW_WIDTH:
        Value = gpStateInfo->WindowSize.X;
        break;
    case IDD_WINDOW_HEIGHT:
        Value = gpStateInfo->WindowSize.Y;
        break;
    case IDD_WINDOW_POSX:
        Value = gpStateInfo->WindowPosX;
        break;
    case IDD_WINDOW_POSY:
        Value = gpStateInfo->WindowPosY;
        break;
    default:
        Value = 0;
        bRet = FALSE;
        break;
    }

    *lpValue = Value;
    return bRet;
}

/*++
    Dialog proc for the screen size dialog box.
--*/
INT_PTR WINAPI ScreenSizeDlgProc(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
    UINT Value;
    UINT Item;
    HWND hWnd;
    BOOL bOK;
    LONG xScreen;
    LONG yScreen;
    LONG cxScreen;
    LONG cyScreen;
    LONG cxFrame;
    LONG cyFrame;

    switch (wMsg)
    {
    case WM_INITDIALOG:
        // initialize size of edit controls

        SendDlgItemMessage(hDlg, IDD_SCRBUF_WIDTH, EM_LIMITTEXT, 4, 0);
        SendDlgItemMessage(hDlg, IDD_SCRBUF_HEIGHT, EM_LIMITTEXT, 4, 0);
        SendDlgItemMessage(hDlg, IDD_WINDOW_WIDTH, EM_LIMITTEXT, 4, 0);
        SendDlgItemMessage(hDlg, IDD_WINDOW_HEIGHT, EM_LIMITTEXT, 4, 0);
        SendDlgItemMessage(hDlg, IDD_WINDOW_POSX, EM_LIMITTEXT, 5, 0);
        SendDlgItemMessage(hDlg, IDD_WINDOW_POSY, EM_LIMITTEXT, 5, 0);

        // Get some system parameters

        xScreen = GetSystemMetrics(SM_XVIRTUALSCREEN);
        yScreen = GetSystemMetrics(SM_YVIRTUALSCREEN);
        cxScreen = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        cyScreen = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        cxFrame = GetSystemMetrics(SM_CXFRAME);
        cyFrame = GetSystemMetrics(SM_CYFRAME);

        // initialize arrow controls

        SendDlgItemMessage(hDlg, IDD_SCRBUF_WIDTHSCROLL, UDM_SETRANGE, 0, MAKELONG(9999, 1));
        SendDlgItemMessage(hDlg, IDD_SCRBUF_WIDTHSCROLL, UDM_SETPOS, 0, MAKELONG(gpStateInfo->ScreenBufferSize.X, 0));
        SendDlgItemMessage(hDlg, IDD_SCRBUF_HEIGHTSCROLL, UDM_SETRANGE, 0, MAKELONG(9999, 1));
        SendDlgItemMessage(hDlg, IDD_SCRBUF_HEIGHTSCROLL, UDM_SETPOS, 0, MAKELONG(gpStateInfo->ScreenBufferSize.Y, 0));
        SendDlgItemMessage(hDlg, IDD_WINDOW_WIDTHSCROLL, UDM_SETRANGE, 0, MAKELONG(9999, 1));
        SendDlgItemMessage(hDlg, IDD_WINDOW_WIDTHSCROLL, UDM_SETPOS, 0, MAKELONG(gpStateInfo->WindowSize.X, 0));
        SendDlgItemMessage(hDlg, IDD_WINDOW_HEIGHTSCROLL, UDM_SETRANGE, 0, MAKELONG(9999, 1));
        SendDlgItemMessage(hDlg, IDD_WINDOW_HEIGHTSCROLL, UDM_SETPOS, 0, MAKELONG(gpStateInfo->WindowSize.Y, 0));
        SendDlgItemMessage(hDlg, IDD_WINDOW_POSXSCROLL, UDM_SETRANGE, 0, MAKELONG(xScreen + cxScreen - cxFrame, xScreen - cxFrame));
        SendDlgItemMessage(hDlg, IDD_WINDOW_POSXSCROLL, UDM_SETPOS, 0, MAKELONG(gpStateInfo->WindowPosX, 0));
        SendDlgItemMessage(hDlg, IDD_WINDOW_POSYSCROLL, UDM_SETRANGE, 0, MAKELONG(yScreen + cyScreen - cyFrame, yScreen - cyFrame));
        SendDlgItemMessage(hDlg, IDD_WINDOW_POSYSCROLL, UDM_SETPOS, 0, MAKELONG(gpStateInfo->WindowPosY, 0));

        //
        // put current values in dialog box
        //

        CheckDlgButton(hDlg, IDD_AUTO_POSITION, gpStateInfo->AutoPosition);
        SendMessage(hDlg, WM_COMMAND, IDD_AUTO_POSITION, 0);

        CheckDlgButton(hDlg, IDD_LINE_WRAP, gpStateInfo->fWrapText);
        CreateAndAssociateToolTipToControl(IDD_LINE_WRAP, hDlg, IDS_TOOLTIP_LINE_WRAP);
        ToggleV2ColorControls(hDlg);
        g_fScreenSizeDlgInitialized = TRUE;

        return TRUE;

    case WM_VSCROLL:
        /*
         * Fake the dialog proc into thinking the edit control just
         * lost focus so it'll update properly
         */
        Item = GetDlgCtrlID((HWND)lParam) - 1;
        SendMessage(hDlg, WM_COMMAND, MAKELONG(Item, EN_KILLFOCUS), 0);
        return TRUE;

    case WM_COMMAND:
        Item = LOWORD(wParam);
        switch (Item)
        {
        case IDD_SCRBUF_WIDTH:
        case IDD_SCRBUF_HEIGHT:
        case IDD_WINDOW_WIDTH:
        case IDD_WINDOW_HEIGHT:
        case IDD_WINDOW_POSX:
        case IDD_WINDOW_POSY:
            switch (HIWORD(wParam))
            {
            case EN_UPDATE:
                if (!CheckNum(hDlg, Item))
                {
                    Undo((HWND)lParam);
                }
                else if (!g_fInScreenSizeSETACTIVE && g_fScreenSizeDlgInitialized)
                {
                    UpdateApplyButton(hDlg);
                }

                break;
            case EN_KILLFOCUS:
                /*
                 * Update the state info structure
                 */
                Value = (UINT)SendDlgItemMessage(hDlg, Item + 1, UDM_GETPOS, 0, 0);
                if (HIWORD(Value) == 0)
                {
                    UpdateStateInfo(hDlg, Item, (SHORT)LOWORD(Value));
                }
                else
                {
                    Value = GetStateInfo(hDlg, Item, &bOK);
                    if (bOK)
                    {
                        UpdateItem(hDlg, Item, Value);
                    }
                }

                /*
                 * Update the preview window with the new value
                 */
                hWnd = GetDlgItem(hDlg, IDD_PREVIEWWINDOW);
                SendMessage(hWnd, CM_PREVIEW_UPDATE, 0, 0);
                break;
            }
            return TRUE;

        case IDD_LINE_WRAP:
        {
            const BOOL fCurrentLineWrap = (IsDlgButtonChecked(hDlg, IDD_LINE_WRAP) == BST_CHECKED);
            gpStateInfo->fWrapText = fCurrentLineWrap;
            EnableWindow(GetDlgItem(hDlg, IDD_SCRBUF_WIDTH), g_fForceV2 ? !fCurrentLineWrap : TRUE);
            UpdateApplyButton(hDlg);
            return TRUE;
        }

        case IDD_AUTO_POSITION:
            Value = IsDlgButtonChecked(hDlg, IDD_AUTO_POSITION);
            UpdateStateInfo(hDlg, IDD_AUTO_POSITION, Value);
            if (g_fScreenSizeDlgInitialized)
            {
                UpdateApplyButton(hDlg);
            }

            for (Item = IDD_WINDOW_POSX; Item < IDD_AUTO_POSITION; Item++)
            {
                hWnd = GetDlgItem(hDlg, Item);
                EnableWindow(hWnd, (Value == FALSE));
            }
            break;

        default:
            break;
        }
        break;

    case WM_NOTIFY:
    {
        const PSHNOTIFY* const pshn = (LPPSHNOTIFY)lParam;
        switch (pshn->hdr.code)
        {
        case PSN_APPLY:
            /*
             * Write out the state values and exit.
             */
            EndDlgPage(hDlg, !pshn->lParam);
            return TRUE;

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

        case PSN_SETACTIVE:
            // When page becomes active, ensure that window and screen size box availablility
            // is updated based on the Word Wrap status.
            g_fInScreenSizeSETACTIVE = TRUE;
            if (g_fForceV2 && gpStateInfo->fWrapText)
            {
                EnableWindow(GetDlgItem(hDlg, IDD_SCRBUF_WIDTH), FALSE);

                gpStateInfo->ScreenBufferSize.X = gpStateInfo->WindowSize.X;
                UpdateItem(hDlg, IDD_SCRBUF_WIDTH, gpStateInfo->ScreenBufferSize.X);

                // Force the preview window to update as well
                hWnd = GetDlgItem(hDlg, IDD_PREVIEWWINDOW);
                SendMessage(hWnd, CM_PREVIEW_UPDATE, 0, 0);
            }
            else
            {
                EnableWindow(GetDlgItem(hDlg, IDD_SCRBUF_WIDTH), TRUE);
            }

            ToggleV2LayoutControls(hDlg);
            g_fInScreenSizeSETACTIVE = FALSE;
            return 0;
        }
        break;
    }

    default:
        break;
    }

    return FALSE;
}

// enables or disables layout page dialog controls depending on whether V2 is enabled or not
void ToggleV2LayoutControls(__in const HWND hDlg)
{
    EnableWindow(GetDlgItem(hDlg, IDD_LINE_WRAP), g_fForceV2);
    CheckDlgButton(hDlg, IDD_LINE_WRAP, g_fForceV2 ? gpStateInfo->fWrapText : FALSE);
    EnableWindow(GetDlgItem(hDlg, IDD_SCRBUF_WIDTH), g_fForceV2 ? !gpStateInfo->fWrapText : TRUE);
}
