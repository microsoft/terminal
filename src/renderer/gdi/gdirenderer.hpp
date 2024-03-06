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

#include "../inc/RenderEngineBase.hpp"
#include "../inc/FontResource.hpp"

namespace Microsoft::Console::Render
{
    class GdiEngine final : public RenderEngineBase
    {
    public:
        GdiEngine();
        ~GdiEngine() override;

        [[nodiscard]] HRESULT SetHwnd(const HWND hwnd) noexcept;

        [[nodiscard]] HRESULT InvalidateSelection(const std::vector<til::rect>& rectangles) noexcept override;
        [[nodiscard]] HRESULT InvalidateScroll(const til::point* const pcoordDelta) noexcept override;
        [[nodiscard]] HRESULT InvalidateSystem(const til::rect* const prcDirtyClient) noexcept override;
        [[nodiscard]] HRESULT Invalidate(const til::rect* const psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateCursor(const til::rect* const psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateAll() noexcept override;
        [[nodiscard]] HRESULT PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept override;

        [[nodiscard]] HRESULT StartPaint() noexcept override;
        [[nodiscard]] HRESULT EndPaint() noexcept override;
        [[nodiscard]] HRESULT Present() noexcept override;

        [[nodiscard]] HRESULT ScrollFrame() noexcept override;

        [[nodiscard]] HRESULT ResetLineTransform() noexcept override;
        [[nodiscard]] HRESULT PrepareLineTransform(const LineRendition lineRendition,
                                                   const til::CoordType targetRow,
                                                   const til::CoordType viewportLeft) noexcept override;

        [[nodiscard]] HRESULT PaintBackground() noexcept override;
        [[nodiscard]] HRESULT PaintBufferLine(const std::span<const Cluster> clusters,
                                              const til::point coord,
                                              const bool trimLeft,
                                              const bool lineWrapped) noexcept override;
        [[nodiscard]] HRESULT PaintBufferGridLines(const GridLineSet lines,
                                                   const COLORREF gridlineColor,
                                                   const COLORREF underlineColor,
                                                   const size_t cchLine,
                                                   const til::point coordTarget) noexcept override;
        [[nodiscard]] HRESULT PaintSelection(const til::rect& rect) noexcept override;
        [[nodiscard]] HRESULT PaintSelections(const std::vector<til::rect>& rects) noexcept override;

        [[nodiscard]] HRESULT PaintCursor(const CursorOptions& options) noexcept override;

        [[nodiscard]] HRESULT UpdateDrawingBrushes(const TextAttribute& textAttributes,
                                                   const RenderSettings& renderSettings,
                                                   const gsl::not_null<IRenderData*> pData,
                                                   const bool usingSoftFont,
                                                   const bool isSettingDefaultBrushes) noexcept override;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& FontInfoDesired,
                                         _Out_ FontInfo& FontInfo) noexcept override;
        [[nodiscard]] HRESULT UpdateSoftFont(const std::span<const uint16_t> bitPattern,
                                             const til::size cellSize,
                                             const size_t centeringHint) noexcept override;
        [[nodiscard]] HRESULT UpdateDpi(const int iDpi) noexcept override;
        [[nodiscard]] HRESULT UpdateViewport(const til::inclusive_rect& srNewViewport) noexcept override;

        [[nodiscard]] HRESULT GetProposedFont(const FontInfoDesired& FontDesired,
                                              _Out_ FontInfo& Font,
                                              const int iDpi) noexcept override;

        [[nodiscard]] HRESULT GetDirtyArea(std::span<const til::rect>& area) noexcept override;
        [[nodiscard]] HRESULT GetFontSize(_Out_ til::size* const pFontSize) noexcept override;
        [[nodiscard]] HRESULT IsGlyphWideByFont(const std::wstring_view glyph, _Out_ bool* const pResult) noexcept override;

    protected:
        [[nodiscard]] HRESULT _DoUpdateTitle(_In_ const std::wstring_view newTitle) noexcept override;

    private:
        HWND _hwndTargetWindow;

        [[nodiscard]] static HRESULT s_SetWindowLongWHelper(const HWND hWnd,
                                                            const int nIndex,
                                                            const LONG dwNewLong) noexcept;

        static bool FontHasWesternScript(HDC hdc);

        bool _fPaintStarted;

        til::rect _invalidCharacters;
        PAINTSTRUCT _psInvalidData;
        HDC _hdcMemoryContext;
        bool _isTrueTypeFont;
        UINT _fontCodepage;
        HFONT _hfont;
        HFONT _hfontItalic;
        TEXTMETRICW _tmFontMetrics;
        FontResource _softFont;

        static const size_t s_cPolyTextCache = 80;
        POLYTEXTW _pPolyText[s_cPolyTextCache];
        size_t _cPolyText;
        [[nodiscard]] HRESULT _FlushBufferLines() noexcept;

        std::vector<RECT> cursorInvertRects;
        XFORM cursorInvertTransform;

        struct LineMetrics
        {
            int gridlineWidth;
            int underlineCenter;
            int underlineWidth;
            int doubleUnderlinePosTop;
            int doubleUnderlinePosBottom;
            int doubleUnderlineWidth;
            int strikethroughOffset;
            int strikethroughWidth;
            int curlyLineCenter;
            int curlyLinePeriod;
            int curlyLineControlPointOffset;
        };

        LineMetrics _lineMetrics;
        til::size _coordFontLast;
        int _iCurrentDpi;

        static const int s_iBaseDpi = USER_DEFAULT_SCREEN_DPI;

        til::size _szMemorySurface;
        HBITMAP _hbitmapMemorySurface;
        [[nodiscard]] HRESULT _PrepareMemoryBitmap(const HWND hwnd) noexcept;

        til::size _szInvalidScroll;
        til::rect _rcInvalid;
        bool _fInvalidRectUsed;

        COLORREF _lastFg;
        COLORREF _lastBg;

        enum class FontType : uint8_t
        {
            Undefined,
            Default,
            Italic,
            Soft
        };
        FontType _lastFontType;
        bool _fontHasWesternScript = false;

        XFORM _currentLineTransform;
        LineRendition _currentLineRendition;

        // Memory pooling to save alloc/free work to the OS for things
        // frequently created and dropped.
        // It's important the pool is first so it can be given to the others on construction.
        std::pmr::unsynchronized_pool_resource _pool;
        std::pmr::vector<std::pmr::wstring> _polyStrings;
        std::pmr::vector<std::pmr::basic_string<int>> _polyWidths;

        [[nodiscard]] HRESULT _InvalidCombine(const til::rect* const prc) noexcept;
        [[nodiscard]] HRESULT _InvalidOffset(const til::point* const ppt) noexcept;
        [[nodiscard]] HRESULT _InvalidRestrict() noexcept;

        [[nodiscard]] HRESULT _InvalidateRect(const til::rect* const prc) noexcept;

        [[nodiscard]] HRESULT _PaintBackgroundColor(const RECT* const prc) noexcept;

        static const ULONG s_ulMinCursorHeightPercent = 25;
        static const ULONG s_ulMaxCursorHeightPercent = 100;

        static int s_ScaleByDpi(const int iPx, const int iDpi);
        static int s_ShrinkByDpi(const int iPx, const int iDpi);

        til::point _GetInvalidRectPoint() const;
        til::size _GetInvalidRectSize() const;
        til::size _GetRectSize(const RECT* const pRect) const;

        void _OrRect(_In_ til::rect* const pRectExisting, const til::rect* const pRectToOr) const;

        bool _IsFontTrueType() const;

        [[nodiscard]] HRESULT _GetProposedFont(const FontInfoDesired& FontDesired,
                                               _Out_ FontInfo& Font,
                                               const int iDpi,
                                               _Inout_ wil::unique_hfont& hFont,
                                               _Inout_ wil::unique_hfont& hFontItalic) noexcept;

        til::size _GetFontSize() const;
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

    constexpr XFORM IDENTITY_XFORM = { 1, 0, 0, 1 };

    inline bool operator==(const XFORM& lhs, const XFORM& rhs) noexcept
    {
        return ::memcmp(&lhs, &rhs, sizeof(XFORM)) == 0;
    };
}
