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
    static auto reverse = true;
    static SearchFlag flags{ SearchFlag::CaseInsensitive };
    static std::wstring lastFindString;
    static Search searcher;

    switch (Message)
    {
    case WM_INITDIALOG:
        SetWindowLongPtrW(hWnd, DWLP_USER, lParam);
        CheckRadioButton(hWnd, ID_CONSOLE_FINDUP, ID_CONSOLE_FINDDOWN, (reverse ? ID_CONSOLE_FINDUP : ID_CONSOLE_FINDDOWN));
        CheckDlgButton(hWnd, ID_CONSOLE_FINDCASE, WI_IsFlagClear(flags, SearchFlag::CaseInsensitive));
        CheckDlgButton(hWnd, ID_CONSOLE_FINDREGEX, WI_IsFlagSet(flags, SearchFlag::RegularExpression));
        SetDlgItemTextW(hWnd, ID_CONSOLE_FINDSTR, lastFindString.c_str());
        return TRUE;
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDOK:
        {
            auto length = SendDlgItemMessageW(hWnd, ID_CONSOLE_FINDSTR, WM_GETTEXTLENGTH, 0, 0);
            lastFindString.resize(length);
            length = GetDlgItemTextW(hWnd, ID_CONSOLE_FINDSTR, lastFindString.data(), gsl::narrow_cast<int>(length + 1));
            lastFindString.resize(length);

            WI_UpdateFlag(flags, SearchFlag::CaseInsensitive, IsDlgButtonChecked(hWnd, ID_CONSOLE_FINDCASE) == 0);
            WI_UpdateFlag(flags, SearchFlag::RegularExpression, IsDlgButtonChecked(hWnd, ID_CONSOLE_FINDREGEX) != 0);
            reverse = IsDlgButtonChecked(hWnd, ID_CONSOLE_FINDDOWN) == 0;

            LockConsole();
            auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

            if (searcher.IsStale(gci.renderData, lastFindString, flags))
            {
                searcher.Reset(gci.renderData, lastFindString, flags, reverse);
            }
            else
            {
                searcher.FindNext(reverse);
            }

            if (searcher.SelectCurrent())
            {
                return TRUE;
            }

            std::ignore = gci.GetActiveOutputBuffer().SendNotifyBeep();
            break;
        }
        case IDCANCEL:
            EndDialog(hWnd, 0);
            searcher = Search{};
            return TRUE;
        default:
            break;
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
