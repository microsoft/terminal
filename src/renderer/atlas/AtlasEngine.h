// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <dwrite_3.h>
#include <d3d11_2.h>
#include <dxgi1_3.h>

#include "common.h"

namespace Microsoft::Console::Render::Atlas
{
    struct TextAnalysisSinkResult;

    class AtlasEngine final : public IRenderEngine
    {
    public:
        explicit AtlasEngine();

        AtlasEngine(const AtlasEngine&) = delete;
        AtlasEngine& operator=(const AtlasEngine&) = delete;

        // IRenderEngine
        [[nodiscard]] HRESULT StartPaint() noexcept override;
        [[nodiscard]] HRESULT EndPaint() noexcept override;
        [[nodiscard]] bool RequiresContinuousRedraw() noexcept override;
        void WaitUntilCanRender() noexcept override;
        [[nodiscard]] HRESULT Present() noexcept override;
        [[nodiscard]] HRESULT ScrollFrame() noexcept override;
        [[nodiscard]] HRESULT Invalidate(const til::rect* psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateCursor(const til::rect* psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateSystem(const til::rect* prcDirtyClient) noexcept override;
        [[nodiscard]] HRESULT InvalidateSelection(std::span<const til::rect> selections) noexcept override;
        [[nodiscard]] HRESULT InvalidateHighlight(std::span<const til::point_span> highlights, const TextBuffer& buffer) noexcept override;
        [[nodiscard]] HRESULT InvalidateScroll(const til::point* pcoordDelta) noexcept override;
        [[nodiscard]] HRESULT InvalidateAll() noexcept override;
        [[nodiscard]] HRESULT InvalidateTitle(std::wstring_view proposedTitle) noexcept override;
        [[nodiscard]] HRESULT NotifyNewText(const std::wstring_view newText) noexcept override;
        [[nodiscard]] HRESULT PrepareRenderInfo(RenderFrameInfo info) noexcept override;
        [[nodiscard]] HRESULT ResetLineTransform() noexcept override;
        [[nodiscard]] HRESULT PrepareLineTransform(LineRendition lineRendition, til::CoordType targetRow, til::CoordType viewportLeft) noexcept override;
        [[nodiscard]] HRESULT PaintBackground() noexcept override;
        [[nodiscard]] HRESULT PaintBufferLine(std::span<const Cluster> clusters, til::point coord, bool fTrimLeft, bool lineWrapped) noexcept override;
        [[nodiscard]] HRESULT PaintBufferGridLines(const GridLineSet lines, const COLORREF gridlineColor, const COLORREF underlineColor, const size_t cchLine, const til::point coordTarget) noexcept override;
        [[nodiscard]] HRESULT PaintImageSlice(const ImageSlice& imageSlice, til::CoordType targetRow, til::CoordType viewportLeft) noexcept override;
        [[nodiscard]] HRESULT PaintSelection(const til::rect& rect) noexcept override;
        [[nodiscard]] HRESULT PaintCursor(const CursorOptions& options) noexcept override;
        [[nodiscard]] HRESULT UpdateDrawingBrushes(const TextAttribute& textAttributes, const RenderSettings& renderSettings, gsl::not_null<IRenderData*> pData, bool usingSoftFont, bool isSettingDefaultBrushes) noexcept override;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& FontInfoDesired, _Out_ FontInfo& FontInfo) noexcept override;
        [[nodiscard]] HRESULT UpdateSoftFont(std::span<const uint16_t> bitPattern, til::size cellSize, size_t centeringHint) noexcept override;
        [[nodiscard]] HRESULT UpdateDpi(int iDpi) noexcept override;
        [[nodiscard]] HRESULT UpdateViewport(const til::inclusive_rect& srNewViewport) noexcept override;
        [[nodiscard]] HRESULT GetProposedFont(const FontInfoDesired& FontInfoDesired, _Out_ FontInfo& FontInfo, int iDpi) noexcept override;
        [[nodiscard]] HRESULT GetDirtyArea(std::span<const til::rect>& area) noexcept override;
        [[nodiscard]] HRESULT GetFontSize(_Out_ til::size* pFontSize) noexcept override;
        [[nodiscard]] HRESULT IsGlyphWideByFont(std::wstring_view glyph, _Out_ bool* pResult) noexcept override;
        [[nodiscard]] HRESULT UpdateTitle(std::wstring_view newTitle) noexcept override;
        void UpdateHyperlinkHoveredId(uint16_t hoveredId) noexcept override;

        // getter
        [[nodiscard]] std::wstring_view GetPixelShaderPath() noexcept;
        [[nodiscard]] bool GetRetroTerminalEffect() const noexcept;
        [[nodiscard]] Types::Viewport GetViewportInCharacters(const Types::Viewport& viewInPixels) const noexcept;
        [[nodiscard]] Types::Viewport GetViewportInPixels(const Types::Viewport& viewInCharacters) const noexcept;
        // setter
        void SetAntialiasingMode(D2D1_TEXT_ANTIALIAS_MODE antialiasingMode) noexcept;
        void SetCallback(std::function<void(HANDLE)> pfn) noexcept;
        void EnableTransparentBackground(const bool isTransparent) noexcept;
        [[nodiscard]] HRESULT SetHwnd(HWND hwnd) noexcept;
        void SetPixelShaderPath(std::wstring_view value) noexcept;
        void SetPixelShaderImagePath(std::wstring_view value) noexcept;
        void SetRetroTerminalEffect(bool enable) noexcept;
        void SetSoftwareRendering(bool enable) noexcept;
        void SetDisablePartialInvalidation(bool enable) noexcept;
        void SetGraphicsAPI(GraphicsAPI graphicsAPI) noexcept;
        void SetWarningCallback(std::function<void(HRESULT, wil::zwstring_view)> pfn) noexcept;
        [[nodiscard]] HRESULT SetWindowSize(til::size pixels) noexcept;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& pfiFontInfoDesired, FontInfo& fiFontInfo, const std::unordered_map<std::wstring_view, float>& features, const std::unordered_map<std::wstring_view, float>& axes) noexcept;

    private:
        // AtlasEngine.cpp
        ATLAS_ATTR_COLD void _handleSettingsUpdate();
        void _recreateFontDependentResources();
        void _recreateCellCountDependentResources();
        void _flushBufferLine();
        void _mapRegularText(size_t offBeg, size_t offEnd);
        void _mapBuiltinGlyphs(size_t offBeg, size_t offEnd);
        void _mapCharacters(const wchar_t* text, u32 textLength, u32* mappedLength, IDWriteFontFace2** mappedFontFace) const;
        void _mapComplex(IDWriteFontFace2* mappedFontFace, u32 idx, u32 length, ShapedRow& row);
        ATLAS_ATTR_COLD void _mapReplacementCharacter(u32 from, u32 to, ShapedRow& row);
        void _fillColorBitmap(const size_t y, const size_t x1, const size_t x2, const u32 fgColor, const u32 bgColor) noexcept;
        [[nodiscard]] HRESULT _drawHighlighted(std::span<const til::point_span>& highlights, const u16 row, const u16 begX, const u16 endX, const u32 fgColor, const u32 bgColor) noexcept;

        // AtlasEngine.api.cpp
        void _resolveTransparencySettings() noexcept;
        [[nodiscard]] HRESULT _updateFont(const FontInfoDesired& fontInfoDesired, FontInfo& fontInfo, const std::unordered_map<std::wstring_view, float>& features, const std::unordered_map<std::wstring_view, float>& axes) noexcept;
        void _resolveFontMetrics(const FontInfoDesired& fontInfoDesired, FontInfo& fontInfo, FontSettings* fontMetrics = nullptr);
        [[nodiscard]] bool _updateWithNearbyFontCollection() noexcept;
        void _invalidateSpans(std::span<const til::point_span> spans, const TextBuffer& buffer) noexcept;

        // AtlasEngine.r.cpp
        ATLAS_ATTR_COLD void _recreateAdapter();
        ATLAS_ATTR_COLD void _recreateBackend();
        ATLAS_ATTR_COLD void _handleSwapChainUpdate();
        void _createSwapChain();
        void _destroySwapChain();
        void _resizeBuffers();
        void _updateMatrixTransform();
        void _waitUntilCanRender() noexcept;
        void _present();

        static constexpr u16 u16min = 0x0000;
        static constexpr u16 u16max = 0xffff;
        static constexpr i16 i16min = -0x8000;
        static constexpr i16 i16max = 0x7fff;
        static constexpr u16r invalidatedAreaNone = { u16max, u16max, u16min, u16min };
        static constexpr range<u16> invalidatedRowsNone{ u16max, u16min };
        static constexpr range<u16> invalidatedRowsAll{ u16min, u16max };

        static constexpr u32 highlightBg = 0xff00ffff;
        static constexpr u32 highlightFg = 0xff000000;
        static constexpr u32 highlightFocusBg = 0xff3296ff;
        static constexpr u32 highlightFocusFg = 0xff000000;

        std::unique_ptr<IBackend> _b;
        RenderingPayload _p;

        struct ApiState
        {
            GenerationalSettings s = DirtyGenerationalSettings();

            // This structure is loosely sorted in chunks from "very often accessed together"
            // to seldom accessed and/or usually not together.

            bool invalidatedTitle = false;
            // These two are redundant with TargetSettings/MiscellaneousSettings, but that's because _resolveTransparencySettings()
            // turns the given settings into potentially different actual settings (which are then written into the Settings).
            bool enableTransparentBackground = false;
            AntialiasingMode antialiasingMode = DefaultAntialiasingMode;

            std::vector<wchar_t> bufferLine;
            std::vector<u16> bufferLineColumn;

            std::array<Buffer<DWRITE_FONT_AXIS_VALUE>, 4> textFormatAxes;
            std::vector<TextAnalysisSinkResult> analysisResults;
            Buffer<u16> clusterMap;
            Buffer<DWRITE_SHAPING_TEXT_PROPERTIES> textProps;
            Buffer<u16> glyphIndices;
            Buffer<DWRITE_SHAPING_GLYPH_PROPERTIES> glyphProps;
            Buffer<f32> glyphAdvances;
            Buffer<DWRITE_GLYPH_OFFSET> glyphOffsets;

            wil::com_ptr<IDWriteFontFallback> systemFontFallback;
            wil::com_ptr<IDWriteFontFace2> replacementCharacterFontFace;
            u16 replacementCharacterGlyphIndex = 0;
            bool replacementCharacterLookedUp = false;

            // PrepareLineTransform()
            LineRendition lineRendition = LineRendition::SingleWidth;
            // UpdateDrawingBrushes()
            u32 backgroundOpaqueMixin = 0xff000000;
            u32 currentBackground = 0;
            u32 currentForeground = 0;
            FontRelevantAttributes attributes = FontRelevantAttributes::None;
            u16x2 lastPaintBufferLineCoord{};
            // UpdateHyperlinkHoveredId()
            u16 hyperlinkHoveredId = 0;

            // These tracks the highlighted regions on the screen that are yet to be painted.
            std::span<const til::point_span> searchHighlights;
            std::span<const til::point_span> searchHighlightFocused;
            std::span<const til::point_span> selectionSpans;

            // dirtyRect is a computed value based on invalidatedRows.
            til::rect dirtyRect;
            // These "invalidation" fields are reset in EndPaint()
            u16r invalidatedCursorArea = invalidatedAreaNone;
            range<u16> invalidatedRows = invalidatedRowsNone; // x is treated as "top" and y as "bottom"
            i16 scrollOffset = 0;

            // The position of the viewport inside the text buffer (in cells).
            u16x2 viewportOffset{ 0, 0 };
        } _api;
    };
}

namespace Microsoft::Console::Render
{
    using AtlasEngine = Atlas::AtlasEngine;
}
