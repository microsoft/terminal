// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "dbcs.h"
#include "misc.h"

#include "../types/inc/convert.hpp"

#include "../interactivity/inc/ServiceLocator.hpp"

#pragma hdrstop

using Microsoft::Console::Interactivity::ServiceLocator;
// Routine Description:
// - This routine check bisected on Ascii string end.
// Arguments:
// - pchBuf - Pointer to Ascii string buffer.
// - cbBuf - Number of Ascii string.
// Return Value:
// - TRUE - Bisected character.
// - FALSE - Correctly.
bool CheckBisectStringA(_In_reads_bytes_(cbBuf) PCHAR pchBuf, _In_ DWORD cbBuf, const CPINFO* const pCPInfo)
{
    while (cbBuf)
    {
        if (IsDBCSLeadByteConsole(*pchBuf, pCPInfo))
        {
            if (cbBuf <= 1)
            {
                return true;
            }
            else
            {
                pchBuf += 2;
                cbBuf -= 2;
            }
        }
        else
        {
            pchBuf++;
            cbBuf--;
        }
    }

    return false;
}

// Routine Description:
// - Checks if a char is a lead byte for a given code page.
// Arguments:
// - ch - the char to check.
// - pCPInfo - the code page to check the char in.
// Return Value:
// true if ch is a lead byte, false otherwise.
bool IsDBCSLeadByteConsole(const CHAR ch, const CPINFO* const pCPInfo)
{
    FAIL_FAST_IF_NULL(pCPInfo);
    // NOTE: This must be unsigned for the comparison. If we compare signed, this will never hit
    // because lead bytes are ironically enough always above 0x80 (signed char negative range).
    const auto uchComparison = (unsigned char)ch;

    auto i = 0;
    // this is ok because the array is guaranteed to have 2
    // null bytes at the end.
    while (pCPInfo->LeadByte[i])
    {
        if (pCPInfo->LeadByte[i] <= uchComparison && uchComparison <= pCPInfo->LeadByte[i + 1])
        {
            return true;
        }
        i += 2;
    }
    return false;
}

BYTE CodePageToCharSet(const UINT uiCodePage)
{
    CHARSETINFO csi{};

    if (!TranslateCharsetInfo((DWORD*)IntToPtr(uiCodePage), &csi, TCI_SRCCODEPAGE))
    {
        // On OneCore-based editions of Windows, the extension apiset containing
        // TranslateCharsetInfo is not hosted. OneCoreUAP hosts it, but the lower
        // editions do not. If we find that we failed to delay-load it, fall back
        // to our "simple" OneCore-OK implementation.
        if (GetLastError() == ERROR_PROC_NOT_FOUND)
        {
            switch (uiCodePage)
            {
            case CP_JAPANESE:
                csi.ciCharset = SHIFTJIS_CHARSET;
                break;
            case CP_CHINESE_SIMPLIFIED:
                csi.ciCharset = GB2312_CHARSET;
                break;
            case CP_KOREAN:
                csi.ciCharset = HANGEUL_CHARSET;
                break;
            case CP_CHINESE_TRADITIONAL:
                csi.ciCharset = CHINESEBIG5_CHARSET;
                break;
            }
        }
        else
        {
            csi.ciCharset = OEM_CHARSET;
        }
    }

    return (BYTE)csi.ciCharset;
}

BOOL IsAvailableEastAsianCodePage(const UINT uiCodePage)
{
    const auto CharSet = CodePageToCharSet(uiCodePage);

    switch (CharSet)
    {
    case SHIFTJIS_CHARSET:
    case HANGEUL_CHARSET:
    case CHINESEBIG5_CHARSET:
    case GB2312_CHARSET:
        return true;
    default:
        return false;
    }
}
