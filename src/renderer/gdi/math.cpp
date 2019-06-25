// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "gdirenderer.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Render;

// Routine Description:
// - Gets the size in characters of the current dirty portion of the frame.
// Arguments:
// - <none>
// Return Value:
// - The character dimensions of the current dirty area of the frame.
//      This is an Inclusive rect.
SMALL_RECT GdiEngine::GetDirtyRectInChars()
{
    RECT rc = _psInvalidData.rcPaint;

    SMALL_RECT sr = { 0 };
    LOG_IF_FAILED(_ScaleByFont(&rc, &sr));

    return sr;
}

// Routine Description:
// - Uses the currently selected font to determine how wide the given character will be when renderered.
// - NOTE: Only supports determining half-width/full-width status for CJK-type languages (e.g. is it 1 character wide or 2. a.k.a. is it a rectangle or square.)
// Arguments:
// - glyph - utf16 encoded codepoint to check
// - pResult - recieves return value, True if it is full-width (2 wide). False if it is half-width (1 wide).
// Return Value:
// - S_OK
[[nodiscard]] HRESULT GdiEngine::IsGlyphWideByFont(const std::wstring_view glyph, _Out_ bool* const pResult) noexcept
{
    bool isFullWidth = false;

    if (glyph.size() == 1)
    {
        const wchar_t wch = glyph.front();
        if (_IsFontTrueType())
        {
            ABC abc;
            if (GetCharABCWidthsW(_hdcMemoryContext, wch, wch, &abc))
            {
                int const totalWidth = abc.abcA + abc.abcB + abc.abcC;

                isFullWidth = totalWidth > _GetFontSize().X;
            }
        }
        else
        {
            INT cpxWidth = 0;
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
// - Scales a character region (SMALL_RECT) into a pixel region (RECT) by the current font size.
// Arguments:
// - psr = Character region (SMALL_RECT) from the console text buffer.
// - prc - Pixel region (RECT) for drawing to the client surface.
// Return Value:
// - S_OK or safe math failure value.
[[nodiscard]] HRESULT GdiEngine::_ScaleByFont(const SMALL_RECT* const psr, _Out_ RECT* const prc) const noexcept
{
    COORD const coordFontSize = _GetFontSize();
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_STATE), coordFontSize.X == 0 || coordFontSize.Y == 0);

    RECT rc;
    RETURN_IF_FAILED(LongMult(psr->Left, coordFontSize.X, &rc.left));
    RETURN_IF_FAILED(LongMult(psr->Right, coordFontSize.X, &rc.right));
    RETURN_IF_FAILED(LongMult(psr->Top, coordFontSize.Y, &rc.top));
    RETURN_IF_FAILED(LongMult(psr->Bottom, coordFontSize.Y, &rc.bottom));

    *prc = rc;

    return S_OK;
}

// Routine Description:
// - Scales a character coordinate (COORD) into a pixel coordinate (POINT) by the current font size.
// Arguments:
// - pcoord - Character coordinate (COORD) from the console text buffer.
// - ppt - Pixel coordinate (POINT) for drawing to the client surface.
// Return Value:
// - S_OK or safe math failure value.
[[nodiscard]] HRESULT GdiEngine::_ScaleByFont(const COORD* const pcoord, _Out_ POINT* const pPoint) const noexcept
{
    COORD const coordFontSize = _GetFontSize();
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_STATE), coordFontSize.X == 0 || coordFontSize.Y == 0);

    POINT pt;
    RETURN_IF_FAILED(LongMult(pcoord->X, coordFontSize.X, &pt.x));
    RETURN_IF_FAILED(LongMult(pcoord->Y, coordFontSize.Y, &pt.y));

    *pPoint = pt;

    return S_OK;
}

// Routine Description:
// - Scales a pixel region (RECT) into a character region (SMALL_RECT) by the current font size.
// Arguments:
// - prc - Pixel region (RECT) from drawing to the client surface.
// - psr - Character region (SMALL_RECT) from the console text buffer.
// Return Value:
// - S_OK or safe math failure value.
[[nodiscard]] HRESULT GdiEngine::_ScaleByFont(const RECT* const prc, _Out_ SMALL_RECT* const psr) const noexcept
{
    COORD const coordFontSize = _GetFontSize();
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_STATE), coordFontSize.X == 0 || coordFontSize.Y == 0);

    SMALL_RECT sr;
    sr.Left = static_cast<SHORT>(prc->left / coordFontSize.X);
    sr.Top = static_cast<SHORT>(prc->top / coordFontSize.Y);

    // We're dividing integers so we're always going to round down to the next whole number on division.
    // To make sure that when we round down, we remain an exclusive rectangle, we need to add the width (or height) - 1 before
    // dividing such that a 1 px size rectangle will become a 1 ch size character.
    // For example:
    // L = 1px, R = 2px. Font Width = 8. What we want to see is a character rect that will only draw the 0th character (0 to 1).
    // A. Simple divide
    // 1px / 8px = 0ch for the Left measurement.
    // 2px / 8px = 0ch for the Right which would be inclusive not exclusive.
    // A Conclusion = doesn't work.
    // B. Add a character
    // 1px / 8px = 0ch for the Left measurement.
    // (2px + 8px) / 8px = 1ch for the Right which seems alright.
    // B Conclusion = plausible, but see C for why not.
    // C. Add one pixel less than a full character, but this time R = 8px (which in exclusive terms still only addresses 1 character of pixels.)
    // 1px / 8px = 0ch for the Left measurement.
    // (8px + 8px) / 8px = 2ch for the Right measurement. Now we're redrawing 2 chars when we only needed to do one because this caused us to effectively round up.
    // C Conclusion = this works because our addition can never completely push us over to adding an additional ch to the rectangle.
    // So the algorithm below is using the C conclusion's math.

    // Do math as long and fit to short at the end.
    LONG lRight = prc->right;
    LONG lBottom = prc->bottom;

    // Add the width of a font (in pixels) to the rect
    RETURN_IF_FAILED(LongAdd(lRight, coordFontSize.X, &lRight));
    RETURN_IF_FAILED(LongAdd(lBottom, coordFontSize.Y, &lBottom));

    // Subtract 1 to ensure that we round down.
    RETURN_IF_FAILED(LongSub(lRight, 1, &lRight));
    RETURN_IF_FAILED(LongSub(lBottom, 1, &lBottom));

    // Divide by font size to see how many rows/columns
    // note: no safe math for div.
    lRight /= coordFontSize.X;
    lBottom /= coordFontSize.Y;

    // Attempt to fit into SMALL_RECT's short variable.
    RETURN_IF_FAILED(LongToShort(lRight, &sr.Right));
    RETURN_IF_FAILED(LongToShort(lBottom, &sr.Bottom));

    // Pixels are exclusive and character rects are inclusive. Subtract 1 to go from exclusive to inclusive rect.
    RETURN_IF_FAILED(ShortSub(sr.Right, 1, &sr.Right));
    RETURN_IF_FAILED(ShortSub(sr.Bottom, 1, &sr.Bottom));

    *psr = sr;

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
POINT GdiEngine::_GetInvalidRectPoint() const
{
    POINT pt;
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
SIZE GdiEngine::_GetInvalidRectSize() const
{
    return _GetRectSize(&_psInvalidData.rcPaint);
}

// Routine Description:
// - Converts a pixel region (RECT) into its width/height (SIZE)
// Arguments:
// - Pixel region (RECT)
// Return Value:
// - Pixel dimensions (SIZE)
SIZE GdiEngine::_GetRectSize(const RECT* const pRect) const
{
    SIZE sz;
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
void GdiEngine::_OrRect(_In_ RECT* const pRectExisting, const RECT* const pRectToOr) const
{
    pRectExisting->left = std::min(pRectExisting->left, pRectToOr->left);
    pRectExisting->top = std::min(pRectExisting->top, pRectToOr->top);
    pRectExisting->right = std::max(pRectExisting->right, pRectToOr->right);
    pRectExisting->bottom = std::max(pRectExisting->bottom, pRectToOr->bottom);
}
