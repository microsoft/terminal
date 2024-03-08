// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <stb_rect_pack.h>
#include <til/flat_set.h>
#include <til/small_vector.h>

#include "Backend.h"

#include "TextureLoader.h"

namespace Microsoft::Console::Render::Atlas
{
    struct BackendD3D : IBackend
    {
        BackendD3D(const RenderingPayload& p);

        void ReleaseResources() noexcept override;
        void Render(RenderingPayload& payload) override;
        bool RequiresContinuousRedraw() noexcept override;

        // NOTE: D3D constant buffers sizes must be a multiple of 16 bytes.
        struct alignas(16) VSConstBuffer
        {
            // WARNING: Modify this carefully after understanding how HLSL struct packing works. The gist is:
            // * Minimum alignment is 4 bytes
            // * Members cannot straddle 16 byte boundaries
            //   This means a structure like {u32; u32; u32; u32x2} would require
            //   padding so that it is {u32; u32; u32; <4 byte padding>; u32x2}.
            // * bool will probably not work the way you want it to,
            //   because HLSL uses 32-bit bools and C++ doesn't.
            alignas(sizeof(f32x2)) f32x2 positionScale;
#pragma warning(suppress : 4324) // 'VSConstBuffer': structure was padded due to alignment specifier
        };

        // WARNING: Same rules as for VSConstBuffer above apply.
        struct alignas(16) PSConstBuffer
        {
            alignas(sizeof(f32x4)) f32x4 backgroundColor;
            alignas(sizeof(f32x2)) f32x2 backgroundCellSize;
            alignas(sizeof(f32x2)) f32x2 backgroundCellCount;
            alignas(sizeof(f32x4)) f32 gammaRatios[4]{};
            alignas(sizeof(f32)) f32 enhancedContrast = 0;
            alignas(sizeof(f32)) f32 underlineWidth = 0;
            alignas(sizeof(f32)) f32 doubleUnderlineWidth = 0;
            alignas(sizeof(f32)) f32 curlyLineHalfHeight = 0;
            alignas(sizeof(f32)) f32 shadedGlyphDotSize = 0;
#pragma warning(suppress : 4324) // 'PSConstBuffer': structure was padded due to alignment specifier
        };

        // WARNING: Same rules as for VSConstBuffer above apply.
        struct alignas(16) CustomConstBuffer
        {
            alignas(sizeof(f32)) f32 time = 0;
            alignas(sizeof(f32)) f32 scale = 0;
            alignas(sizeof(f32x2)) f32x2 resolution;
            alignas(sizeof(f32x4)) f32x4 background;
#pragma warning(suppress : 4324) // 'CustomConstBuffer': structure was padded due to alignment specifier
        };

        enum class ShadingType : u8 // u8 so that it packs perfectly into AtlasGlyphEntry
        {
            Default = 0,
            Background = 0,

            // This block of values will be used for the TextDrawingFirst/Last range and need to stay together.
            // This is used to quickly check if an instance is related to a "text drawing primitive".
            TextGrayscale,
            TextClearType,
            TextBuiltinGlyph,
            TextPassthrough,
            DottedLine,
            DashedLine,
            CurlyLine,
            // All items starting here will be drawing as a solid RGBA color
            SolidLine,

            Cursor,
            Selection,

            TextDrawingFirst = TextGrayscale,
            TextDrawingLast = SolidLine,
        };

        // NOTE: Don't initialize any members in this struct. This ensures that no
        // zero-initialization needs to occur when we allocate large buffers of this object.
        struct QuadInstance
        {
            // `position` might clip outside of the bounds of the viewport and so it needs to be a
            // signed coordinate. i16x2 is used as the size of the instance buffer made the largest
            // impact on performance and power draw. If (when?) displays with >32k resolution make their
            // appearance in the future, this should be changed to f32x2. But if you do so, please change
            // all other occurrences of i16x2 positions/offsets throughout the class to keep it consistent.
            alignas(u16) u16 shadingType; // not using ShadingType here to avoid struct padding which causes optimization failures with MSVC
            alignas(u16) u8x2 renditionScale;
            alignas(u32) i16x2 position;
            alignas(u32) u16x2 size;
            alignas(u32) u16x2 texcoord;
            alignas(u32) u32 color;
        };

        // NOTE: Don't initialize any members in this struct. This ensures that no
        // zero-initialization needs to occur when we allocate large buffers of this object.
        struct AtlasGlyphEntry
        {
            u32 glyphIndex;
            u8 occupied;
            ShadingType shadingType;
            u16 overlapSplit;
            i16x2 offset;
            u16x2 size;
            u16x2 texcoord;
        };

        struct AtlasGlyphEntryHashTrait
        {
            static constexpr bool occupied(const AtlasGlyphEntry& entry) noexcept
            {
                return entry.occupied != 0;
            }

            static constexpr size_t hash(const u16 glyphIndex) noexcept
            {
                return til::flat_set_hash_integer(glyphIndex);
            }

            static constexpr size_t hash(const AtlasGlyphEntry& entry) noexcept
            {
                return til::flat_set_hash_integer(entry.glyphIndex);
            }

            static constexpr bool equals(const AtlasGlyphEntry& entry, u16 glyphIndex) noexcept
            {
                return entry.glyphIndex == glyphIndex;
            }

            static constexpr void assign(AtlasGlyphEntry& entry, u16 glyphIndex) noexcept
            {
                entry.glyphIndex = glyphIndex;
                entry.occupied = 1;
            }
        };

        struct AtlasFontFaceEntry
        {
            // BODGY: At the time of writing IDWriteFontFallback::MapCharacters returns the same IDWriteFontFace instance
            // for the same font face variant as long as someone is holding a reference to the instance (see ActiveFaceCache).
            // This allows us to hash the value of the pointer as if it was uniquely identifying the font face variant.
            wil::com_ptr<IDWriteFontFace2> fontFace;

            // The 4 entries map to the 4 corresponding LineRendition enum values.
            til::linear_flat_set<AtlasGlyphEntry, AtlasGlyphEntryHashTrait> glyphs[4];
        };

        struct AtlasFontFaceEntryHashTrait
        {
            static bool occupied(const AtlasFontFaceEntry& entry) noexcept
            {
                return static_cast<bool>(entry.fontFace);
            }

            static constexpr size_t hash(const IDWriteFontFace2* fontFace) noexcept
            {
                return til::flat_set_hash_integer(std::bit_cast<uintptr_t>(fontFace));
            }

            static size_t hash(const AtlasFontFaceEntry& entry) noexcept
            {
                return hash(entry.fontFace.get());
            }

            static bool equals(const AtlasFontFaceEntry& entry, const IDWriteFontFace2* fontFace) noexcept
            {
                return entry.fontFace.get() == fontFace;
            }

            static void assign(AtlasFontFaceEntry& entry, IDWriteFontFace2* fontFace) noexcept
            {
                entry.fontFace = fontFace;
            }
        };

    private:
        struct CursorRect
        {
            i16x2 position;
            u16x2 size;
            u32 background;
            u32 foreground;
        };

        ATLAS_ATTR_COLD void _handleSettingsUpdate(const RenderingPayload& p);
        void _updateFontDependents(const RenderingPayload& p);
        void _d2dRenderTargetUpdateFontSettings(const RenderingPayload& p) const noexcept;
        void _recreateCustomShader(const RenderingPayload& p);
        void _recreateCustomRenderTargetView(const RenderingPayload& p);
        void _recreateBackgroundColorBitmap(const RenderingPayload& p);
        void _recreateConstBuffer(const RenderingPayload& p) const;
        void _setupDeviceContextState(const RenderingPayload& p);
        void _debugUpdateShaders(const RenderingPayload& p) noexcept;
        void _debugShowDirty(const RenderingPayload& p);
        void _debugDumpRenderTarget(const RenderingPayload& p);
        void _d2dBeginDrawing() noexcept;
        void _d2dEndDrawing();
        ATLAS_ATTR_COLD void _resetGlyphAtlas(const RenderingPayload& p);
        ATLAS_ATTR_COLD void _resizeGlyphAtlas(const RenderingPayload& p, u16 u, u16 v);
        QuadInstance& _getLastQuad() noexcept;
        QuadInstance& _appendQuad();
        ATLAS_ATTR_COLD void _bumpInstancesSize();
        void _flushQuads(const RenderingPayload& p);
        ATLAS_ATTR_COLD void _recreateInstanceBuffers(const RenderingPayload& p);
        void _drawBackground(const RenderingPayload& p);
        void _uploadBackgroundBitmap(const RenderingPayload& p);
        void _drawText(RenderingPayload& p);
        ATLAS_ATTR_COLD void _drawTextOverlapSplit(const RenderingPayload& p, u16 y);
        [[nodiscard]] ATLAS_ATTR_COLD AtlasGlyphEntry* _drawGlyph(const RenderingPayload& p, const ShapedRow& row, AtlasFontFaceEntry& fontFaceEntry, u32 glyphIndex);
        AtlasGlyphEntry* _drawBuiltinGlyph(const RenderingPayload& p, const ShapedRow& row, AtlasFontFaceEntry& fontFaceEntry, u32 glyphIndex);
        void _drawSoftFontGlyph(const RenderingPayload& p, const stbrp_rect& rect, u32 glyphIndex);
        void _drawGlyphAtlasAllocate(const RenderingPayload& p, stbrp_rect& rect);
        static AtlasGlyphEntry* _drawGlyphAllocateEntry(const ShapedRow& row, AtlasFontFaceEntry& fontFaceEntry, u32 glyphIndex);
        static void _splitDoubleHeightGlyph(const RenderingPayload& p, const ShapedRow& row, AtlasFontFaceEntry& fontFaceEntry, AtlasGlyphEntry* glyphEntry);
        void _drawGridlines(const RenderingPayload& p, u16 y);
        void _drawCursorBackground(const RenderingPayload& p);
        ATLAS_ATTR_COLD void _drawCursorForeground();
        ATLAS_ATTR_COLD size_t _drawCursorForegroundSlowPath(const CursorRect& c, size_t offset);
        void _drawSelection(const RenderingPayload& p);
        void _executeCustomShader(RenderingPayload& p);

        wil::com_ptr<ID3D11RenderTargetView> _renderTargetView;
        wil::com_ptr<ID3D11InputLayout> _inputLayout;
        wil::com_ptr<ID3D11VertexShader> _vertexShader;
        wil::com_ptr<ID3D11PixelShader> _pixelShader;
        wil::com_ptr<ID3D11BlendState> _blendState;
        wil::com_ptr<ID3D11Buffer> _vsConstantBuffer;
        wil::com_ptr<ID3D11Buffer> _psConstantBuffer;
        wil::com_ptr<ID3D11Buffer> _vertexBuffer;
        wil::com_ptr<ID3D11Buffer> _indexBuffer;
        wil::com_ptr<ID3D11Buffer> _instanceBuffer;
        size_t _instanceBufferCapacity = 0;
        Buffer<QuadInstance, 32> _instances;
        size_t _instancesCount = 0;

        wil::com_ptr<ID3D11RenderTargetView> _customRenderTargetView;
        wil::com_ptr<ID3D11Texture2D> _customOffscreenTexture;
        wil::com_ptr<ID3D11ShaderResourceView> _customOffscreenTextureView;
        wil::com_ptr<ID3D11VertexShader> _customVertexShader;
        wil::com_ptr<ID3D11PixelShader> _customPixelShader;
        wil::com_ptr<ID3D11Buffer> _customShaderConstantBuffer;
        wil::com_ptr<ID3D11SamplerState> _customShaderSamplerState;
        ShaderTexture _customShaderTexture;
        std::chrono::steady_clock::time_point _customShaderStartTime;

        wil::com_ptr<ID3D11Texture2D> _backgroundBitmap;
        wil::com_ptr<ID3D11ShaderResourceView> _backgroundBitmapView;
        til::generation_t _backgroundBitmapGeneration;

        wil::com_ptr<ID3D11Texture2D> _glyphAtlas;
        wil::com_ptr<ID3D11ShaderResourceView> _glyphAtlasView;
        til::linear_flat_set<AtlasFontFaceEntry, AtlasFontFaceEntryHashTrait> _glyphAtlasMap;
        AtlasFontFaceEntry _builtinGlyphs;
        Buffer<stbrp_node> _rectPackerData;
        stbrp_context _rectPacker{};
        til::CoordType _ligatureOverhangTriggerLeft = 0;
        til::CoordType _ligatureOverhangTriggerRight = 0;

        wil::com_ptr<ID2D1DeviceContext> _d2dRenderTarget;
        wil::com_ptr<ID2D1DeviceContext4> _d2dRenderTarget4; // Optional. Supported since Windows 10 14393.
        wil::com_ptr<ID2D1SolidColorBrush> _emojiBrush;
        wil::com_ptr<ID2D1SolidColorBrush> _brush;
        wil::com_ptr<ID2D1Bitmap1> _softFontBitmap;
        bool _d2dBeganDrawing = false;
        bool _fontChangedResetGlyphAtlas = false;

        float _gamma = 0;
        float _cleartypeEnhancedContrast = 0;
        float _grayscaleEnhancedContrast = 0;
        wil::com_ptr<IDWriteRenderingParams1> _textRenderingParams;

        til::generation_t _generation;
        til::generation_t _fontGeneration;
        til::generation_t _miscGeneration;
        u16x2 _targetSize{};
        u16x2 _viewportCellCount{};
        ShadingType _textShadingType = ShadingType::Default;

        // An empty-box cursor spanning a wide glyph that has different
        // background colors on each side results in 6 lines being drawn.
        til::small_vector<CursorRect, 6> _cursorRects;
        // The bounding rect of _cursorRects in pixels.
        til::rect _cursorPosition;

        f32 _curlyLineHalfHeight = 0.0f;
        FontDecorationPosition _curlyUnderline;

        bool _requiresContinuousRedraw = false;

#if ATLAS_DEBUG_SHOW_DIRTY
        i32r _presentRects[9]{};
        size_t _presentRectsPos = 0;
#endif

#if ATLAS_DEBUG_DUMP_RENDER_TARGET
        wchar_t _dumpRenderTargetBasePath[MAX_PATH]{};
        size_t _dumpRenderTargetCounter = 0;
#endif

#if ATLAS_DEBUG_COLORIZE_GLYPH_ATLAS
        size_t _colorizeGlyphAtlasCounter = 0;
#endif

#if ATLAS_DEBUG_SHADER_HOT_RELOAD
        std::filesystem::path _sourceDirectory;
        wil::unique_folder_change_reader_nothrow _sourceCodeWatcher;
        std::atomic<int64_t> _sourceCodeInvalidationTime{ INT64_MAX };
#endif
    };
}
