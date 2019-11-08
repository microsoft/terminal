// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <vector>
#include "gdirenderer.hpp"

#include "../inc/unicode.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Render;

// Routine Description:
// - Prepares internal structures for a painting operation.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we started to paint. S_FALSE if we didn't need to paint. HRESULT error code if painting didn't start successfully.
[[nodiscard]] HRESULT GdiEngine::StartPaint() noexcept
{
    // If we have no handle, we don't need to paint. Return quickly.
    RETURN_HR_IF(S_FALSE, !_IsWindowValid());

    // If we're already painting, we don't need to paint. Return quickly.
    RETURN_HR_IF(S_FALSE, _fPaintStarted);

    // If the window we're painting on is invisible, we don't need to paint. Return quickly.
    // If the title changed, we will need to try and paint this frame. This will
    //      make sure the window's title is updated, even if the window isn't visible.
    RETURN_HR_IF(S_FALSE, (!IsWindowVisible(_hwndTargetWindow) && !_titleChanged));

    // At the beginning of a new frame, we have 0 lines ready for painting in PolyTextOut
    _cPolyText = 0;

    // Prepare our in-memory bitmap for double-buffered composition.
    RETURN_IF_FAILED(_PrepareMemoryBitmap(_hwndTargetWindow));

    // We must use Get and Release DC because BeginPaint/EndPaint can only be called in response to a WM_PAINT message (and may hang otherwise)
    // We'll still use the PAINTSTRUCT for information because it's convenient.
    _psInvalidData.hdc = GetDC(_hwndTargetWindow);
    RETURN_HR_IF_NULL(E_FAIL, _psInvalidData.hdc);

    // Signal that we're starting to paint.
    _fPaintStarted = true;

    _psInvalidData.fErase = TRUE;
    _psInvalidData.rcPaint = _rcInvalid;

#if DBG
    _debugContext = GetDC(_debugWindow);
#endif

    return S_OK;
}

// Routine Description:
// - Scrolls the existing data on the in-memory frame by the scroll region
//      deltas we have collectively received through the Invalidate methods
//      since the last time this was called.
// Arguments:
// - <none>
// Return Value:
// - S_OK, suitable GDI HRESULT error, error from Win32 windowing, or safemath error.
[[nodiscard]] HRESULT GdiEngine::ScrollFrame() noexcept
{
    // If we don't have any scrolling to do, return early.
    RETURN_HR_IF(S_OK, 0 == _szInvalidScroll.cx && 0 == _szInvalidScroll.cy);

    // If we have an inverted cursor, we have to see if we have to clean it before we scroll to prevent
    // left behind cursor copies in the scrolled region.
    if (cursorInvertRects.size() > 0)
    {
        for (RECT r : cursorInvertRects)
        {
            // Clean both the in-memory and actual window context.
            RETURN_HR_IF(E_FAIL, !(InvertRect(_hdcMemoryContext, &r)));
            RETURN_HR_IF(E_FAIL, !(InvertRect(_psInvalidData.hdc, &r)));
        }

        cursorInvertRects.clear();
    }

    // We have to limit the region that can be scrolled to not include the gutters.
    // Gutters are defined as sub-character width pixels at the bottom or right of the screen.
    COORD const coordFontSize = _GetFontSize();
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_STATE), coordFontSize.X == 0 || coordFontSize.Y == 0);

    SIZE szGutter;
    szGutter.cx = _szMemorySurface.cx % coordFontSize.X;
    szGutter.cy = _szMemorySurface.cy % coordFontSize.Y;

    RECT rcScrollLimit = { 0 };
    RETURN_IF_FAILED(LongSub(_szMemorySurface.cx, szGutter.cx, &rcScrollLimit.right));
    RETURN_IF_FAILED(LongSub(_szMemorySurface.cy, szGutter.cy, &rcScrollLimit.bottom));

    // Scroll real window and memory buffer in-sync.
    LOG_LAST_ERROR_IF(!ScrollWindowEx(_hwndTargetWindow,
                                      _szInvalidScroll.cx,
                                      _szInvalidScroll.cy,
                                      &rcScrollLimit,
                                      &rcScrollLimit,
                                      nullptr,
                                      nullptr,
                                      0));

    RECT rcUpdate = { 0 };
    LOG_HR_IF(E_FAIL, !(ScrollDC(_hdcMemoryContext, _szInvalidScroll.cx, _szInvalidScroll.cy, &rcScrollLimit, &rcScrollLimit, nullptr, &rcUpdate)));

    LOG_IF_FAILED(_InvalidCombine(&rcUpdate));

    // update invalid rect for the remainder of paint functions
    _psInvalidData.rcPaint = _rcInvalid;

    return S_OK;
}

// Routine Description:
// - BeginPaint helper to prepare the in-memory bitmap for double-buffering
// Arguments:
// - hwnd - Window handle to use for the DC properties when creating a memory DC and for checking the client area size.
// Return Value:
// - S_OK or suitable GDI HRESULT error.
[[nodiscard]] HRESULT GdiEngine::_PrepareMemoryBitmap(const HWND hwnd) noexcept
{
    RECT rcClient;
    RETURN_HR_IF(E_FAIL, !(GetClientRect(hwnd, &rcClient)));

    SIZE const szClient = _GetRectSize(&rcClient);

    // Only do work if the existing memory surface is a different size from the client area.
    // Return quickly if they're the same.
    RETURN_HR_IF(S_OK, _szMemorySurface.cx == szClient.cx && _szMemorySurface.cy == szClient.cy);

    wil::unique_hdc hdcRealWindow(GetDC(_hwndTargetWindow));
    RETURN_HR_IF_NULL(E_FAIL, hdcRealWindow.get());

    // If we already had a bitmap, Blt the old one onto the new one and clean up the old one.
    if (nullptr != _hbitmapMemorySurface)
    {
        // Make a temporary DC for us to Blt with.
        wil::unique_hdc hdcTemp(CreateCompatibleDC(hdcRealWindow.get()));
        RETURN_HR_IF_NULL(E_FAIL, hdcTemp.get());

        // Make the new bitmap we'll use going forward with the new size.
        wil::unique_hbitmap hbitmapNew(CreateCompatibleBitmap(hdcRealWindow.get(), szClient.cx, szClient.cy));
        RETURN_HR_IF_NULL(E_FAIL, hbitmapNew.get());

        // Select it into the DC, but hold onto the junky one pixel bitmap (made by default) to give back when we need to Delete.
        wil::unique_hbitmap hbitmapOnePixelJunk(SelectBitmap(hdcTemp.get(), hbitmapNew.get()));
        RETURN_HR_IF_NULL(E_FAIL, hbitmapOnePixelJunk.get());
        hbitmapNew.release(); // if SelectBitmap worked, GDI took ownership. Detach from smart object.

        // Blt from the DC/bitmap we're already holding onto into the new one.
        RETURN_HR_IF(E_FAIL, !(BitBlt(hdcTemp.get(), 0, 0, _szMemorySurface.cx, _szMemorySurface.cy, _hdcMemoryContext, 0, 0, SRCCOPY)));

        // Put the junky bitmap back into the temp DC and get our new one out.
        hbitmapNew.reset(SelectBitmap(hdcTemp.get(), hbitmapOnePixelJunk.get()));
        RETURN_HR_IF_NULL(E_FAIL, hbitmapNew.get());
        hbitmapOnePixelJunk.release(); // if SelectBitmap worked, GDI took ownership. Detach from smart object.

        // Move our new bitmap into the long-standing DC we're holding onto.
        wil::unique_hbitmap hbitmapOld(SelectBitmap(_hdcMemoryContext, hbitmapNew.get()));
        RETURN_HR_IF_NULL(E_FAIL, hbitmapOld.get());

        // Now save a pointer to our new bitmap into the class state.
        _hbitmapMemorySurface = hbitmapNew.release(); // and prevent it from being freed now that GDI is holding onto it as well.
    }
    else
    {
        _hbitmapMemorySurface = CreateCompatibleBitmap(hdcRealWindow.get(), szClient.cx, szClient.cy);
        RETURN_HR_IF_NULL(E_FAIL, _hbitmapMemorySurface);

        wil::unique_hbitmap hOldBitmap(SelectBitmap(_hdcMemoryContext, _hbitmapMemorySurface)); // DC has a default junk bitmap, take it and delete it.
        RETURN_HR_IF_NULL(E_FAIL, hOldBitmap.get());
    }

    // Save the new client size.
    _szMemorySurface = szClient;

    return S_OK;
}

// Routine Description:
// - EndPaint helper to perform the final BitBlt copy from the memory bitmap onto the final window bitmap (double-buffering.) Also cleans up structures used while painting.
// Arguments:
// - <none>
// Return Value:
// - S_OK or suitable GDI HRESULT error.
[[nodiscard]] HRESULT GdiEngine::EndPaint() noexcept
{
    // If we try to end a paint that wasn't started, it's invalid. Return.
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_STATE), !(_fPaintStarted));

    LOG_IF_FAILED(_FlushBufferLines());

    POINT const pt = _GetInvalidRectPoint();
    SIZE const sz = _GetInvalidRectSize();

    LOG_HR_IF(E_FAIL, !(BitBlt(_psInvalidData.hdc, pt.x, pt.y, sz.cx, sz.cy, _hdcMemoryContext, pt.x, pt.y, SRCCOPY)));
    WHEN_DBG(_DebugBltAll());

    _rcInvalid = { 0 };
    _fInvalidRectUsed = false;
    _szInvalidScroll = { 0 };

    LOG_HR_IF(E_FAIL, !(GdiFlush()));
    LOG_HR_IF(E_FAIL, !(ReleaseDC(_hwndTargetWindow, _psInvalidData.hdc)));
    _psInvalidData.hdc = nullptr;

    _fPaintStarted = false;

#if DBG
    ReleaseDC(_debugWindow, _debugContext);
    _debugContext = nullptr;
#endif

    return S_OK;
}

// Routine Description:
// - Used to perform longer running presentation steps outside the lock so the other threads can continue.
// - Not currently used by GdiEngine.
// Arguments:
// - <none>
// Return Value:
// - S_FALSE since we do nothing.
[[nodiscard]] HRESULT GdiEngine::Present() noexcept
{
    return S_FALSE;
}

// Routine Description:
// - Fills the given rectangle with the background color on the drawing context.
// Arguments:
// - prc - Rectangle to fill with color
// Return Value:
// - S_OK or suitable GDI HRESULT error.
[[nodiscard]] HRESULT GdiEngine::_PaintBackgroundColor(const RECT* const prc) noexcept
{
    wil::unique_hbrush hbr(GetStockBrush(DC_BRUSH));
    RETURN_HR_IF_NULL(E_FAIL, hbr.get());

    WHEN_DBG(_PaintDebugRect(prc));

    LOG_HR_IF(E_FAIL, !(FillRect(_hdcMemoryContext, prc, hbr.get())));

    WHEN_DBG(_DoDebugBlt(prc));

    return S_OK;
}

// Routine Description:
// - Paints the background of the invalid area of the frame.
// Arguments:
// - <none>
// Return Value:
// - S_OK or suitable GDI HRESULT error.
[[nodiscard]] HRESULT GdiEngine::PaintBackground() noexcept
{
    if (_psInvalidData.fErase)
    {
        RETURN_IF_FAILED(_PaintBackgroundColor(&_psInvalidData.rcPaint));
    }

    return S_OK;
}

// Routine Description:
// - Draws one line of the buffer to the screen.
// - This will now be cached in a PolyText buffer and flushed periodically instead of drawing every individual segment. Note this means that the PolyText buffer must be flushed before some operations (changing the brush color, drawing lines on top of the characters, inverting for cursor/selection, etc.)
// Arguments:
// - clusters - text to be written and columns expected per cluster
// - coord - character coordinate target to render within viewport
// - trimLeft - This specifies whether to trim one character width off the left side of the output. Used for drawing the right-half only of a double-wide character.
// Return Value:
// - S_OK or suitable GDI HRESULT error.
// - HISTORICAL NOTES:
// ETO_OPAQUE will paint the background color before painting the text.
// ETO_CLIPPED required for ClearType fonts. Cleartype rendering can escape bounding rectangle unless clipped.
//   Unclipped rectangles results in ClearType cutting off the right edge of the previous character when adding chars
//   and in leaving behind artifacts when backspace/removing chars.
//   This mainly applies to ClearType fonts like Lucida Console at small font sizes (10pt) or bolded.
// See: Win7: 390673, 447839 and then superseded by http://osgvsowi/638274 when FE/non-FE rendering condensed.
//#define CONSOLE_EXTTEXTOUT_FLAGS ETO_OPAQUE | ETO_CLIPPED
//#define MAX_POLY_LINES 80
[[nodiscard]] HRESULT GdiEngine::PaintBufferLine(std::basic_string_view<Cluster> const clusters,
                                                 const COORD coord,
                                                 const bool trimLeft) noexcept
{
    try
    {
        const auto cchLine = clusters.size();

        // Exit early if there are no lines to draw.
        RETURN_HR_IF(S_OK, 0 == cchLine);

        POINT ptDraw = { 0 };
        RETURN_IF_FAILED(_ScaleByFont(&coord, &ptDraw));

        const auto pPolyTextLine = &_pPolyText[_cPolyText];

        auto pwsPoly = std::make_unique<wchar_t[]>(cchLine);
        RETURN_IF_NULL_ALLOC(pwsPoly);

        COORD const coordFontSize = _GetFontSize();

        auto rgdxPoly = std::make_unique<int[]>(cchLine);
        RETURN_IF_NULL_ALLOC(rgdxPoly);

        // Sum up the total widths the entire line/run is expected to take while
        // copying the pixel widths into a structure to direct GDI how many pixels to use per character.
        size_t cchCharWidths = 0;

        // Convert data from clusters into the text array and the widths array.
        for (size_t i = 0; i < cchLine; i++)
        {
            const auto& cluster = clusters.at(i);

            // Our GDI renderer hasn't and isn't going to handle things above U+FFFF or sequences.
            // So replace anything complicated with a replacement character for drawing purposes.
            pwsPoly[i] = cluster.GetTextAsSingle();
            rgdxPoly[i] = gsl::narrow<int>(cluster.GetColumns()) * coordFontSize.X;
            cchCharWidths += rgdxPoly[i];
        }

        // Detect and convert for raster font...
        if (!_isTrueTypeFont)
        {
            // dispatch conversion into our codepage

            // Find out the bytes required
            int const cbRequired = WideCharToMultiByte(_fontCodepage, 0, pwsPoly.get(), (int)cchLine, nullptr, 0, nullptr, nullptr);

            if (cbRequired != 0)
            {
                // Allocate buffer for MultiByte
                auto psConverted = std::make_unique<char[]>(cbRequired);

                // Attempt conversion to current codepage
                int const cbConverted = WideCharToMultiByte(_fontCodepage, 0, pwsPoly.get(), (int)cchLine, psConverted.get(), cbRequired, nullptr, nullptr);

                // If successful...
                if (cbConverted != 0)
                {
                    // Now we have to convert back to Unicode but using the system ANSI codepage. Find buffer size first.
                    int const cchRequired = MultiByteToWideChar(CP_ACP, 0, psConverted.get(), cbRequired, nullptr, 0);

                    if (cchRequired != 0)
                    {
                        auto pwsConvert = std::make_unique<wchar_t[]>(cchRequired);

                        // Then do the actual conversion.
                        int const cchConverted = MultiByteToWideChar(CP_ACP, 0, psConverted.get(), cbRequired, pwsConvert.get(), cchRequired);

                        if (cchConverted != 0)
                        {
                            // If all successful, use this instead.
                            pwsPoly.swap(pwsConvert);
                        }
                    }
                }
            }
        }

        pPolyTextLine->lpstr = pwsPoly.release();
        pPolyTextLine->n = gsl::narrow<UINT>(clusters.size());
        pPolyTextLine->x = ptDraw.x;
        pPolyTextLine->y = ptDraw.y;
        pPolyTextLine->uiFlags = ETO_OPAQUE | ETO_CLIPPED;
        pPolyTextLine->rcl.left = pPolyTextLine->x;
        pPolyTextLine->rcl.top = pPolyTextLine->y;
        pPolyTextLine->rcl.right = pPolyTextLine->rcl.left + ((SHORT)cchCharWidths * coordFontSize.X);
        pPolyTextLine->rcl.bottom = pPolyTextLine->rcl.top + coordFontSize.Y;
        pPolyTextLine->pdx = rgdxPoly.release();

        if (trimLeft)
        {
            pPolyTextLine->rcl.left += coordFontSize.X;
        }

        _cPolyText++;

        if (_cPolyText >= s_cPolyTextCache)
        {
            LOG_IF_FAILED(_FlushBufferLines());
        }

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Flushes any buffer lines in the PolyTextOut cache by drawing them and freeing the strings.
// - See also: PaintBufferLine
// Arguments:
// - <none>
// Return Value:
// - S_OK or E_FAIL if GDI failed.
[[nodiscard]] HRESULT GdiEngine::_FlushBufferLines() noexcept
{
    HRESULT hr = S_OK;

    if (_cPolyText > 0)
    {
        if (!PolyTextOutW(_hdcMemoryContext, _pPolyText, (UINT)_cPolyText))
        {
            hr = E_FAIL;
        }

        for (size_t iPoly = 0; iPoly < _cPolyText; iPoly++)
        {
            if (nullptr != _pPolyText[iPoly].lpstr)
            {
                delete[] _pPolyText[iPoly].lpstr;
                _pPolyText[iPoly].lpstr = nullptr;
            }

            if (nullptr != _pPolyText[iPoly].pdx)
            {
                delete[] _pPolyText[iPoly].pdx;
                _pPolyText[iPoly].pdx = nullptr;
            }
        }

        _cPolyText = 0;
    }

    RETURN_HR(hr);
}

// Routine Description:
// - Draws up to one line worth of grid lines on top of characters.
// Arguments:
// - lines - Enum defining which edges of the rectangle to draw
// - color - The color to use for drawing the edges.
// - cchLine - How many characters we should draw the grid lines along (left to right in a row)
// - coordTarget - The starting X/Y position of the first character to draw on.
// Return Value:
// - S_OK or suitable GDI HRESULT error or E_FAIL for GDI errors in functions that don't reliably return a specific error code.
[[nodiscard]] HRESULT GdiEngine::PaintBufferGridLines(const GridLines lines, const COLORREF color, const size_t cchLine, const COORD coordTarget) noexcept
{
    // Return early if there are no lines to paint.
    RETURN_HR_IF(S_OK, GridLines::None == lines);

    LOG_IF_FAILED(_FlushBufferLines());

    // Convert the target from characters to pixels.
    POINT ptTarget;
    RETURN_IF_FAILED(_ScaleByFont(&coordTarget, &ptTarget));
    // Set the brush color as requested and save the previous brush to restore at the end.
    wil::unique_hbrush hbr(CreateSolidBrush(color));
    RETURN_HR_IF_NULL(E_FAIL, hbr.get());

    wil::unique_hbrush hbrPrev(SelectBrush(_hdcMemoryContext, hbr.get()));
    RETURN_HR_IF_NULL(E_FAIL, hbrPrev.get());
    hbr.release(); // If SelectBrush was successful, GDI owns the brush. Release for now.

    // On exit, be sure we try to put the brush back how it was originally.
    auto restoreBrushOnExit = wil::scope_exit([&] { hbr.reset(SelectBrush(_hdcMemoryContext, hbrPrev.get())); });

    // Get the font size so we know the size of the rectangle lines we'll be inscribing.
    COORD const coordFontSize = _GetFontSize();

    // For each length of the line, inscribe the various lines as specified by the enum
    for (size_t i = 0; i < cchLine; i++)
    {
        if (lines & GridLines::Top)
        {
            RETURN_HR_IF(E_FAIL, !(PatBlt(_hdcMemoryContext, ptTarget.x, ptTarget.y, coordFontSize.X, 1, PATCOPY)));
        }

        if (lines & GridLines::Left)
        {
            RETURN_HR_IF(E_FAIL, !(PatBlt(_hdcMemoryContext, ptTarget.x, ptTarget.y, 1, coordFontSize.Y, PATCOPY)));
        }

        // NOTE: Watch out for inclusive/exclusive rectangles here.
        // We have to remove 1 from the font size for the bottom and right lines to ensure that the
        // starting point remains within the clipping rectangle.
        // For example, if we're drawing a letter at 0,0 and the font size is 8x16....
        // The bottom left corner inclusive is at 0,15 which is Y (0) + Font Height (16) - 1 = 15.
        // The top right corner inclusive is at 7,0 which is X (0) + Font Height (8) - 1 = 7.

        if (lines & GridLines::Bottom)
        {
            RETURN_HR_IF(E_FAIL, !(PatBlt(_hdcMemoryContext, ptTarget.x, ptTarget.y + coordFontSize.Y - 1, coordFontSize.X, 1, PATCOPY)));
        }

        if (lines & GridLines::Right)
        {
            RETURN_HR_IF(E_FAIL, !(PatBlt(_hdcMemoryContext, ptTarget.x + coordFontSize.X - 1, ptTarget.y, 1, coordFontSize.Y, PATCOPY)));
        }

        // Move to the next character in this run.
        ptTarget.x += coordFontSize.X;
    }

    return S_OK;
}

// Routine Description:
// - Draws the cursor on the screen
// Arguments:
// - options - Parameters that affect the way that the cursor is drawn
// Return Value:
// - S_OK, suitable GDI HRESULT error, or safemath error, or E_FAIL in a GDI error where a specific error isn't set.
[[nodiscard]] HRESULT GdiEngine::PaintCursor(const IRenderEngine::CursorOptions& options) noexcept
{
    // if the cursor is off, do nothing - it should not be visible.
    if (!options.isOn)
    {
        return S_FALSE;
    }
    LOG_IF_FAILED(_FlushBufferLines());

    COORD const coordFontSize = _GetFontSize();
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_STATE), coordFontSize.X == 0 || coordFontSize.Y == 0);

    // First set up a block cursor the size of the font.
    RECT rcBoundaries;
    RETURN_IF_FAILED(LongMult(options.coordCursor.X, coordFontSize.X, &rcBoundaries.left));
    RETURN_IF_FAILED(LongMult(options.coordCursor.Y, coordFontSize.Y, &rcBoundaries.top));
    RETURN_IF_FAILED(LongAdd(rcBoundaries.left, coordFontSize.X, &rcBoundaries.right));
    RETURN_IF_FAILED(LongAdd(rcBoundaries.top, coordFontSize.Y, &rcBoundaries.bottom));

    // If we're double-width cursor, make it an extra font wider.
    if (options.fIsDoubleWidth)
    {
        RETURN_IF_FAILED(LongAdd(rcBoundaries.right, coordFontSize.X, &rcBoundaries.right));
    }

    // Make a set of RECTs to paint.
    cursorInvertRects.clear();

    RECT rcInvert = rcBoundaries;
    // depending on the cursorType, add rects to that set
    switch (options.cursorType)
    {
    case CursorType::Legacy:
    {
        // Now adjust the cursor height
        // enforce min/max cursor height
        ULONG ulHeight = options.ulCursorHeightPercent;
        ulHeight = std::max(ulHeight, s_ulMinCursorHeightPercent); // No smaller than 25%
        ulHeight = std::min(ulHeight, s_ulMaxCursorHeightPercent); // No larger than 100%

        ulHeight = MulDiv(coordFontSize.Y, ulHeight, 100); // divide by 100 because percent.

        // Reduce the height of the top to be relative to the bottom by the height we want.
        RETURN_IF_FAILED(LongSub(rcInvert.bottom, ulHeight, &rcInvert.top));

        cursorInvertRects.push_back(rcInvert);
    }
    break;

    case CursorType::VerticalBar:
        LONG proposedWidth;
        RETURN_IF_FAILED(LongAdd(rcInvert.left, options.cursorPixelWidth, &proposedWidth));
        // It can't be wider than one cell or we'll have problems in invalidation, so restrict here.
        // It's either the left + the proposed width from the ease of access setting, or
        // it's the right edge of the block cursor as a maximum.
        rcInvert.right = std::min(rcInvert.right, proposedWidth);
        cursorInvertRects.push_back(rcInvert);
        break;

    case CursorType::Underscore:
        RETURN_IF_FAILED(LongAdd(rcInvert.bottom, -1, &rcInvert.top));
        cursorInvertRects.push_back(rcInvert);
        break;

    case CursorType::EmptyBox:
    {
        RECT top, left, right, bottom;
        top = left = right = bottom = rcBoundaries;
        RETURN_IF_FAILED(LongAdd(top.top, 1, &top.bottom));
        RETURN_IF_FAILED(LongAdd(bottom.bottom, -1, &bottom.top));
        RETURN_IF_FAILED(LongAdd(left.left, 1, &left.right));
        RETURN_IF_FAILED(LongAdd(right.right, -1, &right.left));

        RETURN_IF_FAILED(LongAdd(top.left, 1, &top.left));
        RETURN_IF_FAILED(LongAdd(bottom.left, 1, &bottom.left));
        RETURN_IF_FAILED(LongAdd(top.right, -1, &top.right));
        RETURN_IF_FAILED(LongAdd(bottom.right, -1, &bottom.right));

        cursorInvertRects.push_back(top);
        cursorInvertRects.push_back(left);
        cursorInvertRects.push_back(right);
        cursorInvertRects.push_back(bottom);
    }
    break;

    case CursorType::FullBox:
        cursorInvertRects.push_back(rcInvert);
        break;

    default:
        return E_NOTIMPL;
    }
    // Either invert all the RECTs, or paint them.
    if (options.fUseColor)
    {
        HBRUSH hCursorBrush = CreateSolidBrush(options.cursorColor);
        for (RECT r : cursorInvertRects)
        {
            RETURN_HR_IF(E_FAIL, !(FillRect(_hdcMemoryContext, &r, hCursorBrush)));
        }
        DeleteObject(hCursorBrush);
        // Clear out the inverted rects, so that we don't re-invert them next frame.
        cursorInvertRects.clear();
    }
    else
    {
        for (RECT r : cursorInvertRects)
        {
            RETURN_HR_IF(E_FAIL, !(InvertRect(_hdcMemoryContext, &r)));
        }
    }

    return S_OK;
}

// Routine Description:
//  - Inverts the selected region on the current screen buffer.
//  - Reads the selected area, selection mode, and active screen buffer
//    from the global properties and dispatches a GDI invert on the selected text area.
// Arguments:
//  - rect - Rectangle to invert or highlight to make the selection area
// Return Value:
// - S_OK or suitable GDI HRESULT error.
[[nodiscard]] HRESULT GdiEngine::PaintSelection(const SMALL_RECT rect) noexcept
{
    LOG_IF_FAILED(_FlushBufferLines());

    RECT pixelRect = { 0 };
    RETURN_IF_FAILED(_ScaleByFont(&rect, &pixelRect));

    RETURN_HR_IF(E_FAIL, !InvertRect(_hdcMemoryContext, &pixelRect));

    return S_OK;
}

#ifdef DBG

void GdiEngine::_CreateDebugWindow()
{
    if (_fDebug)
    {
        const auto className = L"ConsoleGdiDebugWindow";

        WNDCLASSEX wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = nullptr;
        wc.lpszClassName = className;

        THROW_LAST_ERROR_IF(0 == RegisterClassExW(&wc));

        _debugWindow = CreateWindowExW(0,
                                       className,
                                       L"ConhostGdiDebugWindow",
                                       0,
                                       0,
                                       0,
                                       0,
                                       0,
                                       0,
                                       nullptr,
                                       nullptr,
                                       nullptr);

        THROW_LAST_ERROR_IF_NULL(_debugWindow);

        ShowWindow(_debugWindow, SW_SHOWNORMAL);
    }
}

// Routine Description:
// - Will fill a given rectangle with a gray shade to help identify which portion of the screen is being debugged.
// - Will attempt immediate BLT so you can see it.
// - NOTE: You must set _fDebug flag for this to operate using a debugger.
// - NOTE: This only works in Debug (DBG) builds.
// Arguments:
// - prc - Pointer to rectangle to fill
// Return Value:
// - <none>
void GdiEngine::_PaintDebugRect(const RECT* const prc) const
{
    if (_fDebug)
    {
        if (!IsRectEmpty(prc))
        {
            wil::unique_hbrush hbr(GetStockBrush(GRAY_BRUSH));
            if (nullptr != LOG_HR_IF_NULL(E_FAIL, hbr.get()))
            {
                LOG_HR_IF(E_FAIL, !(FillRect(_hdcMemoryContext, prc, hbr.get())));

                _DoDebugBlt(prc);
            }
        }
    }
}

// Routine Description:
// - Will immediately Blt the given rectangle to the screen for aid in debugging when it is tough to see
//   what is occuring with the in-memory DC.
// - This will pause the thread for 200ms when called to give you an opportunity to see the paint.
// - NOTE: You must set _fDebug flag for this to operate using a debugger.
// - NOTE: This only works in Debug (DBG) builds.
// Arguments:
// - prc - Pointer to region to immediately Blt to the real screen DC.
// Return Value:
// - <none>
void GdiEngine::_DoDebugBlt(const RECT* const prc) const
{
    if (_fDebug)
    {
        if (!IsRectEmpty(prc))
        {
            LOG_HR_IF(E_FAIL, !(BitBlt(_debugContext, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, _hdcMemoryContext, prc->left, prc->top, SRCCOPY)));
            Sleep(100);
        }
    }
}

void GdiEngine::_DebugBltAll() const
{
    if (_fDebug)
    {
        BitBlt(_debugContext, 0, 0, _szMemorySurface.cx, _szMemorySurface.cy, _hdcMemoryContext, 0, 0, SRCCOPY);
        Sleep(100);
    }
}
#endif
