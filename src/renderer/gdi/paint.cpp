// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "gdirenderer.hpp"

#include <til/small_vector.h>

#include "../inc/unicode.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Render;

// This is an excerpt of GDI's FontHasWesternScript() as
// used by InternalTextOut() which is part of ExtTextOutW().
bool GdiEngine::FontHasWesternScript(HDC hdc)
{
    WORD glyphs[4];
    return (GetGlyphIndicesW(hdc, L"dMr\"", 4, glyphs, GGI_MARK_NONEXISTING_GLYPHS) == 4) &&
           (glyphs[0] != 0xFFFF && glyphs[1] != 0xFFFF && glyphs[2] != 0xFFFF && glyphs[3] != 0xFFFF);
}

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

    // We need the advanced graphics mode in order to set a transform.
    SetGraphicsMode(_psInvalidData.hdc, GM_ADVANCED);

    // Signal that we're starting to paint.
    _fPaintStarted = true;

    _psInvalidData.fErase = TRUE;
    _psInvalidData.rcPaint = _rcInvalid.to_win32_rect();

#if DBG
    _debugContext = GetDC(_debugWindow);
#endif

    _lastFontType = FontType::Undefined;

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
    RETURN_HR_IF(S_OK, 0 == _szInvalidScroll.width && 0 == _szInvalidScroll.height);

    // If we have an inverted cursor, we have to see if we have to clean it before we scroll to prevent
    // left behind cursor copies in the scrolled region.
    if (cursorInvertRects.size() > 0)
    {
        // We first need to apply the transform that was active at the time the cursor
        // was rendered otherwise we won't be clearing the right area of the display.
        // We don't need to do this if it was an identity transform though.
        const auto identityTransform = cursorInvertTransform == IDENTITY_XFORM;
        if (!identityTransform)
        {
            LOG_HR_IF(E_FAIL, !SetWorldTransform(_hdcMemoryContext, &cursorInvertTransform));
            LOG_HR_IF(E_FAIL, !SetWorldTransform(_psInvalidData.hdc, &cursorInvertTransform));
        }

        for (const auto& r : cursorInvertRects)
        {
            // Clean both the in-memory and actual window context.
            LOG_HR_IF(E_FAIL, !(InvertRect(_hdcMemoryContext, &r)));
            LOG_HR_IF(E_FAIL, !(InvertRect(_psInvalidData.hdc, &r)));
        }

        // If we've applied a transform, then we need to reset it.
        if (!identityTransform)
        {
            LOG_HR_IF(E_FAIL, !ModifyWorldTransform(_hdcMemoryContext, nullptr, MWT_IDENTITY));
            LOG_HR_IF(E_FAIL, !ModifyWorldTransform(_psInvalidData.hdc, nullptr, MWT_IDENTITY));
        }

        cursorInvertRects.clear();
    }

    // We have to limit the region that can be scrolled to not include the gutters.
    // Gutters are defined as sub-character width pixels at the bottom or right of the screen.
    const auto coordFontSize = _GetFontSize();
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_STATE), coordFontSize.width == 0 || coordFontSize.height == 0);

    til::size szGutter;
    szGutter.width = _szMemorySurface.width % coordFontSize.width;
    szGutter.height = _szMemorySurface.height % coordFontSize.height;

    RECT rcScrollLimit{};
    RETURN_IF_FAILED(LongSub(_szMemorySurface.width, szGutter.width, &rcScrollLimit.right));
    RETURN_IF_FAILED(LongSub(_szMemorySurface.height, szGutter.height, &rcScrollLimit.bottom));

    // Scroll real window and memory buffer in-sync.
    LOG_LAST_ERROR_IF(!ScrollWindowEx(_hwndTargetWindow,
                                      _szInvalidScroll.width,
                                      _szInvalidScroll.height,
                                      &rcScrollLimit,
                                      &rcScrollLimit,
                                      nullptr,
                                      nullptr,
                                      0));

    til::rect rcUpdate;
    LOG_HR_IF(E_FAIL, !(ScrollDC(_hdcMemoryContext, _szInvalidScroll.width, _szInvalidScroll.height, &rcScrollLimit, &rcScrollLimit, nullptr, rcUpdate.as_win32_rect())));

    LOG_IF_FAILED(_InvalidCombine(&rcUpdate));

    // update invalid rect for the remainder of paint functions
    _psInvalidData.rcPaint = _rcInvalid.to_win32_rect();

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

    const auto szClient = _GetRectSize(&rcClient);

    // Only do work if the existing memory surface is a different size from the client area.
    // Return quickly if they're the same.
    RETURN_HR_IF(S_OK, _szMemorySurface.width == szClient.width && _szMemorySurface.height == szClient.height);

    wil::unique_hdc hdcRealWindow(GetDC(_hwndTargetWindow));
    RETURN_HR_IF_NULL(E_FAIL, hdcRealWindow.get());

    // If we already had a bitmap, Blt the old one onto the new one and clean up the old one.
    if (nullptr != _hbitmapMemorySurface)
    {
        // Make a temporary DC for us to Blt with.
        wil::unique_hdc hdcTemp(CreateCompatibleDC(hdcRealWindow.get()));
        RETURN_HR_IF_NULL(E_FAIL, hdcTemp.get());

        // Make the new bitmap we'll use going forward with the new size.
        wil::unique_hbitmap hbitmapNew(CreateCompatibleBitmap(hdcRealWindow.get(), szClient.width, szClient.height));
        RETURN_HR_IF_NULL(E_FAIL, hbitmapNew.get());

        // Select it into the DC, but hold onto the junky one pixel bitmap (made by default) to give back when we need to Delete.
        wil::unique_hbitmap hbitmapOnePixelJunk(SelectBitmap(hdcTemp.get(), hbitmapNew.get()));
        RETURN_HR_IF_NULL(E_FAIL, hbitmapOnePixelJunk.get());
        hbitmapNew.release(); // if SelectBitmap worked, GDI took ownership. Detach from smart object.

        // Blt from the DC/bitmap we're already holding onto into the new one.
        RETURN_HR_IF(E_FAIL, !(BitBlt(hdcTemp.get(), 0, 0, _szMemorySurface.width, _szMemorySurface.height, _hdcMemoryContext, 0, 0, SRCCOPY)));

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
        _hbitmapMemorySurface = CreateCompatibleBitmap(hdcRealWindow.get(), szClient.width, szClient.height);
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

    const auto pt = _GetInvalidRectPoint();
    const auto sz = _GetInvalidRectSize();

    LOG_HR_IF(E_FAIL, !(BitBlt(_psInvalidData.hdc, pt.x, pt.y, sz.width, sz.height, _hdcMemoryContext, pt.x, pt.y, SRCCOPY)));
    WHEN_DBG(_DebugBltAll());

    _rcInvalid = {};
    _fInvalidRectUsed = false;
    _szInvalidScroll = {};

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
    // We need to clear the cursorInvertRects at the start of a paint cycle so
    // we don't inadvertently retain the invert region from the last paint after
    // the cursor is hidden. If we don't, the ScrollFrame method may attempt to
    // clean up a cursor that is no longer there, and instead leave a bunch of
    // "ghost" cursor instances on the screen.
    cursorInvertRects.clear();

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
[[nodiscard]] HRESULT GdiEngine::PaintBufferLine(const std::span<const Cluster> clusters,
                                                 const til::point coord,
                                                 const bool trimLeft,
                                                 const bool /*lineWrapped*/) noexcept
{
    try
    {
        const auto cchLine = clusters.size();

        // Exit early if there are no lines to draw.
        RETURN_HR_IF(S_OK, 0 == cchLine);

        const auto ptDraw = coord * _GetFontSize();

        const auto pPolyTextLine = &_pPolyText[_cPolyText];

        auto& polyString = _polyStrings.emplace_back();
        polyString.reserve(cchLine);

        const auto coordFontSize = _GetFontSize();

        auto& polyWidth = _polyWidths.emplace_back();
        polyWidth.reserve(cchLine);

        // If we have a soft font, we only use the character's lower 7 bits.
        const auto softFontCharMask = _lastFontType == FontType::Soft ? L'\x7F' : ~0;

        // Sum up the total widths the entire line/run is expected to take while
        // copying the pixel widths into a structure to direct GDI how many pixels to use per character.
        size_t cchCharWidths = 0;

        // Convert data from clusters into the text array and the widths array.
        for (size_t i = 0; i < cchLine; i++)
        {
            const auto& cluster = til::at(clusters, i);

            const auto text = cluster.GetText();
            polyString += text;
            polyString.back() &= softFontCharMask;
            polyWidth.push_back(gsl::narrow<int>(cluster.GetColumns()) * coordFontSize.width);
            cchCharWidths += polyWidth.back();
            polyWidth.append(text.size() - 1, 0);
        }

        // Detect and convert for raster font...
        if (!_isTrueTypeFont)
        {
            // dispatch conversion into our codepage

            // Find out the bytes required
            const auto cbRequired = WideCharToMultiByte(_fontCodepage, 0, polyString.data(), (int)cchLine, nullptr, 0, nullptr, nullptr);

            if (cbRequired != 0)
            {
                // Allocate buffer for MultiByte
                auto psConverted = std::make_unique<char[]>(cbRequired);

                // Attempt conversion to current codepage
                const auto cbConverted = WideCharToMultiByte(_fontCodepage, 0, polyString.data(), (int)cchLine, psConverted.get(), cbRequired, nullptr, nullptr);

                // If successful...
                if (cbConverted != 0)
                {
                    // Now we have to convert back to Unicode but using the system ANSI codepage. Find buffer size first.
                    const auto cchRequired = MultiByteToWideChar(CP_ACP, 0, psConverted.get(), cbRequired, nullptr, 0);

                    if (cchRequired != 0)
                    {
                        std::pmr::wstring polyConvert(cchRequired, UNICODE_NULL, &_pool);

                        // Then do the actual conversion.
                        const auto cchConverted = MultiByteToWideChar(CP_ACP, 0, psConverted.get(), cbRequired, polyConvert.data(), cchRequired);

                        if (cchConverted != 0)
                        {
                            // If all successful, use this instead.
                            polyString.swap(polyConvert);
                        }
                    }
                }
            }
        }

        // If the line rendition is double height, we need to adjust the top or bottom
        // of the clipping rect to clip half the height of the rendered characters.
        const auto halfHeight = coordFontSize.height >> 1;
        const auto topOffset = _currentLineRendition == LineRendition::DoubleHeightBottom ? halfHeight : 0;
        const auto bottomOffset = _currentLineRendition == LineRendition::DoubleHeightTop ? halfHeight : 0;

        pPolyTextLine->lpstr = polyString.data();
        pPolyTextLine->n = gsl::narrow<UINT>(polyString.size());
        pPolyTextLine->x = ptDraw.x;
        pPolyTextLine->y = ptDraw.y;
        pPolyTextLine->uiFlags = ETO_OPAQUE | ETO_CLIPPED;
        pPolyTextLine->rcl.left = pPolyTextLine->x;
        pPolyTextLine->rcl.top = pPolyTextLine->y + topOffset;
        pPolyTextLine->rcl.right = pPolyTextLine->rcl.left + (til::CoordType)cchCharWidths;
        pPolyTextLine->rcl.bottom = pPolyTextLine->y + coordFontSize.height - bottomOffset;
        pPolyTextLine->pdx = polyWidth.data();

        if (trimLeft)
        {
            pPolyTextLine->rcl.left += coordFontSize.width;
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
    auto hr = S_OK;

    if (_cPolyText > 0)
    {
        for (size_t i = 0; i != _cPolyText; ++i)
        {
            const auto& t = _pPolyText[i];

            // The following if/else replicates the essentials of how ExtTextOutW() without ETO_IGNORELANGUAGE works.
            // See InternalTextOut().
            //
            // Unlike the original, we don't check for `GetTextCharacterExtra(hdc) != 0`,
            // because we don't ever call SetTextCharacterExtra() anyways.
            //
            // GH#12294:
            // Additionally we set ss.fOverrideDirection to TRUE, because we need to present RTL
            // text in logical order in order to be compatible with applications like `vim -H`.
            if (_fontHasWesternScript && ScriptIsComplex(t.lpstr, t.n, SIC_COMPLEX) == S_FALSE)
            {
                if (!ExtTextOutW(_hdcMemoryContext, t.x, t.y, t.uiFlags | ETO_IGNORELANGUAGE, &t.rcl, t.lpstr, t.n, t.pdx))
                {
                    hr = E_FAIL;
                    break;
                }
            }
            else
            {
                SCRIPT_STATE ss{};
                ss.fOverrideDirection = TRUE;

                SCRIPT_STRING_ANALYSIS ssa;
                hr = ScriptStringAnalyse(_hdcMemoryContext, t.lpstr, t.n, 0, -1, SSA_GLYPHS | SSA_FALLBACK, 0, nullptr, &ss, t.pdx, nullptr, nullptr, &ssa);
                if (FAILED(hr))
                {
                    break;
                }

                hr = ScriptStringOut(ssa, t.x, t.y, t.uiFlags, &t.rcl, 0, 0, FALSE);
                std::ignore = ScriptStringFree(&ssa);
                if (FAILED(hr))
                {
                    break;
                }
            }
        }

        _polyStrings.clear();
        _polyWidths.clear();

        ZeroMemory(_pPolyText, sizeof(_pPolyText));

        _cPolyText = 0;
    }

    RETURN_HR(hr);
}

// Routine Description:
// - Draws up to one line worth of grid lines on top of characters.
// Arguments:
// - lines - Enum defining which edges of the rectangle to draw
// - gridlineColor - The color to use for drawing the gridlines.
// - underlineColor - The color to use for drawing the underlines.
// - cchLine - How many characters we should draw the grid lines along (left to right in a row)
// - coordTarget - The starting X/Y position of the first character to draw on.
// Return Value:
// - S_OK or suitable GDI HRESULT error or E_FAIL for GDI errors in functions that don't reliably return a specific error code.
[[nodiscard]] HRESULT GdiEngine::PaintBufferGridLines(const GridLineSet lines, const COLORREF gridlineColor, const COLORREF underlineColor, const size_t cchLine, const til::point coordTarget) noexcept
try
{
    LOG_IF_FAILED(_FlushBufferLines());

    // Convert the target from characters to pixels.
    const auto ptTarget = coordTarget * _GetFontSize();

    // Create a brush with the gridline color, and apply it.
    wil::unique_hbrush hbr(CreateSolidBrush(gridlineColor));
    RETURN_HR_IF_NULL(E_FAIL, hbr.get());
    const auto prevBrush = wil::SelectObject(_hdcMemoryContext, hbr.get());
    RETURN_HR_IF_NULL(E_FAIL, prevBrush.get());

    // Get the font size so we know the size of the rectangle lines we'll be inscribing.
    const auto fontWidth = _GetFontSize().width;
    const auto fontHeight = _GetFontSize().height;
    const auto widthOfAllCells = fontWidth * gsl::narrow_cast<til::CoordType>(cchLine);

    const auto DrawLine = [=](const til::CoordType x, const til::CoordType y, const til::CoordType w, const til::CoordType h) {
        return PatBlt(_hdcMemoryContext, x, y, w, h, PATCOPY);
    };
    const auto DrawStrokedLine = [&](const til::CoordType x, const til::CoordType y, const til::CoordType w) {
        RETURN_HR_IF(E_FAIL, !MoveToEx(_hdcMemoryContext, x, y, nullptr));
        RETURN_HR_IF(E_FAIL, !LineTo(_hdcMemoryContext, x + w, y));
        return S_OK;
    };
    const auto DrawCurlyLine = [&](const til::CoordType begX, const til::CoordType y, const til::CoordType width) {
        const auto period = _lineMetrics.curlyLinePeriod;
        const auto halfPeriod = period / 2;
        const auto controlPointOffset = _lineMetrics.curlyLineControlPointOffset;

        // To ensure proper continuity of the wavy line between cells of different line color
        // this code starts/ends the line earlier/later than it should and then clips it.
        // Clipping in GDI is expensive, but it was the easiest approach.
        // I've noticed that subtracting -1px prevents missing pixels when GDI draws. They still
        // occur at certain (small) font sizes, but I couldn't figure out how to prevent those.
        const auto lineStart = ((begX - 1) / period) * period;
        const auto lineEnd = begX + width;

        IntersectClipRect(_hdcMemoryContext, begX, ptTarget.y, begX + width, ptTarget.y + fontHeight);
        const auto restoreRegion = wil::scope_exit([&]() {
            // Luckily no one else uses clip regions. They're weird to use.
            SelectClipRgn(_hdcMemoryContext, nullptr);
        });

        // You can assume that each cell has roughly 5 POINTs on average. 128 POINTs is 1KiB.
        til::small_vector<POINT, 128> points;

        // This is the start point of the Bézier curve.
        points.emplace_back(lineStart, y);

        for (auto x = lineStart; x < lineEnd; x += period)
        {
            points.emplace_back(x + halfPeriod, y - controlPointOffset);
            points.emplace_back(x + halfPeriod, y + controlPointOffset);
            points.emplace_back(x + period, y);
        }

        const auto cpt = gsl::narrow_cast<DWORD>(points.size());
        return PolyBezier(_hdcMemoryContext, points.data(), cpt);
    };

    if (lines.test(GridLines::Left))
    {
        auto x = ptTarget.x;
        for (size_t i = 0; i < cchLine; i++, x += fontWidth)
        {
            RETURN_HR_IF(E_FAIL, !DrawLine(x, ptTarget.y, _lineMetrics.gridlineWidth, fontHeight));
        }
    }

    if (lines.test(GridLines::Right))
    {
        // NOTE: We have to subtract the stroke width from the cell width
        // to ensure the x coordinate remains inside the clipping rectangle.
        auto x = ptTarget.x + fontWidth - _lineMetrics.gridlineWidth;
        for (size_t i = 0; i < cchLine; i++, x += fontWidth)
        {
            RETURN_HR_IF(E_FAIL, !DrawLine(x, ptTarget.y, _lineMetrics.gridlineWidth, fontHeight));
        }
    }

    if (lines.test(GridLines::Top))
    {
        const auto y = ptTarget.y;
        RETURN_HR_IF(E_FAIL, !DrawLine(ptTarget.x, y, widthOfAllCells, _lineMetrics.gridlineWidth));
    }

    if (lines.test(GridLines::Bottom))
    {
        // NOTE: We have to subtract the stroke width from the cell height
        // to ensure the y coordinate remains inside the clipping rectangle.
        const auto y = ptTarget.y + fontHeight - _lineMetrics.gridlineWidth;
        RETURN_HR_IF(E_FAIL, !DrawLine(ptTarget.x, y, widthOfAllCells, _lineMetrics.gridlineWidth));
    }

    if (lines.test(GridLines::Strikethrough))
    {
        const auto y = ptTarget.y + _lineMetrics.strikethroughOffset;
        RETURN_HR_IF(E_FAIL, !DrawLine(ptTarget.x, y, widthOfAllCells, _lineMetrics.strikethroughWidth));
    }

    DWORD underlinePenType = PS_SOLID;
    if (lines.test(GridLines::DottedUnderline))
    {
        underlinePenType = PS_DOT;
    }
    else if (lines.test(GridLines::DashedUnderline))
    {
        underlinePenType = PS_DASH;
    }

    DWORD underlineWidth = _lineMetrics.underlineWidth;
    if (lines.any(GridLines::DoubleUnderline, GridLines::CurlyUnderline))
    {
        underlineWidth = _lineMetrics.doubleUnderlineWidth;
    }

    const LOGBRUSH brushProp{ .lbStyle = BS_SOLID, .lbColor = underlineColor };
    wil::unique_hpen hpen(ExtCreatePen(underlinePenType | PS_GEOMETRIC | PS_ENDCAP_FLAT, underlineWidth, &brushProp, 0, nullptr));

    // Apply the pen.
    const auto prevPen = wil::SelectObject(_hdcMemoryContext, hpen.get());
    RETURN_HR_IF_NULL(E_FAIL, prevPen.get());

    if (lines.test(GridLines::Underline))
    {
        return DrawStrokedLine(ptTarget.x, ptTarget.y + _lineMetrics.underlineCenter, widthOfAllCells);
    }
    else if (lines.test(GridLines::DoubleUnderline))
    {
        RETURN_IF_FAILED(DrawStrokedLine(ptTarget.x, ptTarget.y + _lineMetrics.doubleUnderlinePosTop, widthOfAllCells));
        return DrawStrokedLine(ptTarget.x, ptTarget.y + _lineMetrics.doubleUnderlinePosBottom, widthOfAllCells);
    }
    else if (lines.test(GridLines::CurlyUnderline))
    {
        return DrawCurlyLine(ptTarget.x, ptTarget.y + _lineMetrics.curlyLineCenter, widthOfAllCells);
    }
    else if (lines.test(GridLines::DottedUnderline))
    {
        return DrawStrokedLine(ptTarget.x, ptTarget.y + _lineMetrics.underlineCenter, widthOfAllCells);
    }
    else if (lines.test(GridLines::DashedUnderline))
    {
        return DrawStrokedLine(ptTarget.x, ptTarget.y + _lineMetrics.underlineCenter, widthOfAllCells);
    }

    return S_OK;
}
CATCH_RETURN();

// Routine Description:
// - Draws the cursor on the screen
// Arguments:
// - options - Parameters that affect the way that the cursor is drawn
// Return Value:
// - S_OK, suitable GDI HRESULT error, or safemath error, or E_FAIL in a GDI error where a specific error isn't set.
[[nodiscard]] HRESULT GdiEngine::PaintCursor(const CursorOptions& options) noexcept
{
    // if the cursor is off, do nothing - it should not be visible.
    if (!options.isOn)
    {
        return S_FALSE;
    }
    LOG_IF_FAILED(_FlushBufferLines());

    const auto coordFontSize = _GetFontSize();
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_STATE), coordFontSize.width == 0 || coordFontSize.height == 0);

    // First set up a block cursor the size of the font.
    RECT rcBoundaries;
    rcBoundaries.left = options.coordCursor.x * coordFontSize.width;
    rcBoundaries.top = options.coordCursor.y * coordFontSize.height;
    rcBoundaries.right = rcBoundaries.left + coordFontSize.width;
    rcBoundaries.bottom = rcBoundaries.top + coordFontSize.height;

    // If we're double-width cursor, make it an extra font wider.
    if (options.fIsDoubleWidth)
    {
        rcBoundaries.right = rcBoundaries.right + coordFontSize.width;
    }

    // Make a set of RECTs to paint.
    cursorInvertRects.clear();

    auto rcInvert = rcBoundaries;
    // depending on the cursorType, add rects to that set
    switch (options.cursorType)
    {
    case CursorType::Legacy:
    {
        // Now adjust the cursor height
        // enforce min/max cursor height
        auto ulHeight = options.ulCursorHeightPercent;
        ulHeight = std::max(ulHeight, s_ulMinCursorHeightPercent); // No smaller than 25%
        ulHeight = std::min(ulHeight, s_ulMaxCursorHeightPercent); // No larger than 100%

        ulHeight = MulDiv(coordFontSize.height, ulHeight, 100); // divide by 100 because percent.

        // Reduce the height of the top to be relative to the bottom by the height we want.
        rcInvert.top = rcInvert.bottom - ulHeight;

        cursorInvertRects.push_back(rcInvert);
    }
    break;

    case CursorType::VerticalBar:
        LONG proposedWidth;
        proposedWidth = rcInvert.left + options.cursorPixelWidth;
        // It can't be wider than one cell or we'll have problems in invalidation, so restrict here.
        // It's either the left + the proposed width from the ease of access setting, or
        // it's the right edge of the block cursor as a maximum.
        rcInvert.right = std::min(rcInvert.right, proposedWidth);
        cursorInvertRects.push_back(rcInvert);
        break;

    case CursorType::Underscore:
        rcInvert.top = rcInvert.bottom + -1;
        cursorInvertRects.push_back(rcInvert);
        break;

    case CursorType::DoubleUnderscore:
    {
        RECT top, bottom;
        top = bottom = rcBoundaries;
        bottom.top = bottom.bottom + -1;
        top.top = top.bottom + -3;
        top.bottom = top.top + 1;

        cursorInvertRects.push_back(top);
        cursorInvertRects.push_back(bottom);
    }
    break;

    case CursorType::EmptyBox:
    {
        RECT top, left, right, bottom;
        top = left = right = bottom = rcBoundaries;
        top.bottom = top.top + 1;
        bottom.top = bottom.bottom + -1;
        left.right = left.left + 1;
        right.left = right.right + -1;

        top.left = top.left + 1;
        bottom.left = bottom.left + 1;
        top.right = top.right + -1;
        bottom.right = bottom.right + -1;

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

    // Prepare the appropriate line transform for the current row.
    LOG_IF_FAILED(PrepareLineTransform(options.lineRendition, 0, options.viewportLeft));
    auto resetLineTransform = wil::scope_exit([&]() {
        LOG_IF_FAILED(ResetLineTransform());
    });

    // Either invert all the RECTs, or paint them.
    if (options.fUseColor)
    {
        auto hCursorBrush = CreateSolidBrush(options.cursorColor);
        for (auto r : cursorInvertRects)
        {
            RETURN_HR_IF(E_FAIL, !(FillRect(_hdcMemoryContext, &r, hCursorBrush)));
        }
        DeleteObject(hCursorBrush);
        // Clear out the inverted rects, so that we don't re-invert them next frame.
        cursorInvertRects.clear();
    }
    else
    {
        // Save the current line transform in case we need to reapply these
        // inverted rects to hide the cursor in the ScrollFrame method.
        cursorInvertTransform = _currentLineTransform;

        for (auto r : cursorInvertRects)
        {
            // Make sure the cursor is always readable (see gh-3647)
            const auto PrevObject = SelectObject(_hdcMemoryContext, GetStockObject(LTGRAY_BRUSH));
            const auto Result = PatBlt(_hdcMemoryContext, r.left, r.top, r.right - r.left, r.bottom - r.top, PATINVERT);
            SelectObject(_hdcMemoryContext, PrevObject);
            RETURN_HR_IF(E_FAIL, !Result);
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
[[nodiscard]] HRESULT GdiEngine::PaintSelection(const til::rect& rect) noexcept
{
    LOG_IF_FAILED(_FlushBufferLines());

    const auto pixelRect = rect.scale_up(_GetFontSize()).to_win32_rect();

    RETURN_HR_IF(E_FAIL, !InvertRect(_hdcMemoryContext, &pixelRect));

    return S_OK;
}

[[nodiscard]] HRESULT GdiEngine::PaintSelections(const std::vector<til::rect>& rects) noexcept
{
    UNREFERENCED_PARAMETER(rects);

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
                                       nullptr,
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
//   what is occurring with the in-memory DC.
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
        BitBlt(_debugContext, 0, 0, _szMemorySurface.width, _szMemorySurface.height, _hdcMemoryContext, 0, 0, SRCCOPY);
        Sleep(100);
    }
}
#endif
