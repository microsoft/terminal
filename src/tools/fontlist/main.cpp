// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <windows.h>
#include <windowsx.h>
#include <wil\result.h>

int CALLBACK FontEnumForV2Console(ENUMLOGFONT* pelf, NEWTEXTMETRIC* pntm, int nFontType, LPARAM lParam);
int AddFont(
    ENUMLOGFONT* pelf,
    NEWTEXTMETRIC* pntm,
    int nFontType,
    HDC hDC);

// This application exists to be connected to a console session while doing exactly nothing.
// This keeps a console session alive and doesn't interfere with tests or other hooks.
int __cdecl wmain(int /*argc*/, WCHAR* /*argv[]*/)
{
    wil::unique_hdc hdc(CreateCompatibleDC(nullptr));
    RETURN_LAST_ERROR_IF_NULL(hdc);

    LOGFONTW LogFont = { 0 };
    LogFont.lfCharSet = DEFAULT_CHARSET;
    wcscpy_s(LogFont.lfFaceName, L"Terminal");

    EnumFontFamiliesExW(hdc.get(), &LogFont, (FONTENUMPROC)FontEnumForV2Console, (LPARAM)hdc.get(), 0);

    return 0;
}

#define CONTINUE_ENUM 1
#define END_ENUM 0

// clang-format off
#define IS_ANY_DBCS_CHARSET( CharSet )                              \
                   ( ((CharSet) == SHIFTJIS_CHARSET)    ? TRUE :    \
                     ((CharSet) == HANGEUL_CHARSET)     ? TRUE :    \
                     ((CharSet) == CHINESEBIG5_CHARSET) ? TRUE :    \
                     ((CharSet) == GB2312_CHARSET)      ? TRUE : FALSE )
// clang-format on

#define CP_US ((UINT)437)
#define CP_JPN ((UINT)932)
#define CP_WANSUNG ((UINT)949)
#define CP_TC ((UINT)950)
#define CP_SC ((UINT)936)
#define IsEastAsianCP(cp) ((cp) == CP_JPN || (cp) == CP_WANSUNG || (cp) == CP_TC || (cp) == CP_SC)

/*
* TTPoints -- Initial font pixel heights for TT fonts
* NOTE:
*   Font pixel heights for TT fonts of DBCS are the same list except
*   odd point size because font width is (SBCS:DBCS != 1:2).
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

int CALLBACK FontEnumForV2Console(ENUMLOGFONT* pelf, NEWTEXTMETRIC* pntm, int nFontType, LPARAM lParam)
{
    UINT i;
    LPCWSTR pwszFace = pelf->elfLogFont.lfFaceName;

    BOOL const fIsEastAsianCP = IsEastAsianCP(GetACP());

    LPCWSTR pwszCharSet;

    switch (pelf->elfLogFont.lfCharSet)
    {
    case ANSI_CHARSET:
        pwszCharSet = L"ANSI";
        break;
    case CHINESEBIG5_CHARSET:
        pwszCharSet = L"Chinese Big5";
        break;
    case EASTEUROPE_CHARSET:
        pwszCharSet = L"East Europe";
        break;
    case GREEK_CHARSET:
        pwszCharSet = L"Greek";
        break;
    case MAC_CHARSET:
        pwszCharSet = L"Mac";
        break;
    case RUSSIAN_CHARSET:
        pwszCharSet = L"Russian";
        break;
    case SYMBOL_CHARSET:
        pwszCharSet = L"Symbol";
        break;
    case BALTIC_CHARSET:
        pwszCharSet = L"Baltic";
        break;
    case DEFAULT_CHARSET:
        pwszCharSet = L"Default";
        break;
    case GB2312_CHARSET:
        pwszCharSet = L"Chinese GB2312";
        break;
    case HANGUL_CHARSET:
        pwszCharSet = L"Korean Hangul";
        break;
    case OEM_CHARSET:
        pwszCharSet = L"OEM";
        break;
    case SHIFTJIS_CHARSET:
        pwszCharSet = L"Japanese Shift-JIS";
        break;
    case TURKISH_CHARSET:
        pwszCharSet = L"Turkish";
        break;
    default:
        pwszCharSet = L"Unknown";
        break;
    }

    wprintf(L"Enum'd font: '%ls' (X: %d, Y: %d) weight 0x%lx (%d) charset %s \r\n",
            pelf->elfLogFont.lfFaceName,
            pelf->elfLogFont.lfWidth,
            pelf->elfLogFont.lfHeight,
            pelf->elfLogFont.lfWeight,
            pelf->elfLogFont.lfWeight,
            pwszCharSet);

    // reject non-monospaced fonts
    if (!(pelf->elfLogFont.lfPitchAndFamily & FIXED_PITCH))
    {
        wprintf(L"Rejecting non-monospaced font. \r\n");
        return CONTINUE_ENUM;
    }

    // reject non-modern or italic TT fonts
    if ((nFontType == TRUETYPE_FONTTYPE) &&
        (((pelf->elfLogFont.lfPitchAndFamily & 0xf0) != FF_MODERN) ||
         pelf->elfLogFont.lfItalic))
    {
        wprintf(L"Rejecting non-FF_MODERN or Italic TrueType font.\r\n");
        return CONTINUE_ENUM;
    }

    // reject non-TT fonts that aren't OEM
    if ((nFontType != TRUETYPE_FONTTYPE) &&
        (!fIsEastAsianCP || !IS_ANY_DBCS_CHARSET(pelf->elfLogFont.lfCharSet)) &&
        (pelf->elfLogFont.lfCharSet != OEM_CHARSET))
    {
        wprintf(L"Rejecting raster font that isn't OEM_CHARSET.\r\n");
        return CONTINUE_ENUM;
    }

    // reject fonts that are vertical
    if (pwszFace[0] == TEXT('@'))
    {
        wprintf(L"Rejecting font face designed for vertical text.\r\n");
        return CONTINUE_ENUM;
    }

    // reject non-TT fonts that aren't terminal
    if ((nFontType != TRUETYPE_FONTTYPE) && (0 != lstrcmp(pwszFace, L"Terminal")))
    {
        wprintf(L"Rejecting raster font that isn't 'Terminal'.\r\n");
        return CONTINUE_ENUM;
    }

    // reject East Asian TT fonts that aren't East Asian charset.
    if (fIsEastAsianCP && !IS_ANY_DBCS_CHARSET(pelf->elfLogFont.lfCharSet))
    {
        wprintf(L"Rejecting East Asian TrueType font that isn't marked with East Asian charsets.\r\n");
        return CONTINUE_ENUM;
    }

    // reject East Asian TT fonts on non-East Asian systems
    if (!fIsEastAsianCP && IS_ANY_DBCS_CHARSET(pelf->elfLogFont.lfCharSet))
    {
        wprintf(L"Rejecting East Asian TrueType font when Windows non-Unicode codepage isn't from CJK country.\r\n");
        return CONTINUE_ENUM;
    }

    if (nFontType & TRUETYPE_FONTTYPE)
    {
        for (i = 0; i < ARRAYSIZE(TTPoints); i++)
        {
            pelf->elfLogFont.lfHeight = TTPoints[i];

            // If it's an East Asian enum, skip all odd height fonts.
            if (fIsEastAsianCP && (pelf->elfLogFont.lfHeight % 2) != 0)
            {
                continue;
            }

            pelf->elfLogFont.lfWidth = 0;
            pelf->elfLogFont.lfWeight = pntm->tmWeight;
            AddFont(pelf, pntm, nFontType, (HDC)lParam);
        }
    }
    else
    {
        AddFont(pelf, pntm, nFontType, (HDC)lParam);
    }

    return CONTINUE_ENUM; // and continue enumeration
}

// Routine Description:
// - Add the font described by the LOGFONT structure to the font table if
//      it's not already there.
int AddFont(
    ENUMLOGFONT* pelf,
    NEWTEXTMETRIC* /*pntm*/,
    int /*nFontType*/,
    HDC hDC)
{
    wil::unique_hfont hfont(CreateFontIndirectW(&pelf->elfLogFont));
    RETURN_LAST_ERROR_IF_NULL(hfont);

    hfont.reset(SelectFont(hDC, hfont.release()));

    TEXTMETRIC tm;
    GetTextMetricsW(hDC, &tm);

    SIZE sz;
    GetTextExtentPoint32W(hDC, L"0", 1, &sz);

    wprintf(L"  Actual Size: (X: %d, Y: %d)\r\n", sz.cx, tm.tmHeight + tm.tmExternalLeading);

    // restore original DC font
    hfont.reset(SelectFont(hDC, hfont.release()));

    return CONTINUE_ENUM;
}
