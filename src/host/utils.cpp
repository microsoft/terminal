// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "utils.hpp"

#include "../interactivity/inc/ServiceLocator.hpp"

#include "srvinit.h"

using Microsoft::Console::Interactivity::ServiceLocator;

til::CoordType CalcWindowSizeX(const til::inclusive_rect& rect) noexcept
{
    return rect.right - rect.left + 1;
}

til::CoordType CalcWindowSizeY(const til::inclusive_rect& rect) noexcept
{
    return rect.bottom - rect.top + 1;
}

til::CoordType CalcCursorYOffsetInPixels(const til::CoordType sFontSizeY, const ULONG ulSize) noexcept
{
    // TODO: MSFT 10229700 - Note, we want to likely enforce that this isn't negative.
    // Pretty sure there's not a valid case for negative offsets here.
    return (til::CoordType)((sFontSizeY) - (ulSize));
}

WORD ConvertStringToDec(_In_ PCWSTR pwchToConvert, _Out_opt_ PCWSTR* const ppwchEnd) noexcept
{
    WORD val = 0;

    while (*pwchToConvert != L'\0')
    {
        auto ch = *pwchToConvert;
        if (L'0' <= ch && ch <= L'9')
        {
            val = (val * 10) + (ch - L'0');
        }
        else
        {
            break;
        }

        pwchToConvert++;
    }

    if (nullptr != ppwchEnd)
    {
        *ppwchEnd = pwchToConvert;
    }

    return val;
}

// Routine Description:
// - Retrieves string resources from our resource files.
// Arguments:
// - id - Resource id from resource.h to the string we need to load.
// - out - Receives the translated string.
void _LoadString(const UINT id, std::wstring& out)
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    WCHAR ItemString[70];
    size_t ItemLength = 0;
    LANGID LangId;

    const auto Status = GetConsoleLangId(gci.OutputCP, &LangId);
    if (SUCCEEDED_NTSTATUS(Status))
    {
        ItemLength = s_LoadStringEx(ServiceLocator::LocateGlobals().hInstance, id, ItemString, ARRAYSIZE(ItemString), LangId);
    }
    if (FAILED_NTSTATUS(Status) || ItemLength == 0)
    {
        ItemLength = LoadStringW(ServiceLocator::LocateGlobals().hInstance, id, ItemString, ARRAYSIZE(ItemString));
    }

    out.append(ItemString, ItemLength);
}

// Routine Description:
// - Helper to retrieve string resources from a MUI with a particular LANGID.
// Arguments:
// - hModule - The module related to loading the resource
// - wID - The resource ID number
// - lpBuffer - Buffer to place string data when read.
// - cchBufferMax - Size of buffer
// - wLangId - Language ID of resources that we should retrieve.
UINT s_LoadStringEx(_In_ HINSTANCE hModule, _In_ UINT wID, _Out_writes_(cchBufferMax) LPWSTR lpBuffer, _In_ UINT cchBufferMax, _In_ WORD wLangId)
{
    // Make sure the params are valid.
    if (lpBuffer == nullptr)
    {
        return 0;
    }

    UINT cch = 0;

    // String Tables are broken up into 16 string segments.  Find the segment containing the string we are interested in.
    const auto hResInfo = FindResourceEx(hModule, RT_STRING, (LPTSTR)((LONG_PTR)(((USHORT)wID >> 4) + 1)), wLangId);
    if (hResInfo != nullptr)
    {
        // Load that segment.
        const auto hStringSeg = (HRSRC)LoadResource(hModule, (HRSRC)hResInfo);

        // Lock the resource.
        LPTSTR lpsz;
        if (hStringSeg != nullptr && (lpsz = (LPTSTR)LockResource(hStringSeg)) != nullptr)
        {
            // Move past the other strings in this segment. (16 strings in a segment -> & 0x0F)
            wID &= 0x0F;
            for (;;)
            {
                // PASCAL like string count
                // first WCHAR is count of WCHARs
                cch = *((WCHAR*)lpsz++);
                if (wID-- == 0)
                {
                    break;
                }

                lpsz += cch; // Step to start if next string
            }

            // cchBufferMax == 0 means return a pointer to the read-only resource buffer.
            if (cchBufferMax == 0)
            {
                *(LPTSTR*)lpBuffer = lpsz;
            }
            else
            {
                // Account for the nullptr
                cchBufferMax--;

                // Don't copy more than the max allowed.
                if (cch > cchBufferMax)
                    cch = cchBufferMax;

                // Copy the string into the buffer.
                memmove(lpBuffer, lpsz, cch * sizeof(WCHAR));
            }
        }
    }

    // Append a nullptr.
    if (cchBufferMax != 0)
    {
        lpBuffer[cch] = 0;
    }

    return cch;
}

// Routine Description:
// - Compares two coordinate positions to determine whether they're the same, left, or right within the given buffer size
// Arguments:
// - bufferSize - The size of the buffer to use for measurements.
// - coordFirst - The first coordinate position
// - coordSecond - The second coordinate position
// Return Value:
// -  Negative if First is to the left of the Second.
// -  0 if First and Second are the same coordinate.
// -  Positive if First is to the right of the Second.
// -  This is so you can do s_CompareCoords(first, second) <= 0 for "first is left or the same as second".
//    (the < looks like a left arrow :D)
// -  The magnitude of the result is the distance between the two coordinates when typing characters into the buffer (left to right, top to bottom)
int Utils::s_CompareCoords(const til::size bufferSize, const til::point coordFirst, const til::point coordSecond) noexcept
{
    const auto cRowWidth = bufferSize.width;

    // Assert that our coordinates are within the expected boundaries
    const auto cRowHeight = bufferSize.height;
    FAIL_FAST_IF(!(coordFirst.x >= 0 && coordFirst.x < cRowWidth));
    FAIL_FAST_IF(!(coordSecond.x >= 0 && coordSecond.x < cRowWidth));
    FAIL_FAST_IF(!(coordFirst.y >= 0 && coordFirst.y < cRowHeight));
    FAIL_FAST_IF(!(coordSecond.y >= 0 && coordSecond.y < cRowHeight));

    // First set the distance vertically
    //   If first is on row 4 and second is on row 6, first will be -2 rows behind second * an 80 character row would be -160.
    //   For the same row, it'll be 0 rows * 80 character width = 0 difference.
    auto retVal = (coordFirst.y - coordSecond.y) * cRowWidth;

    // Now adjust for horizontal differences
    //   If first is in position 15 and second is in position 30, first is -15 left in relation to 30.
    retVal += (coordFirst.x - coordSecond.x);

    // Further notes:
    //   If we already moved behind one row, this will help correct for when first is right of second.
    //     For example, with row 4, col 79 and row 5, col 0 as first and second respectively, the distance is -1.
    //     Assume the row width is 80.
    //     Step one will set the retVal as -80 as first is one row behind the second.
    //     Step two will then see that first is 79 - 0 = +79 right of second and add 79
    //     The total is -80 + 79 = -1.
    return retVal;
}

// Routine Description:
// - Compares two coordinate positions to determine whether they're the same, left, or right
// Arguments:
// - coordFirst - The first coordinate position
// - coordSecond - The second coordinate position
// Return Value:
// -  Negative if First is to the left of the Second.
// -  0 if First and Second are the same coordinate.
// -  Positive if First is to the right of the Second.
// -  This is so you can do s_CompareCoords(first, second) <= 0 for "first is left or the same as second".
//    (the < looks like a left arrow :D)
// -  The magnitude of the result is the distance between the two coordinates when typing characters into the buffer (left to right, top to bottom)
int Utils::s_CompareCoords(const til::point coordFirst, const til::point coordSecond) noexcept
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    // find the width of one row
    const auto coordScreenBufferSize = gci.GetActiveOutputBuffer().GetBufferSize().Dimensions();
    return s_CompareCoords(coordScreenBufferSize, coordFirst, coordSecond);
}

// Routine Description:
// - Finds the opposite corner given a rectangle and one of its corners.
// - For example, finds the bottom right corner given a rectangle and its top left corner.
// Arguments:
// - srRectangle - The rectangle to check
// - coordCorner - One of the corners of the given rectangle
// Return Value:
// - The opposite corner of the one given.
til::point Utils::s_GetOppositeCorner(const til::inclusive_rect& srRectangle, const til::point coordCorner) noexcept
{
    // Assert we were given coordinates that are indeed one of the corners of the rectangle.
    FAIL_FAST_IF(!(coordCorner.x == srRectangle.left || coordCorner.x == srRectangle.right));
    FAIL_FAST_IF(!(coordCorner.y == srRectangle.top || coordCorner.y == srRectangle.bottom));

    return { (srRectangle.left == coordCorner.x) ? srRectangle.right : srRectangle.left,
             (srRectangle.top == coordCorner.y) ? srRectangle.bottom : srRectangle.top };
}
