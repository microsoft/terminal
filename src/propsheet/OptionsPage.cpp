// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

void InitializeCursorSize(const HWND hOptionsDlg)
{
    unsigned int newRadioValue = IDD_CURSOR_ADVANCED;
    if (gpStateInfo->CursorType != 0)
    {
        // IDD_CURSOR_ADVANCED is used as a placeholder for when a
        // non-legacy shape is selected.
        newRadioValue = IDD_CURSOR_ADVANCED;
    }
    else if (gpStateInfo->CursorSize <= 25)
    {
        newRadioValue = IDD_CURSOR_SMALL;
    }
    else if (gpStateInfo->CursorSize <= 50)
    {
        newRadioValue = IDD_CURSOR_MEDIUM;
    }
    else
    {
        newRadioValue = IDD_CURSOR_LARGE;
    }
    CheckRadioButton(hOptionsDlg, IDD_CURSOR_SMALL, IDD_CURSOR_ADVANCED, newRadioValue);
}

bool OptionsCommandCallback(HWND hDlg, const unsigned int Item, const unsigned int Notification, HWND hControlWindow)
{
    UINT Value;
    BOOL bOK;

    switch (Item)
    {
    case IDD_LANGUAGELIST:
        switch (Notification)
        {
        case CBN_SELCHANGE:
        {
            HWND hWndLanguageCombo;
            LONG lListIndex;

            hWndLanguageCombo = GetDlgItem(hDlg, IDD_LANGUAGELIST);
            lListIndex = (LONG)SendMessage(hWndLanguageCombo, CB_GETCURSEL, 0, 0);
            Value = (UINT)SendMessage(hWndLanguageCombo, CB_GETITEMDATA, lListIndex, 0);
            if (Value != -1)
            {
                fChangeCodePage = (Value != gpStateInfo->CodePage);
                UpdateStateInfo(hDlg, Item, Value);
                UpdateApplyButton(hDlg);
            }
            break;
        }

        default:
            DBGFONTS(("unhandled CBN_%x from POINTSLIST\n", Notification));
            break;
        }
        return TRUE;
    case IDD_CURSOR_SMALL:
    case IDD_CURSOR_MEDIUM:
    case IDD_CURSOR_LARGE:
        UpdateStateInfo(hDlg, Item, 0);
        if (Notification != EN_KILLFOCUS)
        {
            // we don't want to light up the apply button just because a cursor selection lost focus -- this can
            // happen when switching between tabs even if there's no actual change in selection.
            UpdateApplyButton(hDlg);
        }
        return TRUE;

    case IDD_HISTORY_NODUP:
    case IDD_QUICKEDIT:
    case IDD_INSERT:
        Value = IsDlgButtonChecked(hDlg, Item);
        UpdateStateInfo(hDlg, Item, Value);
        UpdateApplyButton(hDlg);
        return TRUE;

    case IDD_HISTORY_SIZE:
    case IDD_HISTORY_NUM:
        switch (Notification)
        {
        case EN_UPDATE:
            if (!CheckNum(hDlg, Item))
            {
                Undo(hControlWindow);
            }
            else if (g_fSettingsDlgInitialized)
            {
                UpdateApplyButton(hDlg);
            }
            break;

        case EN_KILLFOCUS:
            /*
             * Update the state info structure
             */
            Value = GetDlgItemInt(hDlg, Item, &bOK, TRUE);
            if (bOK)
            {
                UpdateStateInfo(hDlg, Item, Value);
                UpdateApplyButton(hDlg);
            }
            break;
        }
        return TRUE;

    case IDD_FORCEV2:
    {
        g_fForceV2 = (IsDlgButtonChecked(hDlg, IDD_FORCEV2) != BST_CHECKED);
        ToggleV2OptionsControls(hDlg);
        UpdateApplyButton(hDlg);
        return TRUE;
    }

    case IDD_LINE_SELECTION:
    {
        const BOOL fCurrentLineSelection = (IsDlgButtonChecked(hDlg, IDD_LINE_SELECTION) == BST_CHECKED);
        gpStateInfo->fLineSelection = fCurrentLineSelection;
        UpdateApplyButton(hDlg);
        return TRUE;
    }

    case IDD_FILTER_ON_PASTE:
    {
        const BOOL fCurrentFilterOnPaste = (IsDlgButtonChecked(hDlg, IDD_FILTER_ON_PASTE) == BST_CHECKED);
        gpStateInfo->fFilterOnPaste = fCurrentFilterOnPaste;
        UpdateApplyButton(hDlg);
        return TRUE;
    }

    case IDD_INTERCEPT_COPY_PASTE:
    {
        const BOOL fCurrentInterceptCopyPaste = (IsDlgButtonChecked(hDlg, IDD_INTERCEPT_COPY_PASTE) == BST_CHECKED);
        gpStateInfo->InterceptCopyPaste = fCurrentInterceptCopyPaste;
        UpdateApplyButton(hDlg);
        return TRUE;
    }

    case IDD_CTRL_KEYS_ENABLED:
    {
        // NOTE: the checkbox being checked means that Ctrl keys should be enabled, hence the negation here
        const BOOL fCurrentCtrlKeysDisabled = !(IsDlgButtonChecked(hDlg, IDD_CTRL_KEYS_ENABLED) == BST_CHECKED);
        gpStateInfo->fCtrlKeyShortcutsDisabled = fCurrentCtrlKeysDisabled;
        UpdateApplyButton(hDlg);
        return TRUE;
    }

    case IDD_EDIT_KEYS:
    {
        const BOOL fCurrentEditKeys = (IsDlgButtonChecked(hDlg, IDD_EDIT_KEYS) == BST_CHECKED);
        g_fEditKeys = fCurrentEditKeys;
        UpdateApplyButton(hDlg);
        return TRUE;
    }
    }
    return FALSE;
}

// Routine Description:
// - Dialog proc for the settings dialog box.
//
INT_PTR WINAPI SettingsDlgProc(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
    UINT Item;
    UINT Notification;

    switch (wMsg)
    {
    case WM_INITDIALOG:
        // Initialize the global handle to this dialog
        g_hOptionsDlg = hDlg;

        CheckDlgButton(hDlg, IDD_HISTORY_NODUP, gpStateInfo->HistoryNoDup);
        CheckDlgButton(hDlg, IDD_QUICKEDIT, gpStateInfo->QuickEdit);
        CheckDlgButton(hDlg, IDD_INSERT, gpStateInfo->InsertMode);

        // v2 options
        CheckDlgButton(hDlg, IDD_FORCEV2, !g_fForceV2);
        CheckDlgButton(hDlg, IDD_LINE_SELECTION, gpStateInfo->fLineSelection);
        CheckDlgButton(hDlg, IDD_FILTER_ON_PASTE, gpStateInfo->fFilterOnPaste);
        CheckDlgButton(hDlg, IDD_CTRL_KEYS_ENABLED, !gpStateInfo->fCtrlKeyShortcutsDisabled);
        CheckDlgButton(hDlg, IDD_EDIT_KEYS, g_fEditKeys);
        CheckDlgButton(hDlg, IDD_INTERCEPT_COPY_PASTE, gpStateInfo->InterceptCopyPaste);

        // tooltips
        CreateAndAssociateToolTipToControl(IDD_LINE_SELECTION, hDlg, IDS_TOOLTIP_LINE_SELECTION);
        CreateAndAssociateToolTipToControl(IDD_FILTER_ON_PASTE, hDlg, IDS_TOOLTIP_FILTER_ON_PASTE);
        CreateAndAssociateToolTipToControl(IDD_CTRL_KEYS_ENABLED, hDlg, IDS_TOOLTIP_CTRL_KEYS);
        CreateAndAssociateToolTipToControl(IDD_EDIT_KEYS, hDlg, IDS_TOOLTIP_EDIT_KEYS);
        CreateAndAssociateToolTipToControl(IDD_INTERCEPT_COPY_PASTE, hDlg, IDS_TOOLTIP_INTERCEPT_COPY_PASTE);

        // initialize cursor radio buttons
        InitializeCursorSize(hDlg);

        SetDlgItemInt(hDlg, IDD_HISTORY_SIZE, gpStateInfo->HistoryBufferSize, FALSE);
        SendDlgItemMessage(hDlg, IDD_HISTORY_SIZE, EM_LIMITTEXT, 3, 0);
        SendDlgItemMessage(hDlg, IDD_HISTORY_SIZESCROLL, UDM_SETRANGE, 0, MAKELONG(999, 1));

        SetDlgItemInt(hDlg, IDD_HISTORY_NUM, gpStateInfo->NumberOfHistoryBuffers, FALSE);
        SendDlgItemMessage(hDlg, IDD_HISTORY_NUM, EM_LIMITTEXT, 3, 0);
        SendDlgItemMessage(hDlg, IDD_HISTORY_NUM, EM_SETSEL, 0, (DWORD)-1);
        SendDlgItemMessage(hDlg, IDD_HISTORY_NUMSCROLL, UDM_SETRANGE, 0, MAKELONG(999, 1));

        if (g_fEastAsianSystem)
        {
            // In CJK systems, we always show the codepage on both the defaults and non-defaults propsheets
            if (gpStateInfo->Defaults)
            {
                LanguageListCreate(hDlg, gpStateInfo->CodePage);
            }
            else
            {
                LanguageDisplay(hDlg, gpStateInfo->CodePage);
            }
        }
        else
        {
            // On non-CJK systems, we show the codepage on a non-default propsheet, but don't allow the user to view or
            // change it on the defaults propsheet
            const HWND hwndLanguageGroupbox = GetDlgItem(hDlg, IDD_LANGUAGE_GROUPBOX);
            if (hwndLanguageGroupbox)
            {
                if (gpStateInfo->Defaults)
                {
                    const HWND hwndLanguageList = GetDlgItem(hDlg, IDD_LANGUAGELIST);
                    ShowWindow(hwndLanguageList, SW_HIDE);
                    ShowWindow(hwndLanguageGroupbox, SW_HIDE);
                }
                else
                {
                    const HWND hwndLanguage = GetDlgItem(hDlg, IDD_LANGUAGE);
                    LanguageDisplay(hDlg, gpStateInfo->CodePage);
                    ShowWindow(hwndLanguage, SW_SHOW);
                    ShowWindow(hwndLanguageGroupbox, SW_SHOW);
                }
            }
        }

        g_fSettingsDlgInitialized = TRUE;
        return TRUE;

    case WM_COMMAND:
        Item = LOWORD(wParam);
        Notification = HIWORD(wParam);
        return OptionsCommandCallback(hDlg, Item, Notification, (HWND)lParam);

    case WM_NOTIFY:
    {
        if (lParam && (wParam == IDD_HELP_SYSLINK || wParam == IDD_HELP_LEGACY_LINK))
        {
            // handle hyperlink click or keyboard activation
            switch (((LPNMHDR)lParam)->code)
            {
            case NM_CLICK:
            case NM_RETURN:
            {
                PNMLINK pnmLink = (PNMLINK)lParam;
                if (0 == pnmLink->item.iLink)
                {
                    ShellExecute(NULL,
                                 L"open",
                                 pnmLink->item.szUrl,
                                 NULL,
                                 NULL,
                                 SW_SHOW);
                }

                break;
            }
            }
        }
        else
        {
            const PSHNOTIFY* const pshn = (LPPSHNOTIFY)lParam;
            if (lParam)
            {
                switch (pshn->hdr.code)
                {
                case PSN_APPLY:
                    /*
                         * Write out the state values and exit.
                         */
                    EndDlgPage(hDlg, !pshn->lParam);
                    return TRUE;

                case PSN_SETACTIVE:
                    ToggleV2OptionsControls(hDlg);
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
            }
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

    default:
        break;
    }

    return FALSE;
}

// enables or disables options page dialog controls depending on whether V2 is enabled or not
void ToggleV2OptionsControls(__in const HWND hDlg)
{
    EnableWindow(GetDlgItem(hDlg, IDD_LINE_SELECTION), g_fForceV2);
    CheckDlgButton(hDlg, IDD_LINE_SELECTION, g_fForceV2 ? gpStateInfo->fLineSelection : FALSE);

    EnableWindow(GetDlgItem(hDlg, IDD_FILTER_ON_PASTE), g_fForceV2);
    CheckDlgButton(hDlg, IDD_FILTER_ON_PASTE, g_fForceV2 ? gpStateInfo->fFilterOnPaste : FALSE);

    EnableWindow(GetDlgItem(hDlg, IDD_CTRL_KEYS_ENABLED), g_fForceV2);
    CheckDlgButton(hDlg, IDD_CTRL_KEYS_ENABLED, g_fForceV2 ? !gpStateInfo->fCtrlKeyShortcutsDisabled : FALSE);

    EnableWindow(GetDlgItem(hDlg, IDD_EDIT_KEYS), g_fForceV2);
    CheckDlgButton(hDlg, IDD_EDIT_KEYS, g_fForceV2 ? g_fEditKeys : FALSE);

    EnableWindow(GetDlgItem(hDlg, IDD_INTERCEPT_COPY_PASTE), g_fForceV2);
    CheckDlgButton(hDlg, IDD_INTERCEPT_COPY_PASTE, g_fForceV2 ? gpStateInfo->InterceptCopyPaste : FALSE);
}
