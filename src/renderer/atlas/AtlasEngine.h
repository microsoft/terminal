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
        [[nodiscard]] HRESULT PrepareForTeardown(_Out_ bool* pForcePaint) noexcept override;
        [[nodiscard]] HRESULT ScrollFrame() noexcept override;
        [[nodiscard]] HRESULT Invalidate(const til::rect* psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateCursor(const til::rect* psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateSystem(const til::rect* prcDirtyClient) noexcept override;
        [[nodiscard]] HRESULT InvalidateSelection(const std::vector<til::rect>& rectangles) noexcept override;
        [[nodiscard]] HRESULT InvalidateScroll(const til::point* pcoordDelta) noexcept override;
        [[nodiscard]] HRESULT InvalidateAll() noexcept override;
        [[nodiscard]] HRESULT InvalidateFlush(_In_ const bool circled, _Out_ bool* const pForcePaint) noexcept override;
        [[nodiscard]] HRESULT InvalidateTitle(std::wstring_view proposedTitle) noexcept override;
        [[nodiscard]] HRESULT NotifyNewText(const std::wstring_view newText) noexcept override;
        [[nodiscard]] HRESULT PrepareRenderInfo(const RenderFrameInfo& info) noexcept override;
        [[nodiscard]] HRESULT ResetLineTransform() noexcept override;
        [[nodiscard]] HRESULT PrepareLineTransform(LineRendition lineRendition, til::CoordType targetRow, til::CoordType viewportLeft) noexcept override;
        [[nodiscard]] HRESULT PaintBackground() noexcept override;
        [[nodiscard]] HRESULT PaintBufferLine(std::span<const Cluster> clusters, til::point coord, bool fTrimLeft, bool lineWrapped) noexcept override;
        [[nodiscard]] HRESULT PaintBufferGridLines(const GridLineSet lines, const COLORREF gridlineColor, const COLORREF underlineColor, const size_t cchLine, const til::point coordTarget) noexcept override;
        [[nodiscard]] HRESULT PaintSelection(const til::rect& rect) noexcept override;
        [[nodiscard]] HRESULT PaintSelections(const std::vector<til::rect>& rects) noexcept override;
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
        void SetSelectionBackground(COLORREF color, float alpha = 0.5f) noexcept;
        void SetSoftwareRendering(bool enable) noexcept;
        void SetDisablePartialInvalidation(bool enable) noexcept;
        void SetGraphicsAPI(GraphicsAPI graphicsAPI) noexcept;
        void SetWarningCallback(std::function<void(HRESULT, wil::zwstring_view)> pfn) noexcept;
        [[nodiscard]] HRESULT SetWindowSize(til::size pixels) noexcept;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& pfiFontInfoDesired, FontInfo& fiFontInfo, const std::unordered_map<std::wstring_view, uint32_t>& features, const std::unordered_map<std::wstring_view, float>& axes) noexcept;

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

        // AtlasEngine.api.cpp
        void _resolveTransparencySettings() noexcept;
        void _updateFont(const FontInfoDesired& fontInfoDesired, FontInfo& fontInfo, const std::unordered_map<std::wstring_view, uint32_t>& features, const std::unordered_map<std::wstring_view, float>& axes);
        void _resolveFontMetrics(const FontInfoDesired& fontInfoDesired, FontInfo& fontInfo, FontSettings* fontMetrics = nullptr) const;

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

        std::unique_ptr<IBackend> _b;
        RenderingPayload _p;

        // _p.s->font->builtinGlyphs is the setting which decides whether we should map box drawing glyphs to
        // our own builtin versions. There's just one problem: BackendD2D doesn't have this functionality.
        // But since AtlasEngine shapes the text before it's handed to the backends, it would need to know
        // whether BackendD2D is in use, before BackendD2D even exists. These two flags solve the issue
        // by triggering a complete, immediate redraw whenever the backend type changes.
        //
        // The proper solution is to move text shaping into the backends.
        // Someone just needs to write a generic "TextBuffer to DWRITE_GLYPH_RUN" function.
        bool _hackIsBackendD2D = false;
        bool _hackWantsBuiltinGlyphs = true;
        bool _hackTriggerRedrawAll = false;

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

            // dirtyRect is a computed value based on invalidatedRows.
            til::rect dirtyRect;
            // These "invalidation" fields are reset in EndPaint()
            u16r invalidatedCursorArea = invalidatedAreaNone;
            range<u16> invalidatedRows = invalidatedRowsNone; // x is treated as "top" and y as "bottom"
            i16 scrollOffset = 0;
        } _api;
    };
}

namespace Microsoft::Console::Render
{
    using AtlasEngine = Atlas::AtlasEngine;
}
