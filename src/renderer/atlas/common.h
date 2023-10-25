// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <til/generational.h>

#include "../../renderer/inc/IRenderEngine.hpp"

namespace Microsoft::Console::Render::Atlas
{
#define ATLAS_FLAG_OPS(type, underlying)                                                       \
    constexpr type operator~(type v) noexcept                                                  \
    {                                                                                          \
        return static_cast<type>(~static_cast<underlying>(v));                                 \
    }                                                                                          \
    constexpr type operator|(type lhs, type rhs) noexcept                                      \
    {                                                                                          \
        return static_cast<type>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs)); \
    }                                                                                          \
    constexpr type operator&(type lhs, type rhs) noexcept                                      \
    {                                                                                          \
        return static_cast<type>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs)); \
    }                                                                                          \
    constexpr type operator^(type lhs, type rhs) noexcept                                      \
    {                                                                                          \
        return static_cast<type>(static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs)); \
    }                                                                                          \
    constexpr void operator|=(type& lhs, type rhs) noexcept                                    \
    {                                                                                          \
        lhs = lhs | rhs;                                                                       \
    }                                                                                          \
    constexpr void operator&=(type& lhs, type rhs) noexcept                                    \
    {                                                                                          \
        lhs = lhs & rhs;                                                                       \
    }                                                                                          \
    constexpr void operator^=(type& lhs, type rhs) noexcept                                    \
    {                                                                                          \
        lhs = lhs ^ rhs;                                                                       \
    }

#define ATLAS_POD_OPS(type)                                    \
    constexpr bool operator==(const type& rhs) const noexcept  \
    {                                                          \
        return __builtin_memcmp(this, &rhs, sizeof(rhs)) == 0; \
    }                                                          \
                                                               \
    constexpr bool operator!=(const type& rhs) const noexcept  \
    {                                                          \
        return !(*this == rhs);                                \
    }

    // My best effort of replicating __attribute__((cold)) from gcc/clang.
#define ATLAS_ATTR_COLD __declspec(noinline)

    template<typename T>
    struct vec2
    {
        // These members aren't zero-initialized to make these trivial types,
        // and allow the compiler to quickly memset() allocations, etc.
        T x;
        T y;

        ATLAS_POD_OPS(vec2)
    };

    template<typename T>
    struct vec4
    {
        // These members aren't zero-initialized to make these trivial types,
        // and allow the compiler to quickly memset() allocations, etc.
        T x;
        T y;
        T z;
        T w;

        ATLAS_POD_OPS(vec4)
    };

    template<typename T>
    struct rect
    {
        // These members aren't zero-initialized to make these trivial types,
        // and allow the compiler to quickly memset() allocations, etc.
        T left;
        T top;
        T right;
        T bottom;

        ATLAS_POD_OPS(rect)

        constexpr bool empty() const noexcept
        {
            return left >= right || top >= bottom;
        }

        constexpr bool non_empty() const noexcept
        {
            return left < right && top < bottom;
        }
    };

    template<typename T>
    struct range
    {
        T start;
        T end;

        ATLAS_POD_OPS(range)

        constexpr bool empty() const noexcept
        {
            return start >= end;
        }

        constexpr bool non_empty() const noexcept
        {
            return start < end;
        }

        constexpr bool contains(T v) const noexcept
        {
            return v >= start && v < end;
        }
    };

    using u8 = uint8_t;

    using u16 = uint16_t;
    using u16x2 = vec2<u16>;
    using u16r = rect<u16>;

    using i16 = int16_t;
    using i16x2 = vec2<i16>;
    using i16x4 = vec4<i16>;
    using i16r = rect<i16>;

    using u32 = uint32_t;
    using u32x2 = vec2<u32>;
    using u32x4 = vec4<u32>;
    using u32r = rect<u32>;

    using i32 = int32_t;
    using i32x2 = vec2<i32>;
    using i32x4 = vec4<i32>;
    using i32r = rect<i32>;

    using f32 = float;
    using f32x2 = vec2<f32>;
    using f32x4 = vec4<f32>;
    using f32r = rect<f32>;

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
#pragma warning(suppress : 26459) // You called an STL function '...' with a raw pointer parameter at position '...' that may be unsafe [...].
            std::uninitialized_copy_n(data, size, _data);
        }

        ~Buffer()
        {
            destroy();
        }

        Buffer(const Buffer& other) = delete;
        Buffer& operator=(const Buffer& other) = delete;

        Buffer(Buffer&& other) noexcept :
            _data{ std::exchange(other._data, nullptr) },
            _size{ std::exchange(other._size, 0) }
        {
        }

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
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
            return _data[index];
        }

        const T& operator[](size_t index) const noexcept
        {
            assert(index < _size);
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
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

        const T* begin() const noexcept
        {
            return _data;
        }

        T* end() noexcept
        {
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
            return _data + _size;
        }

        const T* end() const noexcept
        {
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
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
            if (!size)
            {
                return nullptr;
            }
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

    struct TextAnalysisSinkResult
    {
        uint32_t textPosition;
        uint32_t textLength;
        DWRITE_SCRIPT_ANALYSIS analysis;
    };

    struct TargetSettings
    {
        HWND hwnd = nullptr;
        bool enableTransparentBackground = false;
        bool useSoftwareRendering = false;
    };

    enum class AntialiasingMode : u8
    {
        ClearType = D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE,
        Grayscale = D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE,
        Aliased = D2D1_TEXT_ANTIALIAS_MODE_ALIASED,
    };

    inline constexpr auto DefaultAntialiasingMode = AntialiasingMode::ClearType;

    struct FontDecorationPosition
    {
        u16 position = 0;
        u16 height = 0;
    };

    struct FontSettings
    {
        wil::com_ptr<IDWriteFontCollection> fontCollection;
        wil::com_ptr<IDWriteFontFamily> fontFamily;
        std::wstring fontName;
        std::vector<DWRITE_FONT_FEATURE> fontFeatures;
        std::vector<DWRITE_FONT_AXIS_VALUE> fontAxisValues;
        f32 fontSize = 0;
        u16x2 cellSize;
        u16 fontWeight = 0;
        u16 advanceWidth = 0;
        u16 baseline = 0;
        u16 descender = 0;
        u16 thinLineWidth = 0;

        FontDecorationPosition gridTop;
        FontDecorationPosition gridBottom;
        FontDecorationPosition gridLeft;
        FontDecorationPosition gridRight;

        FontDecorationPosition underline;
        FontDecorationPosition strikethrough;
        FontDecorationPosition doubleUnderline[2];
        FontDecorationPosition overline;

        u16 dpi = 96;
        AntialiasingMode antialiasingMode = DefaultAntialiasingMode;

        std::vector<uint16_t> softFontPattern;
        til::size softFontCellSize;
    };

    struct CursorSettings
    {
        ATLAS_POD_OPS(CursorSettings)

        u32 cursorColor = 0xffffffff;
        u16 cursorType = 0;
        u16 heightPercentage = 20;
    };

    struct MiscellaneousSettings
    {
        u32 backgroundColor = 0;
        u32 selectionColor = 0x7fffffff;
        std::wstring customPixelShaderPath;
        bool useRetroTerminalEffect = false;
    };

    struct Settings
    {
        til::generational<TargetSettings> target;
        til::generational<FontSettings> font;
        til::generational<CursorSettings> cursor;
        til::generational<MiscellaneousSettings> misc;
        // Size of the viewport / swap chain in pixel.
        u16x2 targetSize{ 1, 1 };
        // Size of the portion of the text buffer that we're drawing on the screen.
        u16x2 viewportCellCount{ 1, 1 };
        // The position of the viewport inside the text buffer (in cells).
        u16x2 viewportOffset{ 0, 0 };
    };

    using GenerationalSettings = til::generational<Settings>;

    inline GenerationalSettings DirtyGenerationalSettings() noexcept
    {
        return GenerationalSettings{
            til::generation_t{ 1 },
            til::generational<TargetSettings>{ til::generation_t{ 1 } },
            til::generational<FontSettings>{ til::generation_t{ 1 } },
            til::generational<CursorSettings>{ til::generation_t{ 1 } },
            til::generational<MiscellaneousSettings>{ til::generation_t{ 1 } },
        };
    }

    enum class FontRelevantAttributes : u8
    {
        None = 0,
        Bold = 0b01,
        Italic = 0b10,
    };
    ATLAS_FLAG_OPS(FontRelevantAttributes, u8)

    struct FontMapping
    {
        wil::com_ptr<IDWriteFontFace2> fontFace;
        u32 glyphsFrom = 0;
        u32 glyphsTo = 0;
    };

    struct GridLineRange
    {
        GridLineSet lines;
        u32 color = 0;
        u16 from = 0;
        u16 to = 0;
    };

    struct ShapedRow
    {
        void Clear(u16 y, u16 cellHeight) noexcept
        {
            mappings.clear();
            glyphIndices.clear();
            glyphAdvances.clear();
            glyphOffsets.clear();
            colors.clear();
            gridLineRanges.clear();
            lineRendition = LineRendition::SingleWidth;
            selectionFrom = 0;
            selectionTo = 0;
            dirtyTop = y * cellHeight;
            dirtyBottom = dirtyTop + cellHeight;
        }

        std::vector<FontMapping> mappings;
        std::vector<u16> glyphIndices;
        std::vector<f32> glyphAdvances; // same size as glyphIndices
        std::vector<DWRITE_GLYPH_OFFSET> glyphOffsets; // same size as glyphIndices
        std::vector<u32> colors; // same size as glyphIndices
        std::vector<GridLineRange> gridLineRanges;
        LineRendition lineRendition = LineRendition::SingleWidth;
        u16 selectionFrom = 0;
        u16 selectionTo = 0;
        til::CoordType dirtyTop = 0;
        til::CoordType dirtyBottom = 0;
    };

    struct RenderingPayload
    {
        //// Parameters which are constant across backends.
        wil::com_ptr<ID2D1Factory> d2dFactory;
        wil::com_ptr<IDWriteFactory2> dwriteFactory;
        wil::com_ptr<IDWriteFactory4> dwriteFactory4; // optional, might be nullptr
        wil::com_ptr<IDWriteFontFallback> systemFontFallback;
        wil::com_ptr<IDWriteFontFallback1> systemFontFallback1; // optional, might be nullptr
        wil::com_ptr<IDWriteTextAnalyzer1> textAnalyzer;
        wil::com_ptr<IDWriteRenderingParams1> renderingParams;
        std::function<void(HRESULT)> warningCallback;
        std::function<void(HANDLE)> swapChainChangedCallback;

        //// Parameters which are constant for the existence of the backend.
        struct
        {
            wil::com_ptr<IDXGIFactory2> factory;
            wil::com_ptr<IDXGIAdapter1> adapter;
            LUID adapterLuid{};
            UINT adapterFlags = 0;
        } dxgi;
        struct
        {
            wil::com_ptr<IDXGISwapChain2> swapChain;
            wil::unique_handle handle;
            wil::unique_handle frameLatencyWaitableObject;
            til::generation_t generation;
            til::generation_t targetGeneration;
            til::generation_t fontGeneration;
            u16x2 targetSize{};
            bool waitForPresentation = false;
        } swapChain;
        wil::com_ptr<ID3D11Device2> device;
        wil::com_ptr<ID3D11DeviceContext2> deviceContext;

        //// Parameters which change seldom.
        GenerationalSettings s;

        //// Parameters which change every frame.
        // This is the backing buffer for `rows`.
        Buffer<ShapedRow> unorderedRows;
        // This is used as a scratch buffer during scrolling.
        Buffer<ShapedRow*> rowsScratch;
        // This contains the rows in the right order from row 0 to N.
        // They get rotated around when we scroll the buffer. Technically
        // we could also implement scrolling by using a circular array.
        Buffer<ShapedRow*> rows;
        // This contains two viewport-sized bitmaps back to back, sort of like a Texture2DArray.
        // The first NxM (for instance 120x30 pixel) chunk contains background colors and the
        // second chunk contains foreground colors. The distance in u32 items between the start
        // and the begin of the foreground bitmap is equal to colorBitmapDepthStride.
        //
        // The background part is in premultiplied alpha, whereas the foreground part is in straight
        // alpha. This is mostly because of Direct2D being annoying, as the former is the only thing
        // it supports for bitmaps, whereas the latter is the only thing it supports for text.
        // Since we implement Direct2D's text blending algorithm, we're equally dependent on
        // straight alpha for BackendD3D, as straight alpha is used in the pixel shader there.
        Buffer<u32, 32> colorBitmap;
        // This exists as a convenience access to colorBitmap and
        // contains a view into the background color bitmap.
        std::span<u32> backgroundBitmap;
        // This exists as a convenience access to colorBitmap and
        // contains a view into the foreground color bitmap.
        std::span<u32> foregroundBitmap;
        // This stride of the colorBitmap is a "count" of u32 and not in bytes.
        size_t colorBitmapRowStride = 0;
        // FYI depth refers to the `colorBitmapRowStride * height` size of each bitmap contained
        // in colorBitmap. colorBitmap contains 2 bitmaps (background and foreground colors).
        size_t colorBitmapDepthStride = 0;
        // A generation of 1 ensures that the backends redraw the background on the first Present().
        // The 1st entry in this array corresponds to the background and the 2nd to the foreground bitmap.
        std::array<til::generation_t, 2> colorBitmapGenerations{ 1, 1 };
        // In columns/rows.
        til::rect cursorRect;
        // The viewport/SwapChain area to be presented. In pixel.
        // NOTE:
        //   This cannot use til::rect, because til::rect generally expects positive coordinates only
        //   (`operator!()` checks for negative values), whereas this one can go out of bounds,
        //   whenever glyphs go out of bounds. `AtlasEngine::_present()` will clamp it.
        i32r dirtyRectInPx{};
        // In rows.
        range<u16> invalidatedRows{};
        // In pixel.
        i16 scrollOffset = 0;

        void MarkAllAsDirty() noexcept
        {
            dirtyRectInPx = { 0, 0, s->targetSize.x, s->targetSize.y };
            invalidatedRows = { 0, s->viewportCellCount.y };
            scrollOffset = 0;
        }
    };

    struct IBackend
    {
        virtual ~IBackend() = default;
        virtual void ReleaseResources() noexcept = 0;
        virtual void Render(RenderingPayload& payload) = 0;
        virtual bool RequiresContinuousRedraw() noexcept = 0;
    };
}
