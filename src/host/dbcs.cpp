// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "dbcs.h"
#include "misc.h"

#include "../types/inc/convert.hpp"
#include "../types/inc/GlyphWidth.hpp"

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
// - This routine removes the double copies of characters used when storing DBCS/Double-wide characters in the text buffer.
// - It munges up Unicode cells that are about to be returned whenever there is DBCS data and a raster font is enabled.
// - This function is ONLY FOR COMPATIBILITY PURPOSES. Please do not introduce new usages.
// Arguments:
// - buffer - The buffer to walk and fix
// Return Value:
// - The length of the final modified buffer.
DWORD UnicodeRasterFontCellMungeOnRead(const gsl::span<CHAR_INFO> buffer)
{
    // Walk through the source CHAR_INFO and copy each to the destination.
    // EXCEPT for trailing bytes (this will de-duplicate the leading/trailing byte double copies of the CHAR_INFOs as stored in the buffer).

    // Set up indices used for arrays.
    DWORD iDst = 0;

    // Walk through every CHAR_INFO
    for (DWORD iSrc = 0; iSrc < gsl::narrow<size_t>(buffer.size()); iSrc++)
    {
        // If it's not a trailing byte, copy it straight over, stripping out the Leading/Trailing flags from the attributes field.
        if (!WI_IsFlagSet(buffer.at(iSrc).Attributes, COMMON_LVB_TRAILING_BYTE))
        {
            buffer.at(iDst) = buffer.at(iSrc);
            WI_ClearAllFlags(buffer.at(iDst).Attributes, COMMON_LVB_SBCSDBCS);
            iDst++;
        }

        // If it was a trailing byte, we'll just walk past it and keep going.
    }

    // Zero out the remaining part of the destination buffer that we didn't use.
    DWORD const cchDstToClear = gsl::narrow<DWORD>(buffer.size()) - iDst;

    if (cchDstToClear > 0)
    {
        CHAR_INFO* const pciDstClearStart = buffer.data() + iDst;
        ZeroMemory(pciDstClearStart, cchDstToClear * sizeof(CHAR_INFO));
    }

    // Add the additional length we just modified.
    iDst += cchDstToClear;

    // now that we're done, we should have copied, left alone, or cleared the entire length.
    FAIL_FAST_IF(!(iDst == gsl::narrow<size_t>(buffer.size())));

    return iDst;
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
    unsigned char const uchComparison = (unsigned char)ch;

    int i = 0;
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
    CHARSETINFO csi;

    const auto inputServices = ServiceLocator::LocateInputServices();
    if (nullptr == inputServices || !inputServices->TranslateCharsetInfo((DWORD*)IntToPtr(uiCodePage), &csi, TCI_SRCCODEPAGE))
    {
        csi.ciCharset = OEM_CHARSET;
    }

    return (BYTE)csi.ciCharset;
}

BOOL IsAvailableEastAsianCodePage(const UINT uiCodePage)
{
    BYTE const CharSet = CodePageToCharSet(uiCodePage);

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

_Ret_range_(0, cbAnsi)
    ULONG TranslateUnicodeToOem(_In_reads_(cchUnicode) PCWCHAR pwchUnicode,
                                const ULONG cchUnicode,
                                _Out_writes_bytes_(cbAnsi) PCHAR pchAnsi,
                                const ULONG cbAnsi,
                                _Out_ std::unique_ptr<IInputEvent>& partialEvent)
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    PWCHAR const TmpUni = new (std::nothrow) WCHAR[cchUnicode];
    if (TmpUni == nullptr)
    {
        return 0;
    }

    memcpy(TmpUni, pwchUnicode, cchUnicode * sizeof(WCHAR));

    BYTE AsciiDbcs[2];
    AsciiDbcs[1] = 0;

    ULONG i, j;
    for (i = 0, j = 0; i < cchUnicode && j < cbAnsi; i++, j++)
    {
        if (IsGlyphFullWidth(TmpUni[i]))
        {
            ULONG const NumBytes = sizeof(AsciiDbcs);
            ConvertToOem(gci.CP, &TmpUni[i], 1, (LPSTR)&AsciiDbcs[0], NumBytes);
            if (IsDBCSLeadByteConsole(AsciiDbcs[0], &gci.CPInfo))
            {
                if (j < cbAnsi - 1)
                { // -1 is safe DBCS in buffer
                    pchAnsi[j] = AsciiDbcs[0];
                    j++;
                    pchAnsi[j] = AsciiDbcs[1];
                    AsciiDbcs[1] = 0;
                }
                else
                {
                    pchAnsi[j] = AsciiDbcs[0];
                    break;
                }
            }
            else
            {
                pchAnsi[j] = AsciiDbcs[0];
                AsciiDbcs[1] = 0;
            }
        }
        else
        {
            ConvertToOem(gci.CP, &TmpUni[i], 1, &pchAnsi[j], 1);
        }
    }

    if (AsciiDbcs[1])
    {
        try
        {
            std::unique_ptr<KeyEvent> keyEvent = std::make_unique<KeyEvent>();
            if (keyEvent.get())
            {
                keyEvent->SetCharData(AsciiDbcs[1]);
                partialEvent.reset(static_cast<IInputEvent* const>(keyEvent.release()));
            }
        }
        catch (...)
        {
            LOG_HR(wil::ResultFromCaughtException());
        }
    }

    delete[] TmpUni;
    return j;
}
