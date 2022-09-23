// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <d2d1_1.h>
#include <d3d11_1.h>
#include <dwrite_3.h>

#include <til/hash.h>
#include <til/small_vector.h>

#include "../../renderer/inc/IRenderEngine.hpp"
#include "DWriteTextAnalysis.h"
#include "stb_rect_pack.h"

namespace Microsoft::Console::Render
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
        [[nodiscard]] std::wstring_view GetPixelShaderPath() noexcept override;
        [[nodiscard]] bool GetRetroTerminalEffect() const noexcept override;
        [[nodiscard]] float GetScaling() const noexcept override;
        [[nodiscard]] Types::Viewport GetViewportInCharacters(const Types::Viewport& viewInPixels) const noexcept override;
        [[nodiscard]] Types::Viewport GetViewportInPixels(const Types::Viewport& viewInCharacters) const noexcept override;
        // DxRenderer - setter
        void SetAntialiasingMode(D2D1_TEXT_ANTIALIAS_MODE antialiasingMode) noexcept override;
        void SetCallback(std::function<void(HANDLE)> pfn) noexcept override;
        void EnableTransparentBackground(const bool isTransparent) noexcept override;
        void SetForceFullRepaintRendering(bool enable) noexcept override;
        [[nodiscard]] HRESULT SetHwnd(HWND hwnd) noexcept override;
        void SetPixelShaderPath(std::wstring_view value) noexcept override;
        void SetRetroTerminalEffect(bool enable) noexcept override;
        void SetSelectionBackground(COLORREF color, float alpha = 0.5f) noexcept override;
        void SetSoftwareRendering(bool enable) noexcept override;
        void SetWarningCallback(std::function<void(HRESULT)> pfn) noexcept override;
        [[nodiscard]] HRESULT SetWindowSize(til::size pixels) noexcept override;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& pfiFontInfoDesired, FontInfo& fiFontInfo, const std::unordered_map<std::wstring_view, uint32_t>& features, const std::unordered_map<std::wstring_view, float>& axes) noexcept override;
        void UpdateHyperlinkHoveredId(uint16_t hoveredId) noexcept override;

        // Some helper classes for the implementation.
        // public because I don't want to sprinkle the code with friends.
    public:
#define ATLAS_POD_OPS(type)                                           \
    constexpr auto operator<=>(const type&) const noexcept = default; \
                                                                      \
    constexpr bool operator==(const type& rhs) const noexcept         \
    {                                                                 \
        if constexpr (std::has_unique_object_representations_v<type>) \
        {                                                             \
            return __builtin_memcmp(this, &rhs, sizeof(rhs)) == 0;    \
        }                                                             \
        else                                                          \
        {                                                             \
            return std::is_eq(*this <=> rhs);                         \
        }                                                             \
    }                                                                 \
                                                                      \
    constexpr bool operator!=(const type& rhs) const noexcept         \
    {                                                                 \
        return !(*this == rhs);                                       \
    }

#define ATLAS_FLAG_OPS(type, underlying)                                                                                                                    \
    friend constexpr type operator~(type v) noexcept { return static_cast<type>(~static_cast<underlying>(v)); }                                             \
    friend constexpr type operator|(type lhs, type rhs) noexcept { return static_cast<type>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs)); } \
    friend constexpr type operator&(type lhs, type rhs) noexcept { return static_cast<type>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs)); } \
    friend constexpr type operator^(type lhs, type rhs) noexcept { return static_cast<type>(static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs)); } \
    friend constexpr void operator|=(type& lhs, type rhs) noexcept { lhs = lhs | rhs; }                                                                     \
    friend constexpr void operator&=(type& lhs, type rhs) noexcept { lhs = lhs & rhs; }                                                                     \
    friend constexpr void operator^=(type& lhs, type rhs) noexcept { lhs = lhs ^ rhs; }

        template<typename T>
        struct vec2
        {
            T x{};
            T y{};

            ATLAS_POD_OPS(vec2)
        };

        template<typename T>
        struct vec3
        {
            T x{};
            T y{};
            T z{};

            ATLAS_POD_OPS(vec3)
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

            constexpr bool non_empty() const noexcept
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
        using i32x2 = vec2<i32>;

        using f32 = float;
        using f32x2 = vec2<f32>;
        using f32x3 = vec3<f32>;
        using f32x4 = vec4<f32>;
        using f32r = rect<f32>;

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
        // I wrote `Buffer` instead of using `std::vector`, because I want to convey that these things
        // explicitly _don't_ hold resizeable contents, but rather plain content of a fixed size.
        // For instance I didn't want a resizeable vector with a `push_back` method for my fixed-size
        // viewport arrays - that doesn't make sense after all. `Buffer` also doesn't initialize
        // contents to zero, allowing rapid creation/destruction and you can easily specify a custom
        // (over-)alignment which can improve rendering perf by up to ~20% over `std::vector`.
        template<typename T, size_t Alignment = alignof(T)>
        struct Buffer
        {
            constexpr Buffer() noexcept = default;

            explicit Buffer(size_t size) :
                _data{ allocate(size) },
                _size{ size }
            {
                std::uninitialized_default_construct_n(_data, size);
            }

            Buffer(const T* data, size_t size) :
                _data{ allocate(size) },
                _size{ size }
            {
                // Changing the constructor arguments to accept std::span might
                // be a good future extension, but not to improve security here.
                // You can trivially construct std::span's from invalid ranges.
                // Until then the raw-pointer style is more practical.
#pragma warning(suppress : 26459) // You called an STL function '...' with a raw pointer parameter at position '3' that may be unsafe [...].
                std::uninitialized_copy_n(data, size, _data);
            }

            ~Buffer()
            {
                destroy();
            }

            Buffer(Buffer&& other) noexcept :
                _data{ std::exchange(other._data, nullptr) },
                _size{ std::exchange(other._size, 0) }
            {
            }

#pragma warning(suppress : 26432) // If you define or delete any default operation in the type '...', define or delete them all (c.21).
            Buffer& operator=(Buffer&& other) noexcept
            {
                destroy();
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

            T* begin() noexcept
            {
                return _data;
            }

            T* begin() const noexcept
            {
                return _data;
            }

            T* end() noexcept
            {
                return _data + _size;
            }

            T* end() const noexcept
            {
                return _data + _size;
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

            void destroy() noexcept
            {
                std::destroy_n(_data, _size);
                deallocate(_data);
            }

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

            SmallObjectOptimizer(const SmallObjectOptimizer& other) = delete;
            SmallObjectOptimizer& operator=(const SmallObjectOptimizer& other) = delete;

            SmallObjectOptimizer(SmallObjectOptimizer&& other) noexcept
            {
                memcpy(this, &other, std::max(sizeof(allocated), sizeof(inlined)));
                other.allocated = nullptr;
            }

            SmallObjectOptimizer& operator=(SmallObjectOptimizer&& other) noexcept
            {
                std::destroy_at(this);
                return *std::construct_at(this, std::move(other));
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
            wil::com_ptr<IDWriteFontFamily> fontFamily;
            std::wstring fontName;
            f32 fontSizeInDIP = 0;
            f32 advanceScale = 0;
            f32 baselineInDIP = 0;
            u16 baseline = 0;
            u16x2 cellSize;
            u16 fontWeight = 0;
            u16 underlinePos = 0;
            u16 underlineWidth = 0;
            u16 strikethroughPos = 0;
            u16 strikethroughWidth = 0;
            u16x2 doubleUnderlinePos;
            u16 thinLineWidth = 0;
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

        struct AtlasKeyAttributes
        {
            bool bold = false;
            bool italic = false;

            ATLAS_POD_OPS(AtlasKeyAttributes)
        };

        struct CachedCursorOptions
        {
            u32 cursorColor = INVALID_COLOR;
            u16 cursorType = gsl::narrow_cast<u16>(CursorType::Legacy);
            u8 heightPercentage = 20;
            u8 _padding = 0;

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
            alignas(sizeof(f32x4)) f32x4 positionScale;
            alignas(sizeof(f32x4)) f32 gammaRatios[4]{};
            alignas(sizeof(f32)) f32 cleartypeEnhancedContrast = 0;
            alignas(sizeof(f32)) f32 grayscaleEnhancedContrast = 0;
#pragma warning(suppress : 4324) // 'ConstBuffer': structure was padded due to alignment specifier
        };

        struct alignas(16) CustomConstBuffer
        {
            // WARNING: Same rules as for ConstBuffer above apply.
            alignas(sizeof(f32)) f32 time = 0;
            alignas(sizeof(f32)) f32 scale = 0;
            alignas(sizeof(f32x2)) f32x2 resolution;
            alignas(sizeof(f32x4)) f32x4 background;
#pragma warning(suppress : 4324) // 'CustomConstBuffer': structure was padded due to alignment specifier
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

        struct FontMapping
        {
            wil::com_ptr<IDWriteFontFace> fontFace;
            f32 fontEmSize = 0;
            u32 offset = 0;
        };

        struct ShapedCluster
        {
            u32 offset = 0;
            u32 color = 0;
        };

        struct ShapedRow
        {
            void clear() noexcept
            {
                mappings.clear();
                glyphIndices.clear();
                glyphAdvances.clear();
                glyphOffsets.clear();
                clusters.clear();
            }

            std::vector<FontMapping> mappings;
            std::vector<ShapedCluster> clusters;
            std::vector<u16> glyphIndices;
            std::vector<f32> glyphAdvances; // same size as glyphIndices
            std::vector<DWRITE_GLYPH_OFFSET> glyphOffsets; // same size as glyphIndices
        };

        struct AtlasKey
        {
            wil::com_ptr<IDWriteFontFace> fontFace; // ActiveFaceCache
            til::small_vector<u16, 4> glyphs;

            bool operator==(const AtlasKey& other) const noexcept
            {
                return fontFace == other.fontFace && glyphs == other.glyphs;
            }
        };

        struct AtlasKeyHasher
        {
            size_t operator()(const AtlasKey& v) const noexcept
            {
                til::hasher h;
                h.write(v.fontFace.get());
                h.write(v.glyphs.data(), v.glyphs.size());
                return h.finalize();
            }
        };

        struct AtlasValue
        {
            f32x2 xy;
            f32x2 wh;
            f32x2 offset;
            bool colorGlyph = false;
        };

        struct alignas(16) VertexData
        {
            alignas(sizeof(f32x2)) f32x2 position;
            alignas(sizeof(f32x2)) f32x2 texcoord;
            alignas(sizeof(u32)) u32 color = 0;
            alignas(sizeof(u32)) u32 shadingType = 0;
#pragma warning(suppress : 4324) // 'CustomConstBuffer': structure was padded due to alignment specifier
        };

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
        void _setCellFlags(u16r coords, CellFlags mask, CellFlags bits) noexcept;
        void _flushBufferLine();

        // AtlasEngine.api.cpp
        void _resolveTransparencySettings() noexcept;
        void _updateFont(const wchar_t* faceName, const FontInfoDesired& fontInfoDesired, FontInfo& fontInfo, const std::unordered_map<std::wstring_view, uint32_t>& features, const std::unordered_map<std::wstring_view, float>& axes);
        void _resolveFontMetrics(const wchar_t* faceName, const FontInfoDesired& fontInfoDesired, FontInfo& fontInfo, FontMetrics* fontMetrics = nullptr) const;

        // AtlasEngine.r.cpp
        void _drawGlyphs(const DWRITE_GLYPH_RUN& glyphRun, AtlasValue& value);

        static constexpr bool debugForceD2DMode = false;
        static constexpr bool debugGlyphGenerationPerformance = false;
        static constexpr bool debugTextParsingPerformance = false || debugGlyphGenerationPerformance;
        static constexpr bool debugGeneralPerformance = false || debugTextParsingPerformance;

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
            wil::com_ptr<IDWriteFactory2> dwriteFactory;
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
            // DXGI resources
            wil::com_ptr<IDXGIFactory1> dxgiFactory;

            // D3D resources
            wil::com_ptr<ID3D11Device> device;
            wil::com_ptr<ID3D11DeviceContext1> deviceContext;
            wil::com_ptr<IDXGISwapChain1> swapChain;
            wil::unique_handle frameLatencyWaitableObject;
            wil::com_ptr<ID3D11RenderTargetView> renderTargetView;

            wil::com_ptr<ID3D11VertexShader> vertexShader;
            wil::com_ptr<ID3D11PixelShader> cleartypePixelShader;
            wil::com_ptr<ID3D11PixelShader> grayscalePixelShader;
            wil::com_ptr<ID3D11PixelShader> passthroughPixelShader;
            wil::com_ptr<ID3D11BlendState> cleartypeBlendState;
            wil::com_ptr<ID3D11BlendState> alphaBlendState;

            wil::com_ptr<ID3D11PixelShader> textPixelShader;
            wil::com_ptr<ID3D11BlendState> textBlendState;

            wil::com_ptr<ID3D11PixelShader> wireframePixelShader;
            wil::com_ptr<ID3D11RasterizerState> wireframeRasterizerState;

            wil::com_ptr<ID3D11Buffer> constantBuffer;
            wil::com_ptr<ID3D11InputLayout> textInputLayout;
            wil::com_ptr<ID3D11Buffer> vertexBuffer;

            wil::com_ptr<ID3D11Texture2D> perCellColor;
            wil::com_ptr<ID3D11ShaderResourceView> perCellColorView;

            wil::com_ptr<ID3D11Texture2D> customOffscreenTexture;
            wil::com_ptr<ID3D11ShaderResourceView> customOffscreenTextureView;
            wil::com_ptr<ID3D11RenderTargetView> customOffscreenTextureTargetView;
            wil::com_ptr<ID3D11VertexShader> customVertexShader;
            wil::com_ptr<ID3D11PixelShader> customPixelShader;
            wil::com_ptr<ID3D11Buffer> customShaderConstantBuffer;
            wil::com_ptr<ID3D11SamplerState> customShaderSamplerState;
            std::chrono::steady_clock::time_point customShaderStartTime;

            // D2D resources
            wil::com_ptr<ID3D11Texture2D> atlasBuffer;
            wil::com_ptr<ID3D11ShaderResourceView> atlasView;
            wil::com_ptr<ID2D1DeviceContext> d2dRenderTarget;
            wil::com_ptr<ID2D1SolidColorBrush> brush;
            wil::com_ptr<IDWriteFontFace> fontFaces[4];
            wil::com_ptr<IDWriteTextFormat> textFormats[2][2];
            Buffer<DWRITE_FONT_AXIS_VALUE> textFormatAxes[2][2];
            wil::com_ptr<IDWriteTypography> typography;
            wil::com_ptr<ID2D1StrokeStyle> dottedStrokeStyle;
            
            wil::com_ptr<ID2D1Bitmap> d2dBackgroundBitmap;
            wil::com_ptr<ID2D1BitmapBrush> d2dBackgroundBrush;

            std::unordered_map<AtlasKey, AtlasValue, AtlasKeyHasher> glyphCache;
            std::vector<stbrp_node> rectPackerData;
            stbrp_context rectPacker;
            std::vector<u32> backgroundBitmap;
            std::vector<ShapedRow> rows;
            std::vector<VertexData> vertexData;

            f32x2 cellSizeDIP; // invalidated by ApiInvalidations::Font, caches _api.cellSize but in DIP
            u16x2 cellCount; // invalidated by ApiInvalidations::Font|Size, caches _api.cellCount
            u16 dpi = USER_DEFAULT_SCREEN_DPI; // invalidated by ApiInvalidations::Font, caches _api.dpi
            FontMetrics fontMetrics; // invalidated by ApiInvalidations::Font, cached _api.fontMetrics
            f32 dipPerPixel = 1.0f; // invalidated by ApiInvalidations::Font, caches USER_DEFAULT_SCREEN_DPI / _api.dpi
            f32 pixelPerDIP = 1.0f; // invalidated by ApiInvalidations::Font, caches _api.dpi / USER_DEFAULT_SCREEN_DPI
            u16x2 atlasSizeInPixel; // invalidated by ApiInvalidations::Font

            f32 gamma = 0;
            f32 cleartypeEnhancedContrast = 0;
            f32 grayscaleEnhancedContrast = 0;
            u32 backgroundColor = 0xff000000;
            u32 selectionColor = 0x7fffffff;
            u32 brushColor = 0xffffffff;

            CachedCursorOptions cursorOptions;
            RenderInvalidations invalidations = RenderInvalidations::None;

            til::rect dirtyRect;
            i16 scrollOffset = 0;
            bool d2dMode = false;
            bool waitForPresentation = false;
            bool requiresContinuousRedraw = false;

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
            std::vector<TextAnalysisSinkResult> analysisResults;
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
            u32 backgroundOpaqueMixin = 0xff000000; // changes are flagged as ApiInvalidations::SwapChain
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
            std::function<void(HANDLE)> swapChainChangedCallback;
            wil::unique_handle swapChainHandle;
            HWND hwnd = nullptr;
            u16 dpi = USER_DEFAULT_SCREEN_DPI; // changes are flagged as ApiInvalidations::Font|Size
            u8 antialiasingMode = D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE; // changes are flagged as ApiInvalidations::Font
            u8 realizedAntialiasingMode = D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE; // caches antialiasingMode, depends on antialiasingMode and backgroundOpaqueMixin, see _resolveTransparencySettings
            bool enableTransparentBackground = false;

            std::wstring customPixelShaderPath; // changes are flagged as ApiInvalidations::Device
            bool useRetroTerminalEffect = false; // changes are flagged as ApiInvalidations::Device
            bool useSoftwareRendering = false; // changes are flagged as ApiInvalidations::Device

            ApiInvalidations invalidations = ApiInvalidations::Device;
        } _api;

#undef ATLAS_POD_OPS
#undef ATLAS_FLAG_OPS
    };
}
