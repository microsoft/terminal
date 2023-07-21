// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "find.h"
#include "resource.h"
#include "window.hpp"

#include "../../host/dbcs.h"
#include "../../host/handle.h"
#include "../buffer/out/search.h"

#include "../inc/ServiceLocator.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Interactivity;

INT_PTR CALLBACK FindDialogProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    // This bool is used to track which option - up or down - was used to perform the last search. That way, the next time the
    //   find dialog is opened, it will default to the last used option.
    static auto fFindSearchUp = true;
    static std::wstring lastFindString;

    WCHAR szBuf[SEARCH_STRING_LENGTH + 1];
    switch (Message)
    {
    case WM_INITDIALOG:
        SetWindowLongPtrW(hWnd, DWLP_USER, lParam);
        SendDlgItemMessageW(hWnd, ID_CONSOLE_FINDSTR, EM_LIMITTEXT, ARRAYSIZE(szBuf) - 1, 0);
        CheckRadioButton(hWnd, ID_CONSOLE_FINDUP, ID_CONSOLE_FINDDOWN, (fFindSearchUp ? ID_CONSOLE_FINDUP : ID_CONSOLE_FINDDOWN));
        SetDlgItemText(hWnd, ID_CONSOLE_FINDSTR, lastFindString.c_str());
        return TRUE;
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDOK:
        {
            const auto StringLength = (USHORT)GetDlgItemTextW(hWnd, ID_CONSOLE_FINDSTR, szBuf, ARRAYSIZE(szBuf));
            if (StringLength == 0)
            {
                lastFindString.clear();
                break;
            }
            const auto IgnoreCase = IsDlgButtonChecked(hWnd, ID_CONSOLE_FINDCASE) == 0;
            const auto Reverse = IsDlgButtonChecked(hWnd, ID_CONSOLE_FINDDOWN) == 0;
            fFindSearchUp = !!Reverse;
            auto& ScreenInfo = gci.GetActiveOutputBuffer();

            std::wstring wstr(szBuf, StringLength);
            lastFindString = wstr;
            LockConsole();
            auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

            Search search(gci.renderData,
                          wstr,
                          Reverse ? Search::Direction::Backward : Search::Direction::Forward,
                          IgnoreCase ? Search::Sensitivity::CaseInsensitive : Search::Sensitivity::CaseSensitive);

            if (search.FindNext())
            {
                Telemetry::Instance().LogFindDialogNextClicked(StringLength, (Reverse != 0), (IgnoreCase == 0));
                search.Select();
                return TRUE;
            }
            else
            {
                // The string wasn't found.
                ScreenInfo.SendNotifyBeep();
            }
            break;
        }
        case IDCANCEL:
            Telemetry::Instance().FindDialogClosed();
            EndDialog(hWnd, 0);
            return TRUE;
        }
        break;
    }
    default:
        break;
    }
    return FALSE;
}

void DoFind()
{
    auto& g = ServiceLocator::LocateGlobals();
    const auto pWindow = ServiceLocator::LocateConsoleWindow();

    UnlockConsole();
    if (pWindow != nullptr)
    {
        const auto hwnd = pWindow->GetWindowHandle();

        ++g.uiDialogBoxCount;
        DialogBoxParamW(g.hInstance, MAKEINTRESOURCE(ID_CONSOLE_FINDDLG), hwnd, FindDialogProc, (LPARAM) nullptr);
        --g.uiDialogBoxCount;
    }
}
