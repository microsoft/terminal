// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "misc.h"

#include <til/unicode.h>

#include "dbcs.h"
#include "../types/inc/GlyphWidth.hpp"
#include "../interactivity/inc/ServiceLocator.hpp"

#pragma hdrstop

#define CHAR_NULL ((char)0)

using Microsoft::Console::Interactivity::ServiceLocator;

WCHAR CharToWchar(_In_reads_(cch) const char* const pch, const UINT cch)
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto wc = L'\0';

    FAIL_FAST_IF(!(IsDBCSLeadByteConsole(*pch, &gci.OutputCPInfo) || cch == 1));

    ConvertOutputToUnicode(gci.OutputCP, pch, cch, &wc, 1);

    return wc;
}

void SetConsoleCPInfo(const BOOL fOutput)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    if (fOutput)
    {
        if (!GetCPInfo(gci.OutputCP, &gci.OutputCPInfo))
        {
            gci.OutputCPInfo.LeadByte[0] = 0;
        }
    }
    else
    {
        if (!GetCPInfo(gci.CP, &gci.CPInfo))
        {
            gci.CPInfo.LeadByte[0] = 0;
        }
    }
}

// Routine Description:
// - Converts unicode characters to ANSI given a destination codepage
// Arguments:
// - uiCodePage - codepage for use in conversion
// - pwchSource - unicode string to convert
// - cchSource - length of pwchSource in characters
// - pchTarget - pointer to destination buffer to receive converted ANSI string
// - cchTarget - size of destination buffer in characters
// Return Value:
// - Returns the number characters written to pchTarget, or 0 on failure
int ConvertToOem(const UINT uiCodePage,
                 _In_reads_(cchSource) const WCHAR* const pwchSource,
                 const UINT cchSource,
                 _Out_writes_(cchTarget) CHAR* const pchTarget,
                 const UINT cchTarget) noexcept
{
    FAIL_FAST_IF(!(pwchSource != (LPWSTR)pchTarget));
    // clang-format off
#pragma prefast(suppress: __WARNING_W2A_BEST_FIT, "WC_NO_BEST_FIT_CHARS doesn't work in many codepages. Retain old behavior.")
    // clang-format on
    return LOG_IF_WIN32_BOOL_FALSE(WideCharToMultiByte(uiCodePage, 0, pwchSource, cchSource, pchTarget, cchTarget, nullptr, nullptr));
}

// Output data is always translated via the ansi codepage so glyph translation works.
int ConvertOutputToUnicode(_In_ UINT uiCodePage,
                           _In_reads_(cchSource) const CHAR* const pchSource,
                           _In_ UINT cchSource,
                           _Out_writes_(cchTarget) WCHAR* pwchTarget,
                           _In_ UINT cchTarget) noexcept
{
    FAIL_FAST_IF(!(cchTarget > 0));
    pwchTarget[0] = L'\0';
    return MultiByteToWideChar(uiCodePage, MB_USEGLYPHCHARS, pchSource, cchSource, pwchTarget, cchTarget);
}
