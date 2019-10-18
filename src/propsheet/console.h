/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    console.h

Abstract:

    This module contains the definitions for the console applet

Author:

    Jerry Shea (jerrysh) Feb-3-1992

Revision History:
    Mike Griese, migrie, Oct 2016:
        Moved to cpp and the OpenConsole project

--*/
#pragma once

#include "font.h"
#include "OptionsPage.h"
#include "LayoutPage.h"
#include "ColorsPage.h"
#include "TerminalPage.h"
#include "ColorControl.h"

//
// Icon ID.
//

#define IDI_CONSOLE 1

//
// String table constants
//

// clang-format off
#define IDS_NAME                      1
#define IDS_INFO                      2
#define IDS_TITLE                     3
#define IDS_RASTERFONT                4
#define IDS_FONTSIZE                  5
#define IDS_SELECTEDFONT              6
#define IDS_SAVE                      7
#define IDS_LINKERRCAP                8
#define IDS_LINKERROR                 9
#define IDS_FONTSTRING               10
#define IDS_TOOLTIP_LINE_SELECTION   11
#define IDS_TOOLTIP_FILTER_ON_PASTE  12
#define IDS_TOOLTIP_LINE_WRAP        13
#define IDS_TOOLTIP_CTRL_KEYS        14
#define IDS_TOOLTIP_EDIT_KEYS        15
// unused 16
#define IDS_TOOLTIP_OPACITY          17
#define IDS_TOOLTIP_INTERCEPT_COPY_PASTE    18
// clang-format on

void MakeAltRasterFont(
    __in UINT CodePage,
    __out COORD* AltFontSize,
    __out BYTE* AltFontFamily,
    __out ULONG* AltFontIndex,
    __out_ecount(LF_FACESIZE) LPTSTR AltFaceName);

[[nodiscard]] NTSTATUS InitializeDbcsMisc(VOID);

BYTE CodePageToCharSet(
    UINT CodePage);

BOOL ShouldAllowAllMonoTTFonts();

LPTTFONTLIST
SearchTTFont(
    __in_opt LPCTSTR ptszFace,
    BOOL fCodePage,
    UINT CodePage);

BOOL IsAvailableTTFont(
    LPCTSTR ptszFace);

BOOL IsAvailableTTFontCP(
    LPCTSTR ptszFace,
    UINT CodePage);

BOOL IsDisableBoldTTFont(
    LPCTSTR ptszFace);

LPTSTR
GetAltFaceName(
    LPCTSTR ptszFace);

[[nodiscard]] NTSTATUS DestroyDbcsMisc(VOID);

int LanguageListCreate(
    HWND hDlg,
    UINT CodePage);

int LanguageDisplay(
    HWND hDlg,
    UINT CodePage);

//
// Function prototypes
//

INT_PTR ConsolePropertySheet(
    __in HWND hWnd,
    __in PCONSOLE_STATE_INFO pStateInfo);

VOID RegisterClasses(
    HINSTANCE hModule);

VOID UnregisterClasses(
    HINSTANCE hModule);

INT_PTR APIENTRY FontDlgProc(
    HWND hDlg,
    UINT wMsg,
    WPARAM wParam,
    LPARAM lParam);

VOID InitRegistryValues(
    __out PCONSOLE_STATE_INFO pStateInfo);

DWORD GetRegistryValues(
    __out_opt PCONSOLE_STATE_INFO StateInfo);

VOID SetGlobalRegistryValues();

VOID SetRegistryValues(
    PCONSOLE_STATE_INFO StateInfo,
    DWORD dwPage);

[[nodiscard]] LRESULT CALLBACK FontPreviewWndProc(
    HWND hWnd,
    UINT wMsg,
    WPARAM wParam,
    LPARAM lParam);

[[nodiscard]] LRESULT CALLBACK PreviewWndProc(
    HWND hWnd,
    UINT wMsg,
    WPARAM wParam,
    LPARAM lParam);

VOID EndDlgPage(
    const HWND hDlg,
    const BOOL fSaveNow);

BOOL UpdateStateInfo(
    HWND hDlg,
    UINT Item,
    int Value);

BOOL InitializeConsoleState();
void UninitializeConsoleState();
void UpdateApplyButton(const HWND hDlg);
[[nodiscard]] HRESULT FindFontAndUpdateState();

BOOL PopulatePropSheetPageArray(_Out_writes_(cPsps) PROPSHEETPAGE* pPsp, const size_t cPsps, const BOOL fRegisterCallbacks);

void CreateAndAssociateToolTipToControl(const UINT dlgItem, const HWND hDlg, const UINT idsToolTip);

BOOL CheckNum(HWND hDlg, UINT Item);
void UpdateItem(HWND hDlg, UINT item, UINT nNum);
void Undo(HWND hControlWindow);

//
// Macros
//
#define AttrToRGB(Attr) (gpStateInfo->ColorTable[(Attr)&0x0F])
#define ScreenTextColor(pStateInfo) \
    (AttrToRGB(LOBYTE(pStateInfo->ScreenAttributes) & 0x0F))
#define ScreenBkColor(pStateInfo) \
    (AttrToRGB(LOBYTE(pStateInfo->ScreenAttributes >> 4)))
#define PopupTextColor(pStateInfo) \
    (AttrToRGB(LOBYTE(pStateInfo->PopupAttributes) & 0x0F))
#define PopupBkColor(pStateInfo) \
    (AttrToRGB(LOBYTE(pStateInfo->PopupAttributes >> 4)))

// clang-format off
#if DBG
  #define _DBGFONTS  0x00000001
  #define _DBGFONTS2 0x00000002
  #define _DBGCHARS  0x00000004
  #define _DBGOUTPUT 0x00000008
  #define _DBGALL    0xFFFFFFFF
  extern ULONG gDebugFlag;

  #define DBGFONTS(_params_)
  #define DBGFONTS2(_params_)
  #define DBGCHARS(_params_)
  #define DBGOUTPUT(_params_)
#else
  #define DBGFONTS(_params_)
  #define DBGFONTS2(_params_)
  #define DBGCHARS(_params_)
  #define DBGOUTPUT(_params_)
#endif
// clang-format on

// Macro definitions that handle codepages
//
#define CP_US (UINT)437
#define CP_JPN (UINT)932
#define CP_WANSUNG (UINT)949
#define CP_TC (UINT)950
#define CP_SC (UINT)936

#define IsBilingualCP(cp) ((cp) == CP_JPN || (cp) == CP_WANSUNG)
#define IsEastAsianCP(cp) ((cp) == CP_JPN || (cp) == CP_WANSUNG || (cp) == CP_TC || (cp) == CP_SC)

const unsigned int TRANSPARENCY_RANGE_MIN = 0x4D;

const unsigned int OPTIONS_PAGE_INDEX = 0;
const unsigned int FONT_PAGE_INDEX = 1;
const unsigned int LAYOUT_PAGE_INDEX = 2;
const unsigned int COLORS_PAGE_INDEX = 3;
const unsigned int TERMINAL_PAGE_INDEX = 4;
// number of property sheet pages
static const int V1_NUMBER_OF_PAGES = 4;
static const int NUMBER_OF_PAGES = 5;

BOOL GetConsoleBoolValue(__in PCWSTR pszValueName, __in BOOL fDefault);
