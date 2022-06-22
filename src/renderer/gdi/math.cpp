// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "gdirenderer.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Render;

// Routine Description:
// - Gets the size in characters of the current dirty portion of the frame.
// Arguments:
// - area - The character dimensions of the current dirty area of the frame.
//      This is an Inclusive rect.
// Return Value:
// - S_OK or math failure
[[nodiscard]] HRESULT GdiEngine::GetDirtyArea(gsl::span<const til::rect>& area) noexcept
{
    _invalidCharacters = til::rect{ _psInvalidData.rcPaint }.scale_down(_GetFontSize());

    area = { &_invalidCharacters, 1 };

    return S_OK;
}

// Routine Description:
// - Uses the currently selected font to determine how wide the given character will be when rendered.
// - NOTE: Only supports determining half-width/full-width status for CJK-type languages (e.g. is it 1 character wide or 2. a.k.a. is it a rectangle or square.)
// Arguments:
// - glyph - utf16 encoded codepoint to check
// - pResult - receives return value, True if it is full-width (2 wide). False if it is half-width (1 wide).
// Return Value:
// - S_OK
[[nodiscard]] HRESULT GdiEngine::IsGlyphWideByFont(const std::wstring_view glyph, _Out_ bool* const pResult) noexcept
{
    auto isFullWidth = false;

    if (glyph.size() == 1)
    {
        const auto wch = glyph.front();
        if (_IsFontTrueType())
        {
            ABC abc;
            if (GetCharABCWidthsW(_hdcMemoryContext, wch, wch, &abc))
            {
                const int totalWidth = abc.abcA + abc.abcB + abc.abcC;

                isFullWidth = totalWidth > _GetFontSize().X;
            }
        }
        else
        {
            auto cpxWidth = 0;
            if (GetCharWidth32W(_hdcMemoryContext, wch, wch, &cpxWidth))
            {
                isFullWidth = cpxWidth > _GetFontSize().X;
            }
        }
    }
    else
    {
        // can't find a way to make gdi measure the width of utf16 surrogate pairs.
        // in the meantime, better to be too wide than too narrow.
        isFullWidth = true;
    }

    *pResult = isFullWidth;
    return S_OK;
}

// Routine Description:
// - Scales the given pixel measurement up from the typical system DPI (generally 96) to whatever the given DPI is.
// Arguments:
// - iPx - Pixel length measurement.
// - iDpi - Given DPI scalar value
// Return Value:
// - Pixel measurement scaled against the given DPI scalar.
int GdiEngine::s_ScaleByDpi(const int iPx, const int iDpi)
{
    return MulDiv(iPx, iDpi, s_iBaseDpi);
}

// Routine Description:
// - Shrinks the given pixel measurement down from whatever the given DPI is to the typical system DPI (generally 96).
// Arguments:
// - iPx - Pixel measurement scaled against the given DPI.
// - iDpi - Given DPI for pixel scaling
// Return Value:
// - Pixel length measurement.
int GdiEngine::s_ShrinkByDpi(const int iPx, const int iDpi)
{
    return MulDiv(iPx, s_iBaseDpi, iDpi);
}

// Routine Description:
// - Uses internal invalid structure to determine the top left pixel point of the invalid frame to be painted.
// Arguments:
// - <none>
// Return Value:
// - Top left corner in pixels of where to start repainting the frame.
til::point GdiEngine::_GetInvalidRectPoint() const
{
    til::point pt;
    pt.x = _psInvalidData.rcPaint.left;
    pt.y = _psInvalidData.rcPaint.top;

    return pt;
}

// Routine Description:
// - Uses internal invalid structure to determine the size of the invalid area of the frame to be painted.
// Arguments:
// - <none>
// Return Value:
// - Width and height in pixels of the invalid area of the frame.
til::size GdiEngine::_GetInvalidRectSize() const
{
    return _GetRectSize(&_psInvalidData.rcPaint);
}

// Routine Description:
// - Converts a pixel region (til::rect) into its width/height (til::size)
// Arguments:
// - Pixel region (til::rect)
// Return Value:
// - Pixel dimensions (til::size)
til::size GdiEngine::_GetRectSize(const RECT* const pRect) const
{
    til::size sz;
    sz.cx = pRect->right - pRect->left;
    sz.cy = pRect->bottom - pRect->top;

    return sz;
}

// Routine Description:
// - Performs a "CombineRect" with the "OR" operation.
// - Basically extends the existing rect outward to also encompass the passed-in region.
// Arguments:
// - pRectExisting - Expand this rectangle to encompass the add rect.
// - pRectToOr - Add this rectangle to the existing one.
// Return Value:
// - <none>
void GdiEngine::_OrRect(_In_ til::rect* pRectExisting, const til::rect* pRectToOr) const
{
    pRectExisting->left = std::min(pRectExisting->left, pRectToOr->left);
    pRectExisting->top = std::min(pRectExisting->top, pRectToOr->top);
    pRectExisting->right = std::max(pRectExisting->right, pRectToOr->right);
    pRectExisting->bottom = std::max(pRectExisting->bottom, pRectToOr->bottom);
}
