// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

PCONSOLE_STATE_INFO gpStateInfo;

LONG gcxScreen;
LONG gcyScreen;

BOOL g_fForceV2;
// If we didn't launch as a v2 console window, we don't want to persist v2
// settings when we close, as they'll get zero'd. Use this to track the initial
// launch state.
BOOL g_fEditKeys;
BYTE g_bPreviewOpacity = 0x00; //sentinel value for initial test on dialog entry. Once initialized, won't be less than TRANSPARENCY_RANGE_MIN

BOOL g_fHostedInFileProperties = FALSE;

UINT OEMCP;
BOOL g_fEastAsianSystem;
bool g_fIsComCtlV6Present;

const wchar_t g_szPreviewText[] =
    L"C:\\WINDOWS> dir                       \n"
    L"SYSTEM       <DIR>     10-01-99   5:00a\n"
    L"SYSTEM32     <DIR>     10-01-99   5:00a\n"
    L"README   TXT     26926 10-01-99   5:00a\n"
    L"WINDOWS  BMP     46080 10-01-99   5:00a\n"
    L"NOTEPAD  EXE    337232 10-01-99   5:00a\n"
    L"CLOCK    AVI     39594 10-01-99   5:00p\n"
    L"WIN      INI      7005 10-01-99   5:00a\n";

BOOL fChangeCodePage = FALSE;

WCHAR DefaultFaceName[LF_FACESIZE];
WCHAR DefaultTTFaceName[LF_FACESIZE];
COORD DefaultFontSize;
BYTE DefaultFontFamily;
ULONG DefaultFontIndex = 0;
ULONG g_currentFontIndex = 0;

PFONT_INFO FontInfo = NULL;
ULONG NumberOfFonts;
ULONG FontInfoLength;
BOOL gbEnumerateFaces = FALSE;
PFACENODE gpFaceNames = NULL;

BOOL g_fSettingsDlgInitialized = FALSE;

BOOL InEM_UNDO = FALSE;

// These values are used to "remember" the colors across a disable/re-enable,
//      so that if we disable the setting then re-enable it, we can re-initalize
//      it with the same value it had before.
COLORREF g_fakeForegroundColor = RGB(242, 242, 242); // Default bright white
COLORREF g_fakeBackgroundColor = RGB(12, 12, 12); // Default black
COLORREF g_fakeCursorColor = RGB(242, 242, 242); // Default bright white

HWND g_hTerminalDlg = static_cast<HWND>(INVALID_HANDLE_VALUE);
HWND g_hOptionsDlg = static_cast<HWND>(INVALID_HANDLE_VALUE);
