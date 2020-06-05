#include "precomp.h"
#include <stdio.h>
#include <wchar.h>
#include "wincon.h"

int CALLBACK EnumFontFamiliesExProc(ENUMLOGFONTEX* lpelfe, NEWTEXTMETRICEX* lpntme, int FontType, LPARAM lParam)
{
    lParam;
    FontType;
    lpntme;

    if (lpntme->ntmTm.tmPitchAndFamily & TMPF_FIXED_PITCH)
    {
        // skip non-monospace fonts
        // NOTE: this is weird/backwards and the presence of this flag means non-monospace and the absence means monospace.
        return 1;
    }

    if (lpelfe->elfFullName[0] == L'@')
    {
        return 1; // skip vertical fonts
    }

    if (FontType & DEVICE_FONTTYPE)
    {
        return 1; // skip device type fonts. we're only going to do raster and truetype.
    }

    if (FontType & RASTER_FONTTYPE)
    {
        if (wcscmp(lpelfe->elfFullName, L"Terminal") != 0)
        {
            return 1; // skip non-"Terminal" raster fonts.
        }
    }

    wprintf(L"Charset: %d ", lpntme->ntmTm.tmCharSet);

    wprintf(L"W: %d H: %d", lpntme->ntmTm.tmMaxCharWidth, lpntme->ntmTm.tmHeight);

    wprintf(L"%s, %s, %s\n", lpelfe->elfFullName, lpelfe->elfScript, lpelfe->elfStyle);
    return 1;
}

int __cdecl wmain(int argc, wchar_t** argv)
{
    argc;
    argv;

    HDC hDC = GetDC(NULL);

    /*LOGFONTW lf = { 0, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0,
        0, L"Courier New" };*/

    LOGFONTW lf = { 0 };
    lf.lfCharSet = DEFAULT_CHARSET; // enumerate this charset.
    lf.lfFaceName[0] = L'\0'; // enumerate all font names
    lf.lfPitchAndFamily = 0; // required by API.

    EnumFontFamiliesExW(hDC, &lf, (FONTENUMPROC)EnumFontFamiliesExProc, 0, 0);
    ReleaseDC(NULL, hDC);
    return 0;
}
