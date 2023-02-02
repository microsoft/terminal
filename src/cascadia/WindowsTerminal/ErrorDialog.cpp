// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "resource.h"
#include <WerApi.h>

typedef wil::unique_any<HREPORT, decltype(&WerReportCloseHandle), WerReportCloseHandle> unique_wer_report;

struct ErrorDialogContext
{
    HRESULT hr;
    std::wstring message;
    HFONT font{ nullptr };

    ~ErrorDialogContext()
    {
        if (font)
        {
            DeleteObject(font);
        }
    }
};

static LRESULT CALLBACK ErrDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        // We've passed in the pointer to an error dialog context using DialogBoxParam
        ErrorDialogContext* ctx{ reinterpret_cast<ErrorDialogContext*>(lParam) };
        SetWindowLongPtrW(hDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(ctx));

        HWND editControl{ GetDlgItem(hDlg, IDC_ERRVALUE) };
        const auto message{ std::format(L"HR 0x{0:08x}\r\n{1}", static_cast<unsigned int>(ctx->hr), ctx->message) };
        SetWindowTextW(editControl, message.c_str());

        HFONT& font{ ctx->font };
        const auto fontHeight{ -MulDiv(10, GetDpiForWindow(hDlg), 72) };
        font = CreateFontW(fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE | FIXED_PITCH, L"Cascadia Mono");
        if (!font)
        {
            font = CreateFontW(fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE | FIXED_PITCH, L"Consolas");
        }
        // Context takes ownership of font and deletes it later
        SendMessageW(editControl, WM_SETFONT, reinterpret_cast<WPARAM>(font), 0);
        break;
    }
    case WM_DESTROY:
    {
        ErrorDialogContext* ctx{ reinterpret_cast<ErrorDialogContext*>(GetWindowLongPtrW(hDlg, DWLP_USER)) };
        delete ctx;
        break;
    }
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
        case IDOK:
            EndDialog(hDlg, TRUE);
            return TRUE;
        }
    }
    }
    return FALSE;
}

void DisplayErrorDialogBlockingAndReport(const HRESULT hr, const std::wstring_view message)
{
    auto ctx = new ErrorDialogContext{ hr, std::wstring{ message } };
    DialogBoxParamW(nullptr, MAKEINTRESOURCEW(IDD_ERRDIALOG), nullptr, ErrDlgProc, reinterpret_cast<LPARAM>(ctx));

    unique_wer_report errorReport;
    WerReportCreate(L"AppCrash", WerReportApplicationCrash, nullptr, errorReport.put());
    WerReportAddDump(errorReport.get(), GetCurrentProcess(), nullptr, WerDumpTypeMiniDump, nullptr, nullptr, 0);
    WerReportSubmit(errorReport.get(), WerConsentNotAsked, 0, nullptr);

    TerminateProcess(GetCurrentProcess(), 1);
}
