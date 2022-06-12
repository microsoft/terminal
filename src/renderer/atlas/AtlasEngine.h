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
        [[nodiscard]] HRESULT PrepareLineTransform(LineRendition lineRendition, size_t targetRow, size_t viewportLeft) noexcept override;
        [[nodiscard]] HRESULT PaintBackground() noexcept override;
        [[nodiscard]] HRESULT PaintBufferLine(gsl::span<const Cluster> clusters, til::point coord, bool fTrimLeft, bool lineWrapped) noexcept override;
        [[nodiscard]] HRESULT PaintBufferGridLines(GridLineSet lines, COLORREF color, size_t cchLine, til::point coordTarget) noexcept override;
        [[nodiscard]] HRESULT PaintSelection(const til::rect& rect) noexcept override;
        [[nodiscard]] HRESULT PaintCursor(const CursorOptions& options) noexcept override;
        [[nodiscard]] HRESULT UpdateDrawingBrushes(const TextAttribute& textAttributes, const RenderSettings& renderSettings, gsl::not_null<IRenderData*> pData, bool usingSoftFont, bool isSettingDefaultBrushes) noexcept override;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& FontInfoDesired, _Out_ FontInfo& FontInfo) noexcept override;
        [[nodiscard]] HRESULT UpdateSoftFont(gsl::span<const uint16_t> bitPattern, til::size cellSize, size_t centeringHint) noexcept override;
        [[nodiscard]] HRESULT UpdateDpi(int iDpi) noexcept override;
        [[nodiscard]] HRESULT UpdateViewport(const til::inclusive_rect& srNewViewport) noexcept override;
        [[nodiscard]] HRESULT GetProposedFont(const FontInfoDesired& FontInfoDesired, _Out_ FontInfo& FontInfo, int iDpi) noexcept override;
        [[nodiscard]] HRESULT GetDirtyArea(gsl::span<const til::rect>& area) noexcept override;
        [[nodiscard]] HRESULT GetFontSize(_Out_ til::size* pFontSize) noexcept override;
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
        void EnableTransparentBackground(const bool isTransparent) noexcept override;
        void SetForceFullRepaintRendering(bool enable) noexcept override;
        [[nodiscard]] HRESULT SetHwnd(HWND hwnd) noexcept override;
        void SetPixelShaderPath(std::wstring_view value) noexcept override;
        void SetRetroTerminalEffect(bool enable) noexcept override;
        void SetSelectionBackground(COLORREF color, float alpha = 0.5f) noexcept override;
        void SetSoftwareRendering(bool enable) noexcept override;
        void SetWarningCallback(std::function<void(HRESULT)> pfn) noexcept override;
        [[nodiscard]] HRESULT SetWindowSize(til::size pixels) noexcept override;
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

        using i32 = int32_t;

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

    private:
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
                return _data != nullptr;
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
            // These two functions don't need to use scoped objects or standard allocators,
            // since this class is in fact an scoped allocator object itself.
#pragma warning(push)
#pragma warning(disable : 26402) // Return a scoped object instead of a heap-allocated if it has a move constructor (r.3).
#pragma warning(disable : 26409) // Avoid calling new and delete explicitly, use std::make_unique<T> instead (r.11).
            static T* allocate(size_t size)
            {
                if constexpr (Alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
                {
                    return static_cast<T*>(::operator new(size * sizeof(T)));
                }
                else
                {
                    return static_cast<T*>(::operator new(size * sizeof(T), static_cast<std::align_val_t>(Alignment)));
                }
            }

            static void deallocate(T* data) noexcept
            {
                if constexpr (Alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
                {
                    ::operator delete(data);
                }
                else
                {
                    ::operator delete(data, static_cast<std::align_val_t>(Alignment));
                }
            }
#pragma warning(pop)

            T* _data = nullptr;
            size_t _size = 0;
        };

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
        template<typename T>
        union SmallObjectOptimizer
        {
            static_assert(std::is_trivially_copyable_v<T>);
            static_assert(std::has_unique_object_representations_v<T>);

            T* allocated = nullptr;
            T inlined;

            constexpr SmallObjectOptimizer() = default;

            SmallObjectOptimizer(const SmallObjectOptimizer& other)
            {
                const auto otherData = other.data();
                const auto otherSize = other.size();
                const auto data = initialize(otherSize);
                memcpy(data, otherData, otherSize);
            }

            SmallObjectOptimizer& operator=(const SmallObjectOptimizer& other)
            {
                if (this != &other)
                {
                    delete this;
                    new (this) SmallObjectOptimizer(other);
                }
                return &this;
            }

            SmallObjectOptimizer(SmallObjectOptimizer&& other) noexcept
            {
                memcpy(this, &other, std::max(sizeof(allocated), sizeof(inlined)));
                other.allocated = nullptr;
            }

            SmallObjectOptimizer& operator=(SmallObjectOptimizer&& other) noexcept
            {
                return *new (this) SmallObjectOptimizer(other);
            }

            ~SmallObjectOptimizer()
            {
                if (!is_inline())
                {
#pragma warning(suppress : 26408) // Avoid malloc() and free(), prefer the nothrow version of new with delete (r.10).
                    free(allocated);
                }
            }

            T* initialize(size_t byteSize)
            {
                if (would_inline(byteSize))
                {
                    return &inlined;
                }

#pragma warning(suppress : 26408) // Avoid malloc() and free(), prefer the nothrow version of new with delete (r.10).
                allocated = THROW_IF_NULL_ALLOC(static_cast<T*>(malloc(byteSize)));
                return allocated;
            }

            constexpr bool would_inline(size_t byteSize) const noexcept
            {
                return byteSize <= sizeof(T);
            }

            bool is_inline() const noexcept
            {
                // VSO-1430353: __builtin_bitcast crashes the compiler under /permissive-. (BODGY)
#pragma warning(suppress : 26490) // Don't use reinterpret_cast (type.1).
                return (reinterpret_cast<uintptr_t>(allocated) & 1) != 0;
            }

            const T* data() const noexcept
            {
                return is_inline() ? &inlined : allocated;
            }

            size_t size() const noexcept
            {
                return is_inline() ? sizeof(inlined) : _msize(allocated);
            }
        };

        struct FontMetrics
        {
            wil::com_ptr<IDWriteFontCollection> fontCollection;
            wil::unique_process_heap_string fontName;
            float baselineInDIP = 0.0f;
            float fontSizeInDIP = 0.0f;
            u16x2 cellSize;
            u16 fontWeight = 0;
            u16 underlinePos = 0;
            u16 strikethroughPos = 0;
            u16 lineThickness = 0;
        };

        // These flags are shared with shader_ps.hlsl.
        // If you change this be sure to copy it over to shader_ps.hlsl.
        //
        // clang-format off
        enum class CellFlags : u32
        {
            None            = 0x00000000,
            Inlined         = 0x00000001,

            ColoredGlyph    = 0x00000002,

            Cursor          = 0x00000008,
            Selected        = 0x00000010,

            BorderLeft      = 0x00000020,
            BorderTop       = 0x00000040,
            BorderRight     = 0x00000080,
            BorderBottom    = 0x00000100,
            Underline       = 0x00000200,
            UnderlineDotted = 0x00000400,
            UnderlineDouble = 0x00000800,
            Strikethrough   = 0x00001000,
        };
        // clang-format on
        ATLAS_FLAG_OPS(CellFlags, u32)

        // This structure is shared with the GPU shader and needs to follow certain alignment rules.
        // You can generally assume that only u32 or types of that alignment are allowed.
        struct Cell
        {
            alignas(u32) u16x2 tileIndex;
            alignas(u32) CellFlags flags = CellFlags::None;
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

        struct AtlasKey
        {
            AtlasKey(AtlasKeyAttributes attributes, u16 charCount, const wchar_t* chars)
            {
                const auto size = dataSize(charCount);
                const auto data = _data.initialize(size);
                attributes.inlined = _data.would_inline(size);
                data->attributes = attributes;
                data->charCount = charCount;
                memcpy(&data->chars[0], chars, static_cast<size_t>(charCount) * sizeof(AtlasKeyData::chars[0]));
            }

            const AtlasKeyData* data() const noexcept
            {
                return _data.data();
            }

            size_t hash() const noexcept
            {
                const auto d = data();
#pragma warning(suppress : 26490) // Don't use reinterpret_cast (type.1).
                return std::_Fnv1a_append_bytes(std::_FNV_offset_basis, reinterpret_cast<const u8*>(d), dataSize(d->charCount));
            }

            bool operator==(const AtlasKey& rhs) const noexcept
            {
                const auto a = data();
                const auto b = rhs.data();
                return a->charCount == b->charCount && memcmp(a, b, dataSize(a->charCount)) == 0;
            }

        private:
            SmallObjectOptimizer<AtlasKeyData> _data;

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

        struct AtlasValue
        {
            constexpr AtlasValue() = default;

            u16x2* initialize(CellFlags flags, u16 cellCount)
            {
                const auto size = dataSize(cellCount);
                const auto data = _data.initialize(size);
                WI_SetFlagIf(flags, CellFlags::Inlined, _data.would_inline(size));
                data->flags = flags;
                return &data->coords[0];
            }

            const AtlasValueData* data() const noexcept
            {
                return _data.data();
            }

        private:
            SmallObjectOptimizer<AtlasValueData> _data;

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
            u8 heightPercentage = 20;

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
            // * bool will probably not work the way you want it to,
            //   because HLSL uses 32-bit bools and C++ doesn't.
            alignas(sizeof(f32x4)) f32x4 viewport;
            alignas(sizeof(f32x4)) f32 gammaRatios[4]{};
            alignas(sizeof(f32)) f32 enhancedContrast = 0;
            alignas(sizeof(u32)) u32 cellCountX = 0;
            alignas(sizeof(u32x2)) u32x2 cellSize;
            alignas(sizeof(u32x2)) u32x2 underlinePos;
            alignas(sizeof(u32x2)) u32x2 strikethroughPos;
            alignas(sizeof(u32)) u32 backgroundColor = 0;
            alignas(sizeof(u32)) u32 cursorColor = 0;
            alignas(sizeof(u32)) u32 selectionColor = 0;
            alignas(sizeof(u32)) u32 useClearType = 0;
#pragma warning(suppress : 4324) // 'ConstBuffer': structure was padded due to alignment specifier
        };

        // Handled in BeginPaint()
        enum class ApiInvalidations : u8
        {
            None = 0,
            Title = 1 << 0,
            Device = 1 << 1,
            SwapChain = 1 << 2,
            Size = 1 << 3,
            Font = 1 << 4,
            Settings = 1 << 5,
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
        static constexpr T clamp(T val, T min, T max)
        {
            return std::max(min, std::min(max, val));
        }

        // AtlasEngine.cpp
        [[nodiscard]] HRESULT _handleException(const wil::ResultException& exception) noexcept;
        __declspec(noinline) void _createResources();
        void _releaseSwapChain();
        __declspec(noinline) void _createSwapChain();
        __declspec(noinline) void _recreateSizeDependentResources();
        __declspec(noinline) void _recreateFontDependentResources();
        IDWriteTextFormat* _getTextFormat(bool bold, bool italic) const noexcept;
        const Buffer<DWRITE_FONT_AXIS_VALUE>& _getTextFormatAxis(bool bold, bool italic) const noexcept;
        Cell* _getCell(u16 x, u16 y) noexcept;
        void _setCellFlags(u16r coords, CellFlags mask, CellFlags bits) noexcept;
        u16x2 _allocateAtlasTile() noexcept;
        void _flushBufferLine();
        void _emplaceGlyph(IDWriteFontFace* fontFace, size_t bufferPos1, size_t bufferPos2);

        // AtlasEngine.api.cpp
        void _resolveAntialiasingMode() noexcept;
        void _updateFont(const wchar_t* faceName, const FontInfoDesired& fontInfoDesired, FontInfo& fontInfo, const std::unordered_map<std::wstring_view, uint32_t>& features, const std::unordered_map<std::wstring_view, float>& axes);
        void _resolveFontMetrics(const wchar_t* faceName, const FontInfoDesired& fontInfoDesired, FontInfo& fontInfo, FontMetrics* fontMetrics = nullptr) const;

        // AtlasEngine.r.cpp
        void _setShaderResources() const;
        void _updateConstantBuffer() const noexcept;
        void _adjustAtlasSize();
        void _reserveScratchpadSize(u16 minWidth);
        void _processGlyphQueue();
        void _drawGlyph(const AtlasQueueItem& item) const;
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
            std::filesystem::path sourceDirectory;
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
            u16 underlinePos = 0;
            u16 strikethroughPos = 0;
            u16 lineThickness = 0;
            u16 dpi = USER_DEFAULT_SCREEN_DPI; // invalidated by ApiInvalidations::Font, caches _api.dpi
            u16 maxEncounteredCellCount = 0;
            u16 scratchpadCellWidth = 0;
            u16x2 atlasSizeInPixelLimit; // invalidated by ApiInvalidations::Font
            u16x2 atlasSizeInPixel; // invalidated by ApiInvalidations::Font
            u16x2 atlasPosition;
            std::unordered_map<AtlasKey, AtlasValue, AtlasKeyHasher> glyphs;
            std::vector<AtlasQueueItem> glyphQueue;

            f32 gamma = 0;
            f32 cleartypeEnhancedContrast = 0;
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
            // to seldom accessed and/or usually not together.

            std::vector<wchar_t> bufferLine;
            std::vector<u16> bufferLineColumn;
            Buffer<BufferLineMetadata> bufferLineMetadata;
            std::vector<TextAnalyzerResult> analysisResults;
            Buffer<u16> clusterMap;
            Buffer<DWRITE_SHAPING_TEXT_PROPERTIES> textProps;
            Buffer<u16> glyphIndices;
            Buffer<DWRITE_SHAPING_GLYPH_PROPERTIES> glyphProps;
            Buffer<f32> glyphAdvances;
            Buffer<DWRITE_GLYPH_OFFSET> glyphOffsets;
            std::vector<DWRITE_FONT_FEATURE> fontFeatures; // changes are flagged as ApiInvalidations::Font|Size
            std::vector<DWRITE_FONT_AXIS_VALUE> fontAxisValues; // changes are flagged as ApiInvalidations::Font|Size
            FontMetrics fontMetrics; // changes are flagged as ApiInvalidations::Font|Size

            u16x2 cellCount; // caches `sizeInPixel / cellSize`
            u16x2 sizeInPixel; // changes are flagged as ApiInvalidations::Size

            // UpdateDrawingBrushes()
            u32 backgroundOpaqueMixin = 0xff000000; // changes are flagged as ApiInvalidations::Device
            u32x2 currentColor;
            AtlasKeyAttributes attributes{};
            u16x2 lastPaintBufferLineCoord;
            CellFlags flags = CellFlags::None;
            // SetSelectionBackground()
            u32 selectionColor = 0x7fffffff;
            // UpdateHyperlinkHoveredId()
            u16 hyperlinkHoveredId = 0;
            bool bufferLineWasHyperlinked = false;

            // dirtyRect is a computed value based on invalidatedRows.
            til::rect dirtyRect;
            // These "invalidation" fields are reset in EndPaint()
            u16r invalidatedCursorArea = invalidatedAreaNone;
            u16x2 invalidatedRows = invalidatedRowsNone; // x is treated as "top" and y as "bottom"
            i16 scrollOffset = 0;

            std::function<void(HRESULT)> warningCallback;
            std::function<void()> swapChainChangedCallback;
            wil::unique_handle swapChainHandle;
            HWND hwnd = nullptr;
            u16 dpi = USER_DEFAULT_SCREEN_DPI; // changes are flagged as ApiInvalidations::Font|Size
            u8 antialiasingMode = D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE; // changes are flagged as ApiInvalidations::Font
            u8 realizedAntialiasingMode = D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE; // caches antialiasingMode, depends on antialiasingMode and backgroundOpaqueMixin, see _resolveAntialiasingMode

            ApiInvalidations invalidations = ApiInvalidations::Device;
        } _api;

#undef ATLAS_POD_OPS
#undef ATLAS_FLAG_OPS
    };
}
