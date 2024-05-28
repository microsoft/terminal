// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "gdirenderer.hpp"
#include "../../inc/conattrs.hpp"
#include <winuserp.h> // for GWL_CONSOLE_BKCOLOR
#include "../../interactivity/win32/CustomWindowMessages.h"
#pragma hdrstop

using namespace Microsoft::Console::Render;

// Routine Description:
// - Creates a new GDI-based rendering engine
// - NOTE: Will throw if initialization failure. Caller must catch.
// Arguments:
// - <none>
// Return Value:
// - An instance of a Renderer.
GdiEngine::GdiEngine() :
    _hwndTargetWindow((HWND)INVALID_HANDLE_VALUE),
#if DBG
    _debugWindow((HWND)INVALID_HANDLE_VALUE),
#endif
    _iCurrentDpi(s_iBaseDpi),
    _hbitmapMemorySurface(nullptr),
    _cPolyText(0),
    _fInvalidRectUsed(false),
    _lastFg(INVALID_COLOR),
    _lastBg(INVALID_COLOR),
    _lastFontType(FontType::Undefined),
    _currentLineTransform(IDENTITY_XFORM),
    _currentLineRendition(LineRendition::SingleWidth),
    _fPaintStarted(false),
    _invalidCharacters{},
    _hfont(nullptr),
    _hfontItalic(nullptr),
    _pool{ til::pmr::get_default_resource() }, // It's important the pool is first so it can be given to the others on construction.
    _polyStrings{ &_pool },
    _polyWidths{ &_pool }
{
    ZeroMemory(_pPolyText, sizeof(POLYTEXTW) * s_cPolyTextCache);

    _hdcMemoryContext = CreateCompatibleDC(nullptr);
    THROW_HR_IF_NULL(E_FAIL, _hdcMemoryContext);

    // We need the advanced graphics mode in order to set a transform.
    SetGraphicsMode(_hdcMemoryContext, GM_ADVANCED);

    // On session zero, text GDI APIs might not be ready.
    // Calling GetTextFace causes a wait that will be
    // satisfied while GDI text APIs come online.
    //
    // (Session zero is the non-interactive session
    // where long running services processes are hosted.
    // this increase security and reliability as user
    // applications in interactive session will not be
    // able to interact with services through the common
    // desktop (e.g., window messages)).
    GetTextFaceW(_hdcMemoryContext, 0, nullptr);

#if DBG
    if (_fDebug)
    {
        _CreateDebugWindow();
    }
#endif
}

// Routine Description:
// - Destroys an instance of a GDI-based rendering engine
// Arguments:
// - <none>
// Return Value:
// - <none>
GdiEngine::~GdiEngine()
{
    for (size_t iPoly = 0; iPoly < _cPolyText; iPoly++)
    {
        if (_pPolyText[iPoly].lpstr != nullptr)
        {
            delete[] _pPolyText[iPoly].lpstr;
        }
    }

    if (_hbitmapMemorySurface != nullptr)
    {
        LOG_HR_IF(E_FAIL, !(DeleteObject(_hbitmapMemorySurface)));
        _hbitmapMemorySurface = nullptr;
    }

    if (_hfont != nullptr)
    {
        LOG_HR_IF(E_FAIL, !(DeleteObject(_hfont)));
        _hfont = nullptr;
    }

    if (_hfontItalic != nullptr)
    {
        LOG_HR_IF(E_FAIL, !(DeleteObject(_hfontItalic)));
        _hfontItalic = nullptr;
    }

    if (_hdcMemoryContext != nullptr)
    {
        LOG_HR_IF(E_FAIL, !(DeleteObject(_hdcMemoryContext)));
        _hdcMemoryContext = nullptr;
    }
}

// Routine Description:
// - Updates the window to which this GDI renderer will be bound.
// - A window handle is required for determining the client area and other properties about the rendering surface and monitor.
// Arguments:
// - hwnd - Handle to the window on which we will be drawing.
// Return Value:
// - S_OK if set successfully or relevant GDI error via HRESULT.
[[nodiscard]] HRESULT GdiEngine::SetHwnd(const HWND hwnd) noexcept
{
    // First attempt to get the DC and create an appropriate DC
    const auto hdcRealWindow = GetDC(hwnd);
    RETURN_HR_IF_NULL(E_FAIL, hdcRealWindow);

    const auto hdcNewMemoryContext = CreateCompatibleDC(hdcRealWindow);
    RETURN_HR_IF_NULL(E_FAIL, hdcNewMemoryContext);

    // We need the advanced graphics mode in order to set a transform.
    SetGraphicsMode(hdcNewMemoryContext, GM_ADVANCED);

    // If we had an existing memory context stored, release it before proceeding.
    if (nullptr != _hdcMemoryContext)
    {
        LOG_HR_IF(E_FAIL, !(DeleteObject(_hdcMemoryContext)));
        _hdcMemoryContext = nullptr;
    }

    // Store new window handle and memory context
    _hwndTargetWindow = hwnd;
    _hdcMemoryContext = hdcNewMemoryContext;

    if (nullptr != hdcRealWindow)
    {
        LOG_HR_IF(E_FAIL, !(ReleaseDC(_hwndTargetWindow, hdcRealWindow)));
    }

#if DBG
    if (_debugWindow != INVALID_HANDLE_VALUE && _debugWindow != nullptr)
    {
        RECT rc{};
        THROW_IF_WIN32_BOOL_FALSE(GetWindowRect(_hwndTargetWindow, &rc));

        THROW_IF_WIN32_BOOL_FALSE(SetWindowPos(_debugWindow, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE));
    }
#endif

    return S_OK;
}

// Routine Description:
// - This routine will help call SetWindowLongW with the correct semantics to retrieve the appropriate error code.
// Arguments:
// - hWnd - Window handle to use for setting
// - nIndex - Window handle item offset
// - dwNewLong - Value to update in window structure
// Return Value:
// - S_OK or converted HRESULT from last Win32 error from SetWindowLongW
[[nodiscard]] HRESULT GdiEngine::s_SetWindowLongWHelper(const HWND hWnd, const int nIndex, const LONG dwNewLong) noexcept
{
    // SetWindowLong has strange error handling. On success, it returns the previous Window Long value and doesn't modify the Last Error state.
    // To deal with this, we set the last error to 0/S_OK first, call it, and if the previous long was 0, we check if the error was non-zero before reporting.
    // Otherwise, we'll get an "Error: The operation has completed successfully." and there will be another screenshot on the internet making fun of Windows.
    // See: https://msdn.microsoft.com/en-us/library/windows/desktop/ms633591(v=vs.85).aspx
    SetLastError(0);
    const auto lResult = SetWindowLongW(hWnd, nIndex, dwNewLong);
    if (0 == lResult)
    {
        RETURN_LAST_ERROR_IF(0 != GetLastError());
    }

    return S_OK;
}

// Routine Description
// - Resets the world transform to the identity matrix.
// Arguments:
// - <none>
// Return Value:
// - S_OK if successful. S_FALSE if already reset. E_FAIL if there was an error.
[[nodiscard]] HRESULT GdiEngine::ResetLineTransform() noexcept
{
    // Return early if the current transform is already the identity matrix.
    RETURN_HR_IF(S_FALSE, _currentLineTransform == IDENTITY_XFORM);
    // Flush any buffer lines which would be expecting to use the current transform.
    LOG_IF_FAILED(_FlushBufferLines());
    // Reset the active transform to the identity matrix.
    RETURN_HR_IF(E_FAIL, !ModifyWorldTransform(_hdcMemoryContext, nullptr, MWT_IDENTITY));
    // Reset the current state.
    _currentLineTransform = IDENTITY_XFORM;
    _currentLineRendition = LineRendition::SingleWidth;
    return S_OK;
}

// Routine Description
// - Applies an appropriate transform for the given line rendition and viewport offset.
// Arguments:
// - lineRendition - The line rendition specifying the scaling of the line.
// - targetRow - The row on which the line is expected to be rendered.
// - viewportLeft - The left offset of the current viewport.
// Return Value:
// - S_OK if successful. S_FALSE if already set. E_FAIL if there was an error.
[[nodiscard]] HRESULT GdiEngine::PrepareLineTransform(const LineRendition lineRendition,
                                                      const til::CoordType targetRow,
                                                      const til::CoordType viewportLeft) noexcept
{
    XFORM lineTransform = {};
    // The X delta is to account for the horizontal viewport offset.
    lineTransform.eDx = viewportLeft ? -1.0f * viewportLeft * _GetFontSize().width : 0.0f;
    switch (lineRendition)
    {
    case LineRendition::SingleWidth:
        lineTransform.eM11 = 1; // single width
        lineTransform.eM22 = 1; // single height
        break;
    case LineRendition::DoubleWidth:
        lineTransform.eM11 = 2; // double width
        lineTransform.eM22 = 1; // single height
        break;
    case LineRendition::DoubleHeightTop:
        lineTransform.eM11 = 2; // double width
        lineTransform.eM22 = 2; // double height
        // The Y delta is to negate the offset caused by the scaled height.
        lineTransform.eDy = -1.0f * targetRow * _GetFontSize().height;
        break;
    case LineRendition::DoubleHeightBottom:
        lineTransform.eM11 = 2; // double width
        lineTransform.eM22 = 2; // double height
        // The Y delta is to negate the offset caused by the scaled height.
        // An extra row is added because we need the bottom half of the line.
        lineTransform.eDy = -1.0f * (targetRow + 1) * _GetFontSize().height;
        break;
    }
    // Return early if the new matrix is the same as the current transform.
    RETURN_HR_IF(S_FALSE, _currentLineRendition == lineRendition && _currentLineTransform == lineTransform);
    // Flush any buffer lines which would be expecting to use the current transform.
    LOG_IF_FAILED(_FlushBufferLines());
    // Set the active transform with the new matrix.
    RETURN_HR_IF(E_FAIL, !SetWorldTransform(_hdcMemoryContext, &lineTransform));
    // Save the current state.
    _currentLineTransform = lineTransform;
    _currentLineRendition = lineRendition;
    return S_OK;
}

// Routine Description:
// - This method will set the GDI brushes in the drawing context (and update the hung-window background color)
// Arguments:
// - textAttributes - Text attributes to use for the brush color
// - renderSettings - The color table and modes required for rendering
// - pData - The interface to console data structures required for rendering
// - usingSoftFont - Whether we're rendering characters from a soft font
// - isSettingDefaultBrushes - Lets us know that the default brushes are being set so we can update the DC background
//                             and the hung app background painting color
// Return Value:
// - S_OK if set successfully or relevant GDI error via HRESULT.
[[nodiscard]] HRESULT GdiEngine::UpdateDrawingBrushes(const TextAttribute& textAttributes,
                                                      const RenderSettings& renderSettings,
                                                      const gsl::not_null<IRenderData*> /*pData*/,
                                                      const bool usingSoftFont,
                                                      const bool isSettingDefaultBrushes) noexcept
{
    RETURN_IF_FAILED(_FlushBufferLines());

    RETURN_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_INVALID_STATE), _hdcMemoryContext);

    // Set the colors for painting text
    const auto [colorForeground, colorBackground] = renderSettings.GetAttributeColors(textAttributes);

    if (colorForeground != _lastFg)
    {
        RETURN_HR_IF(E_FAIL, CLR_INVALID == SetTextColor(_hdcMemoryContext, colorForeground));
        _lastFg = colorForeground;
    }
    if (colorBackground != _lastBg)
    {
        RETURN_HR_IF(E_FAIL, CLR_INVALID == SetBkColor(_hdcMemoryContext, colorBackground));
        _lastBg = colorBackground;
    }

    if (isSettingDefaultBrushes)
    {
        // Set the color for painting the extra DC background area
        RETURN_HR_IF(E_FAIL, CLR_INVALID == SetDCBrushColor(_hdcMemoryContext, colorBackground));

        // Set the hung app background painting color
        RETURN_IF_FAILED(s_SetWindowLongWHelper(_hwndTargetWindow, GWL_CONSOLE_BKCOLOR, colorBackground));
    }

    // If the font type has changed, select an appropriate font variant or soft font.
    const auto usingItalicFont = textAttributes.IsItalic();
    const auto fontType = usingSoftFont   ? FontType::Soft :
                          usingItalicFont ? FontType::Italic :
                                            FontType::Default;
    if (fontType != _lastFontType)
    {
        switch (fontType)
        {
        case FontType::Soft:
            SelectFont(_hdcMemoryContext, _softFont);
            break;
        case FontType::Italic:
            SelectFont(_hdcMemoryContext, _hfontItalic);
            break;
        case FontType::Default:
        default:
            SelectFont(_hdcMemoryContext, _hfont);
            break;
        }
        _lastFontType = fontType;
        _fontHasWesternScript = FontHasWesternScript(_hdcMemoryContext);
    }

    return S_OK;
}

// Routine Description:
// - This method will update the active font on the current device context
// - NOTE: It is left up to the underling rendering system to choose the nearest font. Please ask for the font dimensions if they are required using the interface. Do not use the size you requested with this structure.
// Arguments:
// - FontDesired - reference to font information we should use while instantiating a font.
// - Font - reference to font information where the chosen font information will be populated.
// Return Value:
// - S_OK if set successfully or relevant GDI error via HRESULT.
[[nodiscard]] HRESULT GdiEngine::UpdateFont(const FontInfoDesired& FontDesired, _Out_ FontInfo& Font) noexcept
{
    wil::unique_hfont hFont, hFontItalic;
    RETURN_IF_FAILED(_GetProposedFont(FontDesired, Font, _iCurrentDpi, hFont, hFontItalic));

    // Select into DC
    RETURN_HR_IF_NULL(E_FAIL, SelectFont(_hdcMemoryContext, hFont.get()));

    // Save off the font metrics for various other calculations
    RETURN_HR_IF(E_FAIL, !(GetTextMetricsW(_hdcMemoryContext, &_tmFontMetrics)));

    // There is no font metric for the grid line width, so we use a small
    // multiple of the font size, which typically rounds to a pixel.
    const auto cellHeight = static_cast<float>(Font.GetSize().height);
    const auto fontSize = static_cast<float>(_tmFontMetrics.tmHeight - _tmFontMetrics.tmInternalLeading);
    const auto baseline = static_cast<float>(_tmFontMetrics.tmAscent);
    float idealGridlineWidth = std::max(1.0f, fontSize * 0.025f);
    float idealUnderlineTop = 0;
    float idealUnderlineWidth = 0;
    float idealStrikethroughTop = 0;
    float idealStrikethroughWidth = 0;

    OUTLINETEXTMETRICW outlineMetrics;
    if (GetOutlineTextMetricsW(_hdcMemoryContext, sizeof(outlineMetrics), &outlineMetrics))
    {
        // For TrueType fonts, the other line metrics can be obtained from
        // the font's outline text metric structure.
        idealUnderlineTop = static_cast<float>(baseline - outlineMetrics.otmsUnderscorePosition);
        idealUnderlineWidth = static_cast<float>(outlineMetrics.otmsUnderscoreSize);
        idealStrikethroughWidth = static_cast<float>(outlineMetrics.otmsStrikeoutSize);
        idealStrikethroughTop = static_cast<float>(baseline - outlineMetrics.otmsStrikeoutPosition);
    }
    else
    {
        // If we can't obtain the outline metrics for the font, we just pick some reasonable values for the offsets and widths.
        idealUnderlineTop = std::max(1.0f, roundf(baseline - fontSize * 0.05f));
        idealUnderlineWidth = idealGridlineWidth;
        idealStrikethroughTop = std::max(1.0f, roundf(baseline * (2.0f / 3.0f)));
        idealStrikethroughWidth = idealGridlineWidth;
    }

    // GdiEngine::PaintBufferGridLines paints underlines using HPEN and LineTo, etc., which draws lines centered on the given coordinates.
    // This means we need to shift the limit (cellHeight - underlineWidth) and offset (idealUnderlineTop) by half the width.
    const auto underlineWidth = std::max(1.0f, roundf(idealUnderlineWidth));
    const auto underlineCenter = std::min(floorf(cellHeight - underlineWidth / 2.0f), roundf(idealUnderlineTop + underlineWidth / 2.0f));

    const auto strikethroughWidth = std::max(1.0f, roundf(idealStrikethroughWidth));
    const auto strikethroughOffset = std::min(cellHeight - strikethroughWidth, roundf(idealStrikethroughTop));

    // For double underlines we loosely follow what Word does:
    // 1. The lines are half the width of an underline
    // 2. Ideally the bottom line is aligned with the bottom of the underline
    // 3. The top underline is vertically in the middle between baseline and ideal bottom underline
    // 4. If the top line gets too close to the baseline the underlines are shifted downwards
    // 5. The minimum gap between the two lines appears to be similar to Tex (1.2pt)
    // (Additional notes below.)

    // 1.
    const auto doubleUnderlineWidth = std::max(1.0f, roundf(idealUnderlineWidth / 2.0f));
    // 2.
    auto doubleUnderlinePosBottom = underlineCenter + underlineWidth - doubleUnderlineWidth;
    // 3. Since we don't align the center of our two lines, but rather the top borders
    //    we need to subtract half a line width from our center point.
    auto doubleUnderlinePosTop = roundf((baseline + doubleUnderlinePosBottom - doubleUnderlineWidth) / 2.0f);
    // 4.
    doubleUnderlinePosTop = std::max(doubleUnderlinePosTop, baseline + doubleUnderlineWidth);
    // 5. The gap is only the distance _between_ the lines, but we need the distance from the
    //    top border of the top and bottom lines, which includes an additional line width.
    const auto doubleUnderlineGap = std::max(1.0f, roundf(1.2f / 72.0f * _iCurrentDpi));
    doubleUnderlinePosBottom = std::max(doubleUnderlinePosBottom, doubleUnderlinePosTop + doubleUnderlineGap + doubleUnderlineWidth);
    // Our cells can't overlap each other so we additionally clamp the bottom line to be inside the cell boundaries.
    doubleUnderlinePosBottom = std::min(doubleUnderlinePosBottom, cellHeight - doubleUnderlineWidth);

    // The wave line is drawn using a cubic Bézier curve (PolyBezier), because that happens to be cheap with GDI.
    // We use a Bézier curve where, if the start (a) and end (c) points are at (0,0) and (1,0), the control points are
    // at (0.5,0.5) (b) and (0.5,-0.5) (d) respectively. Like this but a/b/c/d are square and the lines are round:
    //
    //       b
    //
    //     ^
    //    / \                here's some text so the compiler ignores the trailing \ character
    //   a   \   c
    //        \ /
    //         v
    //
    //       d
    //
    // If you punch x=0.25 into the cubic bezier formula you get y=0.140625. This constant is
    // important to us because it (plus the line width) tells us the amplitude of the wave.
    //
    // We can use the inverse of the constant to figure out how many px one period of the wave has to be to end up being 1px tall.
    // In our case we want the amplitude of the wave to have a peak-to-peak amplitude that matches our double-underline.
    const auto doubleUnderlineHalfDistance = 0.5f * (doubleUnderlinePosBottom - doubleUnderlinePosTop);
    const auto doubleUnderlineCenter = doubleUnderlinePosTop + doubleUnderlineHalfDistance;
    const auto curlyLineIdealAmplitude = std::max(1.0f, doubleUnderlineHalfDistance);
    // Since GDI can't deal with fractional pixels, we first calculate the control point offsets (0.5 and -0.5) by multiplying by 0.5 and
    // then undo that by multiplying by 2.0 for the period. This ensures that our control points can be at curlyLinePeriod/2, an integer.
    const auto curlyLineControlPointOffset = roundf(curlyLineIdealAmplitude * (1.0f / 0.140625f) * 0.5f);
    const auto curlyLinePeriod = curlyLineControlPointOffset * 2.0f;
    // We can reverse the above to get back the actual amplitude of our Bézier curve. The line
    // will be drawn with a width of doubleUnderlineWidth in the center of the curve (= 0.5x padding).
    const auto curlyLineAmplitude = 0.140625f * curlyLinePeriod + 0.5f * doubleUnderlineWidth;
    // To make the wavy line with its double-underline amplitude look consistent with the double-underline we position it at its center.
    const auto curlyLineOffset = std::min(roundf(doubleUnderlineCenter), floorf(cellHeight - curlyLineAmplitude));

    _lineMetrics.gridlineWidth = lroundf(idealGridlineWidth);
    _lineMetrics.doubleUnderlineWidth = lroundf(doubleUnderlineWidth);
    _lineMetrics.underlineCenter = lroundf(underlineCenter);
    _lineMetrics.underlineWidth = lroundf(underlineWidth);
    _lineMetrics.doubleUnderlinePosTop = lroundf(doubleUnderlinePosTop);
    _lineMetrics.doubleUnderlinePosBottom = lroundf(doubleUnderlinePosBottom);
    _lineMetrics.strikethroughOffset = lroundf(strikethroughOffset);
    _lineMetrics.strikethroughWidth = lroundf(strikethroughWidth);
    _lineMetrics.curlyLineCenter = lroundf(curlyLineOffset);
    _lineMetrics.curlyLinePeriod = lroundf(curlyLinePeriod);
    _lineMetrics.curlyLineControlPointOffset = lroundf(curlyLineControlPointOffset);

    // Now find the size of a 0 in this current font and save it for conversions done later.
    _coordFontLast = Font.GetSize();

    // Persist font for cleanup (and free existing if necessary)
    if (_hfont != nullptr)
    {
        LOG_HR_IF(E_FAIL, !(DeleteObject(_hfont)));
        _hfont = nullptr;
    }

    // Save the font.
    _hfont = hFont.release();

    // Persist italic font for cleanup (and free existing if necessary)
    if (_hfontItalic != nullptr)
    {
        LOG_HR_IF(E_FAIL, !(DeleteObject(_hfontItalic)));
        _hfontItalic = nullptr;
    }

    // Save the italic font.
    _hfontItalic = hFontItalic.release();

    // Save raster vs. TrueType and codepage data in case we need to convert.
    _isTrueTypeFont = Font.IsTrueTypeFont();
    _fontCodepage = Font.GetCodePage();

    // Inform the soft font of the change in size.
    _softFont.SetTargetSize(_GetFontSize());

    LOG_IF_FAILED(InvalidateAll());

    return S_OK;
}

// Routine Description:
// - This method will replace the active soft font with the given bit pattern.
// Arguments:
// - bitPattern - An array of scanlines representing all the glyphs in the font.
// - cellSize - The cell size for an individual glyph.
// - centeringHint - The horizontal extent that glyphs are offset from center.
// Return Value:
// - S_OK if successful. E_FAIL if there was an error.
[[nodiscard]] HRESULT GdiEngine::UpdateSoftFont(const std::span<const uint16_t> bitPattern,
                                                const til::size cellSize,
                                                const size_t centeringHint) noexcept
{
    // If we previously called SelectFont(_hdcMemoryContext, _softFont), it will
    // still hold a reference to the _softFont object we're planning to overwrite.
    // --> First revert back to the standard _hfont, lest we have dangling pointers.
    if (_lastFontType == FontType::Soft)
    {
        RETURN_HR_IF_NULL(E_FAIL, SelectFont(_hdcMemoryContext, _hfont));
        _lastFontType = FontType::Default;
    }

    // Create a new font resource with the updated pattern, or delete if empty.
    _softFont = FontResource{ bitPattern, cellSize, _GetFontSize(), centeringHint };

    return S_OK;
}

// Routine Description:
// - This method will modify the DPI we're using for scaling calculations.
// Arguments:
// - iDpi - The Dots Per Inch to use for scaling. We will use this relative to the system default DPI defined in Windows headers as a constant.
// Return Value:
// - HRESULT S_OK, GDI-based error code, or safemath error
[[nodiscard]] HRESULT GdiEngine::UpdateDpi(const int iDpi) noexcept
{
    _iCurrentDpi = iDpi;
    return S_OK;
}

// Method Description:
// - This method will update our internal reference for how big the viewport is.
//      Does nothing for GDI.
// Arguments:
// - srNewViewport - The bounds of the new viewport.
// Return Value:
// - HRESULT S_OK
[[nodiscard]] HRESULT GdiEngine::UpdateViewport(const til::inclusive_rect& /*srNewViewport*/) noexcept
{
    return S_OK;
}

// Routine Description:
// - This method will figure out what the new font should be given the starting font information and a DPI.
// - When the final font is determined, the FontInfo structure given will be updated with the actual resulting font chosen as the nearest match.
// - NOTE: It is left up to the underling rendering system to choose the nearest font. Please ask for the font dimensions if they are required using the interface. Do not use the size you requested with this structure.
// - If the intent is to immediately turn around and use this font, pass the optional handle parameter and use it immediately.
// Arguments:
// - FontDesired - reference to font information we should use while instantiating a font.
// - Font - reference to font information where the chosen font information will be populated.
// - iDpi - The DPI we will have when rendering
// Return Value:
// - S_OK if set successfully or relevant GDI error via HRESULT.
[[nodiscard]] HRESULT GdiEngine::GetProposedFont(const FontInfoDesired& FontDesired, _Out_ FontInfo& Font, const int iDpi) noexcept
{
    wil::unique_hfont hFont, hFontItalic;
    return _GetProposedFont(FontDesired, Font, iDpi, hFont, hFontItalic);
}

// Method Description:
// - Updates the window's title string. For GDI, this does nothing, because the
//      title must be updated on the main window's windowproc thread.
// Arguments:
// - newTitle: the new string to use for the title of the window
// Return Value:
// -  S_OK if PostMessageW succeeded, otherwise E_FAIL
[[nodiscard]] HRESULT GdiEngine::_DoUpdateTitle(_In_ const std::wstring_view /*newTitle*/) noexcept
{
    // the CM_UPDATE_TITLE handler in windowproc will query the updated title.
    return PostMessageW(_hwndTargetWindow, CM_UPDATE_TITLE, 0, (LPARAM) nullptr) ? S_OK : E_FAIL;
}

// Routine Description:
// - This method will figure out what the new font should be given the starting font information and a DPI.
// - When the final font is determined, the FontInfo structure given will be updated with the actual resulting font chosen as the nearest match.
// - NOTE: It is left up to the underling rendering system to choose the nearest font. Please ask for the font dimensions if they are required using the interface. Do not use the size you requested with this structure.
// - If the intent is to immediately turn around and use this font, pass the optional handle parameter and use it immediately.
// Arguments:
// - FontDesired - reference to font information we should use while instantiating a font.
// - Font - the actual font
// - iDpi - The DPI we will have when rendering
// - hFont - A smart pointer to receive a handle to a ready-to-use GDI font.
// - hFontItalic - A smart pointer to receive a handle to an italic variant of the font.
// Return Value:
// - S_OK if set successfully or relevant GDI error via HRESULT.
[[nodiscard]] HRESULT GdiEngine::_GetProposedFont(const FontInfoDesired& FontDesired,
                                                  _Out_ FontInfo& Font,
                                                  const int iDpi,
                                                  _Inout_ wil::unique_hfont& hFont,
                                                  _Inout_ wil::unique_hfont& hFontItalic) noexcept
{
    wil::unique_hdc hdcTemp(CreateCompatibleDC(_hdcMemoryContext));
    RETURN_HR_IF_NULL(E_FAIL, hdcTemp.get());

    // Get a special engine size because TT fonts can't specify X or we'll get weird scaling under some circumstances.
    auto coordFontRequested = FontDesired.GetEngineSize();

    // First, check to see if we're asking for the default raster font.
    if (FontDesired.IsDefaultRasterFont())
    {
        // We're being asked for the default raster font, which gets special handling. In particular, it's the font
        // returned by GetStockObject(OEM_FIXED_FONT).
        // We do this because, for instance, if we ask GDI for an 8x12 OEM_FIXED_FONT,
        // it may very well decide to choose Courier New instead of the Terminal raster.
#pragma prefast(suppress : 38037, "raster fonts get special handling, we need to get it this way")
        hFont.reset((HFONT)GetStockObject(OEM_FIXED_FONT));
        hFontItalic.reset((HFONT)GetStockObject(OEM_FIXED_FONT));
    }
    else
    {
        // For future reference, here is the engine weighting and internal details on Windows Font Mapping:
        // https://msdn.microsoft.com/en-us/library/ms969909.aspx
        // More relevant links:
        // https://support.microsoft.com/en-us/kb/94646

        // IMPORTANT: Be very careful when modifying the values being passed in below. Even the slightest change can cause
        // GDI to return a font other than the one being requested. If you must change the below for any reason, make sure
        // these fonts continue to work correctly, as they've been known to break:
        //       * Monofur
        //       * Iosevka Extralight
        //
        // While you're at it, make sure that the behavior matches what happens in the Fonts property sheet. Pay very close
        // attention to the font previews to ensure that the font being selected by GDI is exactly the font requested --
        // some monospace fonts look very similar.
        LOGFONTW lf = { 0 };
        lf.lfHeight = s_ScaleByDpi(coordFontRequested.height, iDpi);
        lf.lfWidth = s_ScaleByDpi(coordFontRequested.width, iDpi);
        lf.lfWeight = FontDesired.GetWeight();

        // If we're searching for Terminal, our supported Raster Font, then we must use OEM_CHARSET.
        // If the System's Non-Unicode Setting is set to English (United States) which is 437
        // and we try to enumerate Terminal with the console codepage as 932, that will turn into SHIFTJIS_CHARSET.
        // Despite C:\windows\fonts\vga932.fon always being present, GDI will refuse to load the Terminal font
        // that doesn't correspond to the current System Non-Unicode Setting. It will then fall back to a TrueType
        // font that does support the SHIFTJIS_CHARSET (because Terminal with CP 437 a.k.a. C:\windows\fonts\vgaoem.fon does NOT support it.)
        // This is OK for display purposes (things will render properly) but not OK for API purposes.
        // Because the API is affected by the raster/TT status of the actively selected font, we can't have
        // GDI choosing a TT font for us when we ask for Raster. We have to settle for forcing the current system
        // Terminal font to load even if it doesn't have the glyphs necessary such that the APIs continue to work fine.
        if (FontDesired.GetFaceName() == DEFAULT_RASTER_FONT_FACENAME)
        {
            lf.lfCharSet = OEM_CHARSET;
        }
        else
        {
            CHARSETINFO csi;
            if (!TranslateCharsetInfo((DWORD*)IntToPtr(FontDesired.GetCodePage()), &csi, TCI_SRCCODEPAGE))
            {
                // if we failed to translate from codepage to charset, choose our charset depending on what kind of font we're
                // dealing with. Raster Fonts need to be presented with the OEM charset, while TT fonts need to be ANSI.
                csi.ciCharset = FontDesired.IsTrueTypeFont() ? ANSI_CHARSET : OEM_CHARSET;
            }

            lf.lfCharSet = (BYTE)csi.ciCharset;
        }

        lf.lfQuality = DRAFT_QUALITY;

        // NOTE: not using what GDI gave us because some fonts don't quite roundtrip (e.g. MS Gothic and VL Gothic)
        lf.lfPitchAndFamily = (FIXED_PITCH | FF_MODERN);

        FontDesired.FillLegacyNameBuffer(lf.lfFaceName);

        // Create font.
        hFont.reset(CreateFontIndirectW(&lf));
        RETURN_HR_IF_NULL(E_FAIL, hFont.get());

        // Create italic variant of the font.
        lf.lfItalic = TRUE;
        hFontItalic.reset(CreateFontIndirectW(&lf));
        RETURN_HR_IF_NULL(E_FAIL, hFontItalic.get());
    }

    // Select into DC
    wil::unique_hfont hFontOld(SelectFont(hdcTemp.get(), hFont.get()));
    RETURN_HR_IF_NULL(E_FAIL, hFontOld.get());

    // Save off the font metrics for various other calculations
    TEXTMETRICW tm;
    RETURN_HR_IF(E_FAIL, !(GetTextMetricsW(hdcTemp.get(), &tm)));

    // Now find the size of a 0 in this current font and save it for conversions done later.
    SIZE sz;
    RETURN_HR_IF(E_FAIL, !(GetTextExtentPoint32W(hdcTemp.get(), L"0", 1, &sz)));

    til::size coordFont;
    coordFont.width = sz.cx;
    coordFont.height = sz.cy;

    // The extent point won't necessarily be perfect for the width, so get the ABC metrics for the 0 if possible to improve the measurement.
    // This will fail for non-TrueType fonts and we'll fall back to what GetTextExtentPoint said.
    {
        ABC abc;
        if (0 != GetCharABCWidthsW(hdcTemp.get(), '0', '0', &abc))
        {
            const auto abcTotal = abc.abcA + abc.abcB + abc.abcC;

            // No negatives or zeros or we'll have bad character-to-pixel math later.
            if (abcTotal > 0)
            {
                coordFont.width = abcTotal;
            }
        }
    }

    // Now fill up the FontInfo we were passed with the full details of which font we actually chose
    {
        // Get the actual font face that we chose
        const auto faceNameLength{ gsl::narrow<size_t>(GetTextFaceW(hdcTemp.get(), 0, nullptr)) };

        std::wstring currentFaceName{};
        currentFaceName.resize(faceNameLength);

        RETURN_HR_IF(E_FAIL, !(GetTextFaceW(hdcTemp.get(), gsl::narrow_cast<int>(faceNameLength), currentFaceName.data())));

        currentFaceName.resize(faceNameLength - 1); // remove the null terminator (wstring!)

        if (FontDesired.IsDefaultRasterFont())
        {
            coordFontRequested = coordFont;
        }
        else if (coordFontRequested.width == 0)
        {
            coordFontRequested.width = s_ShrinkByDpi(coordFont.width, iDpi);
        }

        Font.SetFromEngine(currentFaceName,
                           tm.tmPitchAndFamily,
                           gsl::narrow_cast<unsigned int>(tm.tmWeight),
                           FontDesired.IsDefaultRasterFont(),
                           coordFont,
                           coordFontRequested);
    }

    return S_OK;
}

// Routine Description:
// - Retrieves the current pixel size of the font we have selected for drawing.
// Arguments:
// - pFontSize - receives the current X by Y size of the font.
// Return Value:
// - S_OK
[[nodiscard]] HRESULT GdiEngine::GetFontSize(_Out_ til::size* pFontSize) noexcept
{
    *pFontSize = _GetFontSize();
    return S_OK;
}

// Routine Description:
// - Retrieves the current pixel size of the font we have selected for drawing.
// Arguments:
// - <none>
// Return Value:
// - X by Y size of the font.
til::size GdiEngine::_GetFontSize() const
{
    return _coordFontLast;
}

// Routine Description:
// - Retrieves whether or not the window is currently minimized.
// Arguments:
// - <none>
// Return Value:
// - True if minimized (don't need to draw anything). False otherwise.
bool GdiEngine::_IsMinimized() const
{
    return !!IsIconic(_hwndTargetWindow);
}

// Routine Description:
// - Determines whether or not we have a TrueType font selected.
// - Intended only for determining whether we need to perform special raster font scaling.
// Arguments:
// - <none>
// Return Value:
// - True if TrueType. False otherwise (and generally assumed to be raster font type.)
bool GdiEngine::_IsFontTrueType() const
{
    return !!(_tmFontMetrics.tmPitchAndFamily & TMPF_TRUETYPE);
}

// Routine Description:
// - Helper to determine whether our window handle is valid.
//   Allows us to skip operations if we don't have a window.
// Return Value:
// - True if it is valid.
// - False if it is known invalid.
bool GdiEngine::_IsWindowValid() const
{
    return _hwndTargetWindow != INVALID_HANDLE_VALUE &&
           _hwndTargetWindow != nullptr;
}
