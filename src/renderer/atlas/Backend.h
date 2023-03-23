// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "common.h"

namespace Microsoft::Console::Render::Atlas
{
    // If set to 1, this will cause the entire viewport to be invalidated at all times.
    // Helpful for benchmarking our text shaping code based on DirectWrite.
#define ATLAS_DEBUG_DISABLE_PARTIAL_INVALIDATION 0

    // Redraw at display refresh rate at all times. This helps with shader debugging.
#define ATLAS_DEBUG_CONTINUOUS_REDRAW 0

    // Disables the use of DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT.
    // This helps with benchmarking the application as it'll run beyond display refresh rate.
#define ATLAS_DEBUG_DISABLE_FRAME_LATENCY_WAITABLE_OBJECT 0

    // Forces the use of Direct2D for text rendering (= BackendD2D).
#define ATLAS_DEBUG_FORCE_D2D_MODE 1

    // Adds an artificial delay before every render pass. In milliseconds.
#define ATLAS_DEBUG_RENDER_DELAY 0

    // Shows the dirty rects as given to IDXGISwapChain2::Present1() during each frame.
#define ATLAS_DEBUG_SHOW_DIRTY 0

    // Dumps the contents of the swap chain on each render pass into the given directory as PNG.
    // I highly recommend setting ATLAS_DEBUG_RENDER_DELAY to 250 or similar if this is used.
#define ATLAS_DEBUG_DUMP_RENDER_TARGET 0
#define ATLAS_DEBUG_DUMP_RENDER_TARGET_PATH LR"(%USERPROFILE%\Downloads\AtlasEngine)"

    // Draws a background behind each glyph placed into the BackendD3D glyph atlas.
    // This can be helpful when debugging bugs in the algorithm that measures the size of glyph.
#define ATLAS_DEBUG_COLORIZE_GLYPH_ATLAS 0

    struct SwapChainManager
    {
        void UpdateSwapChainSettings(const RenderingPayload& p, IUnknown* device, auto&& prepareRecreate, auto&& prepareResize)
        {
            if (_targetGeneration != p.s->target.generation())
            {
                if (_swapChain)
                {
                    prepareRecreate();
                }
                _createSwapChain(p, device);
            }
            else if (_targetSize != p.s->targetSize)
            {
                prepareResize();
                THROW_IF_FAILED(_swapChain->ResizeBuffers(0, p.s->targetSize.x, p.s->targetSize.y, DXGI_FORMAT_UNKNOWN, flags));
                _targetSize = p.s->targetSize;
            }

            _updateMatrixTransform(p);
        }

        wil::com_ptr<ID3D11Texture2D> GetBuffer() const;
        void Present(const RenderingPayload& p);
        void WaitUntilCanRender() noexcept;

    private:
        void _createSwapChain(const RenderingPayload& p, IUnknown* device);
        void _updateMatrixTransform(const RenderingPayload& p) const;

        static constexpr DXGI_SWAP_CHAIN_FLAG flags = ATLAS_DEBUG_DISABLE_FRAME_LATENCY_WAITABLE_OBJECT ? DXGI_SWAP_CHAIN_FLAG{} : DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

        wil::com_ptr<IDXGISwapChain2> _swapChain;
        wil::unique_handle _swapChainHandle;
        wil::unique_handle _frameLatencyWaitableObject;
        til::generation_t _targetGeneration;
        til::generation_t _fontGeneration;
        u16x2 _targetSize;
        bool _waitForPresentation = false;
    };

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

    // MSVC STL (version 22000) implements std::clamp<T>(T, T, T) in terms of the generic
    // std::clamp<T, Predicate>(T, T, T, Predicate) with std::less{} as the argument,
    // which introduces branching. While not perfect, this is still better than std::clamp.
    template<typename T>
    constexpr T clamp(T val, T min, T max)
    {
        return val < min ? min : (max < val ? max : val);
    }

    f32r GetGlyphRunBlackBox(const DWRITE_GLYPH_RUN& glyphRun, f32 baselineX, f32 baselineY);
    bool DrawGlyphRun(ID2D1DeviceContext* d2dRenderTarget, ID2D1DeviceContext4* d2dRenderTarget4, IDWriteFactory4* dwriteFactory4, D2D_POINT_2F baselineOrigin, const DWRITE_GLYPH_RUN* glyphRun, ID2D1Brush* foregroundBrush);
}
