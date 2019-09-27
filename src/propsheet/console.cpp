/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:
    menu.c

Abstract:
        This file implements the system menu management.

Author:
    Therese Stowell (thereses) Jan-24-1992 (swiped from Win3.1)

Revision History:
    Mike Griese (migrie) Oct-2016 - Move to OpenConsole project

--*/

#include "precomp.h"

#pragma hdrstop

UINT gnCurrentPage;

#define SYSTEM_ROOT (L"%SystemRoot%")
#define SYSTEM_ROOT_LENGTH (sizeof(SYSTEM_ROOT) - sizeof(WCHAR))

void RecreateFontHandles(const HWND hWnd);

void UpdateItem(HWND hDlg, UINT item, UINT nNum)
{
    SetDlgItemInt(hDlg, item, nNum, TRUE);
    SendDlgItemMessage(hDlg, item, EM_SETSEL, 0, -1);
}

// Routine Description:
// Sends an EM_UNDO message. Typically used after some user data is determined to be invalid.
void Undo(HWND hControlWindow)
{
    if (!InEM_UNDO)
    {
        InEM_UNDO = TRUE;
        SendMessage(hControlWindow, EM_UNDO, 0, 0);
        InEM_UNDO = FALSE;
    }
}

// Routine Description:
//  - Validates a string in the TextItem with id=Item represents a number
BOOL CheckNum(HWND hDlg, UINT Item)
{
    int i;
    TCHAR szNum[5];
    BOOL fSigned;

    // The window position corrdinates can be signed, nothing else.
    if (Item == IDD_WINDOW_POSX || Item == IDD_WINDOW_POSY)
    {
        fSigned = TRUE;
    }
    else
    {
        fSigned = FALSE;
    }

    GetDlgItemText(hDlg, Item, szNum, ARRAYSIZE(szNum));
    for (i = 0; szNum[i]; i++)
    {
        if (!iswdigit(szNum[i]) && (!fSigned || i > 0 || szNum[i] != TEXT('-')))
        {
            return FALSE;
        }
    }

    return TRUE;
}

void SaveConsoleSettingsIfNeeded(const HWND hwnd)
{
    if (gpStateInfo->UpdateValues)
    {
        // If we're looking at the default font, clear the values before we save them
        if ((gpStateInfo->FontFamily == DefaultFontFamily) &&
            (gpStateInfo->FontSize.X == DefaultFontSize.X) &&
            (gpStateInfo->FontSize.Y == DefaultFontSize.Y) &&
            (gpStateInfo->FontWeight == FW_NORMAL) &&
            (wcscmp(gpStateInfo->FaceName, DefaultFaceName) == 0))
        {
            gpStateInfo->FontFamily = 0;
            gpStateInfo->FontSize.X = 0;
            gpStateInfo->FontSize.Y = 0;
            gpStateInfo->FontWeight = 0;
            gpStateInfo->FaceName[0] = TEXT('\0');
        }

        if (gpStateInfo->LinkTitle != NULL)
        {
            SetGlobalRegistryValues();
            if (!NT_SUCCESS(ShortcutSerialization::s_SetLinkValues(gpStateInfo,
                                                                   g_fEastAsianSystem,
                                                                   g_fForceV2,
                                                                   gpStateInfo->fIsV2Console)))
            {
                WCHAR szMessage[MAX_PATH + 100];
                WCHAR awchBuffer[MAX_PATH] = { 0 };
                STARTUPINFOW si;

                // An error occured try to save the link file, display a message box to that effect...
                GetStartupInfoW(&si);
                LoadStringW(ghInstance, IDS_LINKERROR, awchBuffer, ARRAYSIZE(awchBuffer));
                StringCchPrintf(szMessage,
                                ARRAYSIZE(szMessage),
                                awchBuffer,
                                gpStateInfo->LinkTitle);
                LoadStringW(ghInstance, IDS_LINKERRCAP, awchBuffer, ARRAYSIZE(awchBuffer));

                MessageBoxW(hwnd,
                            szMessage,
                            awchBuffer,
                            MB_APPLMODAL | MB_OK | MB_ICONSTOP | MB_SETFOREGROUND);
            }
            else
            {
                // we're up to date, so mark ourselves as such (needed for "Apply" case)
                gpStateInfo->UpdateValues = FALSE;
            }
        }
        else
        {
            SetRegistryValues(gpStateInfo, gnCurrentPage);

            // we're up to date, so mark ourselves as such (needed for "Apply" case)
            gpStateInfo->UpdateValues = FALSE;
        }
    }
}

void EndDlgPage(const HWND hDlg, const BOOL fSaveNow)
{
    HWND hParent;
    HWND hTabCtrl;

    /*
     * If we've already made a decision, we're done
     */
    if (gpStateInfo->UpdateValues)
    {
        SetDlgMsgResult(hDlg, PSN_APPLY, PSNRET_NOERROR);
        return;
    }

    /*
     * Get the current page number
     */
    hParent = GetParent(hDlg);
    hTabCtrl = PropSheet_GetTabControl(hParent);
    gnCurrentPage = TabCtrl_GetCurSel(hTabCtrl);

    gpStateInfo->UpdateValues = TRUE;

    SetDlgMsgResult(hDlg, PSN_APPLY, PSNRET_NOERROR);

    if (fSaveNow)
    {
        // needed for "Apply" scenario
        SaveConsoleSettingsIfNeeded(hDlg);
    }

    PropSheet_UnChanged(hDlg, 0);
}

#define TOOLTIP_MAXLENGTH (256)
void CreateAndAssociateToolTipToControl(const UINT dlgItem, const HWND hDlg, const UINT idsToolTip)
{
    HWND hwndTooltip = CreateWindowEx(0 /*dwExtStyle*/,
                                      TOOLTIPS_CLASS,
                                      NULL /*lpWindowName*/,
                                      TTS_ALWAYSTIP,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      hDlg,
                                      NULL /*hMenu*/,
                                      ghInstance,
                                      NULL /*lpParam*/);

    if (hwndTooltip)
    {
        WCHAR szTooltip[TOOLTIP_MAXLENGTH] = { 0 };
        if (LoadString(ghInstance, idsToolTip, szTooltip, ARRAYSIZE(szTooltip)) > 0)
        {
            TOOLINFO toolInfo = { 0 };
            toolInfo.cbSize = sizeof(toolInfo);
            toolInfo.hwnd = hDlg;
            toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
            toolInfo.uId = (UINT_PTR)GetDlgItem(hDlg, dlgItem);
            toolInfo.lpszText = szTooltip;
            SendMessage(hwndTooltip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
        }
    }
}

BOOL UpdateStateInfo(HWND hDlg, UINT Item, int Value)
{
    switch (Item)
    {
    case IDD_SCRBUF_WIDTH:
        gpStateInfo->ScreenBufferSize.X = (SHORT)Value;

        // If we're in V2 mode with wrap text on OR if the window is larger than the buffer, adjust the window to match.
        if ((g_fForceV2 && gpStateInfo->fWrapText) || gpStateInfo->WindowSize.X > Value)
        {
            gpStateInfo->WindowSize.X = (SHORT)Value;
            UpdateItem(hDlg, IDD_WINDOW_WIDTH, Value);
        }
        break;
    case IDD_SCRBUF_HEIGHT:
        gpStateInfo->ScreenBufferSize.Y = (SHORT)Value;
        if (gpStateInfo->WindowSize.Y > Value)
        {
            gpStateInfo->WindowSize.Y = (SHORT)Value;
            UpdateItem(hDlg, IDD_WINDOW_HEIGHT, Value);
        }
        break;
    case IDD_WINDOW_WIDTH:
        gpStateInfo->WindowSize.X = (SHORT)Value;

        // If we're in V2 mode with wrap text on OR if the buffer is smaller than the window, adjust the buffer to match.
        if ((g_fForceV2 && gpStateInfo->fWrapText) || gpStateInfo->ScreenBufferSize.X < Value)
        {
            gpStateInfo->ScreenBufferSize.X = (SHORT)Value;
            UpdateItem(hDlg, IDD_SCRBUF_WIDTH, Value);
        }
        break;
    case IDD_WINDOW_HEIGHT:
        gpStateInfo->WindowSize.Y = (SHORT)Value;
        if (gpStateInfo->ScreenBufferSize.Y < Value)
        {
            gpStateInfo->ScreenBufferSize.Y = (SHORT)Value;
            UpdateItem(hDlg, IDD_SCRBUF_HEIGHT, Value);
        }
        break;
    case IDD_WINDOW_POSX:
        if (Value < 0)
        {
            gpStateInfo->WindowPosX = max(SHORT_MIN, Value);
        }
        else
        {
            gpStateInfo->WindowPosX = min(SHORT_MAX, Value);
        }
        break;
    case IDD_WINDOW_POSY:
        if (Value < 0)
        {
            gpStateInfo->WindowPosY = max(SHORT_MIN, Value);
        }
        else
        {
            gpStateInfo->WindowPosY = min(SHORT_MAX, Value);
        }
        break;
    case IDD_AUTO_POSITION:
        gpStateInfo->AutoPosition = Value;
        break;
    case IDD_COLOR_SCREEN_TEXT:
        gpStateInfo->ScreenAttributes =
            (gpStateInfo->ScreenAttributes & 0xF0) |
            (Value & 0x0F);
        break;
    case IDD_COLOR_SCREEN_BKGND:
        gpStateInfo->ScreenAttributes =
            (gpStateInfo->ScreenAttributes & 0x0F) |
            (WORD)(Value << 4);
        break;
    case IDD_COLOR_POPUP_TEXT:
        gpStateInfo->PopupAttributes =
            (gpStateInfo->PopupAttributes & 0xF0) |
            (Value & 0x0F);
        break;
    case IDD_COLOR_POPUP_BKGND:
        gpStateInfo->PopupAttributes =
            (gpStateInfo->PopupAttributes & 0x0F) |
            (WORD)(Value << 4);
        break;
    case IDD_COLOR_1:
    case IDD_COLOR_2:
    case IDD_COLOR_3:
    case IDD_COLOR_4:
    case IDD_COLOR_5:
    case IDD_COLOR_6:
    case IDD_COLOR_7:
    case IDD_COLOR_8:
    case IDD_COLOR_9:
    case IDD_COLOR_10:
    case IDD_COLOR_11:
    case IDD_COLOR_12:
    case IDD_COLOR_13:
    case IDD_COLOR_14:
    case IDD_COLOR_15:
    case IDD_COLOR_16:
        gpStateInfo->ColorTable[Item - IDD_COLOR_1] = Value;
        break;
    case IDD_LANGUAGELIST:
        /*
         * Value is a code page
         */
        gpStateInfo->CodePage = Value;
        break;
    case IDD_QUICKEDIT:
        gpStateInfo->QuickEdit = Value;
        break;
    case IDD_INSERT:
        gpStateInfo->InsertMode = Value;
        break;
    case IDD_HISTORY_SIZE:
        gpStateInfo->HistoryBufferSize = max(Value, 1);
        break;
    case IDD_HISTORY_NUM:
        gpStateInfo->NumberOfHistoryBuffers = max(Value, 1);
        break;
    case IDD_HISTORY_NODUP:
        gpStateInfo->HistoryNoDup = Value;
        break;
    case IDD_CURSOR_SMALL:
        gpStateInfo->CursorSize = 25;
        // Set the cursor to legacy style
        gpStateInfo->CursorType = 0;
        // Check the legacy radio button on the terminal page
        if (g_hTerminalDlg != INVALID_HANDLE_VALUE)
        {
            CheckRadioButton(g_hTerminalDlg,
                             IDD_TERMINAL_LEGACY_CURSOR,
                             IDD_TERMINAL_SOLIDBOX,
                             IDD_TERMINAL_LEGACY_CURSOR);
        }

        break;
    case IDD_CURSOR_MEDIUM:
        gpStateInfo->CursorSize = 50;
        // Set the cursor to legacy style
        gpStateInfo->CursorType = 0;
        // Check the legacy radio button on the terminal page
        if (g_hTerminalDlg != INVALID_HANDLE_VALUE)
        {
            CheckRadioButton(g_hTerminalDlg,
                             IDD_TERMINAL_LEGACY_CURSOR,
                             IDD_TERMINAL_SOLIDBOX,
                             IDD_TERMINAL_LEGACY_CURSOR);
        }

        break;
    case IDD_CURSOR_LARGE:
        gpStateInfo->CursorSize = 100;
        // Set the cursor to legacy style
        gpStateInfo->CursorType = 0;
        // Check the legacy radio button on the terminal page
        if (g_hTerminalDlg != INVALID_HANDLE_VALUE)
        {
            CheckRadioButton(g_hTerminalDlg,
                             IDD_TERMINAL_LEGACY_CURSOR,
                             IDD_TERMINAL_SOLIDBOX,
                             IDD_TERMINAL_LEGACY_CURSOR);
        }

        break;
    default:
        return FALSE;
    }

    return TRUE;
}

// Copied as a subset of open/src/host/srvinit.cpp's TranslateConsoleTitle
// Routine Description:
// - This routine translates path characters into '_' characters because the NT registry apis do not allow the creation of keys with
//   names that contain path characters. It also converts absolute paths into %SystemRoot% relative ones. As an example, if both behaviors were
//   specified it would convert a title like C:\WINNT\System32\cmd.exe to %SystemRoot%_System32_cmd.exe.
// Arguments:
// - ConsoleTitle - Pointer to string to translate.
// Return Value:
// - Pointer to translated title or nullptr.
// Note:
// - This routine allocates a buffer that must be freed.
PWSTR TranslateConsoleTitle(_In_ PCWSTR pwszConsoleTitle)
{
    bool fUnexpand = true;
    bool fSubstitute = true;

    LPWSTR Tmp = nullptr;

    size_t cbConsoleTitle;
    size_t cbSystemRoot;

    LPWSTR pwszSysRoot = new (std::nothrow) wchar_t[MAX_PATH];
    if (nullptr != pwszSysRoot)
    {
        if (0 != GetWindowsDirectoryW(pwszSysRoot, MAX_PATH))
        {
            if (SUCCEEDED(StringCbLengthW(pwszConsoleTitle, STRSAFE_MAX_CCH, &cbConsoleTitle)) &&
                SUCCEEDED(StringCbLengthW(pwszSysRoot, MAX_PATH, &cbSystemRoot)))
            {
                int const cchSystemRoot = (int)(cbSystemRoot / sizeof(WCHAR));
                int const cchConsoleTitle = (int)(cbConsoleTitle / sizeof(WCHAR));
                cbConsoleTitle += sizeof(WCHAR); // account for nullptr terminator

                if (fUnexpand &&
                    cchConsoleTitle >= cchSystemRoot &&
#pragma prefast(suppress : 26018, "We've guaranteed that cchSystemRoot is equal to or smaller than cchConsoleTitle in size.")
                    (CSTR_EQUAL == CompareStringOrdinal(pwszConsoleTitle, cchSystemRoot, pwszSysRoot, cchSystemRoot, TRUE)))
                {
                    cbConsoleTitle -= cbSystemRoot;
                    pwszConsoleTitle += cchSystemRoot;
                    cbSystemRoot = SYSTEM_ROOT_LENGTH;
                }
                else
                {
                    cbSystemRoot = 0;
                }

                LPWSTR TranslatedConsoleTitle;
                // This has to be a HeapAlloc, because it gets HeapFree'd later
                Tmp = TranslatedConsoleTitle = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, (cchSystemRoot + cchConsoleTitle) * sizeof(WCHAR));

                if (TranslatedConsoleTitle == nullptr)
                {
                    return nullptr;
                }

                memmove(TranslatedConsoleTitle, SYSTEM_ROOT, cbSystemRoot);
                TranslatedConsoleTitle += (cbSystemRoot / sizeof(WCHAR)); // skip by characters -- not bytes

                for (UINT i = 0; i < cbConsoleTitle; i += sizeof(WCHAR))
                {
#pragma prefast(suppress : 26018, "We are reading the null portion of the buffer on purpose and will escape on reaching it below.")
                    if (fSubstitute && *pwszConsoleTitle == '\\')
                    {
#pragma prefast(suppress : 26019, "Console title must contain system root if this path was followed.")
                        *TranslatedConsoleTitle++ = (WCHAR)'_';
                    }
                    else
                    {
                        *TranslatedConsoleTitle++ = *pwszConsoleTitle;
                        if (*pwszConsoleTitle == L'\0')
                        {
                            break;
                        }
                    }

                    pwszConsoleTitle++;
                }
            }
        }
        delete[] pwszSysRoot;
    }

    return Tmp;
}

// For use by property sheets when added to file props dialog -- maintain refcount of each page and release things we've
// registered when we hit 0. Needed because the lifetime of the property sheets isn't tied to the lifetime of our
// IShellPropSheetExt object.
UINT CALLBACK PropSheetPageProc(_In_ HWND hWnd, _In_ UINT uMsg, _Inout_ LPPROPSHEETPAGE /*ppsp*/)
{
    static UINT cRefs = 0;
    switch (uMsg)
    {
    case PSPCB_ADDREF:
    {
        cRefs++;
        break;
    }

    case PSPCB_RELEASE:
    {
        cRefs--;
        if (cRefs == 0)
        {
            if (gpStateInfo->UpdateValues)
            {
                // only persist settings if they've changed
                SaveConsoleSettingsIfNeeded(hWnd);
            }

            UninitializeConsoleState();
        }
        break;
    }
    }

    return 1;
}

BOOL PopulatePropSheetPageArray(_Out_writes_(cPsps) PROPSHEETPAGE* pPsp, const size_t cPsps, const BOOL fRegisterCallbacks)
{
    BOOL fRet = (cPsps == NUMBER_OF_PAGES);
    if (fRet)
    {
        // This has been validated above. OACR is being silly. Restate it so it can see the condition.
        __analysis_assume(cPsps == NUMBER_OF_PAGES);

        PROPSHEETPAGE* const pOptionsPage = &(pPsp[OPTIONS_PAGE_INDEX]);
        PROPSHEETPAGE* const pFontPage = &(pPsp[FONT_PAGE_INDEX]);
        PROPSHEETPAGE* const pLayoutPage = &(pPsp[LAYOUT_PAGE_INDEX]);
        PROPSHEETPAGE* const pColorsPage = &(pPsp[COLORS_PAGE_INDEX]);
        PROPSHEETPAGE* const pTerminalPage = &(pPsp[TERMINAL_PAGE_INDEX]);

        pOptionsPage->dwSize = sizeof(PROPSHEETPAGE);
        pOptionsPage->hInstance = ghInstance;
        if (g_fIsComCtlV6Present)
        {
            pOptionsPage->pszTemplate = (gpStateInfo->Defaults) ? MAKEINTRESOURCE(DID_SETTINGS) : MAKEINTRESOURCE(DID_SETTINGS2);
        }
        else
        {
            pOptionsPage->pszTemplate = (gpStateInfo->Defaults) ? MAKEINTRESOURCE(DID_SETTINGS_COMCTL5) : MAKEINTRESOURCE(DID_SETTINGS2_COMCTL5);
        }
        pOptionsPage->pfnDlgProc = SettingsDlgProc;
        pOptionsPage->lParam = OPTIONS_PAGE_INDEX;
        pOptionsPage->dwFlags = PSP_DEFAULT;

        pFontPage->dwSize = sizeof(PROPSHEETPAGE);
        pFontPage->hInstance = ghInstance;
        pFontPage->pszTemplate = MAKEINTRESOURCE(DID_FONTDLG);
        pFontPage->pfnDlgProc = FontDlgProc;
        pFontPage->lParam = FONT_PAGE_INDEX;
        pOptionsPage->dwFlags = PSP_DEFAULT;

        pLayoutPage->dwSize = sizeof(PROPSHEETPAGE);
        pLayoutPage->hInstance = ghInstance;
        pLayoutPage->pszTemplate = MAKEINTRESOURCE(DID_SCRBUFSIZE);
        pLayoutPage->pfnDlgProc = ScreenSizeDlgProc;
        pLayoutPage->lParam = LAYOUT_PAGE_INDEX;
        pOptionsPage->dwFlags = PSP_DEFAULT;

        pColorsPage->dwSize = sizeof(PROPSHEETPAGE);
        pColorsPage->hInstance = ghInstance;
        pColorsPage->pszTemplate = MAKEINTRESOURCE(DID_COLOR);
        pColorsPage->pfnDlgProc = ColorDlgProc;
        pColorsPage->lParam = COLORS_PAGE_INDEX;
        pOptionsPage->dwFlags = PSP_DEFAULT;
        if (g_fForceV2)
        {
            pTerminalPage->dwSize = sizeof(PROPSHEETPAGE);
            pTerminalPage->hInstance = ghInstance;
            pTerminalPage->pszTemplate = MAKEINTRESOURCE(DID_TERMINAL);
            pTerminalPage->pfnDlgProc = TerminalDlgProc;
            pTerminalPage->lParam = TERMINAL_PAGE_INDEX;
            pTerminalPage->dwFlags = PSP_DEFAULT;
        }

        // Register callbacks if requested (used for file property sheet purposes)
        if (fRegisterCallbacks)
        {
            for (UINT iPage = 0; iPage < cPsps; iPage++)
            {
                pPsp[iPage].pfnCallback = &PropSheetPageProc;
                pPsp[iPage].dwFlags |= PSP_USECALLBACK;
            }
        }

        fRet = TRUE;
    }

    return fRet;
}

// Routine Description:
// - Creates the property sheet to change console settings.
INT_PTR ConsolePropertySheet(__in HWND hWnd, __in PCONSOLE_STATE_INFO pStateInfo)
{
    PROPSHEETPAGE psp[NUMBER_OF_PAGES];
    PROPSHEETHEADER psh;
    INT_PTR Result = IDCANCEL;
    WCHAR awchBuffer[MAX_PATH] = { 0 };

    gpStateInfo = pStateInfo;

    // In v2 console, consider this an East Asian system if we're currently in a CJK charset. In v1, look at the system codepage.
    if (gpStateInfo->fIsV2Console)
    {
        g_fEastAsianSystem = IS_ANY_DBCS_CHARSET(CodePageToCharSet(gpStateInfo->CodePage));
    }
    else
    {
        g_fEastAsianSystem = IsEastAsianCP(GetOEMCP());
    }

    //
    // Initialize the state information.
    //
    if (gpStateInfo->Defaults)
    {
        InitRegistryValues(pStateInfo);
        GetRegistryValues(pStateInfo);
    }

    //
    // Initialize the font cache and current font index
    //

    InitializeFonts();
    g_currentFontIndex = FindCreateFont(gpStateInfo->FontFamily,
                                        gpStateInfo->FaceName,
                                        gpStateInfo->FontSize,
                                        gpStateInfo->FontWeight,
                                        gpStateInfo->CodePage);

    // since we just triggered font enumeration, recreate our font handles to adapt for DPI
    RecreateFontHandles(hWnd);

    //
    // Get the current page number
    //

    gnCurrentPage = GetRegistryValues(NULL);

    //
    // Initialize the property sheet structures
    //

    RtlZeroMemory(psp, sizeof(psp));
    PopulatePropSheetPageArray(psp, ARRAYSIZE(psp), FALSE /*fRegisterCallbacks*/);

    psh.dwSize = sizeof(PROPSHEETHEADER);
    psh.dwFlags = PSH_PROPTITLE | PSH_USEHICON | PSH_PROPSHEETPAGE |
                  PSH_NOAPPLYNOW | PSH_USECALLBACK | PSH_NOCONTEXTHELP;
    if (gpStateInfo->Defaults)
    {
        LoadString(ghInstance, IDS_TITLE, awchBuffer, ARRAYSIZE(awchBuffer));
    }
    else
    {
        awchBuffer[0] = L'"';
        ExpandEnvironmentStrings(gpStateInfo->OriginalTitle,
                                 &awchBuffer[1],
                                 ARRAYSIZE(awchBuffer) - 2);
        StringCchCat(awchBuffer, ARRAYSIZE(awchBuffer), TEXT("\""));
        gpStateInfo->OriginalTitle = TranslateConsoleTitle(gpStateInfo->OriginalTitle);
    }

    psh.hwndParent = hWnd;
    psh.hIcon = gpStateInfo->hIcon;
    psh.hInstance = ghInstance;
    psh.pszCaption = awchBuffer;
    psh.nPages = g_fForceV2 ? NUMBER_OF_PAGES : V1_NUMBER_OF_PAGES;
    psh.nStartPage = min(gnCurrentPage, ARRAYSIZE(psp));
    psh.ppsp = psp;
    psh.pfnCallback = NULL;

    //
    // Create the property sheet
    //

    Result = PropertySheet(&psh);

    //
    // Save our changes to the registry
    //
    const BOOL fUpdatedValues = gpStateInfo->UpdateValues;
    SaveConsoleSettingsIfNeeded(hWnd);
    gpStateInfo->UpdateValues = fUpdatedValues;

    if (!gpStateInfo->Defaults)
    {
        if (gpStateInfo->OriginalTitle != NULL)
        {
            HeapFree(GetProcessHeap(), 0, gpStateInfo->OriginalTitle);
        }
    }

    //
    // Destroy the font cache.
    //
    DestroyFonts();

    return Result;
}

void RegisterClasses(HINSTANCE hModule)
{
    WNDCLASS wc;
    wc.lpszClassName = TEXT("SimpleColor");
    wc.hInstance = hModule;
    wc.lpfnWndProc = SimpleColorControlProc;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = NULL;
    wc.lpszMenuName = NULL;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    RegisterClass(&wc);

    wc.lpszClassName = TEXT("ColorTableColor");
    wc.hInstance = hModule;
    wc.lpfnWndProc = ColorTableControlProc;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = NULL;
    wc.lpszMenuName = NULL;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    RegisterClass(&wc);

    wc.lpszClassName = TEXT("WOAWinPreview");
    wc.lpfnWndProc = PreviewWndProc;
    wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
    wc.style = 0;
    RegisterClass(&wc);

    wc.lpszClassName = TEXT("WOAFontPreview");
    wc.lpfnWndProc = FontPreviewWndProc;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.style = 0;
    RegisterClass(&wc);
}

void UnregisterClasses(HINSTANCE hModule)
{
    UnregisterClass(TEXT("cpColor"), hModule);
    UnregisterClass(TEXT("WOAWinPreview"), hModule);
    UnregisterClass(TEXT("WOAFontPreview"), hModule);
}

[[nodiscard]] HRESULT FindFontAndUpdateState()
{
    g_currentFontIndex = FindCreateFont(gpStateInfo->FontFamily,
                                        gpStateInfo->FaceName,
                                        gpStateInfo->FontSize,
                                        gpStateInfo->FontWeight,
                                        gpStateInfo->CodePage);

    gpStateInfo->FontFamily = FontInfo[g_currentFontIndex].Family;
    gpStateInfo->FontSize = FontInfo[g_currentFontIndex].Size;
    gpStateInfo->FontWeight = FontInfo[g_currentFontIndex].Weight;
    return StringCchCopyW(gpStateInfo->FaceName, ARRAYSIZE(gpStateInfo->FaceName), FontInfo[g_currentFontIndex].FaceName);
}
