// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <dwrite_3.h>

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

        void WaitUntilCanRender() noexcept override;
        void Render(const Render::RenderingPayload& payload) override;

    private:
        // AtlasEngine.cpp
        ATLAS_ATTR_COLD void _handleSettingsUpdate();
        void _recreateFontDependentResources();
        void _recreateCellCountDependentResources();
        void _flushBufferLine();
        void _mapCharacters(const wchar_t* text, u32 textLength, u32* mappedLength, IDWriteFontFace2** mappedFontFace) const;
        void _mapComplex(IDWriteFontFace2* mappedFontFace, u32 idx, u32 length, ShapedRow& row);
        ATLAS_ATTR_COLD void _mapReplacementCharacter(u32 from, u32 to, ShapedRow& row);

        // AtlasEngine.api.cpp
        void _resolveTransparencySettings() noexcept;
        void _updateFont(const wchar_t* faceName, const FontSettings& fontSettings);
        void _resolveFontMetrics(const wchar_t* faceName, const FontSettings& fontSettings, ResolvedFontSettings* fontMetrics = nullptr) const;

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

        struct ApiState
        {
            GenerationalSettings s = DirtyGenerationalSettings();

            // This structure is loosely sorted in chunks from "very often accessed together"
            // to seldom accessed and/or usually not together.

            bool invalidatedTitle = false;
            // These two are redundant with TargetSettings/MiscellaneousSettings, but that's because _resolveTransparencySettings()
            // turns the given settings into potentially different actual settings (which are then written into the Settings).
            bool enableTransparentBackground = false;
            TextAntialiasMode antialiasingMode = DefaultAntialiasingMode;

            std::vector<wchar_t> bufferLine;
            std::vector<u16> bufferLineColumn;

            std::wstring userLocaleName;

            std::array<Buffer<DWRITE_FONT_AXIS_VALUE>, 4> textFormatAxes;
            std::vector<TextAnalysisSinkResult> analysisResults;
            Buffer<u16> clusterMap;
            Buffer<DWRITE_SHAPING_TEXT_PROPERTIES> textProps;
            Buffer<u16> glyphIndices;
            Buffer<DWRITE_SHAPING_GLYPH_PROPERTIES> glyphProps;
            Buffer<f32> glyphAdvances;
            Buffer<DWRITE_GLYPH_OFFSET> glyphOffsets;

            wil::com_ptr<IDWriteFontFace2> replacementCharacterFontFace;
            u16 replacementCharacterGlyphIndex = 0;
            bool replacementCharacterLookedUp = false;

            // UpdateDrawingBrushes()
            u32 backgroundOpaqueMixin = 0xff000000;
            u32 currentBackground = 0;
            u32 currentForeground = 0;
            FontRelevantAttributes attributes = FontRelevantAttributes::None;
            u16x2 lastPaintBufferLineCoord{};
            // UpdateHyperlinkHoveredId()
            u16 hyperlinkHoveredId = 0;
        } _api;
    };
}

namespace Microsoft::Console::Render
{
    using AtlasEngine = Atlas::AtlasEngine;
}
