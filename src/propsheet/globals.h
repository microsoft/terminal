// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Module Name:
//     globals.h
//
// Abstract:
//     One seperate container for many of the global variables in the propsheet
//
// Author:
//     Mike Griese (mikegr) 2016-Oct
//
// Revision History:

#pragma once
#include "font.h"

extern HINSTANCE ghInstance;
extern PCONSOLE_STATE_INFO gpStateInfo;
extern PFONT_INFO FontInfo;
extern ULONG NumberOfFonts;
extern ULONG FontInfoLength;
extern ULONG g_currentFontIndex;
extern ULONG DefaultFontIndex;
extern WCHAR DefaultFaceName[LF_FACESIZE];
extern WCHAR DefaultTTFaceName[LF_FACESIZE];
extern COORD DefaultFontSize;
extern BYTE DefaultFontFamily;
extern const wchar_t g_szPreviewText[];

//Initial default fonts and face names
extern PFACENODE gpFaceNames;

extern BOOL gbEnumerateFaces;
extern LONG gcxScreen;
extern LONG gcyScreen;
extern BOOL g_fForceV2;
extern BOOL g_fEditKeys;
extern BYTE g_bPreviewOpacity;
extern BOOL g_fHostedInFileProperties;

extern UINT OEMCP;
extern BOOL g_fEastAsianSystem;
extern bool g_fIsComCtlV6Present;

extern BOOL fChangeCodePage;

extern BOOL g_fSettingsDlgInitialized;
extern BOOL InEM_UNDO;

extern COLORREF g_fakeForegroundColor;
extern COLORREF g_fakeBackgroundColor;
extern COLORREF g_fakeCursorColor;

extern HWND g_hTerminalDlg;
extern HWND g_hOptionsDlg;
