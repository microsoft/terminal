/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    fontdlg.dlg

Abstract:

    This module contains the code for console font dialog

Author:

    Therese Stowell (thereses) Feb-3-1992 (swiped from Win3.1)

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop
#include "fontdlg.h"

#define DEFAULT_TT_FONT_FACENAME L"__DefaultTTFont__"

/* ----- Prototypes ----- */

int FontListCreate(
    __in HWND hDlg,
    __in_ecount_opt(LF_FACESIZE) LPWSTR pwszTTFace,
    __in BOOL bNewFaceList);

BOOL PreviewUpdate(
    HWND hDlg,
    BOOL bLB);

int SelectCurrentSize(
    HWND hDlg,
    BOOL bLB,
    int FontIndex);

BOOL PreviewInit(
    HWND hDlg);

VOID DrawItemFontList(const HWND hDlg, const LPDRAWITEMSTRUCT lpdis);

void RecreateFontHandles(const HWND hWnd);

/* ----- Globals ----- */

HBITMAP hbmTT = NULL; // handle of TT logo bitmap
BITMAP bmTT; // attributes of TT source bitmap

BOOL gbPointSizeError = FALSE;
BOOL gbBold = FALSE;
BOOL gbUserChoseBold = FALSE; // TRUE if bold state was due to user choice

BOOL SelectCurrentFont(
    HWND hDlg,
    int FontIndex);

// Globals strings loaded from resource
WCHAR wszSelectedFont[CCH_SELECTEDFONT + 1];
WCHAR wszRasterFonts[CCH_RASTERFONTS + 1];

UINT GetItemHeight(const HWND hDlg)
{
    // Load the TrueType logo bitmap
    if (hbmTT != NULL)
    {
        DeleteObject(hbmTT);
        hbmTT = NULL;
    }

    hbmTT = LoadBitmap(ghInstance, MAKEINTRESOURCE(BM_TRUETYPE_ICON));
    GetObject(hbmTT, sizeof(BITMAP), &bmTT);

    // Compute the height of face name listbox entries
    HDC hDC = GetDC(hDlg);
    HFONT hFont = GetWindowFont(hDlg);
    if (hFont)
    {
        hFont = (HFONT)SelectObject(hDC, hFont);
    }
    TEXTMETRIC tm;
    GetTextMetrics(hDC, &tm);
    if (hFont)
    {
        SelectObject(hDC, hFont);
    }
    ReleaseDC(hDlg, hDC);
    return max(tm.tmHeight, bmTT.bmHeight);
}

// The V1 console doesn't support arbitrary TTF fonts, so only allow the enumeration of all TT fonts in the conditions below:
BOOL ShouldAllowAllMonoTTFonts()
{
    return (gpStateInfo->fIsV2Console || // allow if connected to a v2 conhost or
            (gpStateInfo->Defaults && g_fForceV2)); // we're in defaults and v2 is turned on
}

// Given pwszTTFace and optional pwszAltTTFace, determine if the font is only available in bold weights
BOOL IsBoldOnlyTTFont(_In_ PCWSTR pwszTTFace, _In_opt_ PCWSTR pwszAltTTFace)
{
    BOOL fFoundNormalWeightFont = FALSE;

    for (ULONG i = 0; i < NumberOfFonts; i++)
    {
        // only care about truetype fonts
        if (!TM_IS_TT_FONT(FontInfo[i].Family))
        {
            continue;
        }

        // only care about fonts in the correct charset
        if (g_fEastAsianSystem)
        {
            if (!IS_DBCS_OR_OEM_CHARSET(FontInfo[i].tmCharSet))
            {
                continue;
            }
        }
        else
        {
            if (IS_DBCS_OR_OEM_CHARSET(FontInfo[i].tmCharSet))
            {
                continue;
            }
        }

        // only care if this TT font's name matches
        if ((0 != lstrcmp(FontInfo[i].FaceName, pwszTTFace)) && // wrong face name and
            (pwszAltTTFace == NULL || // either pwszAltTTFace is NULL or
             (0 != lstrcmp(FontInfo[i].FaceName, pwszAltTTFace)))) // pwszAltTTFace is wrong too
        {
            // A TrueType font, but not the one we're interested in
            continue;
        }

        // the current entry is one of the entries that we care about. is it non-bold?
        if (!IS_BOLD(FontInfo[i].Weight))
        {
            // yes, non-bold. note it as such and bail.
            fFoundNormalWeightFont = TRUE;
            break;
        }
    }

    return !fFoundNormalWeightFont;
}

// Given a handle to our dialog:
// 1. Get currently entered font size
// 2. Check to see if the size is a valid custom size
// 3. If the size is custom, add it to the points size list
static void AddCustomFontSizeToListIfNeeded(__in const HWND hDlg)
{
    WCHAR wszBuf[3]; // only need space for point sizes. the max we allow is "72"

    // check to see if we have text
    if (GetDlgItemText(hDlg, IDD_POINTSLIST, wszBuf, ARRAYSIZE(wszBuf)) > 0)
    {
        // we have text, now retrieve it as an actual size
        BOOL fTranslated;
        const SHORT nPointSize = (SHORT)GetDlgItemInt(hDlg, IDD_POINTSLIST, &fTranslated, TRUE);
        if (fTranslated &&
            nPointSize >= MIN_PIXEL_HEIGHT &&
            nPointSize <= MAX_PIXEL_HEIGHT &&
            IsFontSizeCustom(gpStateInfo->FaceName, nPointSize))
        {
            // we got a proper custom size. let's see if it's in our point size list
            LONG iSize = (LONG)SendDlgItemMessage(hDlg, IDD_POINTSLIST, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)wszBuf);
            if (iSize == CB_ERR)
            {
                // the size isn't in our list, so we haven't created them yet. do so now.
                CreateSizeForAllTTFonts(nPointSize);

                // add the size to the dialog list and select it
                iSize = (LONG)SendDlgItemMessage(hDlg, IDD_POINTSLIST, CB_ADDSTRING, 0, (LPARAM)wszBuf);
                SendDlgItemMessage(hDlg, IDD_POINTSLIST, CB_SETCURSEL, iSize, 0);

                // get the current font selection
                LONG lCurrentFont = (LONG)SendDlgItemMessage(hDlg, IDD_FACENAME, LB_GETCURSEL, 0, 0);

                // now get the current font's face name
                WCHAR wszFontFace[LF_FACESIZE];
                SendDlgItemMessage(hDlg,
                                   IDD_FACENAME,
                                   LB_GETTEXT,
                                   (WPARAM)lCurrentFont,
                                   (LPARAM)wszFontFace);

                // now cause the hFont for this face/size combination to get loaded -- we need to do this so that the
                // font preview has an hFont with which to render
                COORD coordFontSize;
                coordFontSize.X = 0;
                coordFontSize.Y = nPointSize;
                const int iFont = FindCreateFont(FF_MODERN | TMPF_VECTOR | TMPF_TRUETYPE,
                                                 wszFontFace,
                                                 coordFontSize,
                                                 0,
                                                 gpStateInfo->CodePage);
                SendDlgItemMessage(hDlg, IDD_POINTSLIST, CB_SETITEMDATA, (WPARAM)iSize, (LPARAM)iFont);
            }
        }
    }
}

INT_PTR
APIENTRY
FontDlgProc(
    HWND hDlg,
    UINT wMsg,
    WPARAM wParam,
    LPARAM lParam)

/*++

    Dialog proc for the font selection dialog box.
    Returns the near offset into the far table of LOGFONT structures.

--*/

{
    HWND hWndFocus;
    HWND hWndList;
    int FontIndex = g_currentFontIndex; // init to keep compiler happy
    BOOL bLB;

    switch (wMsg)
    {
    case WM_INITDIALOG:
        /*
         * Load the font description strings
         */
        LoadString(ghInstance, IDS_RASTERFONT, wszRasterFonts, ARRAYSIZE(wszRasterFonts));
        DBGFONTS(("wszRasterFonts = \"%ls\"\n", wszRasterFonts));

        LoadString(ghInstance, IDS_SELECTEDFONT, wszSelectedFont, ARRAYSIZE(wszSelectedFont));
        DBGFONTS(("wszSelectedFont = \"%ls\"\n", wszSelectedFont));

        /* Save current font size as dialog window's user data */

        if (g_fEastAsianSystem)
        {
            SetWindowLongPtr(hDlg, GWLP_USERDATA, MAKELONG(FontInfo[g_currentFontIndex].tmCharSet, FontInfo[g_currentFontIndex].Size.Y));
        }
        else
        {
            SetWindowLongPtr(hDlg, GWLP_USERDATA, MAKELONG(FontInfo[g_currentFontIndex].Size.X, FontInfo[g_currentFontIndex].Size.Y));
        }

        if (g_fHostedInFileProperties || gpStateInfo->Defaults)
        {
            LOG_IF_FAILED(FindFontAndUpdateState());
        }

        // IMPORTANT NOTE: When the propsheet and conhost disagree on a font (e.g. user has switched charsets and forgot
        // to change to a more appropriate font), we will fall back to terminal in the propsheet. We refer to
        // FontInfo[g_currentFontIndex] below which will either be the user's preference (if appropriate) or terminal.
        // This gets set prior to here in ConsolePropertySheet() via the FindCreateFont() function.
        //
        // Before this change, we referred directly to what was being passed in to gpStateInfo, which might represent an
        // inappropriate font. Failure to find the appropriate font would end up causing us to show an incorrect
        // combination of "Raster fonts" for the face and the point size list, which is supposed to be for TT. Since the
        // correct raster font sizes weren't being listed, we weren't able to select a font, which left us with a blank
        // edit box, which ultimately caused us to show the "Point size must be between 5 and 72" dialog, which was very
        // annoying.
        //
        // Don't let this happen again.

        /* Create the list of suitable fonts */
        gbEnumerateFaces = TRUE;
        bLB = !TM_IS_TT_FONT(FontInfo[g_currentFontIndex].Family);

        gbBold = IS_BOLD(FontInfo[g_currentFontIndex].Weight);
        CheckDlgButton(hDlg, IDD_BOLDFONT, gbBold);
        if (gbBold)
        {
            // if we're bold, we need to figure out if it's because the user chose this font or if it's because the font
            // is only available in bold
            if (IsBoldOnlyTTFont(FontInfo[g_currentFontIndex].FaceName, NULL))
            {
                // Bold-only TT font, disable the bold checkbox
                EnableWindow(GetDlgItem(hDlg, IDD_BOLDFONT), FALSE);
            }
            else
            {
                // Bold was a user choice. Leave the bold checkbox enabled, and keep track of the fact that the user
                // chose this.
                gbUserChoseBold = TRUE;
            }
        }

        FontListCreate(hDlg,
                       bLB ? NULL : FontInfo[g_currentFontIndex].FaceName,
                       TRUE);

        /* Initialize the preview window - selects current face & size too */
        bLB = PreviewInit(hDlg);
        PreviewUpdate(hDlg, bLB);

        /* Make sure the list box has the focus */
        hWndList = GetDlgItem(hDlg, bLB ? IDD_PIXELSLIST : IDD_POINTSLIST);
        SetFocus(hWndList);
        break;

    case WM_FONTCHANGE:
        gbEnumerateFaces = TRUE;
        bLB = !TM_IS_TT_FONT(gpStateInfo->FontFamily);
        FontListCreate(hDlg, NULL, TRUE);
        FontIndex = FindCreateFont(gpStateInfo->FontFamily,
                                   gpStateInfo->FaceName,
                                   gpStateInfo->FontSize,
                                   gpStateInfo->FontWeight,
                                   gpStateInfo->CodePage);
        SelectCurrentSize(hDlg, bLB, FontIndex);
        return TRUE;

    case WM_PAINT:
        if (fChangeCodePage)
        {
            fChangeCodePage = FALSE;

            /* Create the list of suitable fonts */
            bLB = !TM_IS_TT_FONT(gpStateInfo->FontFamily);
            FontListCreate(hDlg,
                           !bLB ? NULL : gpStateInfo->FaceName,
                           TRUE);
            FontIndex = FontListCreate(hDlg,
                                       bLB ? NULL : gpStateInfo->FaceName,
                                       TRUE);

            /* Initialize the preview window - selects current face & size too */
            bLB = PreviewInit(hDlg);
            PreviewUpdate(hDlg, bLB);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDD_BOLDFONT:
            gbBold = IsDlgButtonChecked(hDlg, IDD_BOLDFONT);
            gbUserChoseBold = gbBold; // explicit user action to enable or disable bold. mark it.
            UpdateApplyButton(hDlg);
            goto RedoFontListAndPreview;

        case IDD_FACENAME:
            switch (HIWORD(wParam))
            {
            case LBN_SELCHANGE:
            RedoFontListAndPreview:
            {
                // if the font we're switching away from is a bold-only TT font, and the user didn't explicitly ask
                // for bold earlier, then unset bold. note that we're depending on the fact that by this point
                // FontIndex hasn't yet been updated to refer to the new font that the user selected.
                if (IS_BOLD(FontInfo[FontIndex].Weight) &&
                    IsBoldOnlyTTFont(FontInfo[FontIndex].FaceName, NULL) &&
                    !gbUserChoseBold)
                {
                    gbBold = FALSE;
                }

                WCHAR atchNewFace[LF_FACESIZE];
                LONG l;

                DBGFONTS(("LBN_SELCHANGE from FACENAME\n"));
                l = (LONG)SendDlgItemMessage(hDlg, IDD_FACENAME, LB_GETCURSEL, 0, 0L);
                bLB = (BOOL)SendDlgItemMessage(hDlg, IDD_FACENAME, LB_GETITEMDATA, l, 0L);
                if (!bLB)
                {
                    SendDlgItemMessage(hDlg, IDD_FACENAME, LB_GETTEXT, l, (LPARAM)atchNewFace);
                    DBGFONTS(("LBN_EDITUPDATE, got TT face \"%ls\"\n", atchNewFace));
                }
                FontIndex = FontListCreate(hDlg,
                                           bLB ? NULL : atchNewFace,
                                           FALSE);
                SelectCurrentSize(hDlg, bLB, FontIndex);
                PreviewUpdate(hDlg, bLB);
                UpdateApplyButton(hDlg);
                return TRUE;
            }
            }
            break;

        case IDD_POINTSLIST:
            switch (HIWORD(wParam))
            {
            case CBN_SELCHANGE:
                DBGFONTS(("CBN_SELCHANGE from POINTSLIST\n"));
                PreviewUpdate(hDlg, FALSE);
                UpdateApplyButton(hDlg);
                return TRUE;

            case CBN_KILLFOCUS:
                DBGFONTS(("CBN_KILLFOCUS from POINTSLIST\n"));
                if (!gbPointSizeError)
                {
                    hWndFocus = GetFocus();
                    if (hWndFocus != NULL && IsChild(hDlg, hWndFocus) &&
                        hWndFocus != GetDlgItem(hDlg, IDCANCEL))
                    {
                        AddCustomFontSizeToListIfNeeded(hDlg);
                        PreviewUpdate(hDlg, FALSE);
                    }
                }
                return TRUE;

            default:
                DBGFONTS(("unhandled CBN_%x from POINTSLIST\n", HIWORD(wParam)));
                break;
            }
            break;

        case IDD_PIXELSLIST:
            switch (HIWORD(wParam))
            {
            case LBN_SELCHANGE:
                DBGFONTS(("LBN_SELCHANGE from PIXELSLIST\n"));
                PreviewUpdate(hDlg, TRUE);
                UpdateApplyButton(hDlg);
                return TRUE;

            default:
                break;
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
        case PSN_KILLACTIVE:
            //
            // If the TT combo box is visible, update selection
            //
            hWndList = GetDlgItem(hDlg, IDD_POINTSLIST);
            if (hWndList != NULL && IsWindowVisible(hWndList))
            {
                if (!PreviewUpdate(hDlg, FALSE))
                {
                    SetDlgMsgResult(hDlg, PSN_KILLACTIVE, TRUE);
                    return TRUE;
                }
                SetDlgMsgResult(hDlg, PSN_KILLACTIVE, FALSE);
            }

            FontIndex = g_currentFontIndex;

            if (FontInfo[FontIndex].SizeWant.Y == 0)
            {
                // Raster Font, so save actual size
                gpStateInfo->FontSize = FontInfo[FontIndex].Size;
            }
            else
            {
                // TT Font, so save desired size
                gpStateInfo->FontSize = FontInfo[FontIndex].SizeWant;
            }

            gpStateInfo->FontWeight = FontInfo[FontIndex].Weight;
            gpStateInfo->FontFamily = FontInfo[FontIndex].Family;
            StringCchCopy(gpStateInfo->FaceName,
                          ARRAYSIZE(gpStateInfo->FaceName),
                          FontInfo[FontIndex].FaceName);

            return TRUE;

        case PSN_APPLY:
            /*
             * Write out the state values and exit.
             */
            EndDlgPage(hDlg, !pshn->lParam);
            return TRUE;
        }
        break;
    }

    /*
     *  For WM_MEASUREITEM and WM_DRAWITEM, since there is only one
     *  owner-draw item (combobox) in the entire dialog box, we don't have
     *  to do a GetDlgItem to figure out who he is.
     */
    case WM_MEASUREITEM:
        ((LPMEASUREITEMSTRUCT)lParam)->itemHeight = GetItemHeight(hDlg);
        return TRUE;

    case WM_DRAWITEM:
        DrawItemFontList(hDlg, (LPDRAWITEMSTRUCT)lParam);
        return TRUE;

    case WM_DESTROY:
        /*
         * Delete the TrueType logo bitmap
         */
        if (hbmTT != NULL)
        {
            DeleteObject(hbmTT);
            hbmTT = NULL;
        }
        return TRUE;

    case WM_DPICHANGED_BEFOREPARENT:
        // DPI has changed -- recreate our font handles to get appropriately scaled fonts
        RecreateFontHandles(hDlg);

        // Now reset our item height. This is to work around a limitation of automatic dialog DPI scaling where
        // WM_MEASUREITEM doesn't get sent when the DPI has changed.
        SendDlgItemMessage(hDlg, IDD_FACENAME, LB_SETITEMHEIGHT, 0, GetItemHeight(hDlg));
        break;

    default:
        break;
    }

    return FALSE;
}

// Iterate through all of our fonts to find the font entries that match the desired family, charset, name (TT), and
// boldness (TT). Each entry in FontInfo represents a specific combination of font states. We expect to encounter
// numerous entries for each size/boldness/charset of TT fonts. If fAddBoldFonts is true, we'll add a font even if it's
// bold, regardless of whether the user has chosen bold fonts or not.
void AddFontSizesToList(PCWSTR pwszTTFace,
                        PCWSTR pwszAltTTFace,
                        const LONG_PTR dwExStyle,
                        const BOOL fDbcsCharSet,
                        const BOOL fRasterFont,
                        const HWND hWndShow,
                        const BOOL fAddBoldFonts)
{
    WCHAR wszText[80];
    int iLastShowX = 0;
    int iLastShowY = 0;
    int nSameSize = 0;

    for (ULONG i = 0; i < NumberOfFonts; i++)
    {
        if (!fRasterFont == !TM_IS_TT_FONT(FontInfo[i].Family))
        {
            DBGFONTS(("  Font %x not right type\n", i));
            continue;
        }
        if (fDbcsCharSet)
        {
            if (!IS_DBCS_OR_OEM_CHARSET(FontInfo[i].tmCharSet))
            {
                DBGFONTS(("  Font %x not right type for DBCS character set\n", i));
                continue;
            }
        }
        else
        {
            if (IS_ANY_DBCS_CHARSET(FontInfo[i].tmCharSet))
            {
                DBGFONTS(("  Font %x not right type for SBCS character set\n", i));
                continue;
            }
        }

        if (!fRasterFont)
        {
            if ((0 != lstrcmp(FontInfo[i].FaceName, pwszTTFace)) &&
                (0 != lstrcmp(FontInfo[i].FaceName, pwszAltTTFace)))
            {
                /*
                 * A TrueType font, but not the one we're interested in,
                 * so don't add it to the list of point sizes.
                 */
                DBGFONTS(("  Font %x is TT, but not %ls\n", i, pwszTTFace));
                continue;
            }

            // if we're being asked to add bold fonts, add unconditionally according to weight. Otherwise, only this
            // entry to the list of it's in line with user choice. Raster fonts aren't available in bold.
            if (!fAddBoldFonts && gbBold != IS_BOLD(FontInfo[i].Weight))
            {
                DBGFONTS(("  Font %x has weight %d, but we wanted %sbold\n",
                          i,
                          FontInfo[i].Weight,
                          gbBold ? "" : "not "));
                continue;
            }
        }

        int ShowX;
        if (FontInfo[i].SizeWant.X > 0)
        {
            ShowX = FontInfo[i].SizeWant.X;
        }
        else
        {
            ShowX = FontInfo[i].Size.X;
        }

        int ShowY;
        if (FontInfo[i].SizeWant.Y > 0)
        {
            ShowY = FontInfo[i].SizeWant.Y;
        }
        else
        {
            ShowY = FontInfo[i].Size.Y;
        }

        /*
         * Add the size description string to the end of the right list
         */
        if (TM_IS_TT_FONT(FontInfo[i].Family))
        {
            // point size
            StringCchPrintf(wszText,
                            ARRAYSIZE(wszText),
                            TEXT("%2d"),
                            FontInfo[i].SizeWant.Y);
        }
        else
        {
            // pixel size
            if ((iLastShowX == ShowX) && (iLastShowY == ShowY))
            {
                nSameSize++;
            }
            else
            {
                iLastShowX = ShowX;
                iLastShowY = ShowY;
                nSameSize = 0;
            }

            /*
             * The number nSameSize is appended to the string to distinguish
             * between Raster fonts of the same size.  It is not intended to
             * be visible and exists off the edge of the list
             */
            if (((dwExStyle & WS_EX_RIGHT) && !(dwExStyle & WS_EX_LAYOUTRTL)) ||
                (!(dwExStyle & WS_EX_RIGHT) && (dwExStyle & WS_EX_LAYOUTRTL)))
            {
                // flip  it so that the hidden part be at the far left
                StringCchPrintf(wszText,
                                ARRAYSIZE(wszText),
                                TEXT("#%d                %2d x %2d"),
                                nSameSize,
                                ShowX,
                                ShowY);
            }
            else
            {
                StringCchPrintf(wszText,
                                ARRAYSIZE(wszText),
                                TEXT("%2d x %2d                #%d"),
                                ShowX,
                                ShowY,
                                nSameSize);
            }
        }

        LONG lListIndex = lcbFINDSTRINGEXACT(hWndShow, fRasterFont, wszText);
        if (lListIndex == LB_ERR)
        {
            lListIndex = lcbADDSTRING(hWndShow, fRasterFont, wszText);
        }
        DBGFONTS(("  added %ls to %sSLIST(%p) index %lx\n",
                  wszText,
                  fRasterFont ? "PIXEL" : "POINT",
                  hWndShow,
                  lListIndex));
        lcbSETITEMDATA(hWndShow, fRasterFont, (DWORD)lListIndex, i);
    }
}

/*++

    Initializes the font list by enumerating all fonts and picking the
    proper ones for our list.

    Returns
        FontIndex of selected font (LB_ERR if none)
--*/
int FontListCreate(
    __in HWND hDlg,
    __in_ecount_opt(LF_FACESIZE) LPWSTR pwszTTFace,
    __in BOOL bNewFaceList)
{
    LONG lListIndex;
    ULONG i;
    HWND hWndShow; // List or Combo box
    HWND hWndHide; // Combo or List box
    HWND hWndFaceCombo;
    BOOL bLB;
    UINT CodePage = gpStateInfo->CodePage;
    BOOL fFindTTFont = FALSE;
    LPWSTR pwszAltTTFace;
    LONG_PTR dwExStyle;

    FAIL_FAST_IF(!(OEMCP != 0)); // must be initialized

    bLB = ((pwszTTFace == nullptr) || (pwszTTFace[0] == TEXT('\0')));
    if (bLB)
    {
        pwszAltTTFace = NULL;
    }
    else
    {
        if (ShouldAllowAllMonoTTFonts() || IsAvailableTTFont(pwszTTFace))
        {
            pwszAltTTFace = GetAltFaceName(pwszTTFace);
        }
        else
        {
            pwszAltTTFace = pwszTTFace;
        }
    }

    DBGFONTS(("FontListCreate %p, %s, %s new FaceList\n", hDlg, bLB ? "Raster" : "TrueType", bNewFaceList ? "Make" : "No"));

    /*
     * This only enumerates face names and font sizes if necessary.
     */
    if (!NT_SUCCESS(EnumerateFonts(bLB ? EF_OEMFONT : EF_TTFONT)))
    {
        return LB_ERR;
    }

    /* init the TTFaceNames */

    DBGFONTS(("  Create %s fonts\n", bLB ? "Raster" : "TrueType"));

    if (bNewFaceList)
    {
        PFACENODE panFace;
        hWndFaceCombo = GetDlgItem(hDlg, IDD_FACENAME);

        // empty faces list
        SendMessage(hWndFaceCombo, LB_RESETCONTENT, 0, 0);

        // before doing anything else, add raster fonts to the list. Note that the item data being set here indicates
        // that it's a raster font. the actual font indices are stored as item data on the pixels list.
        lListIndex = (LONG)SendMessage(hWndFaceCombo, LB_ADDSTRING, 0, (LPARAM)wszRasterFonts);
        SendMessage(hWndFaceCombo, LB_SETITEMDATA, lListIndex, TRUE);
        DBGFONTS(("Added \"%ls\", set Item Data %d = TRUE\n", wszRasterFonts, lListIndex));

        // now enumerate all of the new truetype font face names we've loaded that are appropriate for our codepage. add them to
        // the faces list. if we find an exact match for pwszTTFace or pwszAltTTFace, note that in fFindTTFont.
        for (panFace = gpFaceNames; panFace; panFace = panFace->pNext)
        {
            if ((panFace->dwFlag & (EF_TTFONT | EF_NEW)) != (EF_TTFONT | EF_NEW))
            {
                continue;
            }
            if (!g_fEastAsianSystem && (panFace->dwFlag & EF_DBCSFONT))
            {
                continue;
            }

            // NOTE: in v2 we don't depend on the registry list to determine if a TT font should be listed in the font
            // face dialog list -- this is handled in DoFontEnum by using the FontEnumForV2Console enumerator
            if (ShouldAllowAllMonoTTFonts() ||
                (g_fEastAsianSystem && IsAvailableTTFontCP(panFace->atch, CodePage)) ||
                (!g_fEastAsianSystem && IsAvailableTTFontCP(panFace->atch, 0)))
            {
                if (!bLB &&
                    (lstrcmp(pwszTTFace, panFace->atch) == 0 ||
                     lstrcmp(pwszAltTTFace, panFace->atch) == 0))
                {
                    fFindTTFont = TRUE;
                }

                lListIndex = (LONG)SendMessage(hWndFaceCombo, LB_ADDSTRING, 0, (LPARAM)panFace->atch);
                SendMessage(hWndFaceCombo, LB_SETITEMDATA, lListIndex, FALSE);
                DBGFONTS(("Added \"%ls\", set Item Data %d = FALSE\n",
                          panFace->atch,
                          lListIndex));
            }
        }

        // if we haven't found the specific TT font we're looking for, choose *any* TT font that's appropriate for our
        // codepage
        if (!bLB && !fFindTTFont)
        {
            for (panFace = gpFaceNames; panFace; panFace = panFace->pNext)
            {
                if ((panFace->dwFlag & (EF_TTFONT | EF_NEW)) != (EF_TTFONT | EF_NEW))
                {
                    continue;
                }

                if (!g_fEastAsianSystem && (panFace->dwFlag & EF_DBCSFONT))
                {
                    continue;
                }

                if ((g_fEastAsianSystem && IsAvailableTTFontCP(panFace->atch, CodePage)) ||
                    (!g_fEastAsianSystem && IsAvailableTTFontCP(panFace->atch, 0)))
                {
                    if (lstrcmp(pwszTTFace, panFace->atch) != 0)
                    {
                        // found a reasonably appropriate font that isn't the one being requested (we couldn't find that
                        // one). use this one instead.
                        StringCchCopy(pwszTTFace,
                                      LF_FACESIZE,
                                      panFace->atch);
                        break;
                    }
                }
            }
        }
    }

    // update the state of the bold checkbox. check the box if the currently selected TT font is bold. some TT fonts
    // aren't allowed to be bold depending on the charset. also, raster fonts aren't allowed to be bold.
    hWndShow = GetDlgItem(hDlg, IDD_BOLDFONT);

    /*
     * For JAPAN, We uses "MS Gothic" TT font.
     * So, Bold of this font is not 1:2 width between SBCS:DBCS.
     */
    if (g_fEastAsianSystem && IsDisableBoldTTFont(pwszTTFace))
    {
        EnableWindow(hWndShow, FALSE);
        gbBold = FALSE;
        CheckDlgButton(hDlg, IDD_BOLDFONT, FALSE);
    }
    else
    {
        CheckDlgButton(hDlg, IDD_BOLDFONT, (bLB || !gbBold) ? FALSE : TRUE);
        EnableWindow(hWndShow, bLB ? FALSE : TRUE);
    }

    // if the current font is raster, disable and hide the point size list.
    // if the current font is TT, disable and hide the pixel size list.
    hWndHide = GetDlgItem(hDlg, bLB ? IDD_POINTSLIST : IDD_PIXELSLIST);
    ShowWindow(hWndHide, SW_HIDE);
    EnableWindow(hWndHide, FALSE);

    // if the current font is raster, enable and show the pixel size list.
    // if the current font is TT, enable and show the point size list.
    hWndShow = GetDlgItem(hDlg, bLB ? IDD_PIXELSLIST : IDD_POINTSLIST);
    ShowWindow(hWndShow, SW_SHOW);
    EnableWindow(hWndShow, TRUE);

    // if we're building a new face list (basically any time we're not handling a selection change), empty the contents
    // of the pixel size list (raster) or point size list (TT) as appropriate.
    if (bNewFaceList)
    {
        lcbRESETCONTENT(hWndShow, bLB);
    }

    dwExStyle = GetWindowLongPtr(hWndShow, GWL_EXSTYLE);
    if ((dwExStyle & WS_EX_LAYOUTRTL) && !(dwExStyle & WS_EX_RTLREADING))
    {
        // if mirrored RTL Reading means LTR !!
        SetWindowLongPtr(hWndShow, GWL_EXSTYLE, dwExStyle | WS_EX_RTLREADING);
    }

    /* Initialize hWndShow list/combo box */

    const BOOL fIsBoldOnlyTTFont = (!bLB && IsBoldOnlyTTFont(pwszTTFace, pwszAltTTFace));

    AddFontSizesToList(pwszTTFace,
                       pwszAltTTFace,
                       dwExStyle,
                       g_fEastAsianSystem,
                       bLB,
                       hWndShow,
                       fIsBoldOnlyTTFont);

    if (fIsBoldOnlyTTFont)
    {
        // since this is a bold-only font, check and disable the bold checkbox
        EnableWindow(GetDlgItem(hDlg, IDD_BOLDFONT), FALSE);
        CheckDlgButton(hDlg, IDD_BOLDFONT, TRUE);
    }

    /*
     * Get the FontIndex from the currently selected item.
     * (i will be LB_ERR if no currently selected item).
     */
    lListIndex = lcbGETCURSEL(hWndShow, bLB);
    i = lcbGETITEMDATA(hWndShow, bLB, lListIndex);

    DBGFONTS(("FontListCreate returns 0x%x\n", i));
    FAIL_FAST_IF(!(i == LB_ERR || (ULONG)i < NumberOfFonts));
    return i;
}

/** DrawItemFontList
 *
 *  Answer the WM_DRAWITEM message sent from the font list box or
 *  facename list box.
 *
 *  Entry:
 *      lpdis     -> DRAWITEMSTRUCT describing object to be drawn
 *
 *  Returns:
 *      None.
 *
 *      The object is drawn.
 */
VOID DrawItemFontList(const HWND hDlg, const LPDRAWITEMSTRUCT lpdis)
{
    HDC hDC, hdcMem;
    DWORD rgbBack, rgbText, rgbFill;
    WCHAR wszFace[LF_FACESIZE];
    HBITMAP hOld;
    int dy, dxttbmp;
    HBRUSH hbrFill;
    HWND hWndItem;
    BOOL bLB;

    if ((int)lpdis->itemID < 0)
    {
        return;
    }

    hDC = lpdis->hDC;

    if (lpdis->itemAction & ODA_FOCUS)
    {
        if (lpdis->itemState & ODS_SELECTED)
        {
            DrawFocusRect(hDC, &lpdis->rcItem);
        }
    }
    else
    {
        if (lpdis->itemState & ODS_SELECTED)
        {
            rgbText = SetTextColor(hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
            rgbBack = SetBkColor(hDC, rgbFill = GetSysColor(COLOR_HIGHLIGHT));
        }
        else
        {
            rgbText = SetTextColor(hDC, GetSysColor(COLOR_WINDOWTEXT));
            rgbBack = SetBkColor(hDC, rgbFill = GetSysColor(COLOR_WINDOW));
        }
        // draw selection background
        hbrFill = CreateSolidBrush(rgbFill);
        if (hbrFill)
        {
            FillRect(hDC, &lpdis->rcItem, hbrFill);
            DeleteObject(hbrFill);
        }

        // get the string
        hWndItem = lpdis->hwndItem;
        if (!IsWindow(hWndItem))
        {
            return;
        }

        /*
         * This line is here mostly to quiet prefast, which expects a
         * LB_GETTEXTLEN to be sent before a LB_GETTEXT. However, this call
         * is useless for three reasons: First, the size can change between
         * the two calls, so the return value is useless. Second, since these
         * are fonts their lengths are the same. Third, a buffer overrun here
         * isn't really interesting from a security perspective, anyway.
         */
        if (SendMessage(hWndItem, LB_GETTEXTLEN, lpdis->itemID, 0) >= ARRAYSIZE(wszFace))
        {
            return;
        }

        SendMessage(hWndItem, LB_GETTEXT, lpdis->itemID, (LPARAM)wszFace);
        bLB = (BOOL)SendMessage(hWndItem, LB_GETITEMDATA, lpdis->itemID, 0L);
        dxttbmp = bLB ? 0 : bmTT.bmWidth;

        DBGFONTS(("DrawItemFontList must redraw \"%ls\" %s\n", wszFace, bLB ? "Raster" : "TrueType"));

        // draw the text
        TabbedTextOut(hDC, lpdis->rcItem.left + dxttbmp, lpdis->rcItem.top, wszFace, (UINT)wcslen(wszFace), 0, NULL, dxttbmp);

        // and the TT bitmap if needed
        if (!bLB)
        {
            hdcMem = CreateCompatibleDC(hDC);
            if (hdcMem)
            {
                hOld = (HBITMAP)SelectObject(hdcMem, hbmTT);

                dy = ((lpdis->rcItem.bottom - lpdis->rcItem.top) - bmTT.bmHeight) / 2;

                BitBlt(hDC, lpdis->rcItem.left, lpdis->rcItem.top + dy, dxttbmp, GetItemHeight(hDlg), hdcMem, 0, 0, SRCINVERT);

                if (hOld)
                    SelectObject(hdcMem, hOld);
                DeleteDC(hdcMem);
            }
        }

        SetTextColor(hDC, rgbText);
        SetBkColor(hDC, rgbBack);

        if (lpdis->itemState & ODS_FOCUS)
        {
            DrawFocusRect(hDC, &lpdis->rcItem);
        }
    }
}

UINT GetPointSizeInRange(
    HWND hDlg,
    INT Min,
    INT Max)
/*++

Routine Description:

   Get a size from the Point Size ComboBox edit field

Return Value:

   Point Size - of the edit field limited by Min/Max size
   0 - if the field is empty or invalid

--*/

{
    WCHAR szBuf[90];
    int nTmp = 0;
    BOOL bOK;

    if (GetDlgItemText(hDlg, IDD_POINTSLIST, szBuf, ARRAYSIZE(szBuf)))
    {
        nTmp = GetDlgItemInt(hDlg, IDD_POINTSLIST, &bOK, TRUE);
        if (bOK && nTmp >= Min && nTmp <= Max)
        {
            return nTmp;
        }
    }

    return 0;
}

/* ----- Preview routines ----- */

[[nodiscard]] LRESULT
    CALLBACK
    FontPreviewWndProc(
        HWND hWnd,
        UINT wMessage,
        WPARAM wParam,
        LPARAM lParam)

/*  FontPreviewWndProc
 *      Handles the font preview window
 */

{
    PAINTSTRUCT ps;
    RECT rect;
    HFONT hfontOld;
    HBRUSH hbrNew;
    HBRUSH hbrOld;
    COLORREF rgbText;
    COLORREF rgbBk;

    switch (wMessage)
    {
    case WM_ERASEBKGND:
        break;

    case WM_PAINT:
        BeginPaint(hWnd, &ps);

        /* Draw the font sample */
        if (GetWindowLong(hWnd, GWL_ID) == IDD_COLOR_POPUP_COLORS)
        {
            rgbText = GetNearestColor(ps.hdc, PopupTextColor(gpStateInfo));
            rgbBk = GetNearestColor(ps.hdc, PopupBkColor(gpStateInfo));
        }
        else
        {
            rgbText = GetNearestColor(ps.hdc, ScreenTextColor(gpStateInfo));
            rgbBk = GetNearestColor(ps.hdc, ScreenBkColor(gpStateInfo));
        }
        SetTextColor(ps.hdc, rgbText);
        SetBkColor(ps.hdc, rgbBk);
        GetClientRect(hWnd, &rect);
        hfontOld = (HFONT)SelectObject(ps.hdc, FontInfo[g_currentFontIndex].hFont);
        hbrNew = CreateSolidBrush(rgbBk);
        hbrOld = (HBRUSH)SelectObject(ps.hdc, hbrNew);
        PatBlt(ps.hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, PATCOPY);
        InflateRect(&rect, -2, -2);
        DrawText(ps.hdc, g_szPreviewText, -1, &rect, 0);
        SelectObject(ps.hdc, hbrOld);
        DeleteObject(hbrNew);
        SelectObject(ps.hdc, hfontOld);

        EndPaint(hWnd, &ps);
        break;

    case WM_DPICHANGED:
        // DPI has changed -- recreate our font handles to get appropriately scaled fonts
        RecreateFontHandles(hWnd);
        break;

    default:
        return DefWindowProc(hWnd, wMessage, wParam, lParam);
    }
    return 0L;
}

/*
 * Get the font index for a new font
 * If necessary, attempt to create the font.
 * Always return a valid FontIndex (even if not correct)
 * Family:   Find/Create a font with of this Family
 *           0    - don't care
 * pwszFace: Find/Create a font with this face name.
 *           NULL or TEXT("")  - use DefaultFaceName
 * Size:     Must match SizeWant or actual Size.
 */
int FindCreateFont(
    __in DWORD Family,
    __in_ecount(LF_FACESIZE) LPWSTR pwszFace,
    __in COORD Size,
    __in LONG Weight,
    __in UINT CodePage)
{
#define NOT_CREATED_NOR_FOUND -1
#define CREATED_BUT_NOT_FOUND -2

    int FontIndex = NOT_CREATED_NOR_FOUND;
    BOOL bFontOK;
    WCHAR AltFaceName[LF_FACESIZE];
    COORD AltFontSize;
    BYTE AltFontFamily;
    ULONG AltFontIndex = 0, i;
    LPWSTR pwszAltFace;

    BYTE CharSet = CodePageToCharSet(CodePage);

    FAIL_FAST_IF(!(OEMCP != 0));

    DBGFONTS(("FindCreateFont Family=%x %ls (%d,%d) %d %d %x\n",
              Family,
              pwszFace,
              Size.X,
              Size.Y,
              Weight,
              CodePage,
              CharSet));

    if (g_fEastAsianSystem)
    {
        if (IS_DBCS_OR_OEM_CHARSET(CharSet))
        {
            if (pwszFace == NULL || *pwszFace == TEXT('\0'))
            {
                pwszFace = DefaultFaceName;
            }
            if (Size.Y == 0)
            {
                Size = DefaultFontSize;
            }
        }
        else
        {
            MakeAltRasterFont(CodePage, &AltFontSize, &AltFontFamily, &AltFontIndex, AltFaceName);

            if (pwszFace == NULL || *pwszFace == TEXT('\0'))
            {
                pwszFace = AltFaceName;
            }
            if (Size.Y == 0)
            {
                Size.X = AltFontSize.X;
                Size.Y = AltFontSize.Y;
            }
        }
    }
    else
    {
        if (pwszFace == NULL || *pwszFace == TEXT('\0'))
        {
            pwszFace = DefaultFaceName;
        }
        if (Size.Y == 0)
        {
            Size = DefaultFontSize;
        }
    }

    // If _DefaultTTFont_ is specified, find the appropriate face name for our current codepage.
    if (wcscmp(pwszFace, DEFAULT_TT_FONT_FACENAME) == 0)
    {
        // retrieve default font face name for this codepage, and then set it as our current face
        WCHAR szDefaultCodepageTTFont[LF_FACESIZE] = { 0 };
        if (NT_SUCCESS(GetTTFontFaceForCodePage(CodePage, szDefaultCodepageTTFont, ARRAYSIZE(szDefaultCodepageTTFont))) &&
            NT_SUCCESS(StringCchCopyW(DefaultTTFaceName, ARRAYSIZE(DefaultTTFaceName), szDefaultCodepageTTFont)))
        {
            pwszFace = DefaultTTFaceName;
            Size.X = 0;
        }
    }

    if (ShouldAllowAllMonoTTFonts() || IsAvailableTTFont(pwszFace))
    {
        pwszAltFace = GetAltFaceName(pwszFace);
    }
    else
    {
        pwszAltFace = pwszFace;
    }

    /*
     * Try to find the exact font
     */
TryFindExactFont:
    for (i = 0; i < NumberOfFonts; i++)
    {
        /*
         * If looking for a particular Family, skip non-matches
         */
        if (Family != 0 && (BYTE)Family != FontInfo[i].Family)
        {
            continue;
        }

        /*
         * Skip non-matching sizes
         */
        if ((FontInfo[i].SizeWant.Y != Size.Y) &&
            !SIZE_EQUAL(FontInfo[i].Size, Size))
        {
            continue;
        }

        /*
         * Skip non-matching weights
         */
        if ((Weight != 0) && (Weight != FontInfo[i].Weight))
        {
            continue;
        }

        if (!TM_IS_TT_FONT(FontInfo[i].Family) &&
            (FontInfo[i].tmCharSet != CharSet &&
             !(FontInfo[i].tmCharSet == OEM_CHARSET && g_fEastAsianSystem)))
        {
            continue;
        }

        /*
         * Size (and maybe Family) match. If we don't care about the name or
         * if it matches, use this font. Otherwise, if name doesn't match and
         * it is a raster font, consider it.
         */
        if ((pwszFace == NULL) || (pwszFace[0] == TEXT('\0')) ||
            (lstrcmp(FontInfo[i].FaceName, pwszFace) == 0) ||
            (lstrcmp(FontInfo[i].FaceName, pwszAltFace) == 0))
        {
            FontIndex = i;
            goto FoundFont;
        }
        else if (!TM_IS_TT_FONT(FontInfo[i].Family))
        {
            FontIndex = i;
        }
    }

    if (FontIndex == NOT_CREATED_NOR_FOUND)
    {
        /*
         * Didn't find the exact font, so try to create it
         */
        if (Size.Y < 0)
        {
            Size.Y = -Size.Y;
        }
        bFontOK = DoFontEnum(NULL, pwszFace, &Size.Y, 1);
        if (bFontOK)
        {
            DBGFONTS(("FindCreateFont created font!\n"));
            FontIndex = CREATED_BUT_NOT_FOUND;
            goto TryFindExactFont;
        }
        else
        {
            DBGFONTS(("FindCreateFont failed to create font!\n"));
        }
    }
    else if (FontIndex >= 0)
    {
        // a close Raster Font fit - only the name doesn't match.
        goto FoundFont;
    }

    /*
     * Failed to find exact match, even after enumeration, so now try to find
     * a font of same family and same size or bigger.
     */
    for (i = 0; i < NumberOfFonts; i++)
    {
        if (g_fEastAsianSystem)
        {
            if (Family != 0 && (BYTE)Family != FontInfo[i].Family)
            {
                continue;
            }

            if (!TM_IS_TT_FONT(FontInfo[i].Family) &&
                FontInfo[i].tmCharSet != CharSet)
            {
                continue;
            }
        }
        else
        {
            if ((BYTE)Family != FontInfo[i].Family)
            {
                continue;
            }
        }

        if (FontInfo[i].Size.Y >= Size.Y && FontInfo[i].Size.X >= Size.X)
        {
            // Same family, size >= desired.
            FontIndex = i;
            break;
        }
    }

    if (FontIndex < 0)
    {
        DBGFONTS(("FindCreateFont defaults!\n"));
        if (g_fEastAsianSystem)
        {
            if (CodePage == OEMCP)
            {
                FontIndex = DefaultFontIndex;
            }
            else
            {
                FontIndex = AltFontIndex;
            }
        }
        else
        {
            FontIndex = DefaultFontIndex;
        }
    }

FoundFont:
    FAIL_FAST_IF(!(FontIndex < (int)NumberOfFonts));
    DBGFONTS(("FindCreateFont returns %x : %ls (%d,%d)\n", FontIndex, FontInfo[FontIndex].FaceName, FontInfo[FontIndex].Size.X, FontInfo[FontIndex].Size.Y));
    return FontIndex;

#undef NOT_CREATED_NOR_FOUND
#undef CREATED_BUT_NOT_FOUND
}

/*
 * SelectCurrentSize - Select the right line of the Size listbox/combobox.
 *   bLB       : Size controls is a listbox (TRUE for RasterFonts)
 *   FontIndex : Index into FontInfo[] cache
 *               If < 0 then choose a good font.
 * Returns
 *   FontIndex : Index into FontInfo[] cache
 */
int SelectCurrentSize(HWND hDlg, BOOL bLB, int FontIndex)
{
    int iCB;
    HWND hWndList;

    DBGFONTS(("SelectCurrentSize %p %s %x\n",
              hDlg,
              bLB ? "Raster" : "TrueType",
              FontIndex));

    hWndList = GetDlgItem(hDlg, bLB ? IDD_PIXELSLIST : IDD_POINTSLIST);
    iCB = lcbGETCOUNT(hWndList, bLB);
    DBGFONTS(("  Count of items in %p = %lx\n", hWndList, iCB));

    if (FontIndex >= 0)
    {
        /*
         * look for FontIndex
         */
        while (iCB > 0)
        {
            iCB--;
            if (lcbGETITEMDATA(hWndList, bLB, iCB) == FontIndex)
            {
                lcbSETCURSEL(hWndList, bLB, iCB);
                break;
            }
        }
    }
    else
    {
        /*
         * Look for a reasonable default size: looking backwards, find
         * the first one same height or smaller.
         */
        DWORD Size = GetWindowLong(hDlg, GWLP_USERDATA);

        if (g_fEastAsianSystem &&
            bLB &&
            FontInfo[g_currentFontIndex].tmCharSet != LOBYTE(LOWORD(Size)))
        {
            WCHAR AltFaceName[LF_FACESIZE];
            COORD AltFontSize;
            BYTE AltFontFamily;
            ULONG AltFontIndex = 0;

            MakeAltRasterFont(gpStateInfo->CodePage,
                              &AltFontSize,
                              &AltFontFamily,
                              &AltFontIndex,
                              AltFaceName);

            while (iCB > 0)
            {
                iCB--;
                if (lcbGETITEMDATA(hWndList, bLB, iCB) == (int)AltFontIndex)
                {
                    lcbSETCURSEL(hWndList, bLB, iCB);
                    break;
                }
            }
        }
        else
        {
            while (iCB > 0)
            {
                iCB--;
                FontIndex = lcbGETITEMDATA(hWndList, bLB, iCB);
                if (FontInfo[FontIndex].Size.Y <= HIWORD(Size))
                {
                    lcbSETCURSEL(hWndList, bLB, iCB);
                    break;
                }
            }
        }
    }

    DBGFONTS(("SelectCurrentSize returns 0x%x\n", FontIndex));
    return FontIndex;
}

BOOL SelectCurrentFont(
    HWND hDlg,
    int FontIndex)
{
    BOOL bLB;

    DBGFONTS(("SelectCurrentFont hDlg=%p, FontIndex=%x\n", hDlg, FontIndex));

    bLB = !TM_IS_TT_FONT(FontInfo[FontIndex].Family);

    SendDlgItemMessage(hDlg,
                       IDD_FACENAME,
                       LB_SELECTSTRING,
                       (WPARAM)-1,
                       bLB ? (LPARAM)wszRasterFonts : (LPARAM)FontInfo[FontIndex].FaceName);

    SelectCurrentSize(hDlg, bLB, FontIndex);
    return bLB;
}

BOOL PreviewInit(
    HWND hDlg)

/*  PreviewInit
 *      Prepares the preview code, sizing the window and the dialog to
 *      make an attractive preview.
 *  Returns TRUE if Raster Fonts, FALSE if TT Font
 */

{
    int nFont;

    DBGFONTS(("PreviewInit hDlg=%p\n", hDlg));

    /*
     * Set the current font
     */
    nFont = FindCreateFont(gpStateInfo->FontFamily,
                           gpStateInfo->FaceName,
                           gpStateInfo->FontSize,
                           gpStateInfo->FontWeight,
                           gpStateInfo->CodePage);

    DBGFONTS(("Changing Font Number from %d to %d\n",
              g_currentFontIndex,
              nFont));
    FAIL_FAST_IF(!((ULONG)nFont < NumberOfFonts));
    g_currentFontIndex = nFont;

    if (g_fHostedInFileProperties)
    {
        gpStateInfo->FontFamily = FontInfo[g_currentFontIndex].Family;
        gpStateInfo->FontSize = FontInfo[g_currentFontIndex].Size;
        gpStateInfo->FontWeight = FontInfo[g_currentFontIndex].Weight;
        StringCchCopyW(gpStateInfo->FaceName, ARRAYSIZE(gpStateInfo->FaceName), FontInfo[g_currentFontIndex].FaceName);
    }

    return SelectCurrentFont(hDlg, nFont);
}

BOOL PreviewUpdate(
    HWND hDlg,
    BOOL bLB)

/*++

    Does the preview of the selected font.

--*/

{
    PFONT_INFO lpFont;
    int FontIndex;
    LONG lIndex;
    HWND hWnd;
    WCHAR wszText[60];
    WCHAR wszFace[LF_FACESIZE + CCH_SELECTEDFONT];
    HWND hWndList;
    DWORD_PTR Parameters[2];

    DBGFONTS(("PreviewUpdate hDlg=%p, %s\n", hDlg, bLB ? "Raster" : "TrueType"));

    hWndList = GetDlgItem(hDlg, bLB ? IDD_PIXELSLIST : IDD_POINTSLIST);

    /* When we select a font, we do the font preview by setting it into
     *  the appropriate list box
     */
    lIndex = lcbGETCURSEL(hWndList, bLB);
    DBGFONTS(("PreviewUpdate GETCURSEL gets %x\n", lIndex));
    if ((lIndex < 0) && !bLB)
    {
        COORD NewSize;

        lIndex = (LONG)SendDlgItemMessage(hDlg, IDD_FACENAME, LB_GETCURSEL, 0, 0L);
        SendDlgItemMessage(hDlg, IDD_FACENAME, LB_GETTEXT, lIndex, (LPARAM)wszFace);
        NewSize.X = 0;
        NewSize.Y = (SHORT)GetPointSizeInRange(hDlg, MIN_PIXEL_HEIGHT, MAX_PIXEL_HEIGHT);

        if (NewSize.Y == 0)
        {
            WCHAR wszBuf[60];
            /*
             * Use wszText, wszBuf to put up an error msg for bad point size
             */
            gbPointSizeError = TRUE;
            LoadString(ghInstance, IDS_FONTSIZE, wszBuf, ARRAYSIZE(wszBuf));
            StringCchPrintf(wszText,
                            ARRAYSIZE(wszText),
                            wszBuf,
                            MIN_PIXEL_HEIGHT,
                            MAX_PIXEL_HEIGHT);

            GetWindowText(hDlg, wszBuf, ARRAYSIZE(wszBuf));
            MessageBox(hDlg, wszText, wszBuf, MB_OK | MB_ICONINFORMATION);
            SetFocus(hWndList);
            gbPointSizeError = FALSE;
            return FALSE;
        }

        FontIndex = FindCreateFont(FF_MODERN | TMPF_VECTOR | TMPF_TRUETYPE,
                                   wszFace,
                                   NewSize,
                                   0,
                                   gpStateInfo->CodePage);
    }
    else
    {
        FontIndex = lcbGETITEMDATA(hWndList, bLB, lIndex);
    }

    if (FontIndex < 0)
    {
        FontIndex = DefaultFontIndex;
    }

    /*
     * If we've selected a new font, tell the property sheet we've changed
     */
    FAIL_FAST_IF(!((ULONG)FontIndex < NumberOfFonts));
    if ((ULONG)FontIndex >= NumberOfFonts)
    {
        FontIndex = 0;
    }

    if (g_currentFontIndex != (ULONG)FontIndex)
    {
        g_currentFontIndex = FontIndex;
    }

    lpFont = &FontInfo[FontIndex];

    /* Display the new font */

    Parameters[0] = (DWORD_PTR)wszSelectedFont;
    Parameters[1] = (DWORD_PTR)lpFont->FaceName;
    FormatMessageW(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ARGUMENT_ARRAY,
                   ghInstance,
                   MSG_FONTSTRING_FORMATTING,
                   LANG_NEUTRAL,
                   wszFace,
                   ARRAYSIZE(wszFace),
                   (va_list*)Parameters);
    SetDlgItemText(hDlg, IDD_GROUP, wszFace);

    /* Put the font size in the static boxes */
    StringCchPrintf(wszText, ARRAYSIZE(wszText), TEXT("%u"), lpFont->Size.X);
    hWnd = GetDlgItem(hDlg, IDD_FONTWIDTH);
    SetWindowText(hWnd, wszText);
    InvalidateRect(hWnd, NULL, TRUE);
    StringCchPrintf(wszText, ARRAYSIZE(wszText), TEXT("%u"), lpFont->Size.Y);
    hWnd = GetDlgItem(hDlg, IDD_FONTHEIGHT);
    SetWindowText(hWnd, wszText);
    InvalidateRect(hWnd, NULL, TRUE);

    /* Force the preview windows to repaint */
    hWnd = GetDlgItem(hDlg, IDD_PREVIEWWINDOW);
    SendMessage(hWnd, CM_PREVIEW_UPDATE, 0, 0);
    hWnd = GetDlgItem(hDlg, IDD_FONTWINDOW);
    InvalidateRect(hWnd, NULL, TRUE);

    DBGFONTS(("Font %x, (%d,%d) %ls\n",
              FontIndex,
              FontInfo[FontIndex].Size.X,
              FontInfo[FontIndex].Size.Y,
              FontInfo[FontIndex].FaceName));

    return TRUE;
}
