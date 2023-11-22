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

typedef struct _FONTENUMDATA
{
    HDC hDC;
    BOOL bFindFaces;
    ULONG ulFE;
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
    if (pNew == nullptr)
    {
        return nullptr;
    }

    pNew->pNext = nullptr;
    pNew->dwFlag = 0;
    StringCchCopy(pNew->atch, cch + 1, ptsz);
    *ppTmp = pNew;
    return pNew;
}

VOID DestroyFaceNodes(VOID)
{
    PFACENODE pNext, pTmp;

    pTmp = gpFaceNames;
    while (pTmp != nullptr)
    {
        pNext = pTmp->pNext;
        HeapFree(GetProcessHeap(), 0, pTmp);
        pTmp = pNext;
    }

    gpFaceNames = nullptr;
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
    SIZE Size;
    auto fCreatingBoldFont = FALSE;
    auto ptszFace = pelf->elfLogFont.lfFaceName;

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
        if (FontInfo[nFont].hFont == nullptr)
        {
            DBGFONTS(("!   Font %x has a NULL hFont\n", nFont));
            continue;
        }

        // Note that we're relying on pntm->tmWeight below because some fonts (e.g. Iosevka Extralight) show up as bold
        // via GetTextMetrics. pntm->tmWeight doesn't have this issue. However, on the second pass through (see
        // :CreateBoldFont) we should use what's in tm.tmWeight
        if (FontInfo[nFont].Family == tm.tmPitchAndFamily &&
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
    }

    /*
     * If we have to grow our font table, do it.
     */
    if (NumberOfFonts == FontInfoLength)
    {
        PFONT_INFO Temp = nullptr;

        FontInfoLength += FONT_INCREMENT;
        if (FontInfoLength < MAX_FONT_INFO_ALLOC)
        {
            Temp = (PFONT_INFO)HeapReAlloc(GetProcessHeap(),
                                           0,
                                           FontInfo,
                                           sizeof(FONT_INFO) * FontInfoLength);
        }

        if (Temp == nullptr)
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
     * Store the font info
     */
    FontInfo[nFont].hFont = hFont;
    FontInfo[nFont].Family = tm.tmPitchAndFamily;
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
        fCreatingBoldFont = TRUE;
        goto CreateBoldFont;
    }

    return FE_FONTOK; // and continue enumeration
}

VOID InitializeFonts(VOID)
{
    LOG_IF_FAILED(EnumerateFonts(EF_DEFFACE)); // Just the Default font
}

VOID DestroyFonts(VOID)
{
    ULONG FontIndex;

    if (FontInfo != nullptr)
    {
        for (FontIndex = 0; FontIndex < NumberOfFonts; FontIndex++)
        {
            DeleteObject(FontInfo[FontIndex].hFont);
        }
        HeapFree(GetProcessHeap(), 0, FontInfo);
        FontInfo = nullptr;
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
    LPCTSTR ptszFace = pelf->elfLogFont.lfFaceName;
    PFACENODE pFN;
    auto pfed = (PFONTENUMDATA)lParam;

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
    if (pFN == nullptr)
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

    pfed->ulFE |= AddFont(pelf, pntm, nFontType, pfed->hDC, pFN);
    if (pfed->ulFE == FE_ABANDONFONT)
    {
        return FE_ABANDONFONT;
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
    LPCTSTR ptszFace = pelf->elfLogFont.lfFaceName;
    PFACENODE pFN;
    auto pfed = (PFONTENUMDATA)lParam;

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
     * reject TT fonts for whom family is not modern, that is do not use
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
     * reject non-TT fonts that are vertical font
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
    if (pFN == nullptr)
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

    pfed->ulFE |= AddFont(pelf, pntm, nFontType, pfed->hDC, pFN);
    if (pfed->ulFE == FE_ABANDONFONT)
    {
        return FE_ABANDONFONT;
    }

    return FE_FONTOK; // and continue enumeration
}

BOOL DoFontEnum(__in_opt HDC hDC, __in_ecount_opt(LF_FACESIZE) LPTSTR ptszFace)
{
    auto bDeleteDC = FALSE;
    FONTENUMDATA fed;
    LOGFONT LogFont;

    DBGFONTS(("DoFontEnum \"%ls\"\n", ptszFace));
    if (hDC == nullptr)
    {
        hDC = CreateCompatibleDC(nullptr);
        bDeleteDC = TRUE;
    }

    fed.hDC = hDC;
    fed.bFindFaces = (ptszFace == nullptr);
    fed.ulFE = 0;
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
    auto nToRemove = 0;

    DBGFONTS(("RemoveFace %ls\n", ptszFace));
    //
    // Delete & Remove fonts with Face Name == ptszFace
    //
    for (i = 0; i < NumberOfFonts; i++)
    {
        if (0 == lstrcmp(FontInfo[i].FaceName, ptszFace))
        {
            auto bDeleted = DeleteObject(FontInfo[i].hFont);
            DBGFONTS(("RemoveFace: hFont %p was %sdeleted\n",
                      FontInfo[i].hFont,
                      bDeleted ? "" : "NOT "));
            bDeleted; // to fix x86 build complaining
            FontInfo[i].hFont = nullptr;
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

    if (FontInfo == nullptr)
    {
        //
        // allocate memory for the font array
        //
        NumberOfFonts = 0;

        FontInfo = (PFONT_INFO)HeapAlloc(GetProcessHeap(), 0, sizeof(FONT_INFO) * INITIAL_FONTS);
        if (FontInfo == nullptr)
        {
            return STATUS_NO_MEMORY;
        }

        FontInfoLength = INITIAL_FONTS;
    }

    hDC = CreateCompatibleDC(nullptr);

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
        if (pFN != nullptr)
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
        DoFontEnum(hDC, nullptr);
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

        DoFontEnum(hDC, pFN->atch);
        pFN->dwFlag |= EF_ENUMERATED;
    }

    DeleteDC(hDC);

    if (g_fEastAsianSystem)
    {
        for (FontIndex = 0; FontIndex < NumberOfFonts; FontIndex++)
        {
            if (IS_DBCS_OR_OEM_CHARSET(FontInfo[FontIndex].tmCharSet) &&
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
            if (FontInfo[FontIndex].Family == DefaultFontFamily)
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

    std::vector<std::wstring> fonts;
    for (FontIndex = 0; FontIndex < NumberOfFonts; FontIndex++)
    {
        const auto& f = FontInfo[FontIndex];
        wchar_t buffer[512];
        const auto len = swprintf_s(buffer, L"%-32s    %4d    %4d    %4d\n", f.FaceName, f.Weight, f.Family, f.tmCharSet);
        fonts.emplace_back(&buffer[0], len);
    }

    std::stable_sort(fonts.begin(), fonts.end());

    for (const auto& s : fonts)
    {
        OutputDebugStringW(s.c_str());
    }

    return STATUS_SUCCESS;
}
