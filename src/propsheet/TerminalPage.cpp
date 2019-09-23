// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "TerminalPage.h"
#include "OptionsPage.h" // For InitializeCursorSize
#include "ColorControl.h"
#include <functional>

// From conattrs.h
const COLORREF INVALID_COLOR = 0xffffffff;

const int COLOR_MAX = 255;

void _UseForeground(const HWND hDlg, const bool useFg) noexcept
{
    EnableWindow(GetDlgItem(hDlg, IDD_TERMINAL_FG_REDSCROLL), useFg);
    EnableWindow(GetDlgItem(hDlg, IDD_TERMINAL_FG_GREENSCROLL), useFg);
    EnableWindow(GetDlgItem(hDlg, IDD_TERMINAL_FG_BLUESCROLL), useFg);
    EnableWindow(GetDlgItem(hDlg, IDD_TERMINAL_FG_RED), useFg);
    EnableWindow(GetDlgItem(hDlg, IDD_TERMINAL_FG_GREEN), useFg);
    EnableWindow(GetDlgItem(hDlg, IDD_TERMINAL_FG_BLUE), useFg);

    if (useFg)
    {
        const auto r = GetDlgItemInt(hDlg, IDD_TERMINAL_FG_RED, nullptr, FALSE);
        const auto g = GetDlgItemInt(hDlg, IDD_TERMINAL_FG_GREEN, nullptr, FALSE);
        const auto b = GetDlgItemInt(hDlg, IDD_TERMINAL_FG_BLUE, nullptr, FALSE);
        gpStateInfo->DefaultForeground = RGB(r, g, b);
    }
    else
    {
        gpStateInfo->DefaultForeground = INVALID_COLOR;
    }
}

void _UseBackground(const HWND hDlg, const bool useBg) noexcept
{
    EnableWindow(GetDlgItem(hDlg, IDD_TERMINAL_BG_REDSCROLL), useBg);
    EnableWindow(GetDlgItem(hDlg, IDD_TERMINAL_BG_GREENSCROLL), useBg);
    EnableWindow(GetDlgItem(hDlg, IDD_TERMINAL_BG_BLUESCROLL), useBg);
    EnableWindow(GetDlgItem(hDlg, IDD_TERMINAL_BG_RED), useBg);
    EnableWindow(GetDlgItem(hDlg, IDD_TERMINAL_BG_GREEN), useBg);
    EnableWindow(GetDlgItem(hDlg, IDD_TERMINAL_BG_BLUE), useBg);

    if (useBg)
    {
        auto r = GetDlgItemInt(hDlg, IDD_TERMINAL_BG_RED, nullptr, FALSE);
        auto g = GetDlgItemInt(hDlg, IDD_TERMINAL_BG_GREEN, nullptr, FALSE);
        auto b = GetDlgItemInt(hDlg, IDD_TERMINAL_BG_BLUE, nullptr, FALSE);
        gpStateInfo->DefaultBackground = RGB(r, g, b);
    }
    else
    {
        gpStateInfo->DefaultBackground = INVALID_COLOR;
    }
}

void _UseCursorColor(const HWND hDlg, const bool useColor) noexcept
{
    EnableWindow(GetDlgItem(hDlg, IDD_TERMINAL_CURSOR_REDSCROLL), useColor);
    EnableWindow(GetDlgItem(hDlg, IDD_TERMINAL_CURSOR_GREENSCROLL), useColor);
    EnableWindow(GetDlgItem(hDlg, IDD_TERMINAL_CURSOR_BLUESCROLL), useColor);
    EnableWindow(GetDlgItem(hDlg, IDD_TERMINAL_CURSOR_RED), useColor);
    EnableWindow(GetDlgItem(hDlg, IDD_TERMINAL_CURSOR_GREEN), useColor);
    EnableWindow(GetDlgItem(hDlg, IDD_TERMINAL_CURSOR_BLUE), useColor);

    if (useColor)
    {
        const auto r = GetDlgItemInt(hDlg, IDD_TERMINAL_CURSOR_RED, nullptr, FALSE);
        const auto g = GetDlgItemInt(hDlg, IDD_TERMINAL_CURSOR_GREEN, nullptr, FALSE);
        const auto b = GetDlgItemInt(hDlg, IDD_TERMINAL_CURSOR_BLUE, nullptr, FALSE);
        gpStateInfo->CursorColor = RGB(r, g, b);
    }
    else
    {
        gpStateInfo->CursorColor = INVALID_COLOR;
    }
}

void _UpdateTextAndScroll(const HWND hDlg,
                          const SHORT value,
                          const WORD textItem,
                          const WORD scrollItem) noexcept
{
    UpdateItem(hDlg, textItem, value);
    SendDlgItemMessage(hDlg, scrollItem, UDM_SETPOS, 0, MAKELONG(value, 0));
}

bool InitTerminalDialog(const HWND hDlg) noexcept
{
    // Initialize the global handle to this dialog
    g_hTerminalDlg = hDlg;
    // Group radios
    CheckRadioButton(hDlg, IDD_TERMINAL_INVERSE_CURSOR, IDD_TERMINAL_CURSOR_USECOLOR, IDD_TERMINAL_INVERSE_CURSOR);

    // initialize size of edit controls
    SendDlgItemMessage(hDlg, IDD_TERMINAL_FG_RED, EM_LIMITTEXT, 3, 0);
    SendDlgItemMessage(hDlg, IDD_TERMINAL_FG_GREEN, EM_LIMITTEXT, 3, 0);
    SendDlgItemMessage(hDlg, IDD_TERMINAL_FG_BLUE, EM_LIMITTEXT, 3, 0);

    SendDlgItemMessage(hDlg, IDD_TERMINAL_BG_RED, EM_LIMITTEXT, 3, 0);
    SendDlgItemMessage(hDlg, IDD_TERMINAL_BG_GREEN, EM_LIMITTEXT, 3, 0);
    SendDlgItemMessage(hDlg, IDD_TERMINAL_BG_BLUE, EM_LIMITTEXT, 3, 0);

    SendDlgItemMessage(hDlg, IDD_TERMINAL_CURSOR_RED, EM_LIMITTEXT, 3, 0);
    SendDlgItemMessage(hDlg, IDD_TERMINAL_CURSOR_GREEN, EM_LIMITTEXT, 3, 0);
    SendDlgItemMessage(hDlg, IDD_TERMINAL_CURSOR_BLUE, EM_LIMITTEXT, 3, 0);

    // Cap the color inputs to 255
    const auto colorRange = MAKELONG(COLOR_MAX, 0);
    SendDlgItemMessage(hDlg, IDD_TERMINAL_FG_REDSCROLL, UDM_SETRANGE, 0, colorRange);
    SendDlgItemMessage(hDlg, IDD_TERMINAL_FG_GREENSCROLL, UDM_SETRANGE, 0, colorRange);
    SendDlgItemMessage(hDlg, IDD_TERMINAL_FG_BLUESCROLL, UDM_SETRANGE, 0, colorRange);

    SendDlgItemMessage(hDlg, IDD_TERMINAL_BG_REDSCROLL, UDM_SETRANGE, 0, colorRange);
    SendDlgItemMessage(hDlg, IDD_TERMINAL_BG_GREENSCROLL, UDM_SETRANGE, 0, colorRange);
    SendDlgItemMessage(hDlg, IDD_TERMINAL_BG_BLUESCROLL, UDM_SETRANGE, 0, colorRange);

    SendDlgItemMessage(hDlg, IDD_TERMINAL_CURSOR_REDSCROLL, UDM_SETRANGE, 0, colorRange);
    SendDlgItemMessage(hDlg, IDD_TERMINAL_CURSOR_GREENSCROLL, UDM_SETRANGE, 0, colorRange);
    SendDlgItemMessage(hDlg, IDD_TERMINAL_CURSOR_BLUESCROLL, UDM_SETRANGE, 0, colorRange);

    const bool initialTerminalFG = gpStateInfo->DefaultForeground != INVALID_COLOR;
    const bool initialTerminalBG = gpStateInfo->DefaultBackground != INVALID_COLOR;
    const bool initialCursorLegacy = gpStateInfo->CursorColor == INVALID_COLOR;
    if (initialTerminalFG)
    {
        g_fakeForegroundColor = gpStateInfo->DefaultForeground;
    }
    if (initialTerminalBG)
    {
        g_fakeBackgroundColor = gpStateInfo->DefaultBackground;
    }
    if (!initialCursorLegacy)
    {
        g_fakeCursorColor = gpStateInfo->CursorColor;
    }
    CheckDlgButton(hDlg, IDD_USE_TERMINAL_FG, initialTerminalFG);
    CheckDlgButton(hDlg, IDD_USE_TERMINAL_BG, initialTerminalBG);
    CheckRadioButton(hDlg,
                     IDD_TERMINAL_INVERSE_CURSOR,
                     IDD_TERMINAL_CURSOR_USECOLOR,
                     initialCursorLegacy ? IDD_TERMINAL_INVERSE_CURSOR : IDD_TERMINAL_CURSOR_USECOLOR);

    // Set the initial values in the edit boxes and scroll controls
    const auto fgRed = GetRValue(g_fakeForegroundColor);
    const auto fgGreen = GetGValue(g_fakeForegroundColor);
    const auto fgBlue = GetBValue(g_fakeForegroundColor);
    const auto bgRed = GetRValue(g_fakeBackgroundColor);
    const auto bgGreen = GetGValue(g_fakeBackgroundColor);
    const auto bgBlue = GetBValue(g_fakeBackgroundColor);
    const auto cursorRed = GetRValue(g_fakeCursorColor);
    const auto cursorGreen = GetGValue(g_fakeCursorColor);
    const auto cursorBlue = GetBValue(g_fakeCursorColor);

    _UpdateTextAndScroll(hDlg, fgRed, IDD_TERMINAL_FG_RED, IDD_TERMINAL_FG_REDSCROLL);
    _UpdateTextAndScroll(hDlg, fgGreen, IDD_TERMINAL_FG_GREEN, IDD_TERMINAL_FG_GREENSCROLL);
    _UpdateTextAndScroll(hDlg, fgBlue, IDD_TERMINAL_FG_BLUE, IDD_TERMINAL_FG_BLUESCROLL);

    _UpdateTextAndScroll(hDlg, bgRed, IDD_TERMINAL_BG_RED, IDD_TERMINAL_BG_REDSCROLL);
    _UpdateTextAndScroll(hDlg, bgGreen, IDD_TERMINAL_BG_GREEN, IDD_TERMINAL_BG_GREENSCROLL);
    _UpdateTextAndScroll(hDlg, bgBlue, IDD_TERMINAL_BG_BLUE, IDD_TERMINAL_BG_BLUESCROLL);

    _UpdateTextAndScroll(hDlg, cursorRed, IDD_TERMINAL_CURSOR_RED, IDD_TERMINAL_CURSOR_REDSCROLL);
    _UpdateTextAndScroll(hDlg, cursorGreen, IDD_TERMINAL_CURSOR_GREEN, IDD_TERMINAL_CURSOR_GREENSCROLL);
    _UpdateTextAndScroll(hDlg, cursorBlue, IDD_TERMINAL_CURSOR_BLUE, IDD_TERMINAL_CURSOR_BLUESCROLL);

    _UseForeground(hDlg, initialTerminalFG);
    _UseBackground(hDlg, initialTerminalBG);
    _UseCursorColor(hDlg, !initialCursorLegacy);

    InvalidateRect(GetDlgItem(hDlg, IDD_TERMINAL_FGCOLOR), NULL, FALSE);
    InvalidateRect(GetDlgItem(hDlg, IDD_TERMINAL_BGCOLOR), NULL, FALSE);
    InvalidateRect(GetDlgItem(hDlg, IDD_TERMINAL_CURSOR_COLOR), NULL, FALSE);

    CheckRadioButton(hDlg,
                     IDD_TERMINAL_LEGACY_CURSOR,
                     IDD_TERMINAL_SOLIDBOX,
                     IDD_TERMINAL_LEGACY_CURSOR + gpStateInfo->CursorType);

    CheckDlgButton(hDlg, IDD_DISABLE_SCROLLFORWARD, gpStateInfo->TerminalScrolling);

    return true;
}

void _ChangeColorControl(const HWND hDlg,
                         const WORD item,
                         const WORD redControl,
                         const WORD greenControl,
                         const WORD blueControl,
                         const WORD colorControl,
                         DWORD& setting) noexcept
{
    BOOL bOK = FALSE;
    int newValue = GetDlgItemInt(hDlg, item, &bOK, TRUE);
    int r = GetRValue(setting);
    int g = GetGValue(setting);
    int b = GetBValue(setting);

    if (bOK)
    {
        if (newValue > COLOR_MAX)
        {
            UpdateItem(hDlg, item, COLOR_MAX);
            newValue = COLOR_MAX;
        }
        if (item == redControl)
        {
            r = newValue;
        }
        else if (item == greenControl)
        {
            g = newValue;
        }
        else if (item == blueControl)
        {
            b = newValue;
        }

        setting = RGB(r, g, b);
    }

    InvalidateRect(GetDlgItem(hDlg, colorControl), NULL, FALSE);
}

void _ChangeForegroundRGB(const HWND hDlg, const WORD item) noexcept
{
    _ChangeColorControl(hDlg,
                        item,
                        IDD_TERMINAL_FG_RED,
                        IDD_TERMINAL_FG_GREEN,
                        IDD_TERMINAL_FG_BLUE,
                        IDD_TERMINAL_FGCOLOR,
                        gpStateInfo->DefaultForeground);
    g_fakeForegroundColor = gpStateInfo->DefaultForeground;
}

void _ChangeBackgroundRGB(const HWND hDlg, const WORD item) noexcept
{
    _ChangeColorControl(hDlg,
                        item,
                        IDD_TERMINAL_BG_RED,
                        IDD_TERMINAL_BG_GREEN,
                        IDD_TERMINAL_BG_BLUE,
                        IDD_TERMINAL_BGCOLOR,
                        gpStateInfo->DefaultBackground);
    g_fakeBackgroundColor = gpStateInfo->DefaultBackground;
}

void _ChangeCursorRGB(const HWND hDlg, const WORD item) noexcept
{
    _ChangeColorControl(hDlg,
                        item,
                        IDD_TERMINAL_CURSOR_RED,
                        IDD_TERMINAL_CURSOR_GREEN,
                        IDD_TERMINAL_CURSOR_BLUE,
                        IDD_TERMINAL_CURSOR_COLOR,
                        gpStateInfo->CursorColor);
    g_fakeCursorColor = gpStateInfo->CursorColor;
}

bool _CommandColorInput(const HWND hDlg,
                        const WORD item,
                        const WORD command,
                        const std::function<void(HWND, WORD)> changeFunction) noexcept
{
    bool handled = false;

    switch (command)
    {
    case EN_UPDATE:
    case EN_CHANGE:
        changeFunction(hDlg, item);
        handled = true;
        UpdateApplyButton(hDlg);
        break;
    }
    return handled;
}

bool TerminalDlgCommand(const HWND hDlg, const WORD item, const WORD command) noexcept
{
    bool handled = false;
    switch (item)
    {
    case IDD_TERMINAL_CURSOR_USECOLOR:
    case IDD_TERMINAL_INVERSE_CURSOR:
        _UseCursorColor(hDlg, IsDlgButtonChecked(hDlg, IDD_TERMINAL_CURSOR_USECOLOR));
        handled = true;
        UpdateApplyButton(hDlg);
        break;
    case IDD_USE_TERMINAL_FG:
        _UseForeground(hDlg, IsDlgButtonChecked(hDlg, IDD_USE_TERMINAL_FG));
        handled = true;
        UpdateApplyButton(hDlg);
        break;
    case IDD_USE_TERMINAL_BG:
        _UseBackground(hDlg, IsDlgButtonChecked(hDlg, IDD_USE_TERMINAL_BG));
        handled = true;
        UpdateApplyButton(hDlg);
        break;

    case IDD_TERMINAL_FG_RED:
    case IDD_TERMINAL_FG_GREEN:
    case IDD_TERMINAL_FG_BLUE:
        handled = _CommandColorInput(hDlg, item, command, _ChangeForegroundRGB);
        break;

    case IDD_TERMINAL_BG_RED:
    case IDD_TERMINAL_BG_GREEN:
    case IDD_TERMINAL_BG_BLUE:
        handled = _CommandColorInput(hDlg, item, command, _ChangeBackgroundRGB);
        break;

    case IDD_TERMINAL_CURSOR_RED:
    case IDD_TERMINAL_CURSOR_GREEN:
    case IDD_TERMINAL_CURSOR_BLUE:
        handled = _CommandColorInput(hDlg, item, command, _ChangeCursorRGB);
        break;

    case IDD_TERMINAL_LEGACY_CURSOR:
    case IDD_TERMINAL_VERTBAR:
    case IDD_TERMINAL_UNDERSCORE:
    case IDD_TERMINAL_EMPTYBOX:
    case IDD_TERMINAL_SOLIDBOX:
    {
        gpStateInfo->CursorType = item - IDD_TERMINAL_LEGACY_CURSOR;
        UpdateApplyButton(hDlg);

        // See GH#1219 - When the cursor state is something other than legacy,
        // we need to manually check the "IDD_CURSOR_ADVANCED" radio button on
        // the Options page. This will prevent the Options page from manually
        // resetting the cursor to legacy.
        if (g_hOptionsDlg != INVALID_HANDLE_VALUE)
        {
            InitializeCursorSize(g_hOptionsDlg);
        }

        handled = true;
        break;
    }
    case IDD_DISABLE_SCROLLFORWARD:
        gpStateInfo->TerminalScrolling = IsDlgButtonChecked(hDlg, IDD_DISABLE_SCROLLFORWARD);
        UpdateApplyButton(hDlg);
        handled = true;
        break;
    }

    return handled;
}

INT_PTR WINAPI TerminalDlgProc(const HWND hDlg, const UINT wMsg, const WPARAM wParam, const LPARAM lParam)
{
    static bool fHaveInitialized = false;

    switch (wMsg)
    {
    case WM_INITDIALOG:
        fHaveInitialized = true;
        return InitTerminalDialog(hDlg);
    case WM_COMMAND:
        if (!fHaveInitialized)
        {
            return false;
        }
        return TerminalDlgCommand(hDlg, LOWORD(wParam), HIWORD(wParam));
    case WM_NOTIFY:
    {
        if (lParam && (wParam == IDD_HELP_TERMINAL))
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
                    EndDlgPage(hDlg, !pshn->lParam);
                    return TRUE;
                case PSN_KILLACTIVE:
                {
                    // Fake the dialog proc into thinking the edit control just
                    // lost focus so it'll update properly
                    int item = GetDlgCtrlID(GetFocus());
                    if (item)
                    {
                        SendMessage(hDlg, WM_COMMAND, MAKELONG(item, EN_KILLFOCUS), 0);
                    }
                    return TRUE;
                }
                }
            }
        }
    }
    case WM_VSCROLL:
        // Fake the dialog proc into thinking the edit control just
        // lost focus so it'll update properly
        SendMessage(hDlg, WM_COMMAND, MAKELONG((GetDlgCtrlID((HWND)lParam) - 1), EN_KILLFOCUS), 0);
        return TRUE;

    case WM_DESTROY:
        // MSFT:20740368
        // When the propsheet is opened straight from explorer, NOT from
        //      conhost itself, then explorer will load console.dll once,
        //      and re-use it for subsequent launches. This means that on
        //      the first launch of the propsheet, our fHaveInitialized will
        //      be false until we actually do the init work, but on
        //      subsequent launches, fHaveInitialized will be re-used, and
        //      found to be true, and we'll zero out the values of the
        //      colors. This is because the message loop decides to update
        //      the values of the textboxes before we get a chance to put
        //      the current values into them. When the textboxes update,
        //      they'll overwrite the current color components with whatever
        //      they currently have, which is 0.
        // To avoid this madness, make sure to reset our initialization
        //      state when the dialog is closed.
        fHaveInitialized = false;
        break;
    }

    return false;
}
