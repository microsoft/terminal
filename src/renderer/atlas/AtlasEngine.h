// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <d2d1.h>
#include <d3d11_1.h>
#include <dwrite_3.h>

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

        template<typename T, size_t Alignment = alignof(T)>
        struct Buffer
        {
            constexpr Buffer() noexcept = default;

            explicit Buffer(size_t size) :
                _data{ allocate(size) },
                _size{ size }
            {
            }

            Buffer(const T* data, size_t size) :
                _data{ allocate(size) },
                _size{ size }
            {
                static_assert(std::is_trivially_copyable_v<T>);
                memcpy(_data, data, size * sizeof(T));
            }

            ~Buffer()
            {
                deallocate(_data);
            }

            Buffer(Buffer&& other) noexcept :
                _data{ std::exchange(other._data, nullptr) },
                _size{ std::exchange(other._size, 0) }
            {
            }

#pragma warning(suppress : 26432) // If you define or delete any default operation in the type '...', define or delete them all (c.21).
            Buffer& operator=(Buffer&& other) noexcept
            {
                deallocate(_data);
                _data = std::exchange(other._data, nullptr);
                _size = std::exchange(other._size, 0);
                return *this;
            }

            explicit operator bool() const noexcept
            {
                return _data;
            }

            T& operator[](size_t index) noexcept
            {
                assert(index < _size);
                return _data[index];
            }

            const T& operator[](size_t index) const noexcept
            {
                assert(index < _size);
                return _data[index];
            }

            T* data() noexcept
            {
                return _data;
            }

            const T* data() const noexcept
            {
                return _data;
            }

            size_t size() const noexcept
            {
                return _size;
            }

        private:
            static T* allocate(size_t size)
            {
                if constexpr (Alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
                {
                    return static_cast<T*>(::operator new[](size * sizeof(T)));
                }
                else
                {
                    return static_cast<T*>(::operator new[](size * sizeof(T), static_cast<std::align_val_t>(Alignment)));
                }
            }

            static void deallocate(T* data) noexcept
            {
                if constexpr (Alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
                {
                    ::operator delete[](data);
                }
                else
                {
                    ::operator delete[](data, static_cast<std::align_val_t>(Alignment));
                }
            }

            T* _data = nullptr;
            size_t _size = 0;
        };

    private:
        // u16 so it neatly fits into AtlasValue.
        // These flags are shared with shader_ps.hlsl.
        enum class CellFlags : u32
        {
            None = 0,
            Inlined = 1,

            ColoredGlyph = 2,
            ThinFont = 4,

            Cursor = 8,
            Selected = 16,

            BorderLeft = 32,
            BorderTop = 64,
            BorderRight = 128,
            BorderBottom = 256,
            Underline = 512,
            UnderlineDotted = 1024,
            UnderlineDouble = 2048,
            Strikethrough = 4096,
        };
        ATLAS_FLAG_OPS(CellFlags, u32)

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
            u16 inlined : 1;
            u16 bold : 1;
            u16 italic : 1;
            u16 cellCount : 13;

            ATLAS_POD_OPS(AtlasKeyAttributes)
        };

        struct AtlasKeyData
        {
            AtlasKeyAttributes attributes;
            u16 charCount;
            wchar_t chars[14];
        };

        union AtlasKey
        {
            AtlasKey(AtlasKeyAttributes attributes, u16 charCount, const wchar_t* chars)
            {
                AtlasKeyData* data;

                if (charCount <= std::size(inlined.chars))
                {
                    attributes.inlined = 1;
                    data = &inlined;
                }
                else
                {
                    data = heap = THROW_IF_NULL_ALLOC(static_cast<AtlasKeyData*>(malloc(dataSize(charCount))));
                }

                data->attributes = attributes;
                data->charCount = charCount;
                memcpy(&data->chars[0], chars, static_cast<size_t>(charCount) * sizeof(AtlasKeyData::chars[0]));
            }

            AtlasKey(const AtlasKey& other)
            {
                if (other.inlined.attributes.inlined)
                {
                    memcpy(&inlined, &other.inlined, sizeof(inlined));
                }
                else
                {
                    const auto size = dataSize(other.heap->charCount);
                    heap = THROW_IF_NULL_ALLOC(static_cast<AtlasKeyData*>(malloc(size)));
                    memcpy(heap, other.heap, size);
                }
            }

            AtlasKey(AtlasKey&&) = default;
            AtlasKey& operator=(AtlasKey&&) = default;

            ~AtlasKey()
            {
                if (!inlined.attributes.inlined)
                {
                    free(heap);
                }
            }

            const AtlasKeyData& data() const noexcept
            {
                return inlined.attributes.inlined ? inlined : *heap;
            }

            size_t hash() const noexcept
            {
                const auto& d = data();
                return std::_Fnv1a_append_bytes(std::_FNV_offset_basis, reinterpret_cast<const u8*>(&d), dataSize(d.charCount));
            }

            bool operator==(const AtlasKey& rhs) const noexcept
            {
                const auto& a = data();
                const auto& b = rhs.data();
                return a.charCount == b.charCount && memcmp(&a, &b, dataSize(a.charCount)) == 0;
            }

        private:
            // This structure works similar to how std::string works:
            // You can think of a std::string as a structure consisting of:
            //   char*  data;
            //   size_t size;
            //   size_t capacity;
            // where data is some backing memory allocated on the heap.
            //
            // But std::string employs an optimization called "small string optimization" (SSO).
            // To simplify things it could be explained as:
            // If the string capacity is small, then the characters are stored inside the "data"
            // pointer and you make sure to set the lowest bit in the pointer one way or another.
            // Heap allocations are always aligned by at least 4-8 bytes on any platform.
            // If the address of the "data" pointer is not even you know data is stored inline.
            //
            // We use the same trick here:
            // If we have only few .chars (<= 2) then we store all our fields inside the .heap pointer.
            // Otherwise (.charCount > 2) we allocate an AtlasKey on the heap.
            AtlasKeyData* heap = nullptr;
            AtlasKeyData inlined;

            static constexpr size_t dataSize(u16 charCount) noexcept
            {
                // This returns the actual byte size of a AtlasKeyData struct for the given charCount.
                // The `wchar_t chars[2]` is only a buffer for the inlined variant after
                // all and the actual charCount can be smaller or larger. Due to this we
                // remove the size of the `chars` array and add it's true length on top.
                return sizeof(AtlasKeyData) - sizeof(AtlasKeyData::chars) + static_cast<size_t>(charCount) * sizeof(AtlasKeyData::chars[0]);
            }
        };

        struct AtlasKeyHasher
        {
            size_t operator()(const AtlasKey& key) const noexcept
            {
                return key.hash();
            }
        };

        struct AtlasValueData
        {
            CellFlags flags = CellFlags::None;
            u16x2 coords[7];
        };

        union AtlasValue
        {
            AtlasValue() = default;
            AtlasValue(const AtlasValue& other)
            {
                if (WI_IsFlagSet(other.inlined.flags, CellFlags::Inlined))
                {
                    memcpy(&inlined, &other.inlined, sizeof(inlined));
                }
                else
                {
                    const auto size = _msize(other.heap);
                    heap = THROW_IF_NULL_ALLOC(static_cast<AtlasValueData*>(malloc(size)));
                    memcpy(heap, other.heap, size);
                }
            }

            AtlasValue(AtlasValue&&) = default;
            AtlasValue& operator=(AtlasValue&&) = default;

            ~AtlasValue()
            {
                if (!WI_IsFlagSet(inlined.flags, CellFlags::Inlined))
                {
                    free(heap);
                }
            }

            u16x2* initialize(CellFlags flags, u16 cellCount)
            {
                AtlasValueData* data;

                if (cellCount <= std::size(inlined.coords))
                {
                    WI_SetFlag(flags, CellFlags::Inlined);
                    data = &inlined;
                }
                else
                {
                    data = heap = THROW_IF_NULL_ALLOC(static_cast<AtlasValueData*>(malloc(dataSize(cellCount))));
                }

                data->flags = flags;
                return &data->coords[0];
            }

            const AtlasValueData& data() const noexcept
            {
                return WI_IsFlagSet(inlined.flags, CellFlags::Inlined) ? inlined : *heap;
            }

        private:
            // See AtlasKey for a long description about how this works.
            AtlasValueData* heap = nullptr;
            AtlasValueData inlined;

            static constexpr size_t dataSize(u16 coordCount) noexcept
            {
                return sizeof(AtlasValueData) - sizeof(AtlasValueData::coords) + static_cast<size_t>(coordCount) * sizeof(AtlasValueData::coords[0]);
            }
        };

        struct AtlasQueueItem
        {
            const AtlasKey* key;
            const AtlasValue* value;
        };

        struct CachedCursorOptions
        {
            u32 cursorColor = INVALID_COLOR;
            u16 cursorType = gsl::narrow_cast<u16>(CursorType::Legacy);
            u8 ulCursorHeightPercent = 25;

            ATLAS_POD_OPS(CachedCursorOptions)
        };

        struct BufferLineMetadata
        {
            u32x2 colors;
            CellFlags flags = CellFlags::None;
        };

        // NOTE: D3D constant buffers sizes must be a multiple of 16 bytes.
        struct alignas(16) ConstBuffer
        {
            // WARNING: Modify this carefully after understanding how HLSL struct packing works.
            // The gist is:
            // * Minimum alignment is 4 bytes (like `#pragma pack 4`)
            // * Members cannot straddle 16 byte boundaries
            //   This means a structure like {u32; u32; u32; u32x2} would require
            //   padding so that it is {u32; u32; u32; <4 byte padding>; u32x2}.
            alignas(sizeof(f32x4)) f32x4 viewport;
            alignas(sizeof(u32x2)) u32x2 cellSize;
            alignas(sizeof(u32)) u32 cellCountX = 0;
            alignas(sizeof(f32)) f32 gamma = 0;
            alignas(sizeof(f32)) f32 grayscaleEnhancedContrast = 0;
            alignas(sizeof(u32)) u32 backgroundColor = 0;
            alignas(sizeof(u32)) u32 cursorColor = 0;
            alignas(sizeof(u32)) u32 selectionColor = 0;
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

        // AtlasEngine.cpp
        [[nodiscard]] HRESULT _handleException(const wil::ResultException& exception) noexcept;
        __declspec(noinline) void _createResources();
        __declspec(noinline) void _recreateSizeDependentResources();
        __declspec(noinline) void _recreateFontDependentResources();
        HRESULT _createTextFormat(const wchar_t* fontFamilyName, DWRITE_FONT_WEIGHT fontWeight, DWRITE_FONT_STYLE fontStyle, float fontSize, _COM_Outptr_ IDWriteTextFormat** textFormat) const noexcept;
        IDWriteTextFormat* _getTextFormat(bool bold, bool italic) const noexcept;
        const Buffer<DWRITE_FONT_AXIS_VALUE>& _getTextFormatAxis(bool bold, bool italic) const noexcept;
        Cell* _getCell(u16 x, u16 y) noexcept;
        void _setCellFlags(SMALL_RECT coords, CellFlags mask, CellFlags bits) noexcept;
        u16x2 _allocateAtlasTile() noexcept;
        void _flushBufferLine();
        void _emplaceGlyph(IDWriteFontFace* fontFace, const wchar_t* chars, size_t charCount, u16 x1, u16 x2);

        // AtlasEngine.r.cpp
        void _setShaderResources() const;
        void _updateConstantBuffer() const noexcept;
        void _adjustAtlasSize();
        void _reserveScratchpadSize(u16 minWidth);
        void _processGlyphQueue();
        void _drawGlyph(const AtlasQueueItem& pair) const;
        void _drawCursor();
        void _copyScratchpadTile(uint32_t scratchpadIndex, u16x2 target, uint32_t copyFlags = 0) const noexcept;

        static constexpr bool debugGlyphGenerationPerformance = false;
        static constexpr bool debugGeneralPerformance = false || debugGlyphGenerationPerformance;
        static constexpr bool continuousRedraw = false || debugGeneralPerformance;

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
            wil::com_ptr<IDWriteFactory1> dwriteFactory;
            wil::com_ptr<IDWriteFontFallback> systemFontFallback;
            wil::com_ptr<IDWriteTextAnalyzer1> textAnalyzer;
            bool isWindows10OrGreater = true;

#ifndef NDEBUG
            wil::unique_folder_change_reader_nothrow sourceCodeWatcher;
            std::atomic<int64_t> sourceCodeInvalidationTime{ INT64_MAX };
#endif
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
            Buffer<DWRITE_FONT_AXIS_VALUE> textFormatAxes[2][2];
            wil::com_ptr<IDWriteTypography> typography;

            Buffer<Cell, 32> cells; // invalidated by ApiInvalidations::Size
            f32x2 cellSizeDIP; // invalidated by ApiInvalidations::Font, caches _api.cellSize but in DIP
            u16x2 cellSize; // invalidated by ApiInvalidations::Font, caches _api.cellSize
            u16x2 cellCount; // invalidated by ApiInvalidations::Font|Size, caches _api.cellCount
            u16 dpi = USER_DEFAULT_SCREEN_DPI; // invalidated by ApiInvalidations::Font, caches _api.dpi
            u16 maxEncounteredCellCount = 0;
            u16 scratchpadCellWidth = 0;
            u16x2 atlasSizeInPixelLimit; // invalidated by ApiInvalidations::Font
            u16x2 atlasSizeInPixel; // invalidated by ApiInvalidations::Font
            u16x2 atlasPosition;
            std::unordered_map<AtlasKey, AtlasValue, AtlasKeyHasher> glyphs;
            std::vector<AtlasQueueItem> glyphQueue;

            f32 gamma = 0;
            f32 grayscaleEnhancedContrast = 0;
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
            std::vector<u16> bufferLineColumn;
            Buffer<BufferLineMetadata> bufferLineMetadata;
            std::vector<TextAnalyzerResult> analysisResults;
            Buffer<u16> clusterMap;
            Buffer<DWRITE_SHAPING_TEXT_PROPERTIES> textProps;
            Buffer<u16> glyphIndices;
            Buffer<DWRITE_SHAPING_GLYPH_PROPERTIES> glyphProps;
            std::vector<DWRITE_FONT_FEATURE> fontFeatures; // changes are flagged as ApiInvalidations::Font|Size

            u16x2 cellSize; // changes are flagged as ApiInvalidations::Font
            u16x2 cellCount; // caches `sizeInPixel / cellSize`
            u16x2 sizeInPixel; // changes are flagged as ApiInvalidations::Size

            // UpdateDrawingBrushes()
            u32 backgroundOpaqueMixin = 0xff000000; // changes are flagged as ApiInvalidations::Device
            u32x2 currentColor;
            AtlasKeyAttributes attributes{};
            u16 currentRow = 0;
            CellFlags flags = CellFlags::None;
            // SetSelectionBackground()
            u32 selectionColor = 0x7fffffff;

            // dirtyRect is a computed value based on invalidatedRows.
            til::rectangle dirtyRect;
            // These "invalidation" fields are reset in EndPaint()
            u16r invalidatedCursorArea = invalidatedAreaNone;
            u16x2 invalidatedRows = invalidatedRowsNone; // x is treated as "top" and y as "bottom"
            i16 scrollOffset = 0;

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
