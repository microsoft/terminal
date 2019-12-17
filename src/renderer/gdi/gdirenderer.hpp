/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- GdiRenderer.hpp

Abstract:
- This is the definition of the GDI specific implementation of the renderer.

Author(s):
- Michael Niksa (MiNiksa) 17-Nov-2015
--*/

#pragma once

#include "..\inc\RenderEngineBase.hpp"

namespace Microsoft::Console::Render
{
    class GdiEngine final : public RenderEngineBase
    {
    public:
        GdiEngine();
        ~GdiEngine() override;

        [[nodiscard]] HRESULT SetHwnd(const HWND hwnd) noexcept;

        [[nodiscard]] HRESULT InvalidateSelection(const std::vector<SMALL_RECT>& rectangles) noexcept override;
        [[nodiscard]] HRESULT InvalidateScroll(const COORD* const pcoordDelta) noexcept override;
        [[nodiscard]] HRESULT InvalidateSystem(const RECT* const prcDirtyClient) noexcept override;
        [[nodiscard]] HRESULT Invalidate(const SMALL_RECT* const psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateCursor(const COORD* const pcoordCursor) noexcept override;
        [[nodiscard]] HRESULT InvalidateAll() noexcept override;
        [[nodiscard]] HRESULT InvalidateCircling(_Out_ bool* const pForcePaint) noexcept override;
        [[nodiscard]] HRESULT PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept override;

        [[nodiscard]] HRESULT StartPaint() noexcept override;
        [[nodiscard]] HRESULT EndPaint() noexcept override;
        [[nodiscard]] HRESULT Present() noexcept override;

        [[nodiscard]] HRESULT ScrollFrame() noexcept override;

        [[nodiscard]] HRESULT PaintBackground() noexcept override;
        [[nodiscard]] HRESULT PaintBufferLine(std::basic_string_view<Cluster> const clusters,
                                              const COORD coord,
                                              const bool trimLeft) noexcept override;
        [[nodiscard]] HRESULT PaintBufferGridLines(const GridLines lines,
                                                   const COLORREF color,
                                                   const size_t cchLine,
                                                   const COORD coordTarget) noexcept override;
        [[nodiscard]] HRESULT PaintSelection(const SMALL_RECT rect) noexcept override;

        [[nodiscard]] HRESULT PaintCursor(const CursorOptions& options) noexcept override;

        [[nodiscard]] HRESULT UpdateDrawingBrushes(const COLORREF colorForeground,
                                                   const COLORREF colorBackground,
                                                   const WORD legacyColorAttribute,
                                                   const ExtendedAttributes extendedAttrs,
                                                   const bool isSettingDefaultBrushes) noexcept override;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& FontInfoDesired,
                                         _Out_ FontInfo& FontInfo) noexcept override;
        [[nodiscard]] HRESULT UpdateDpi(const int iDpi) noexcept override;
        [[nodiscard]] HRESULT UpdateViewport(const SMALL_RECT srNewViewport) noexcept override;

        [[nodiscard]] HRESULT GetProposedFont(const FontInfoDesired& FontDesired,
                                              _Out_ FontInfo& Font,
                                              const int iDpi) noexcept override;

        SMALL_RECT GetDirtyRectInChars() override;
        [[nodiscard]] HRESULT GetFontSize(_Out_ COORD* const pFontSize) noexcept override;
        [[nodiscard]] HRESULT IsGlyphWideByFont(const std::wstring_view glyph, _Out_ bool* const pResult) noexcept override;

    protected:
        [[nodiscard]] HRESULT _DoUpdateTitle(_In_ const std::wstring& newTitle) noexcept override;

    private:
        HWND _hwndTargetWindow;

        [[nodiscard]] static HRESULT s_SetWindowLongWHelper(const HWND hWnd,
                                                            const int nIndex,
                                                            const LONG dwNewLong) noexcept;

        bool _fPaintStarted;

        PAINTSTRUCT _psInvalidData;
        HDC _hdcMemoryContext;
        bool _isTrueTypeFont;
        UINT _fontCodepage;
        HFONT _hfont;
        TEXTMETRICW _tmFontMetrics;

        static const size_t s_cPolyTextCache = 80;
        POLYTEXTW _pPolyText[s_cPolyTextCache];
        size_t _cPolyText;
        [[nodiscard]] HRESULT _FlushBufferLines() noexcept;

        std::vector<RECT> cursorInvertRects;

        COORD _coordFontLast;
        int _iCurrentDpi;

        static const int s_iBaseDpi = USER_DEFAULT_SCREEN_DPI;

        SIZE _szMemorySurface;
        HBITMAP _hbitmapMemorySurface;
        [[nodiscard]] HRESULT _PrepareMemoryBitmap(const HWND hwnd) noexcept;

        SIZE _szInvalidScroll;
        RECT _rcInvalid;
        bool _fInvalidRectUsed;

        COLORREF _lastFg;
        COLORREF _lastBg;

        [[nodiscard]] HRESULT _InvalidCombine(const RECT* const prc) noexcept;
        [[nodiscard]] HRESULT _InvalidOffset(const POINT* const ppt) noexcept;
        [[nodiscard]] HRESULT _InvalidRestrict() noexcept;

        [[nodiscard]] HRESULT _InvalidateRect(const RECT* const prc) noexcept;

        [[nodiscard]] HRESULT _PaintBackgroundColor(const RECT* const prc) noexcept;

        static const ULONG s_ulMinCursorHeightPercent = 25;
        static const ULONG s_ulMaxCursorHeightPercent = 100;

        [[nodiscard]] HRESULT _ScaleByFont(const COORD* const pcoord, _Out_ POINT* const pPoint) const noexcept;
        [[nodiscard]] HRESULT _ScaleByFont(const SMALL_RECT* const psr, _Out_ RECT* const prc) const noexcept;
        [[nodiscard]] HRESULT _ScaleByFont(const RECT* const prc, _Out_ SMALL_RECT* const psr) const noexcept;

        static int s_ScaleByDpi(const int iPx, const int iDpi);
        static int s_ShrinkByDpi(const int iPx, const int iDpi);

        POINT _GetInvalidRectPoint() const;
        SIZE _GetInvalidRectSize() const;
        SIZE _GetRectSize(const RECT* const pRect) const;

        void _OrRect(_In_ RECT* const pRectExisting, const RECT* const pRectToOr) const;

        bool _IsFontTrueType() const;

        [[nodiscard]] HRESULT _GetProposedFont(const FontInfoDesired& FontDesired,
                                               _Out_ FontInfo& Font,
                                               const int iDpi,
                                               _Inout_ wil::unique_hfont& hFont) noexcept;

        COORD _GetFontSize() const;
        bool _IsMinimized() const;
        bool _IsWindowValid() const;

#ifdef DBG
        // Helper functions to diagnose issues with painting from the in-memory buffer.
        // These are only actually effective/on in Debug builds when the flag is set using an attached debugger.
        bool _fDebug = false;
        void _PaintDebugRect(const RECT* const prc) const;
        void _DoDebugBlt(const RECT* const prc) const;

        void _DebugBltAll() const;

        HWND _debugWindow;
        void _CreateDebugWindow();
        HDC _debugContext;
#endif
    };
}
