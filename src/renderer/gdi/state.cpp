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
    _fPaintStarted(false),
    _hfont((HFONT)INVALID_HANDLE_VALUE)
{
    ZeroMemory(_pPolyText, sizeof(POLYTEXTW) * s_cPolyTextCache);
    _rcInvalid = { 0 };
    _szInvalidScroll = { 0 };
    _szMemorySurface = { 0 };

    _hdcMemoryContext = CreateCompatibleDC(nullptr);
    THROW_HR_IF_NULL(E_FAIL, _hdcMemoryContext);

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
    HDC const hdcRealWindow = GetDC(hwnd);
    RETURN_HR_IF_NULL(E_FAIL, hdcRealWindow);

    HDC const hdcNewMemoryContext = CreateCompatibleDC(hdcRealWindow);
    RETURN_HR_IF_NULL(E_FAIL, hdcNewMemoryContext);

    // If we had an existing memory context stored, release it before proceeding.
    if (nullptr != _hdcMemoryContext)
    {
        LOG_HR_IF(E_FAIL, !(DeleteObject(_hdcMemoryContext)));
        _hdcMemoryContext = nullptr;
    }

    // Store new window handle and memory context
    _hwndTargetWindow = hwnd;
    _hdcMemoryContext = hdcNewMemoryContext;

    // If we have a font, apply it to the context.
    if (nullptr != _hfont)
    {
        LOG_HR_IF_NULL(E_FAIL, SelectFont(_hdcMemoryContext, _hfont));
    }

    if (nullptr != hdcRealWindow)
    {
        LOG_HR_IF(E_FAIL, !(ReleaseDC(_hwndTargetWindow, hdcRealWindow)));
    }

#if DBG
    if (_debugWindow != INVALID_HANDLE_VALUE && _debugWindow != 0)
    {
        RECT rc = { 0 };
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
    LONG const lResult = SetWindowLongW(hWnd, nIndex, dwNewLong);
    if (0 == lResult)
    {
        RETURN_LAST_ERROR_IF(0 != GetLastError());
    }

    return S_OK;
}

// Routine Description:
// - This method will set the GDI brushes in the drawing context (and update the hung-window background color)
// Arguments:
// - colorForeground - Foreground Color
// - colorBackground - Background colo
// - legacyColorAttribute - <unused>
// - extendedAttrs - <unused>
// - isSettingDefaultBrushes - Lets us know that the default brushes are being set so we can update the DC background
//                             and the hung app background painting color
// Return Value:
// - S_OK if set successfully or relevant GDI error via HRESULT.
[[nodiscard]] HRESULT GdiEngine::UpdateDrawingBrushes(const COLORREF colorForeground,
                                                      const COLORREF colorBackground,
                                                      const WORD /*legacyColorAttribute*/,
                                                      const ExtendedAttributes /*extendedAttrs*/,
                                                      const bool isSettingDefaultBrushes) noexcept
{
    RETURN_IF_FAILED(_FlushBufferLines());

    RETURN_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_INVALID_STATE), _hdcMemoryContext);

    // Set the colors for painting text
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
    wil::unique_hfont hFont;
    RETURN_IF_FAILED(_GetProposedFont(FontDesired, Font, _iCurrentDpi, hFont));

    // Select into DC
    RETURN_HR_IF_NULL(E_FAIL, SelectFont(_hdcMemoryContext, hFont.get()));

    // Save off the font metrics for various other calculations
    RETURN_HR_IF(E_FAIL, !(GetTextMetricsW(_hdcMemoryContext, &_tmFontMetrics)));

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

    // Save raster vs. TrueType and codepage data in case we need to convert.
    _isTrueTypeFont = Font.IsTrueTypeFont();
    _fontCodepage = Font.GetCodePage();

    LOG_IF_FAILED(InvalidateAll());

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
[[nodiscard]] HRESULT GdiEngine::UpdateViewport(const SMALL_RECT /*srNewViewport*/) noexcept
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
    wil::unique_hfont hFont;
    return _GetProposedFont(FontDesired, Font, iDpi, hFont);
}

// Method Description:
// - Updates the window's title string. For GDI, this does nothing, because the
//      title must be updated on the main window's windowproc thread.
// Arguments:
// - newTitle: the new string to use for the title of the window
// Return Value:
// -  S_OK if PostMessageW succeeded, otherwise E_FAIL
[[nodiscard]] HRESULT GdiEngine::_DoUpdateTitle(_In_ const std::wstring& /*newTitle*/) noexcept
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
// Return Value:
// - S_OK if set successfully or relevant GDI error via HRESULT.
[[nodiscard]] HRESULT GdiEngine::_GetProposedFont(const FontInfoDesired& FontDesired,
                                                  _Out_ FontInfo& Font,
                                                  const int iDpi,
                                                  _Inout_ wil::unique_hfont& hFont) noexcept
{
    wil::unique_hdc hdcTemp(CreateCompatibleDC(_hdcMemoryContext));
    RETURN_HR_IF_NULL(E_FAIL, hdcTemp.get());

    // Get a special engine size because TT fonts can't specify X or we'll get weird scaling under some circumstances.
    COORD coordFontRequested = FontDesired.GetEngineSize();

    // First, check to see if we're asking for the default raster font.
    if (FontDesired.IsDefaultRasterFont())
    {
        // We're being asked for the default raster font, which gets special handling. In particular, it's the font
        // returned by GetStockObject(OEM_FIXED_FONT).
        // We do this because, for instance, if we ask GDI for an 8x12 OEM_FIXED_FONT,
        // it may very well decide to choose Courier New instead of the Terminal raster.
#pragma prefast(suppress : 38037, "raster fonts get special handling, we need to get it this way")
        hFont.reset((HFONT)GetStockObject(OEM_FIXED_FONT));
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
        lf.lfHeight = s_ScaleByDpi(coordFontRequested.Y, iDpi);
        lf.lfWidth = s_ScaleByDpi(coordFontRequested.X, iDpi);
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

        RETURN_IF_FAILED(FontDesired.FillLegacyNameBuffer(gsl::make_span(lf.lfFaceName)));

        // Create font.
        hFont.reset(CreateFontIndirectW(&lf));
        RETURN_HR_IF_NULL(E_FAIL, hFont.get());
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

    COORD coordFont;
    coordFont.X = static_cast<SHORT>(sz.cx);
    coordFont.Y = static_cast<SHORT>(sz.cy);

    // The extent point won't necessarily be perfect for the width, so get the ABC metrics for the 0 if possible to improve the measurement.
    // This will fail for non-TrueType fonts and we'll fall back to what GetTextExtentPoint said.
    {
        ABC abc;
        if (0 != GetCharABCWidthsW(hdcTemp.get(), '0', '0', &abc))
        {
            int const abcTotal = abc.abcA + abc.abcB + abc.abcC;

            // No negatives or zeros or we'll have bad character-to-pixel math later.
            if (abcTotal > 0)
            {
                coordFont.X = static_cast<SHORT>(abcTotal);
            }
        }
    }

    // Now fill up the FontInfo we were passed with the full details of which font we actually chose
    {
        // Get the actual font face that we chose
        const size_t faceNameLength{ gsl::narrow<size_t>(GetTextFaceW(hdcTemp.get(), 0, nullptr)) };

        std::wstring currentFaceName{};
        currentFaceName.resize(faceNameLength);

        RETURN_HR_IF(E_FAIL, !(GetTextFaceW(hdcTemp.get(), gsl::narrow_cast<int>(faceNameLength), currentFaceName.data())));

        currentFaceName.resize(faceNameLength - 1); // remove the null terminator (wstring!)

        if (FontDesired.IsDefaultRasterFont())
        {
            coordFontRequested = coordFont;
        }
        else if (coordFontRequested.X == 0)
        {
            coordFontRequested.X = (SHORT)s_ShrinkByDpi(coordFont.X, iDpi);
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
// - pFontSize - recieves the current X by Y size of the font.
// Return Value:
// - S_OK
[[nodiscard]] HRESULT GdiEngine::GetFontSize(_Out_ COORD* const pFontSize) noexcept
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
COORD GdiEngine::_GetFontSize() const
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
