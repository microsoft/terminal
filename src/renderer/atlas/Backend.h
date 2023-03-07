#pragma once

#include "common.h"

namespace Microsoft::Console::Render::Atlas
{
    inline constexpr bool debugContinuousRedraw = false;
    inline constexpr bool debugDisableFrameLatencyWaitableObject = false;
    inline constexpr bool debugDisablePartialInvalidation = false;
    inline constexpr bool debugForceD2DMode = false;

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

        static constexpr DXGI_SWAP_CHAIN_FLAG flags = debugDisableFrameLatencyWaitableObject ? DXGI_SWAP_CHAIN_FLAG{} : DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

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
