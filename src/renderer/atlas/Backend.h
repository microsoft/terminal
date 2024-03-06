// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "common.h"

namespace Microsoft::Console::Render::Atlas
{
    // Don't use this definition in the code elsewhere.
    // It only exists to make the definitions below possible.
#ifdef NDEBUG
#define ATLAS_DEBUG__IS_DEBUG 0
#else
#define ATLAS_DEBUG__IS_DEBUG 1
#endif

    // If set to 1, this will cause the entire viewport to be invalidated at all times.
    // Helpful for benchmarking our text shaping code based on DirectWrite.
#define ATLAS_DEBUG_DISABLE_PARTIAL_INVALIDATION 0

    // Redraw at display refresh rate at all times. This helps with shader debugging.
#define ATLAS_DEBUG_CONTINUOUS_REDRAW 0

    // Hot reload the builtin .hlsl files whenever they change on disk.
    // Enabled by default in debug builds.
#define ATLAS_DEBUG_SHADER_HOT_RELOAD ATLAS_DEBUG__IS_DEBUG

    // Disables the use of DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT.
    // This helps with benchmarking the application as it'll run beyond display refresh rate.
#define ATLAS_DEBUG_DISABLE_FRAME_LATENCY_WAITABLE_OBJECT 0

    // Forces the use of Direct2D for text rendering (= BackendD2D).
#define ATLAS_DEBUG_FORCE_D2D_MODE 0

    // Adds an artificial delay before every render pass. In milliseconds.
#define ATLAS_DEBUG_RENDER_DELAY 0

    // Shows the dirty rects as given to IDXGISwapChain2::Present1() during each frame.
#define ATLAS_DEBUG_SHOW_DIRTY 0

    // Dumps the contents of the swap chain on each render pass into the given directory as PNG.
    // I highly recommend setting ATLAS_DEBUG_RENDER_DELAY to 250 or similar if this is used.
#define ATLAS_DEBUG_DUMP_RENDER_TARGET 0
#define ATLAS_DEBUG_DUMP_RENDER_TARGET_PATH LR"(%USERPROFILE%\Downloads\AtlasEngine)"

    template<typename T = D2D1_COLOR_F>
    constexpr T colorFromU32(u32 rgba)
    {
        const auto r = static_cast<f32>((rgba >> 0) & 0xff) / 255.0f;
        const auto g = static_cast<f32>((rgba >> 8) & 0xff) / 255.0f;
        const auto b = static_cast<f32>((rgba >> 16) & 0xff) / 255.0f;
        const auto a = static_cast<f32>((rgba >> 24) & 0xff) / 255.0f;
        return { r, g, b, a };
    }

    template<typename T = D2D1_COLOR_F>
    constexpr T colorFromU32Premultiply(u32 rgba)
    {
        const auto r = static_cast<f32>((rgba >> 0) & 0xff) / 255.0f;
        const auto g = static_cast<f32>((rgba >> 8) & 0xff) / 255.0f;
        const auto b = static_cast<f32>((rgba >> 16) & 0xff) / 255.0f;
        const auto a = static_cast<f32>((rgba >> 24) & 0xff) / 255.0f;
        return { r * a, g * a, b * a, a };
    }

    constexpr u32 u32ColorPremultiply(u32 rgba)
    {
        auto rb = rgba & 0x00ff00ff;
        auto g = rgba & 0x0000ff00;
        const auto a = rgba & 0xff000000;

        const auto m = rgba >> 24;
        rb = (rb * m / 0xff) & 0x00ff00ff;
        g = (g * m / 0xff) & 0x0000ff00;

        return rb | g | a;
    }

    // MSVC STL (version 22000) implements std::clamp<T>(T, T, T) in terms of the generic
    // std::clamp<T, Predicate>(T, T, T, Predicate) with std::less{} as the argument,
    // which introduces branching. While not perfect, this is still better than std::clamp.
    template<typename T>
    constexpr T clamp(T val, T min, T max) noexcept
    {
        return val < min ? min : (max < val ? max : val);
    }

    inline constexpr D2D1_RECT_F GlyphRunEmptyBounds{ 1e38f, 1e38f, -1e38f, -1e38f };
    void GlyphRunAccumulateBounds(const ID2D1DeviceContext* d2dRenderTarget, D2D1_POINT_2F baselineOrigin, const DWRITE_GLYPH_RUN* glyphRun, D2D1_RECT_F& bounds);

    wil::com_ptr<IDWriteColorGlyphRunEnumerator1> TranslateColorGlyphRun(IDWriteFactory4* dwriteFactory4, D2D_POINT_2F baselineOrigin, const DWRITE_GLYPH_RUN* glyphRun) noexcept;
    bool ColorGlyphRunMoveNext(IDWriteColorGlyphRunEnumerator1* enumerator);
    const DWRITE_COLOR_GLYPH_RUN1* ColorGlyphRunGetCurrentRun(IDWriteColorGlyphRunEnumerator1* enumerator);
    void ColorGlyphRunAccumulateBounds(const ID2D1DeviceContext* d2dRenderTarget, const DWRITE_COLOR_GLYPH_RUN1* colorGlyphRun, D2D1_RECT_F& bounds);
    void ColorGlyphRunDraw(ID2D1DeviceContext4* d2dRenderTarget4, ID2D1SolidColorBrush* emojiBrush, ID2D1SolidColorBrush* foregroundBrush, const DWRITE_COLOR_GLYPH_RUN1* colorGlyphRun) noexcept;
}
