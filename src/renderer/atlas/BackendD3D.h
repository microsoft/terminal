#pragma once

#include <stb_rect_pack.h>
#include <til/small_vector.h>

#include "Backend.h"

namespace Microsoft::Console::Render::Atlas
{
    struct BackendD3D : IBackend
    {
        BackendD3D(wil::com_ptr<ID3D11Device2> device, wil::com_ptr<ID3D11DeviceContext2> deviceContext);

        void Render(RenderingPayload& payload) override;
        bool RequiresContinuousRedraw() noexcept override;
        void WaitUntilCanRender() noexcept override;

    private:
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
            alignas(sizeof(f32x2)) f32x2 cellSize;
            alignas(sizeof(f32x2)) f32x2 cellCount;
            alignas(sizeof(f32x4)) f32 gammaRatios[4]{};
            alignas(sizeof(f32)) f32 enhancedContrast = 0;
            alignas(sizeof(f32)) f32 dashedLineLength = 0;
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

        enum class ShadingType : u32
        {
            Background = 0,
            TextGrayscale,
            TextClearType,
            Passthrough,
            DashedLine,
            SolidFill,
        };

        struct QuadInstance
        {
            // `position` might clip outside of the bounds of the viewport and so it needs to be a
            // signed coordinate. i16x2 is used as the size of the instance buffer made the largest
            // impact on performance and power draw. If (when?) displays with >32k resolution make their
            // appearance in the future, this should be changed to f32x2. But if you do so, please change
            // all other occurrences of i16x2 positions/offsets throughout the class to keep it consistent.
            alignas(sizeof(i16x2)) i16x2 position;
            alignas(sizeof(i16x2)) u16x2 size;
            alignas(sizeof(i16x2)) u16x2 texcoord;
            alignas(sizeof(u32)) u32 shadingType = 0;
            alignas(sizeof(u32)) u32 color = 0;
        };

        struct GlyphCacheEntry
        {
            // BODGY: The IDWriteFontFace results from us calling IDWriteFontFallback::MapCharacters
            // which at the time of writing returns the same IDWriteFontFace as long as someone is
            // holding a reference / the reference count doesn't drop to 0 (see ActiveFaceCache).
            IDWriteFontFace* fontFace = nullptr;
            u16 glyphIndex = 0;
            u16 shadingType = 0;
            i16x2 offset;
            u16x2 size;
            u16x2 texcoord;
        };

        struct GlyphCacheMap
        {
            GlyphCacheMap() = default;
            ~GlyphCacheMap();

            GlyphCacheMap(const GlyphCacheMap&) = delete;
            GlyphCacheMap(GlyphCacheMap&&) = delete;

            const GlyphCacheMap& operator=(const GlyphCacheMap&) = delete;
            GlyphCacheMap& operator=(GlyphCacheMap&& other) noexcept;

            void Clear() noexcept;
            GlyphCacheEntry& FindOrInsert(IDWriteFontFace* fontFace, u16 glyphIndex, bool& inserted);

        private:
            static size_t _hash(IDWriteFontFace* fontFace, u16 glyphIndex) noexcept;
            GlyphCacheEntry& _insert(IDWriteFontFace* fontFace, u16 glyphIndex, size_t hash);
            void _bumpSize();

            static constexpr u32 initialSize = 256;

            Buffer<GlyphCacheEntry> _map{ initialSize };
            size_t _mapMask = initialSize - 1;
            size_t _capacity = initialSize / 2;
            size_t _size = 0;
        };

        __declspec(noinline) void _handleSettingsUpdate(const RenderingPayload& p);
        void _recreateCustomShader(const RenderingPayload& p);
        void _recreateCustomRenderTargetView(u16x2 targetSize);
        void _d2dRenderTargetUpdateFontSettings(const FontSettings& font) noexcept;
        void _recreateBackgroundColorBitmap(u16x2 cellCount);
        void _recreateConstBuffer(const RenderingPayload& p);
        void _setupDeviceContextState(const RenderingPayload& p);
        void _debugUpdateShaders(const RenderingPayload& p) noexcept;
        void _d2dBeginDrawing() noexcept;
        void _d2dEndDrawing();
        void _handleFontChangedResetGlyphAtlas(RenderingPayload& p);
        void _resetGlyphAtlasAndBeginDraw(const RenderingPayload& p);
        void _markStateChange(ID3D11BlendState* blendState);
        QuadInstance& _getLastQuad() noexcept;
        void _appendQuad(i16x2 position, u16x2 size, u32 color, ShadingType shadingType);
        void _appendQuad(i16x2 position, u16x2 size, u16x2 texcoord, u32 color, ShadingType shadingType);
        __declspec(noinline) void _bumpInstancesSize();
        void _flushQuads(const RenderingPayload& p);
        __declspec(noinline) void _recreateInstanceBuffers(const RenderingPayload& p);
        void _drawBackground(const RenderingPayload& p);
        void _drawText(RenderingPayload& p);
        __declspec(noinline) void _drawGlyph(const RenderingPayload& p, GlyphCacheEntry& entry, f32 fontEmSize);
        void _drawGridlines(const RenderingPayload& p);
        void _drawGridlineRow(const RenderingPayload& p, const ShapedRow* row, u16 y);
        void _drawCursorPart1(const RenderingPayload& p);
        void _drawCursorPart2(const RenderingPayload& p);
        void _drawSelection(const RenderingPayload& p);
        void _executeCustomShader(RenderingPayload& p);

        SwapChainManager _swapChainManager;

        wil::com_ptr<ID3D11Device2> _device;
        wil::com_ptr<ID3D11DeviceContext2> _deviceContext;
        wil::com_ptr<ID3D11RenderTargetView> _renderTargetView;

        wil::com_ptr<ID3D11InputLayout> _inputLayout;
        wil::com_ptr<ID3D11VertexShader> _vertexShader;
        wil::com_ptr<ID3D11PixelShader> _pixelShader;
        wil::com_ptr<ID3D11BlendState> _blendState;
        wil::com_ptr<ID3D11BlendState> _blendStateInvert;
        wil::com_ptr<ID3D11Buffer> _vsConstantBuffer;
        wil::com_ptr<ID3D11Buffer> _psConstantBuffer;
        wil::com_ptr<ID3D11Buffer> _vertexBuffer;
        wil::com_ptr<ID3D11Buffer> _indexBuffer;
        wil::com_ptr<ID3D11Buffer> _instanceBuffer;
        size_t _instanceBufferCapacity = 0;
        Buffer<QuadInstance> _instances;
        size_t _instancesCount = 0;

        // This allows us to batch inverted cursors into the same
        // _instanceBuffer upload as the rest of all other instances.
        struct StateChange
        {
            ID3D11BlendState* blendState;
            size_t offset;
        };
        // 3 allows for 1 state change to _blendStateInvert, followed by 1 change back to _blendState,
        // and finally 1 entry to signal the past-the-end size, as used by _flushQuads.
        til::small_vector<StateChange, 3> _instancesStateChanges;

        wil::com_ptr<ID3D11RenderTargetView> _customRenderTargetView;
        wil::com_ptr<ID3D11Texture2D> _customOffscreenTexture;
        wil::com_ptr<ID3D11ShaderResourceView> _customOffscreenTextureView;
        wil::com_ptr<ID3D11VertexShader> _customVertexShader;
        wil::com_ptr<ID3D11PixelShader> _customPixelShader;
        wil::com_ptr<ID3D11Buffer> _customShaderConstantBuffer;
        wil::com_ptr<ID3D11SamplerState> _customShaderSamplerState;
        std::chrono::steady_clock::time_point _customShaderStartTime;

        wil::com_ptr<ID3D11Texture2D> _backgroundBitmap;
        wil::com_ptr<ID3D11ShaderResourceView> _backgroundBitmapView;
        til::generation_t _backgroundBitmapGeneration;

        wil::com_ptr<ID3D11Texture2D> _glyphAtlas;
        wil::com_ptr<ID3D11ShaderResourceView> _glyphAtlasView;
        GlyphCacheMap _glyphCache;
        Buffer<stbrp_node> _rectPackerData;
        stbrp_context _rectPacker{};

        wil::com_ptr<ID2D1DeviceContext> _d2dRenderTarget;
        wil::com_ptr<ID2D1DeviceContext4> _d2dRenderTarget4; // Optional. Supported since Windows 10 14393.
        wil::com_ptr<ID2D1SolidColorBrush> _brush;
        bool _d2dBeganDrawing = false;
        bool _fontChangedResetGlyphAtlas = false;

        float _gamma = 0;
        float _cleartypeEnhancedContrast = 0;
        float _grayscaleEnhancedContrast = 0;
        wil::com_ptr<IDWriteRenderingParams1> _textRenderingParams;

        til::generation_t _generation;
        til::generation_t _fontGeneration;
        til::generation_t _miscGeneration;
        u16x2 _targetSize;
        u16x2 _cellCount;

        // An empty-box cursor spanning a wide glyph that has different
        // background colors on each side results in 6 lines being drawn.
        struct CursorRect
        {
            i16x2 position;
            u16x2 size;
            u32 color = 0;
        };
        til::small_vector<CursorRect, 6> _cursorRects;

        bool _requiresContinuousRedraw = false;

#ifndef NDEBUG
        std::filesystem::path _sourceDirectory;
        wil::unique_folder_change_reader_nothrow _sourceCodeWatcher;
        std::atomic<int64_t> _sourceCodeInvalidationTime{ INT64_MAX };
#endif
    };
}
