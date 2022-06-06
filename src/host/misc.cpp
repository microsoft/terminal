// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "misc.h"
#include "dbcs.h"

#include "../types/inc/convert.hpp"
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
// - This routine check bisected on Unicode string end.
// Arguments:
// - pwchBuffer - Pointer to Unicode string buffer.
// - cWords - Number of Unicode string.
// - cBytes - Number of bisect position by byte counts.
// Return Value:
// - TRUE - Bisected character.
// - FALSE - Correctly.
BOOL CheckBisectStringW(_In_reads_bytes_(cBytes) const WCHAR* pwchBuffer,
                        _In_ size_t cWords,
                        _In_ size_t cBytes) noexcept
{
    while (cWords && cBytes)
    {
        if (IsGlyphFullWidth(*pwchBuffer))
        {
            if (cBytes < 2)
            {
                return TRUE;
            }
            else
            {
                cWords--;
                cBytes -= 2;
                pwchBuffer++;
            }
        }
        else
        {
            cWords--;
            cBytes--;
            pwchBuffer++;
        }
    }

    return FALSE;
}

// Routine Description:
// - This routine check bisected on Unicode string end.
// Arguments:
// - ScreenInfo - reference to screen information structure.
// - pwchBuffer - Pointer to Unicode string buffer.
// - cWords - Number of Unicode string.
// - cBytes - Number of bisect position by byte counts.
// - fPrintableControlChars - TRUE if control characters are being expanded (to ^X)
// Return Value:
// - TRUE - Bisected character.
// - FALSE - Correctly.
BOOL CheckBisectProcessW(const SCREEN_INFORMATION& ScreenInfo,
                         _In_reads_bytes_(cBytes) const WCHAR* pwchBuffer,
                         _In_ size_t cWords,
                         _In_ size_t cBytes,
                         _In_ til::CoordType sOriginalXPosition,
                         _In_ BOOL fPrintableControlChars)
{
    if (WI_IsFlagSet(ScreenInfo.OutputMode, ENABLE_PROCESSED_OUTPUT))
    {
        while (cWords && cBytes)
        {
            const auto Char = *pwchBuffer;
            if (Char >= UNICODE_SPACE)
            {
                if (IsGlyphFullWidth(Char))
                {
                    if (cBytes < 2)
                    {
                        return TRUE;
                    }
                    else
                    {
                        cWords--;
                        cBytes -= 2;
                        pwchBuffer++;
                        sOriginalXPosition += 2;
                    }
                }
                else
                {
                    cWords--;
                    cBytes--;
                    pwchBuffer++;
                    sOriginalXPosition++;
                }
            }
            else
            {
                cWords--;
                pwchBuffer++;
                switch (Char)
                {
                case UNICODE_BELL:
                    if (fPrintableControlChars)
                        goto CtrlChar;
                    break;
                case UNICODE_BACKSPACE:
                case UNICODE_LINEFEED:
                case UNICODE_CARRIAGERETURN:
                    break;
                case UNICODE_TAB:
                {
                    size_t TabSize = NUMBER_OF_SPACES_IN_TAB(sOriginalXPosition);
                    sOriginalXPosition = (til::CoordType)(sOriginalXPosition + TabSize);
                    if (cBytes < TabSize)
                        return TRUE;
                    cBytes -= TabSize;
                    break;
                }
                default:
                    if (fPrintableControlChars)
                    {
                    CtrlChar:
                        if (cBytes < 2)
                            return TRUE;
                        cBytes -= 2;
                        sOriginalXPosition += 2;
                    }
                    else
                    {
                        cBytes--;
                        sOriginalXPosition++;
                    }
                }
            }
        }
        return FALSE;
    }
    else
    {
        return CheckBisectStringW(pwchBuffer, cWords, cBytes);
    }
}

// Routine Description:
// - Converts all key events in the deque to the oem char data and adds
// them back to events.
// Arguments:
// - events - on input the IInputEvents to convert. on output, the
// converted input events
// Note: may throw on error
void SplitToOem(std::deque<std::unique_ptr<IInputEvent>>& events)
{
    const auto codepage = ServiceLocator::LocateGlobals().getConsoleInformation().CP;

    // convert events to oem codepage
    std::deque<std::unique_ptr<IInputEvent>> convertedEvents;
    while (!events.empty())
    {
        auto currentEvent = std::move(events.front());
        events.pop_front();
        if (currentEvent->EventType() == InputEventType::KeyEvent)
        {
            const auto pKeyEvent = static_cast<const KeyEvent* const>(currentEvent.get());
            // convert from wchar to char
            std::wstring wstr{ pKeyEvent->GetCharData() };
            const auto str = ConvertToA(codepage, wstr);

            for (auto& ch : str)
            {
                auto tempEvent = std::make_unique<KeyEvent>(*pKeyEvent);
                tempEvent->SetCharData(ch);
                convertedEvents.push_back(std::move(tempEvent));
            }
        }
        else
        {
            convertedEvents.push_back(std::move(currentEvent));
        }
    }
    // move all events back
    while (!convertedEvents.empty())
    {
        events.push_back(std::move(convertedEvents.front()));
        convertedEvents.pop_front();
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
    DBGCHARS(("ConvertToOem U->%d %.*ls\n", uiCodePage, cchSource > 10 ? 10 : cchSource, pwchSource));
    // clang-format off
#pragma prefast(suppress: __WARNING_W2A_BEST_FIT, "WC_NO_BEST_FIT_CHARS doesn't work in many codepages. Retain old behavior.")
    // clang-format on
    return LOG_IF_WIN32_BOOL_FALSE(WideCharToMultiByte(uiCodePage, 0, pwchSource, cchSource, pchTarget, cchTarget, nullptr, nullptr));
}

// Data in the output buffer is the true unicode value.
int ConvertInputToUnicode(const UINT uiCodePage,
                          _In_reads_(cchSource) const CHAR* const pchSource,
                          const UINT cchSource,
                          _Out_writes_(cchTarget) WCHAR* const pwchTarget,
                          const UINT cchTarget) noexcept
{
    DBGCHARS(("ConvertInputToUnicode %d->U %.*s\n", uiCodePage, cchSource > 10 ? 10 : cchSource, pchSource));

    return MultiByteToWideChar(uiCodePage, 0, pchSource, cchSource, pwchTarget, cchTarget);
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

    DBGCHARS(("ConvertOutputToUnicode %d->U %.*s\n", uiCodePage, cchSource > 10 ? 10 : cchSource, pchSource));

    if (DoBuffersOverlap(reinterpret_cast<const BYTE* const>(pchSource),
                         cchSource * sizeof(CHAR),
                         reinterpret_cast<const BYTE* const>(pwchTarget),
                         cchTarget * sizeof(WCHAR)))
    {
        try
        {
            // buffers overlap so we need to copy one
            std::string copyData(pchSource, cchSource);
            return MultiByteToWideChar(uiCodePage, MB_USEGLYPHCHARS, copyData.data(), cchSource, pwchTarget, cchTarget);
        }
        catch (...)
        {
            return 0;
        }
    }
    else
    {
        return MultiByteToWideChar(uiCodePage, MB_USEGLYPHCHARS, pchSource, cchSource, pwchTarget, cchTarget);
    }
}

// Routine Description:
// - checks if two buffers overlap
// Arguments:
// - pBufferA - pointer to start of first buffer
// - cbBufferA - size of first buffer, in bytes
// - pBufferB - pointer to start of second buffer
// - cbBufferB - size of second buffer, in bytes
// Return Value:
// - true if buffers overlap, false otherwise
bool DoBuffersOverlap(const BYTE* const pBufferA,
                      const UINT cbBufferA,
                      const BYTE* const pBufferB,
                      const UINT cbBufferB) noexcept
{
    const auto pBufferAEnd = pBufferA + cbBufferA;
    const auto pBufferBEnd = pBufferB + cbBufferB;
    return (pBufferA <= pBufferB && pBufferAEnd >= pBufferB) || (pBufferB <= pBufferA && pBufferBEnd >= pBufferA);
}
