// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <d2d1.h>
#include <d3d11_1.h>
#include <dwrite_3.h>
#include <til/pair.h>

#include "../../renderer/inc/IRenderEngine.hpp"

namespace Microsoft::Console::Render
{
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
        [[nodiscard]] HRESULT Invalidate(const SMALL_RECT* psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateCursor(const SMALL_RECT* psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateSystem(const RECT* prcDirtyClient) noexcept override;
        [[nodiscard]] HRESULT InvalidateSelection(const std::vector<SMALL_RECT>& rectangles) noexcept override;
        [[nodiscard]] HRESULT InvalidateScroll(const COORD* pcoordDelta) noexcept override;
        [[nodiscard]] HRESULT InvalidateAll() noexcept override;
        [[nodiscard]] HRESULT InvalidateCircling(_Out_ bool* pForcePaint) noexcept override;
        [[nodiscard]] HRESULT InvalidateTitle(std::wstring_view proposedTitle) noexcept override;
        [[nodiscard]] HRESULT PrepareRenderInfo(const RenderFrameInfo& info) noexcept override;
        [[nodiscard]] HRESULT ResetLineTransform() noexcept override;
        [[nodiscard]] HRESULT PrepareLineTransform(LineRendition lineRendition, size_t targetRow, size_t viewportLeft) noexcept override;
        [[nodiscard]] HRESULT PaintBackground() noexcept override;
        [[nodiscard]] HRESULT PaintBufferLine(gsl::span<const Cluster> clusters, COORD coord, bool fTrimLeft, bool lineWrapped) noexcept override;
        [[nodiscard]] HRESULT PaintBufferGridLines(GridLineSet lines, COLORREF color, size_t cchLine, COORD coordTarget) noexcept override;
        [[nodiscard]] HRESULT PaintSelection(SMALL_RECT rect) noexcept override;
        [[nodiscard]] HRESULT PaintCursor(const CursorOptions& options) noexcept override;
        [[nodiscard]] HRESULT UpdateDrawingBrushes(const TextAttribute& textAttributes, gsl::not_null<IRenderData*> pData, bool usingSoftFont, bool isSettingDefaultBrushes) noexcept override;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& FontInfoDesired, _Out_ FontInfo& FontInfo) noexcept override;
        [[nodiscard]] HRESULT UpdateSoftFont(gsl::span<const uint16_t> bitPattern, SIZE cellSize, size_t centeringHint) noexcept override;
        [[nodiscard]] HRESULT UpdateDpi(int iDpi) noexcept override;
        [[nodiscard]] HRESULT UpdateViewport(SMALL_RECT srNewViewport) noexcept override;
        [[nodiscard]] HRESULT GetProposedFont(const FontInfoDesired& FontInfoDesired, _Out_ FontInfo& FontInfo, int iDpi) noexcept override;
        [[nodiscard]] HRESULT GetDirtyArea(gsl::span<const til::rectangle>& area) noexcept override;
        [[nodiscard]] HRESULT GetFontSize(_Out_ COORD* pFontSize) noexcept override;
        [[nodiscard]] HRESULT IsGlyphWideByFont(std::wstring_view glyph, _Out_ bool* pResult) noexcept override;
        [[nodiscard]] HRESULT UpdateTitle(std::wstring_view newTitle) noexcept override;

        // DxRenderer - getter
        HRESULT Enable() noexcept override;
        [[nodiscard]] bool GetRetroTerminalEffect() const noexcept override;
        [[nodiscard]] float GetScaling() const noexcept override;
        [[nodiscard]] HANDLE GetSwapChainHandle() override;
        [[nodiscard]] Types::Viewport GetViewportInCharacters(const Types::Viewport& viewInPixels) const noexcept override;
        [[nodiscard]] Types::Viewport GetViewportInPixels(const Types::Viewport& viewInCharacters) const noexcept override;
        // DxRenderer - setter
        void SetAntialiasingMode(D2D1_TEXT_ANTIALIAS_MODE antialiasingMode) noexcept override;
        void SetCallback(std::function<void()> pfn) noexcept override;
        void SetDefaultTextBackgroundOpacity(float opacity) noexcept override;
        void SetForceFullRepaintRendering(bool enable) noexcept override;
        [[nodiscard]] HRESULT SetHwnd(HWND hwnd) noexcept override;
        void SetPixelShaderPath(std::wstring_view value) noexcept override;
        void SetRetroTerminalEffect(bool enable) noexcept override;
        void SetSelectionBackground(COLORREF color, float alpha = 0.5f) noexcept override;
        void SetSoftwareRendering(bool enable) noexcept override;
        void SetIntenseIsBold(bool enable) noexcept override;
        void SetWarningCallback(std::function<void(HRESULT)> pfn) noexcept override;
        [[nodiscard]] HRESULT SetWindowSize(SIZE pixels) noexcept override;
        void ToggleShaderEffects() noexcept override;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& pfiFontInfoDesired, FontInfo& fiFontInfo, const std::unordered_map<std::wstring_view, uint32_t>& features, const std::unordered_map<std::wstring_view, float>& axes) noexcept override;
        void UpdateHyperlinkHoveredId(uint16_t hoveredId) noexcept override;

        // Some helper classes for the implementation.
        // public because I don't want to sprinkle the code with friends.
    public:
#define ATLAS_POD_OPS(type)                                    \
    constexpr bool operator==(const type& rhs) const noexcept  \
    {                                                          \
        return __builtin_memcmp(this, &rhs, sizeof(rhs)) == 0; \
    }                                                          \
                                                               \
    constexpr bool operator!=(const type& rhs) const noexcept  \
    {                                                          \
        return __builtin_memcmp(this, &rhs, sizeof(rhs)) != 0; \
    }

#define ATLAS_FLAG_OPS(type, underlying)                                                                                                                    \
    friend constexpr type operator~(type v) noexcept { return static_cast<type>(~static_cast<underlying>(v)); }                                             \
    friend constexpr type operator|(type lhs, type rhs) noexcept { return static_cast<type>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs)); } \
    friend constexpr type operator&(type lhs, type rhs) noexcept { return static_cast<type>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs)); } \
    friend constexpr void operator|=(type& lhs, type rhs) noexcept { lhs = lhs | rhs; }                                                                     \
    friend constexpr void operator&=(type& lhs, type rhs) noexcept { lhs = lhs & rhs; }

        template<typename T>
        struct vec2
        {
            T x{};
            T y{};

            ATLAS_POD_OPS(vec2)

            constexpr vec2 operator/(const vec2& rhs) noexcept
            {
                assert(rhs.x != 0 && rhs.y != 0);
                return { gsl::narrow_cast<T>(x / rhs.x), gsl::narrow_cast<T>(y / rhs.y) };
            }
        };

        template<typename T>
        struct vec4
        {
            T x{};
            T y{};
            T z{};
            T w{};

            ATLAS_POD_OPS(vec4)
        };

        template<typename T>
        struct rect
        {
            T left{};
            T top{};
            T right{};
            T bottom{};

            ATLAS_POD_OPS(rect)

            constexpr bool non_empty() noexcept
            {
                return (left < right) & (top < bottom);
            }
        };

        using u8 = uint8_t;

        using u16 = uint16_t;
        using u16x2 = vec2<u16>;
        using u16r = rect<u16>;

        using i16 = int16_t;

        using u32 = uint32_t;
        using u32x2 = vec2<u32>;

        using f32 = float;
        using f32x2 = vec2<f32>;
        using f32x4 = vec4<f32>;

        struct TextAnalyzerResult
        {
            u32 textPosition = 0;
            u32 textLength = 0;

            // These 2 fields represent DWRITE_SCRIPT_ANALYSIS.
            // Not using DWRITE_SCRIPT_ANALYSIS drops the struct size from 20 down to 12 bytes.
            u16 script = 0;
            u8 shapes = 0;

            u8 bidiLevel = 0;
        };

        template<typename T>
        struct AlignedBuffer
        {
            constexpr AlignedBuffer() noexcept = default;

            explicit AlignedBuffer(size_t size, size_t alignment) :
                _data{ THROW_IF_NULL_ALLOC(static_cast<T*>(_aligned_malloc(size * sizeof(T), alignment))) },
                _size{ size }
            {
            }

            ~AlignedBuffer()
            {
                _aligned_free(_data);
            }

            AlignedBuffer(AlignedBuffer&& other) noexcept :
                _data{ std::exchange(other._data, nullptr) },
                _size{ std::exchange(other._size, 0) }
            {
            }

#pragma warning(suppress : 26432) // If you define or delete any default operation in the type '...', define or delete them all (c.21).
            AlignedBuffer& operator=(AlignedBuffer&& other) noexcept
            {
                _aligned_free(_data);
                _data = std::exchange(other._data, nullptr);
                _size = std::exchange(other._size, 0);
                return *this;
            }

            T* data() noexcept
            {
                return _data;
            }

            size_t size() noexcept
            {
                return _size;
            }

        private:
            T* _data = nullptr;
            size_t _size = 0;
        };

    private:
        // u16 so it neatly fits into AtlasValue.
        // These flags are shared with shader_ps.hlsl.
        enum class CellFlags : u16
        {
            None = 0,
            ColoredGlyph = 1,

            Cursor = 2,
            Selected = 4,

            BorderLeft = 8,
            BorderTop = 16,
            BorderRight = 32,
            BorderBottom = 64,

            Underline = 128,
            UnderlineDotted = 256,
            UnderlineDouble = 512,
            Strikethrough = 1024,
        };
        ATLAS_FLAG_OPS(CellFlags, u16)

        // This structure is shared with the GPU shader and needs to follow certain alignment rules.
        // You can generally assume that only u32 or types of that alignment are allowed.
        struct Cell
        {
            alignas(u32) u16x2 tileIndex;
            alignas(u32) CellFlags flags;
            u32x2 color;
        };

        struct AtlasKeyAttributes
        {
            wchar_t cells : 4;
            wchar_t bold : 1;
            wchar_t italic : 1;
        };

        struct AtlasKey
        {
            wchar_t chars[15];
            AtlasKeyAttributes attributes;

            ATLAS_POD_OPS(AtlasKey)
        };

        struct AtlasKeyHasher
        {
            size_t operator()(const AtlasKey& entry) const noexcept
            {
                static_assert(sizeof(entry) == 32);
                return gsl::narrow_cast<size_t>(XXH3_len_32_64b(&entry));
            }
        };

        struct AtlasValue
        {
            u16x2 coords[15];
            CellFlags flags = CellFlags::None;
        };

        struct CachedCursorOptions
        {
            u32 cursorColor = INVALID_COLOR;
            u16 cursorType = gsl::narrow_cast<u16>(CursorType::Legacy);
            u8 ulCursorHeightPercent = 25;

            ATLAS_POD_OPS(CachedCursorOptions)
        };

        // D3D constant buffers sizes must be a multiple of 16 bytes.
        struct alignas(16) ConstBuffer
        {
            f32x4 viewport;
            u32x2 cellSize;
            u32 cellCountX = 0;
            u32 backgroundColor = 0;
            u32 cursorColor = 0;
            u32 selectionColor = 0;
#pragma warning(suppress : 4324) // structure was padded due to alignment specifier
        };

        // Handled in BeginPaint()
        enum class ApiInvalidations : u8
        {
            None = 0,
            Title = 1 << 0,
            Device = 1 << 1,
            Size = 1 << 2,
            Font = 1 << 3,
            Settings = 1 << 4,
        };
        ATLAS_FLAG_OPS(ApiInvalidations, u8)

        // Handled in Present()
        enum class RenderInvalidations : u8
        {
            None = 0,
            Cursor = 1 << 0,
            ConstBuffer = 1 << 1,
        };
        ATLAS_FLAG_OPS(RenderInvalidations, u8)

        // MSVC STL (version 22000) implements std::clamp<T>(T, T, T) in terms of the generic
        // std::clamp<T, Predicate>(T, T, T, Predicate) with std::less{} as the argument,
        // which introduces branching. While not perfect, this is still better than std::clamp.
        template<typename T>
        constexpr T clamp(T val, T min, T max)
        {
            return std::max(min, std::min(max, val));
        }

        static uint64_t XXH3_len_32_64b(const void* data) noexcept;

        // AtlasEngine.cpp
        [[nodiscard]] HRESULT _handleException(const wil::ResultException& exception) noexcept;
        __declspec(noinline) void _createResources();
        __declspec(noinline) void _recreateSizeDependentResources();
        __declspec(noinline) void _recreateFontDependentResources();
        HRESULT _createTextFormat(const wchar_t* fontFamilyName, DWRITE_FONT_WEIGHT fontWeight, DWRITE_FONT_STYLE fontStyle, float fontSize, _COM_Outptr_ IDWriteTextFormat** textFormat) const noexcept;
        IDWriteTextFormat* _getTextFormat(bool bold, bool italic) const noexcept;
        Cell* _getCell(u16 x, u16 y) noexcept;
        void _setCellFlags(SMALL_RECT coords, CellFlags mask, CellFlags bits) noexcept;
        u16x2 _allocateAtlasTile() noexcept;
        void _emplaceGlyph(IDWriteFontFace* fontFace, const wchar_t* key, size_t keyLen, u16 y, u16 x1, u16 x2);

        // AtlasEngine.r.cpp
        void _setShaderResources() const noexcept;
        void _updateConstantBuffer() const noexcept;
        void _adjustAtlasSize();
        void _processGlyphQueue();
        void _drawGlyph(const til::pair<AtlasKey, AtlasValue>& pair) const;
        void _drawCursor() const;
        void _copyScratchpadTile(uint32_t scratchpadIndex, u16x2 target, uint32_t copyFlags = 0) const noexcept;

        static constexpr bool debugGlyphGenerationPerformance = false;
        static constexpr bool debugGeneralPerformance = false || debugGlyphGenerationPerformance;

        static constexpr u16 u16min = 0x0000;
        static constexpr u16 u16max = 0xffff;
        static constexpr i16 i16min = -0x8000;
        static constexpr i16 i16max = 0x7fff;
        static constexpr u16r invalidatedAreaNone = { u16max, u16max, u16min, u16min };
        static constexpr u16x2 invalidatedRowsNone{ u16max, u16min };
        static constexpr u16x2 invalidatedRowsAll{ u16min, u16max };

        struct StaticResources
        {
            wil::com_ptr<ID2D1Factory> d2dFactory;
            wil::com_ptr<IDWriteFactory> dwriteFactory;
            wil::com_ptr<IDWriteFontFallback> systemFontFallback;
            wil::com_ptr<IDWriteTextAnalyzer1> textAnalyzer;
            bool isWindows10OrGreater = true;
        } _sr;

        struct Resources
        {
            // D3D resources
            wil::com_ptr<ID3D11Device> device;
            wil::com_ptr<ID3D11DeviceContext1> deviceContext;
            wil::com_ptr<IDXGISwapChain1> swapChain;
            wil::unique_handle frameLatencyWaitableObject;
            wil::com_ptr<ID3D11RenderTargetView> renderTargetView;
            wil::com_ptr<ID3D11VertexShader> vertexShader;
            wil::com_ptr<ID3D11PixelShader> pixelShader;
            wil::com_ptr<ID3D11Buffer> constantBuffer;
            wil::com_ptr<ID3D11Buffer> cellBuffer;
            wil::com_ptr<ID3D11ShaderResourceView> cellView;

            // D2D resources
            wil::com_ptr<ID3D11Texture2D> atlasBuffer;
            wil::com_ptr<ID3D11ShaderResourceView> atlasView;
            wil::com_ptr<ID3D11Texture2D> atlasScratchpad;
            wil::com_ptr<ID2D1RenderTarget> d2dRenderTarget;
            wil::com_ptr<ID2D1Brush> brush;
            wil::com_ptr<IDWriteTextFormat> textFormats[2][2];
            wil::com_ptr<IDWriteTypography> typography;

            AlignedBuffer<Cell> cells; // invalidated by ApiInvalidations::Size
            f32x2 cellSizeDIP; // invalidated by ApiInvalidations::Font, caches _api.cellSize but in DIP
            u16x2 cellSize; // invalidated by ApiInvalidations::Font, caches _api.cellSize
            u16x2 cellCount; // invalidated by ApiInvalidations::Font|size, caches _api.cellCount
            u16x2 atlasSizeInPixelLimit; // invalidated by ApiInvalidations::Font
            u16x2 atlasSizeInPixel; // invalidated by ApiInvalidations::Font
            u16x2 atlasPosition;
            std::unordered_map<AtlasKey, AtlasValue, AtlasKeyHasher> glyphs;
            std::vector<til::pair<AtlasKey, AtlasValue>> glyphQueue;

            u32 backgroundColor = 0xff000000;
            u32 selectionColor = 0x7fffffff;

            CachedCursorOptions cursorOptions;
            RenderInvalidations invalidations = RenderInvalidations::None;

#ifndef NDEBUG
            // See documentation for IDXGISwapChain2::GetFrameLatencyWaitableObject method:
            // > For every frame it renders, the app should wait on this handle before starting any rendering operations.
            // > Note that this requirement includes the first frame the app renders with the swap chain.
            bool frameLatencyWaitableObjectUsed = false;
#endif
        } _r;

        struct ApiState
        {
            // This structure is loosely sorted in chunks from "very often accessed together"
            // to seldom accessed and/or usually not together. Try to sorta stick with this.

            std::vector<wchar_t> bufferLine;
            std::vector<u16> bufferLinePos;
            std::vector<TextAnalyzerResult> analysisResults;
            std::vector<UINT16> clusterMap;
            std::vector<DWRITE_SHAPING_TEXT_PROPERTIES> textProps;
            std::vector<UINT16> glyphIndices;
            std::vector<DWRITE_SHAPING_GLYPH_PROPERTIES> glyphProps;
            std::vector<DWRITE_FONT_FEATURE> fontFeatures; // changes are flagged as ApiInvalidations::Font|Size

            // UpdateDrawingBrushes()
            u32 backgroundOpaqueMixin = 0xff000000; // changes are flagged as ApiInvalidations::Device
            u32x2 currentColor{};
            AtlasKeyAttributes attributes{};
            CellFlags flags = CellFlags::None;
            // SetSelectionBackground()
            u32 selectionColor = 0x7fffffff;

            // dirtyRect is a computed value based on invalidatedRows.
            til::rectangle dirtyRect;
            // These "invalidation" fields are reset in EndPaint()
            u16r invalidatedCursorArea = invalidatedAreaNone;
            u16x2 invalidatedRows = invalidatedRowsNone; // x is treated as "top" and y as "bottom"
            i16 scrollOffset = 0;

            u16x2 cellSize; // changes are flagged as ApiInvalidations::Font
            u16x2 cellCount; // caches `sizeInPixel / cellSize`
            u16x2 sizeInPixel; // changes are flagged as ApiInvalidations::Size

            std::vector<DWRITE_FONT_AXIS_VALUE> fontAxisValues; // changes are flagged as ApiInvalidations::Font|Size
            wil::unique_process_heap_string fontName; // changes are flagged as ApiInvalidations::Font|Size
            u16 fontSize = 0; // changes are flagged as ApiInvalidations::Font|Size
            u16 fontWeight = 0; // changes are flagged as ApiInvalidations::Font
            u16 dpi = USER_DEFAULT_SCREEN_DPI; // changes are flagged as ApiInvalidations::Font|Size
            u16 antialiasingMode = D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE; // changes are flagged as ApiInvalidations::Font

            std::function<void(HRESULT)> warningCallback;
            std::function<void()> swapChainChangedCallback;
            wil::unique_handle swapChainHandle;
            HWND hwnd = nullptr;

            ApiInvalidations invalidations = ApiInvalidations::Device;
        } _api;

#undef ATLAS_POD_OPS
#undef ATLAS_FLAG_OPS
    };
}
