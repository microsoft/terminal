/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    dbcs.c

Abstract:

    This module contains the code for console DBCS font dialog

Author:

    kazum Feb-27-1995

Revision History:

--*/

#include "precomp.h"
#include <strsafe.h>
#pragma hdrstop

void MakeAltRasterFont(
    __in UINT CodePage,
    __out COORD* AltFontSize,
    __out BYTE* AltFontFamily,
    __out ULONG* AltFontIndex,
    __out_ecount(LF_FACESIZE) LPTSTR AltFaceName)
{
    DWORD i;
    DWORD Find;
    ULONG FontIndex;
    COORD FontSize = FontInfo[DefaultFontIndex].Size;
    COORD FontDelta;
    BOOL fDbcsCharSet = IS_ANY_DBCS_CHARSET(CodePageToCharSet(CodePage));

    FontIndex = 0;
    Find = (DWORD)-1;
    for (i = 0; i < NumberOfFonts; i++)
    {
        if (!TM_IS_TT_FONT(FontInfo[i].Family) &&
            IS_ANY_DBCS_CHARSET(FontInfo[i].tmCharSet) == fDbcsCharSet)
        {
            FontDelta.X = (SHORT)abs(FontSize.X - FontInfo[i].Size.X);
            FontDelta.Y = (SHORT)abs(FontSize.Y - FontInfo[i].Size.Y);
            if (Find > (DWORD)(FontDelta.X + FontDelta.Y))
            {
                Find = (DWORD)(FontDelta.X + FontDelta.Y);
                FontIndex = i;
            }
        }
    }

    *AltFontIndex = FontIndex;
    StringCchCopy(AltFaceName, LF_FACESIZE, FontInfo[*AltFontIndex].FaceName);
    *AltFontSize = FontInfo[*AltFontIndex].Size;
    *AltFontFamily = FontInfo[*AltFontIndex].Family;

    DBGFONTS(("MakeAltRasterFont : AltFontIndex = %ld\n", *AltFontIndex));
}

[[nodiscard]] NTSTATUS
    InitializeDbcsMisc(
        VOID)
{
    return TrueTypeFontList::s_Initialize();
}

BYTE CodePageToCharSet(
    UINT CodePage)
{
    CHARSETINFO csi;

    if (!TranslateCharsetInfo((DWORD*)IntToPtr(CodePage), &csi, TCI_SRCCODEPAGE))
    {
        csi.ciCharset = OEM_CHARSET;
    }

    return (BYTE)csi.ciCharset;
}

LPTTFONTLIST
SearchTTFont(
    __in_opt LPCTSTR ptszFace,
    BOOL fCodePage,
    UINT CodePage)
{
    return TrueTypeFontList::s_SearchByName(ptszFace, fCodePage, CodePage);
}

BOOL IsAvailableTTFont(
    LPCTSTR ptszFace)
{
    if (SearchTTFont(ptszFace, FALSE, 0))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL IsAvailableTTFontCP(
    LPCTSTR ptszFace,
    UINT CodePage)
{
    if (SearchTTFont(ptszFace, TRUE, CodePage))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL IsDisableBoldTTFont(
    LPCTSTR ptszFace)
{
    LPTTFONTLIST pTTFontList;

    pTTFontList = SearchTTFont(ptszFace, FALSE, 0);
    if (pTTFontList != NULL)
    {
        return pTTFontList->fDisableBold;
    }
    else
    {
        return FALSE;
    }
}

LPTSTR
GetAltFaceName(
    LPCTSTR ptszFace)
{
    LPTTFONTLIST pTTFontList;

    pTTFontList = SearchTTFont(ptszFace, FALSE, 0);
    if (pTTFontList != NULL)
    {
        if (wcscmp(ptszFace, pTTFontList->FaceName1) == 0)
        {
            return pTTFontList->FaceName2;
        }

        if (wcscmp(ptszFace, pTTFontList->FaceName2) == 0)
        {
            return pTTFontList->FaceName1;
        }
    }

    return NULL;
}

[[nodiscard]] NTSTATUS
    DestroyDbcsMisc(
        VOID)
{
    return TrueTypeFontList::s_Destroy();
}

int LanguageListCreate(
    HWND hDlg,
    UINT CodePage)

/*++

    Initializes the Language list by enumerating all Locale Information.

    Returns
--*/

{
    HWND hWndLanguageCombo;
    LONG lListIndex;
    CPINFOEX cpinfo;
    UINT oemcp;

    /*
     * Create ComboBox items
     */
    hWndLanguageCombo = GetDlgItem(hDlg, IDD_LANGUAGELIST);
    SendMessage(hWndLanguageCombo, CB_RESETCONTENT, 0, 0L);

    // Add our current CJK code page to the list
    oemcp = GetOEMCP();
    if (GetCPInfoExW(oemcp, 0, &cpinfo))
    {
        lListIndex = (LONG)SendMessage(hWndLanguageCombo, CB_ADDSTRING, 0, (LPARAM)cpinfo.CodePageName);
        if (lListIndex != CB_ERR)
        {
            SendMessage(hWndLanguageCombo, CB_SETITEMDATA, (DWORD)lListIndex, oemcp);

            if (CodePage == oemcp)
            {
                SendMessage(hWndLanguageCombo, CB_SETCURSEL, lListIndex, 0L);
            }
        }
    }

    // Add SBCS 437 OEM - United States to the list
    if (GetCPInfoExW(437, 0, &cpinfo))
    {
        lListIndex = (LONG)SendMessage(hWndLanguageCombo, CB_ADDSTRING, 0, (LPARAM)cpinfo.CodePageName);
        if (lListIndex != CB_ERR)
        {
            SendMessage(hWndLanguageCombo, CB_SETITEMDATA, (DWORD)lListIndex, 437);

            if (CodePage == 437)
            {
                SendMessage(hWndLanguageCombo, CB_SETCURSEL, lListIndex, 0L);
            }
        }
    }

    /*
     * Get the LocaleIndex from the currently selected item.
     * (i will be LB_ERR if no currently selected item).
     */
    lListIndex = (LONG)SendMessage(hWndLanguageCombo, CB_GETCURSEL, 0, 0L);
    const int iRet = (int)SendMessage(hWndLanguageCombo, CB_GETITEMDATA, lListIndex, 0L);

    EnableWindow(hWndLanguageCombo, g_fEastAsianSystem);

    return iRet;
}

int LanguageDisplay(HWND hDlg, UINT CodePage)
{
    CPINFOEX cpinfo;

    if (GetCPInfoExW(CodePage, 0, &cpinfo))
    {
        SetDlgItemText(hDlg, IDD_LANGUAGE, cpinfo.CodePageName);
    }

    return TRUE;
}

// For a given codepage, determine what the default truetype font should be
[[nodiscard]] NTSTATUS GetTTFontFaceForCodePage(const UINT uiCodePage, // the codepage to examine (note: not charset)
                                                _Out_writes_(cchFaceName) PWSTR pszFaceName, // where to write the facename we find
                                                const size_t cchFaceName) // space available in pszFaceName
{
    return TrueTypeFontList::s_SearchByCodePage(uiCodePage, pszFaceName, cchFaceName);
}
