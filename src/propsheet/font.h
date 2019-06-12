/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    font.h

Abstract:

    This module contains the data structures, data types,
    and procedures related to fonts.

Author:

    Therese Stowell (thereses) 15-Jan-1991

Revision History:

--*/
#pragma once
#ifndef FONT_H
#define FONT_H

#define INITIAL_FONTS 20
#define FONT_INCREMENT 3

// clang-format off
#define EF_NEW         0x0001 // a newly available face
#define EF_OLD         0x0002 // a previously available face
#define EF_ENUMERATED  0x0004 // all sizes have been enumerated
#define EF_OEMFONT     0x0008 // an OEM face
#define EF_TTFONT      0x0010 // a TT face
#define EF_DEFFACE     0x0020 // the default face
#define EF_DBCSFONT    0x0040 // the DBCS font
// clang-format on

/*
 * FONT_INFO
 *
 * The distinction between the desired and actual font dimensions obtained
 * is important in the case of TrueType fonts, in which there is no guarantee
 * that what you ask for is what you will get.
 *
 * Note that the correspondence between "Desired" and "Actual" is broken
 * whenever the user changes his display driver, because GDI uses driver
 * parameters to control the font rasterization.
 *
 * The SizeDesired is {0, 0} if the font is a raster font.
 */
typedef struct _FONT_INFO
{
    HFONT hFont;
    COORD Size; // font size obtained
    COORD SizeWant; // 0;0 if Raster font
    LONG Weight;
    LPTSTR FaceName;
    BYTE Family;
    BYTE tmCharSet;
} FONT_INFO, *PFONT_INFO;

#pragma warning(push)
#pragma warning(disable : 4200) // nonstandard extension used : zero-sized array in struct/union

typedef struct tagFACENODE
{
    struct tagFACENODE* pNext;
    DWORD dwFlag;
    TCHAR atch[];
} FACENODE, *PFACENODE;

#pragma warning(pop)

#define TM_IS_TT_FONT(x) (((x)&TMPF_TRUETYPE) == TMPF_TRUETYPE)
#define IS_BOLD(w) ((w) >= FW_SEMIBOLD)
#define SIZE_EQUAL(s1, s2) (((s1).X == (s2).X) && ((s1).Y == (s2).Y))
#define POINTS_PER_INCH 72
#define MIN_PIXEL_HEIGHT 5
#define MAX_PIXEL_HEIGHT 72

//
// Function prototypes
//

VOID
    InitializeFonts(VOID);

VOID
    DestroyFonts(VOID);

[[nodiscard]] NTSTATUS
EnumerateFonts(DWORD Flags);

int FindCreateFont(
    __in DWORD Family,
    __in_ecount(LF_FACESIZE) LPWSTR ptszFace,
    __in COORD Size,
    __in LONG Weight,
    __in UINT CodePage);

BOOL DoFontEnum(
    __in_opt HDC hDC,
    __in_ecount_opt(LF_FACESIZE) LPTSTR ptszFace,
    __in_ecount_opt(nTTPoints) PSHORT pTTPoints,
    __in UINT nTTPoints);

[[nodiscard]] NTSTATUS GetTTFontFaceForCodePage(const UINT uiCodePage,
                                                _Out_writes_(cchFaceName) PWSTR pszFaceName,
                                                const size_t cchFaceName);

bool IsFontSizeCustom(__in PCWSTR pwszFaceName, __in const SHORT sSize);
void CreateSizeForAllTTFonts(__in const SHORT sSize);

#endif /* !FONT_H */
