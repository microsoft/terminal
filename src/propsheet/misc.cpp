/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    misc.c

Abstract:

        This file implements the NT console server font routines.

Author:

    Therese Stowell (thereses) 22-Jan-1991

Revision History:

--*/

#include "precomp.h"
#include <strsafe.h>
#include <ShellScalingAPI.h>
#pragma hdrstop

#if DBG
ULONG gDebugFlag = 0;
#endif

#define MAX_FONT_INFO_ALLOC (ULONG_MAX / sizeof(FONT_INFO))

#define FE_ABANDONFONT 0
#define FE_SKIPFONT 1
#define FE_FONTOK 2

#define TERMINAL_FACENAME L"Terminal"

/*
 * TTPoints -- Initial font pixel heights for TT fonts
 */
SHORT TTPoints[] = {
    5,
    6,
    7,
    8,
    10,
    12,
    14,
    16,
    18,
    20,
    24,
    28,
    36,
    72
};

/*
 * TTPointsDbcs -- Initial font pixel heights for TT fonts of DBCS.
 * So, This list except odd point size because font width is (SBCS:DBCS != 1:2).
 */
SHORT TTPointsDbcs[] = {
    6,
    8,
    10,
    12,
    14,
    16,
    18,
    20,
    24,
    28,
    36,
    72
};

typedef struct _FONTENUMDATA
{
    HDC hDC;
    BOOL bFindFaces;
    ULONG ulFE;
    __field_ecount_opt(nTTPoints) PSHORT pTTPoints;
    UINT nTTPoints;
} FONTENUMDATA, *PFONTENUMDATA;

PFACENODE
AddFaceNode(
    __in_ecount(LF_FACESIZE) LPCWSTR ptsz)
{
    PFACENODE pNew, *ppTmp;
    size_t cch;

    /*
     * Is it already here?
     */
    for (ppTmp = &gpFaceNames; *ppTmp; ppTmp = &((*ppTmp)->pNext))
    {
        if (0 == lstrcmp(((*ppTmp)->atch), ptsz))
        {
            // already there !
            return *ppTmp;
        }
    }

    cch = wcslen(ptsz);
    pNew = (PFACENODE)HeapAlloc(GetProcessHeap(),
                                0,
                                sizeof(FACENODE) + ((cch + 1) * sizeof(WCHAR)));
    if (pNew == NULL)
    {
        return NULL;
    }

    pNew->pNext = NULL;
    pNew->dwFlag = 0;
    StringCchCopy(pNew->atch, cch + 1, ptsz);
    *ppTmp = pNew;
    return pNew;
}

VOID
    DestroyFaceNodes(
        VOID)
{
    PFACENODE pNext, pTmp;

    pTmp = gpFaceNames;
    while (pTmp != NULL)
    {
        pNext = pTmp->pNext;
        HeapFree(GetProcessHeap(), 0, pTmp);
        pTmp = pNext;
    }

    gpFaceNames = NULL;
}

// TODO: Refactor into lib for use by both conhost and console.dll
//       see http://osgvsowi/677457
UINT GetCurrentDPI(const HWND hWnd, const BOOL fReturnYDPI)
{
    UINT dpiX = 0;
    UINT dpiY = 0;
    GetDpiForMonitor(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST),
                     MDT_EFFECTIVE_DPI,
                     &dpiX,
                     &dpiY);

    return (fReturnYDPI) ? dpiY : dpiX;
}

int GetDPIScaledPixelSize(const int px, const int iCurrentDPI)
{
    return MulDiv(px, iCurrentDPI, 96);
}

int GetDPIYScaledPixelSize(const HWND hWnd, const int px)
{
    return GetDPIScaledPixelSize(px, GetCurrentDPI(hWnd, TRUE));
}

int GetDPIXScaledPixelSize(const HWND hWnd, const int px)
{
    return GetDPIScaledPixelSize(px, GetCurrentDPI(hWnd, FALSE));
}

// If we're running the V2 console, enumerate all of our TrueType fonts and rescale them as appropriate to match the
// current monitor's DPI. This function gets triggered when either the DPI of a single monitor changes, or when the
// properties dialog is moved between monitors of differing DPIs.
void RecreateFontHandles(const HWND hWnd)
{
    if (gpStateInfo->fIsV2Console)
    {
        for (UINT iCurrFont = 0; iCurrFont < NumberOfFonts; iCurrFont++)
        {
            // if the current font is a TrueType font
            if (TM_IS_TT_FONT(FontInfo[iCurrFont].Family))
            {
                LOGFONT lf = { 0 };
                lf.lfWidth = GetDPIXScaledPixelSize(hWnd, FontInfo[iCurrFont].Size.X);
                lf.lfHeight = GetDPIYScaledPixelSize(hWnd, FontInfo[iCurrFont].Size.Y);
                lf.lfWeight = FontInfo[iCurrFont].Weight;
                lf.lfCharSet = FontInfo[iCurrFont].tmCharSet;

                // NOTE: not using what GDI gave us because some fonts don't quite roundtrip (e.g. MS Gothic and VL Gothic)
                lf.lfPitchAndFamily = (FIXED_PITCH | FF_MODERN);
                if (SUCCEEDED(StringCchCopy(lf.lfFaceName, ARRAYSIZE(lf.lfFaceName), FontInfo[iCurrFont].FaceName)))
                {
                    HFONT hRescaledFont = CreateFontIndirect(&lf);
                    if (hRescaledFont != NULL)
                    {
                        // Only replace the existing font if we've got a replacement. The worst that can happen is that
                        // we fail to create our scaled font, so the user sees an incorrectly-scaled font preview.
                        DeleteObject(FontInfo[iCurrFont].hFont);
                        FontInfo[iCurrFont].hFont = hRescaledFont;
                    }
                }
            }
        }
    }
}

// Routine Description:
// - Add the font described by the LOGFONT structure to the font table if
//      it's not already there.
int AddFont(
    ENUMLOGFONT* pelf,
    NEWTEXTMETRIC* pntm,
    int nFontType,
    HDC hDC,
    PFACENODE pFN)
{
    HFONT hFont;
    TEXTMETRIC tm;
    ULONG nFont;
    COORD SizeToShow, SizeActual, SizeWant, SizeOriginal;
    BYTE tmFamily;
    SIZE Size;
    BOOL fCreatingBoldFont = FALSE;
    LPTSTR ptszFace = pelf->elfLogFont.lfFaceName;

    /* get font info */
    SizeWant.X = (SHORT)pelf->elfLogFont.lfWidth;
    SizeWant.Y = (SHORT)pelf->elfLogFont.lfHeight;

    /* save original size request so that we can use it unmodified when doing DPI calculations */
    SizeOriginal.X = (SHORT)pelf->elfLogFont.lfWidth;
    SizeOriginal.Y = (SHORT)pelf->elfLogFont.lfHeight;

CreateBoldFont:
    pelf->elfLogFont.lfQuality = DEFAULT_QUALITY;
    hFont = CreateFontIndirect(&pelf->elfLogFont);
    if (!hFont)
    {
        DBGFONTS(("    REJECT  font (can't create)\n"));
        return FE_SKIPFONT; // same font in other sizes may still be suitable
    }

    DBGFONTS2(("    hFont = %p\n", hFont));

    SelectObject(hDC, hFont);
    GetTextMetrics(hDC, &tm);

    GetTextExtentPoint32(hDC, TEXT("0"), 1, &Size);
    SizeActual.X = (SHORT)Size.cx;
    SizeActual.Y = (SHORT)(tm.tmHeight + tm.tmExternalLeading);
    DBGFONTS2(("    actual size %d,%d\n", SizeActual.X, SizeActual.Y));
    tmFamily = tm.tmPitchAndFamily;
    if (TM_IS_TT_FONT(tmFamily) && (SizeWant.Y >= 0))
    {
        SizeToShow = SizeWant;
        if (SizeWant.X == 0)
        {
            // Asking for zero width height gets a default aspect-ratio width.
            // It's better to show that width rather than 0.
            SizeToShow.X = SizeActual.X;
        }
    }
    else
    {
        SizeToShow = SizeActual;
    }

    DBGFONTS2(("    SizeToShow = (%d,%d), SizeActual = (%d,%d)\n",
               SizeToShow.X,
               SizeToShow.Y,
               SizeActual.X,
               SizeActual.Y));

    /*
     * NOW, determine whether this font entry has already been cached
     * LATER : it may be possible to do this before creating the font, if
     * we can trust the dimensions & other info from pntm.
     * Sort by size:
     *  1) By pixelheight (negative Y values)
     *  2) By height (as shown)
     *  3) By width (as shown)
     */
    for (nFont = 0; nFont < NumberOfFonts; ++nFont)
    {
        COORD SizeShown;

        if (FontInfo[nFont].hFont == NULL)
        {
            DBGFONTS(("!   Font %x has a NULL hFont\n", nFont));
            continue;
        }

        if (FontInfo[nFont].SizeWant.X > 0)
        {
            SizeShown.X = FontInfo[nFont].SizeWant.X;
        }
        else
        {
            SizeShown.X = FontInfo[nFont].Size.X;
        }

        if (FontInfo[nFont].SizeWant.Y > 0)
        {
            // This is a font specified by cell height.
            SizeShown.Y = FontInfo[nFont].SizeWant.Y;
        }
        else
        {
            SizeShown.Y = FontInfo[nFont].Size.Y;
            if (FontInfo[nFont].SizeWant.Y < 0)
            {
                // This is a TT font specified by character height.
                if (SizeWant.Y < 0 && SizeWant.Y > FontInfo[nFont].SizeWant.Y)
                {
                    // Requested pixelheight is smaller than this one.
                    DBGFONTS(("INSERT %d pt at %x, before %d pt\n",
                              -SizeWant.Y,
                              nFont,
                              -FontInfo[nFont].SizeWant.Y));
                    break;
                }
            }
        }

        // Note that we're relying on pntm->tmWeight below because some fonts (e.g. Iosevka Extralight) show up as bold
        // via GetTextMetrics. pntm->tmWeight doesn't have this issue. However, on the second pass through (see
        // :CreateBoldFont) we should use what's in tm.tmWeight
        if (SIZE_EQUAL(SizeShown, SizeToShow) &&
            FontInfo[nFont].Family == tmFamily &&
            FontInfo[nFont].Weight == ((fCreatingBoldFont) ? tm.tmWeight : pntm->tmWeight) &&
            0 == lstrcmp(FontInfo[nFont].FaceName, ptszFace))
        {
            /*
             * Already have this font
             */
            DBGFONTS2(("    Already have the font\n"));
            DeleteObject(hFont);
            return FE_FONTOK;
        }

        if ((SizeToShow.Y < SizeShown.Y) ||
            (SizeToShow.Y == SizeShown.Y && SizeToShow.X < SizeShown.X))
        {
            /*
             * This new font is smaller than nFont
             */
            DBGFONTS(("INSERT at %x, SizeToShow = (%d,%d)\n", nFont, SizeToShow.X, SizeToShow.Y));
            break;
        }
    }

    /*
     * If we have to grow our font table, do it.
     */
    if (NumberOfFonts == FontInfoLength)
    {
        PFONT_INFO Temp = NULL;

        FontInfoLength += FONT_INCREMENT;
        if (FontInfoLength < MAX_FONT_INFO_ALLOC)
        {
            Temp = (PFONT_INFO)HeapReAlloc(GetProcessHeap(),
                                           0,
                                           FontInfo,
                                           sizeof(FONT_INFO) * FontInfoLength);
        }

        if (Temp == NULL)
        {
            FontInfoLength -= FONT_INCREMENT;
            return FE_ABANDONFONT; // no point enumerating more - no memory!
        }
        FontInfo = Temp;
    }

    /*
     * The font we are adding should be inserted into the list, if it is
     * smaller than the last one.
     */
    if (nFont < NumberOfFonts)
    {
        RtlMoveMemory(&FontInfo[nFont + 1],
                      &FontInfo[nFont],
                      sizeof(FONT_INFO) * (NumberOfFonts - nFont));
    }

    /*
     * If we're adding a truetype font for the V2 console, secretly swap out the current hFont with one that's scaled
     * appropriately for DPI
     */
    if (nFontType == TRUETYPE_FONTTYPE && gpStateInfo->fIsV2Console)
    {
        DeleteObject(hFont);
        pelf->elfLogFont.lfWidth = GetDPIXScaledPixelSize(gpStateInfo->hWnd, SizeOriginal.X);
        pelf->elfLogFont.lfHeight = GetDPIYScaledPixelSize(gpStateInfo->hWnd, SizeOriginal.Y);
        hFont = CreateFontIndirect(&pelf->elfLogFont);
        if (!hFont)
        {
            return FE_SKIPFONT;
        }
    }

    /*
     * Store the font info
     */
    FontInfo[nFont].hFont = hFont;
    FontInfo[nFont].Family = tmFamily;
    FontInfo[nFont].Size = SizeActual;
    if (TM_IS_TT_FONT(tmFamily))
    {
        FontInfo[nFont].SizeWant = SizeWant;
    }
    else
    {
        FontInfo[nFont].SizeWant.X = 0;
        FontInfo[nFont].SizeWant.Y = 0;
    }

    FontInfo[nFont].Weight = tm.tmWeight;
    FontInfo[nFont].FaceName = pFN->atch;

    FontInfo[nFont].tmCharSet = tm.tmCharSet;

    ++NumberOfFonts;

    /*
     * If this is a true type font, create a bold version too.
     */
    if (nFontType == TRUETYPE_FONTTYPE && !IS_BOLD(FontInfo[nFont].Weight))
    {
        pelf->elfLogFont.lfWeight = FW_BOLD;
        pelf->elfLogFont.lfWidth = SizeOriginal.X;
        pelf->elfLogFont.lfHeight = SizeOriginal.Y;
        fCreatingBoldFont = TRUE;
        goto CreateBoldFont;
    }

    return FE_FONTOK; // and continue enumeration
}

VOID
    InitializeFonts(
        VOID)
{
    LOG_IF_FAILED(EnumerateFonts(EF_DEFFACE)); // Just the Default font
}

VOID
    DestroyFonts(
        VOID)
{
    ULONG FontIndex;

    if (FontInfo != NULL)
    {
        for (FontIndex = 0; FontIndex < NumberOfFonts; FontIndex++)
        {
            DeleteObject(FontInfo[FontIndex].hFont);
        }
        HeapFree(GetProcessHeap(), 0, FontInfo);
        FontInfo = NULL;
        NumberOfFonts = 0;
    }

    DestroyFaceNodes();
}

/*
 * Returns bit combination
 *  FE_ABANDONFONT  - do not continue enumerating this font
 *  FE_SKIPFONT     - skip this font but keep enumerating
 *  FE_FONTOK       - font was created and added to cache or already there
 *
 * Is called exactly once by GDI for each font in the system. This
 * routine is used to store the FONT_INFO structure.
 */
int CALLBACK FontEnumForV2Console(ENUMLOGFONT* pelf, NEWTEXTMETRIC* pntm, int nFontType, LPARAM lParam)
{
    FAIL_FAST_IF(!(ShouldAllowAllMonoTTFonts()));
    UINT i;
    LPCTSTR ptszFace = pelf->elfLogFont.lfFaceName;
    PFACENODE pFN;
    PFONTENUMDATA pfed = (PFONTENUMDATA)lParam;

    DBGFONTS(("  FontEnum \"%ls\" (%d,%d) weight 0x%lx(%d) %x -- %s\n",
              ptszFace,
              pelf->elfLogFont.lfWidth,
              pelf->elfLogFont.lfHeight,
              pelf->elfLogFont.lfWeight,
              pelf->elfLogFont.lfWeight,
              pelf->elfLogFont.lfCharSet,
              pfed->bFindFaces ? "Finding Faces" : "Creating Fonts"));

    // reject non-monospaced fonts
    if (!(pelf->elfLogFont.lfPitchAndFamily & FIXED_PITCH))
    {
        return pfed->bFindFaces ? FE_SKIPFONT : FE_ABANDONFONT;
    }

    // reject non-modern or italic TT fonts
    if ((nFontType == TRUETYPE_FONTTYPE) &&
        (((pelf->elfLogFont.lfPitchAndFamily & 0xf0) != FF_MODERN) ||
         pelf->elfLogFont.lfItalic))
    {
        DBGFONTS(("    REJECT  face (TT but not FF_MODERN)\n"));
        return pfed->bFindFaces ? FE_SKIPFONT : FE_ABANDONFONT;
    }

    // reject non-TT fonts that aren't OEM
    if ((nFontType != TRUETYPE_FONTTYPE) && !IS_DBCS_OR_OEM_CHARSET(pelf->elfLogFont.lfCharSet))
    {
        DBGFONTS(("    REJECT  face (not TT nor OEM)\n"));
        return FE_SKIPFONT;
    }

    // reject fonts that are vertical
    if (ptszFace[0] == TEXT('@'))
    {
        DBGFONTS(("    REJECT  face (not TT and TATEGAKI)\n"));
        return pfed->bFindFaces ? FE_SKIPFONT : FE_ABANDONFONT;
    }

    // reject non-TT fonts that aren't terminal
    if (g_fEastAsianSystem && (nFontType != TRUETYPE_FONTTYPE) && (0 != lstrcmp(ptszFace, TERMINAL_FACENAME)))
    {
        DBGFONTS(("    REJECT  face (not TT nor Terminal)\n"));
        return pfed->bFindFaces ? FE_SKIPFONT : FE_ABANDONFONT;
    }

    // reject East Asian TT fonts that aren't East Asian charset.
    if (g_fEastAsianSystem && (nFontType == TRUETYPE_FONTTYPE) && !IS_ANY_DBCS_CHARSET(pelf->elfLogFont.lfCharSet))
    {
        DBGFONTS(("    REJECT  face (East Asian charset, but not East Asian TT)\n"));
        return FE_SKIPFONT; // should be enumerate next charset.
    }

    // reject East Asian TT fonts on non-East Asian systems
    if (!g_fEastAsianSystem && (nFontType == TRUETYPE_FONTTYPE) && IS_ANY_DBCS_CHARSET(pelf->elfLogFont.lfCharSet))
    {
        DBGFONTS(("    REJECT  face (East Asian TT and not East Asian charset)\n"));
        return FE_SKIPFONT; // should be enumerate next charset.
    }

    /*
     * Add or find the facename
     */
    pFN = AddFaceNode(ptszFace);
    if (pFN == NULL)
    {
        return FE_ABANDONFONT;
    }

    if (pfed->bFindFaces)
    {
        DWORD dwFontType;

        if (nFontType == TRUETYPE_FONTTYPE)
        {
            DBGFONTS(("NEW TT FACE %ls\n", ptszFace));
            dwFontType = EF_TTFONT;
        }
        else if (nFontType == RASTER_FONTTYPE)
        {
            DBGFONTS(("NEW OEM FACE %ls\n", ptszFace));
            dwFontType = EF_OEMFONT;
        }
        else
        {
            dwFontType = 0;
        }

        pFN->dwFlag |= dwFontType | EF_NEW;
        if (IS_ANY_DBCS_CHARSET(pelf->elfLogFont.lfCharSet))
        {
            pFN->dwFlag |= EF_DBCSFONT;
        }
        return FE_SKIPFONT;
    }

    /*
     * Add the font to the table. If this is a true type font, add the
     * sizes from the array. Otherwise, just add the size we got.
     */
    if (nFontType & TRUETYPE_FONTTYPE)
    {
        for (i = 0; i < pfed->nTTPoints; i++)
        {
            pelf->elfLogFont.lfHeight = pfed->pTTPoints[i];
            pelf->elfLogFont.lfWidth = 0;
            pelf->elfLogFont.lfWeight = pntm->tmWeight;
            pfed->ulFE |= AddFont(pelf, pntm, nFontType, pfed->hDC, pFN);
            if (pfed->ulFE == FE_ABANDONFONT)
            {
                return FE_ABANDONFONT;
            }
        }
    }
    else
    {
        pfed->ulFE |= AddFont(pelf, pntm, nFontType, pfed->hDC, pFN);
        if (pfed->ulFE == FE_ABANDONFONT)
        {
            return FE_ABANDONFONT;
        }
    }

    return FE_FONTOK; // and continue enumeration
}

/*
 * Returns bit combination
 *  FE_ABANDONFONT  - do not continue enumerating this font
 *  FE_SKIPFONT     - skip this font but keep enumerating
 *  FE_FONTOK       - font was created and added to cache or already there
 *
 * Is called exactly once by GDI for each font in the system. This
 * routine is used to store the FONT_INFO structure.
 */
int
    CALLBACK
    FontEnum(
        ENUMLOGFONT* pelf,
        NEWTEXTMETRIC* pntm,
        int nFontType,
        LPARAM lParam)
{
    UINT i;
    LPCTSTR ptszFace = pelf->elfLogFont.lfFaceName;
    PFACENODE pFN;
    PFONTENUMDATA pfed = (PFONTENUMDATA)lParam;

    DBGFONTS(("  FontEnum \"%ls\" (%d,%d) weight 0x%lx(%d) %x -- %s\n",
              ptszFace,
              pelf->elfLogFont.lfWidth,
              pelf->elfLogFont.lfHeight,
              pelf->elfLogFont.lfWeight,
              pelf->elfLogFont.lfWeight,
              pelf->elfLogFont.lfCharSet,
              pfed->bFindFaces ? "Finding Faces" : "Creating Fonts"));

    //
    // reject variable width and italic fonts, also tt fonts with neg ac
    //

    if (
        !(pelf->elfLogFont.lfPitchAndFamily & FIXED_PITCH) ||
        (pelf->elfLogFont.lfItalic) ||
        !(pntm->ntmFlags & NTM_NONNEGATIVE_AC))
    {
        if (!IsAvailableTTFont(ptszFace))
        {
            DBGFONTS(("    REJECT  face (dbcs, variable pitch, italic, or neg a&c)\n"));
            return pfed->bFindFaces ? FE_SKIPFONT : FE_ABANDONFONT;
        }
    }

    /*
     * reject TT fonts for whoom family is not modern, that is do not use
     * FF_DONTCARE    // may be surprised unpleasantly
     * FF_DECORATIVE  // likely to be symbol fonts
     * FF_SCRIPT      // cursive, inappropriate for console
     * FF_SWISS OR FF_ROMAN // variable pitch
     */

    if ((nFontType == TRUETYPE_FONTTYPE) &&
        ((pelf->elfLogFont.lfPitchAndFamily & 0xf0) != FF_MODERN))
    {
        DBGFONTS(("    REJECT  face (TT but not FF_MODERN)\n"));
        return pfed->bFindFaces ? FE_SKIPFONT : FE_ABANDONFONT;
    }

    /*
     * reject non-TT fonts that aren't OEM
     */
    if ((nFontType != TRUETYPE_FONTTYPE) &&
        (!g_fEastAsianSystem || !IS_ANY_DBCS_CHARSET(pelf->elfLogFont.lfCharSet)) &&
        (pelf->elfLogFont.lfCharSet != OEM_CHARSET))
    {
        DBGFONTS(("    REJECT  face (not TT nor OEM)\n"));
        return FE_SKIPFONT;
    }

    /*
     * reject non-TT fonts that are virtical font
     */
    if ((nFontType != TRUETYPE_FONTTYPE) &&
        (ptszFace[0] == TEXT('@')))
    {
        DBGFONTS(("    REJECT  face (not TT and TATEGAKI)\n"));
        return pfed->bFindFaces ? FE_SKIPFONT : FE_ABANDONFONT;
    }

    /*
     * reject non-TT fonts that aren't Terminal
     */
    if (g_fEastAsianSystem && (nFontType != TRUETYPE_FONTTYPE) &&
        (0 != lstrcmp(ptszFace, TERMINAL_FACENAME)))
    {
        DBGFONTS(("    REJECT  face (not TT nor Terminal)\n"));
        return pfed->bFindFaces ? FE_SKIPFONT : FE_ABANDONFONT;
    }

    /*
     * reject East Asian TT fonts that aren't East Asian charset.
     */
    if (IsAvailableTTFont(ptszFace) &&
        !IS_ANY_DBCS_CHARSET(pelf->elfLogFont.lfCharSet) &&
        !IsAvailableTTFontCP(ptszFace, 0))
    {
        DBGFONTS(("    REJECT  face (East Asian TT and not East Asian charset)\n"));
        return FE_SKIPFONT; // should be enumerate next charset.
    }

    /*
     * Add or find the facename
     */
    pFN = AddFaceNode(ptszFace);
    if (pFN == NULL)
    {
        return FE_ABANDONFONT;
    }

    if (pfed->bFindFaces)
    {
        DWORD dwFontType;

        if (nFontType == TRUETYPE_FONTTYPE)
        {
            DBGFONTS(("NEW TT FACE %ls\n", ptszFace));
            dwFontType = EF_TTFONT;
        }
        else if (nFontType == RASTER_FONTTYPE)
        {
            DBGFONTS(("NEW OEM FACE %ls\n", ptszFace));
            dwFontType = EF_OEMFONT;
        }
        else
        {
            dwFontType = 0;
        }

        pFN->dwFlag |= dwFontType | EF_NEW;
        if (IS_ANY_DBCS_CHARSET(pelf->elfLogFont.lfCharSet))
        {
            pFN->dwFlag |= EF_DBCSFONT;
        }
        return FE_SKIPFONT;
    }

    if (IS_BOLD(pelf->elfLogFont.lfWeight))
    {
        DBGFONTS2(("    A bold font (weight %d)\n", pelf->elfLogFont.lfWeight));
        // return FE_SKIPFONT;
    }

    /*
     * Add the font to the table. If this is a true type font, add the
     * sizes from the array. Otherwise, just add the size we got.
     */
    if (nFontType & TRUETYPE_FONTTYPE)
    {
        for (i = 0; i < pfed->nTTPoints; i++)
        {
            pelf->elfLogFont.lfHeight = pfed->pTTPoints[i];
            pelf->elfLogFont.lfWidth = 0;
            pelf->elfLogFont.lfWeight = 400;
            pfed->ulFE |= AddFont(pelf, pntm, nFontType, pfed->hDC, pFN);
            if (pfed->ulFE == FE_ABANDONFONT)
            {
                return FE_ABANDONFONT;
            }
        }
    }
    else
    {
        pfed->ulFE |= AddFont(pelf, pntm, nFontType, pfed->hDC, pFN);
        if (pfed->ulFE == FE_ABANDONFONT)
        {
            return FE_ABANDONFONT;
        }
    }

    return FE_FONTOK; // and continue enumeration
}

BOOL DoFontEnum(
    __in_opt HDC hDC,
    __in_ecount_opt(LF_FACESIZE) LPTSTR ptszFace,
    __in_ecount_opt(nTTPoints) PSHORT pTTPoints,
    __in UINT nTTPoints)
{
    BOOL bDeleteDC = FALSE;
    FONTENUMDATA fed;
    LOGFONT LogFont;

    DBGFONTS(("DoFontEnum \"%ls\"\n", ptszFace));
    if (hDC == NULL)
    {
        hDC = CreateCompatibleDC(NULL);
        bDeleteDC = TRUE;
    }

    fed.hDC = hDC;
    fed.bFindFaces = (ptszFace == NULL);
    fed.ulFE = 0;
    fed.pTTPoints = pTTPoints;
    fed.nTTPoints = nTTPoints;
    RtlZeroMemory(&LogFont, sizeof(LOGFONT));
    LogFont.lfCharSet = DEFAULT_CHARSET;
    if (ptszFace != nullptr)
    {
        StringCchCopy(LogFont.lfFaceName, LF_FACESIZE, ptszFace);

        if (NumberOfFonts == 0 && // We've yet to enumerate fonts
            g_fEastAsianSystem && // And we're currently using a CJK codepage
            !IS_ANY_DBCS_CHARSET(CodePageToCharSet(OEMCP)) && // But the system codepage *isn't* CJK
            0 == lstrcmp(ptszFace, TERMINAL_FACENAME))
        { // and we're looking at the raster font

            // In this specific scenario, the raster font will only be enumerated if we ask for OEM_CHARSET rather than
            // a CJK charset
            LogFont.lfCharSet = OEM_CHARSET;
        }
    }

    /*
     * EnumFontFamiliesEx function enumerates one font in every face in every
     * character set.
     */
    EnumFontFamiliesEx(hDC, &LogFont, (FONTENUMPROC)((ShouldAllowAllMonoTTFonts()) ? FontEnumForV2Console : FontEnum), (LPARAM)&fed, 0);
    if (bDeleteDC)
    {
        DeleteDC(hDC);
    }

    return (fed.ulFE & FE_FONTOK) != 0;
}

VOID RemoveFace(__in_ecount(LF_FACESIZE) LPCTSTR ptszFace)
{
    DWORD i;
    int nToRemove = 0;

    DBGFONTS(("RemoveFace %ls\n", ptszFace));
    //
    // Delete & Remove fonts with Face Name == ptszFace
    //
    for (i = 0; i < NumberOfFonts; i++)
    {
        if (0 == lstrcmp(FontInfo[i].FaceName, ptszFace))
        {
            BOOL bDeleted = DeleteObject(FontInfo[i].hFont);
            DBGFONTS(("RemoveFace: hFont %p was %sdeleted\n",
                      FontInfo[i].hFont,
                      bDeleted ? "" : "NOT "));
            bDeleted; // to fix x86 build complaining
            FontInfo[i].hFont = NULL;
            nToRemove++;
        }
        else if (nToRemove > 0)
        {
            /*
             * Shuffle from FontInfo[i] down nToRemove slots.
             */
            RtlMoveMemory(&FontInfo[i - nToRemove],
                          &FontInfo[i],
                          sizeof(FONT_INFO) * (NumberOfFonts - i));
            NumberOfFonts -= nToRemove;
            i -= nToRemove;
            nToRemove = 0;
        }
    }
    NumberOfFonts -= nToRemove;
}

// Given a desired SHORT size, search pTTPoints to determine if size is in the list.
static bool IsSizePresentInList(__in const SHORT sSizeDesired, __in_ecount(nTTPoints) PSHORT pTTPoints, __in UINT nTTPoints)
{
    bool fSizePresent = false;
    for (UINT i = 0; i < nTTPoints; i++)
    {
        if (pTTPoints[i] == sSizeDesired)
        {
            fSizePresent = true;
            break;
        }
    }
    return fSizePresent;
}

// Given a face name, determine if the size provided is custom (i.e. not on the hardcoded list of sizes). Note that the
// list of sizes we use varies depending on the codepage being used
bool IsFontSizeCustom(__in PCWSTR pszFaceName, __in const SHORT sSize)
{
    bool fUsingCustomFontSize;
    if (g_fEastAsianSystem && !IsAvailableTTFontCP(pszFaceName, 0))
    {
        fUsingCustomFontSize = !IsSizePresentInList(sSize, TTPointsDbcs, ARRAYSIZE(TTPointsDbcs));
    }
    else
    {
        fUsingCustomFontSize = !IsSizePresentInList(sSize, TTPoints, ARRAYSIZE(TTPoints));
    }

    return fUsingCustomFontSize;
}

// Determines if the currently-selected font is using a custom size
static bool IsCurrentFontSizeCustom()
{
    return IsFontSizeCustom(gpStateInfo->FaceName, gpStateInfo->FontSize.Y);
}

// Given a size, iterate through all TT fonts and load them in the provided size (only used for custom (non-hardcoded)
// font sizes)
void CreateSizeForAllTTFonts(__in const SHORT sSize)
{
    HDC hDC = CreateCompatibleDC(NULL);

    // for each font face
    for (PFACENODE pFN = gpFaceNames; pFN; pFN = pFN->pNext)
    {
        if (pFN->dwFlag & EF_TTFONT)
        {
            // if it's a TT font, load the supplied size
            DoFontEnum(hDC, pFN->atch, (PSHORT)&sSize, 1);
        }
    }
}

[[nodiscard]] NTSTATUS
EnumerateFonts(
    DWORD Flags)
{
    TEXTMETRIC tm;
    HDC hDC;
    PFACENODE pFN;
    DWORD FontIndex;
    DWORD dwFontType = 0;

    DBGFONTS(("EnumerateFonts %lx\n", Flags));

    dwFontType = (EF_TTFONT | EF_OEMFONT | EF_DEFFACE) & Flags;

    if (FontInfo == NULL)
    {
        //
        // allocate memory for the font array
        //
        NumberOfFonts = 0;

        FontInfo = (PFONT_INFO)HeapAlloc(GetProcessHeap(), 0, sizeof(FONT_INFO) * INITIAL_FONTS);
        if (FontInfo == NULL)
        {
            return STATUS_NO_MEMORY;
        }

        FontInfoLength = INITIAL_FONTS;
    }

    hDC = CreateCompatibleDC(NULL);

    if (Flags & EF_DEFFACE)
    {
        SelectObject(hDC, GetStockObject(OEM_FIXED_FONT));
        GetTextMetrics(hDC, &tm);
        GetTextFace(hDC, LF_FACESIZE, DefaultFaceName);

        DefaultFontSize.X = (SHORT)(tm.tmMaxCharWidth);
        DefaultFontSize.Y = (SHORT)(tm.tmHeight + tm.tmExternalLeading);
        DefaultFontFamily = tm.tmPitchAndFamily;

        if (IS_ANY_DBCS_CHARSET(tm.tmCharSet))
        {
            DefaultFontSize.X /= 2;
        }

        DBGFONTS(("Default (OEM) Font %ls (%d,%d) CharSet 0x%02X\n", DefaultFaceName, DefaultFontSize.X, DefaultFontSize.Y, tm.tmCharSet));

        // Make sure we are going to enumerate the OEM face.
        pFN = AddFaceNode(DefaultFaceName);
        if (pFN != NULL)
        {
            pFN->dwFlag |= EF_DEFFACE | EF_OEMFONT;
        }
    }

    if (gbEnumerateFaces)
    {
        /*
         * Set the EF_OLD bit and clear the EF_NEW bit
         * for all previously available faces
         */
        for (pFN = gpFaceNames; pFN; pFN = pFN->pNext)
        {
            pFN->dwFlag |= EF_OLD;
            pFN->dwFlag &= ~EF_NEW;
        }

        //
        // Use DoFontEnum to get the names of all the suitable Faces
        // All facenames found will be put in gpFaceNames with
        // the EF_NEW bit set.
        //
        DoFontEnum(hDC, NULL, TTPoints, 1);
        gbEnumerateFaces = FALSE;
    }

    // Use DoFontEnum to get all fonts from the system.  Our FontEnum
    // proc puts just the ones we want into an array
    //
    for (pFN = gpFaceNames; pFN; pFN = pFN->pNext)
    {
        DBGFONTS(("\"%ls\" is %s%s%s%s%s%s\n", pFN->atch, pFN->dwFlag & EF_NEW ? "NEW " : " ", pFN->dwFlag & EF_OLD ? "OLD " : " ", pFN->dwFlag & EF_ENUMERATED ? "ENUMERATED " : " ", pFN->dwFlag & EF_OEMFONT ? "OEMFONT " : " ", pFN->dwFlag & EF_TTFONT ? "TTFONT " : " ", pFN->dwFlag & EF_DEFFACE ? "DEFFACE " : " "));

        if ((pFN->dwFlag & (EF_OLD | EF_NEW)) == EF_OLD)
        {
            // The face is no longer available
            RemoveFace(pFN->atch);
            pFN->dwFlag &= ~EF_ENUMERATED;
            continue;
        }
        if ((pFN->dwFlag & dwFontType) == 0)
        {
            // not the kind of face we want
            continue;
        }
        if (pFN->dwFlag & EF_ENUMERATED)
        {
            // we already enumerated this face
            continue;
        }

        if (pFN->dwFlag & EF_TTFONT)
        {
            if (g_fEastAsianSystem && !IsAvailableTTFontCP(pFN->atch, 0))
                DoFontEnum(hDC, pFN->atch, TTPointsDbcs, ARRAYSIZE(TTPointsDbcs));
            else
                DoFontEnum(hDC, pFN->atch, TTPoints, ARRAYSIZE(TTPoints));
        }
        else
        {
            DoFontEnum(hDC, pFN->atch, NULL, 0);
        }
        pFN->dwFlag |= EF_ENUMERATED;
    }

    // Now check to see if the currently selected font is using a custom size not in the hardcoded list (TTPoints or
    // TTPointsDbcs depending on locale). If so, make sure we populate all of our fonts at that size.
    if (IsCurrentFontSizeCustom())
    {
        CreateSizeForAllTTFonts(gpStateInfo->FontSize.Y);
    }

    DeleteDC(hDC);

    if (g_fEastAsianSystem)
    {
        for (FontIndex = 0; FontIndex < NumberOfFonts; FontIndex++)
        {
            if (FontInfo[FontIndex].Size.X == DefaultFontSize.X &&
                FontInfo[FontIndex].Size.Y == DefaultFontSize.Y &&
                IS_DBCS_OR_OEM_CHARSET(FontInfo[FontIndex].tmCharSet) &&
                FontInfo[FontIndex].Family == DefaultFontFamily)
            {
                break;
            }
        }
    }
    else
    {
        for (FontIndex = 0; FontIndex < NumberOfFonts; FontIndex++)
        {
            if (FontInfo[FontIndex].Size.X == DefaultFontSize.X &&
                FontInfo[FontIndex].Size.Y == DefaultFontSize.Y &&
                FontInfo[FontIndex].Family == DefaultFontFamily)
            {
                break;
            }
        }
    }

    if (FontIndex < NumberOfFonts)
    {
        DefaultFontIndex = FontIndex;
    }
    else
    {
        DefaultFontIndex = 0;
    }

    DBGFONTS(("EnumerateFonts : DefaultFontIndex = %ld\n", DefaultFontIndex));

    return STATUS_SUCCESS;
}
