// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Backend.h"

#include <dwmapi.h>

TIL_FAST_MATH_BEGIN

// Disable a bunch of warnings which get in the way of writing performant code.
#pragma warning(disable : 26429) // Symbol 'data' is never tested for nullness, it can be marked as not_null (f.23).
#pragma warning(disable : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
#pragma warning(disable : 26459) // You called an STL function '...' with a raw pointer parameter at position '...' that may be unsafe [...].
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26482) // Only index into arrays using constant expressions (bounds.2).

using namespace Microsoft::Console::Render::Atlas;

wil::com_ptr<ID3D11Texture2D> SwapChainManager::GetBuffer() const
{
    wil::com_ptr<ID3D11Texture2D> buffer;
    THROW_IF_FAILED(_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), buffer.put_void()));
    return buffer;
}

void SwapChainManager::Present(const RenderingPayload& p)
{
    const til::rect fullRect{ 0, 0, _targetSize.x, _targetSize.y };

    DXGI_PRESENT_PARAMETERS params{};
    RECT scrollRect{};
    POINT scrollOffset{};

    // Since rows might be taller than their cells, they might have drawn outside of the viewport.
    auto dirtyRect = p.dirtyRectInPx;
    dirtyRect.left = std::max(dirtyRect.left, 0);
    dirtyRect.top = std::max(dirtyRect.top, 0);
    dirtyRect.right = std::min(dirtyRect.right, til::CoordType{ _targetSize.x });
    dirtyRect.bottom = std::min(dirtyRect.bottom, til::CoordType{ _targetSize.y });

    if constexpr (!ATLAS_DEBUG_SHOW_DIRTY)
    {
        if (dirtyRect != fullRect)
        {
            params.DirtyRectsCount = 1;
            params.pDirtyRects = dirtyRect.as_win32_rect();

            if (p.scrollOffset)
            {
                const auto offsetInPx = p.scrollOffset * p.s->font->cellSize.y;
                const auto width = p.s->targetSize.x;
                const auto height = p.s->cellCount.y * p.s->font->cellSize.y;
                const auto top = std::max(0, offsetInPx);
                const auto bottom = height + std::min(0, offsetInPx);

                scrollRect = { 0, top, width, bottom };
                scrollOffset = { 0, offsetInPx };

                params.pScrollRect = &scrollRect;
                params.pScrollOffset = &scrollOffset;
            }
        }
    }

    THROW_IF_FAILED(_swapChain->Present1(1, 0, &params));
    _waitForPresentation = true;
}

void SwapChainManager::WaitUntilCanRender() noexcept
{
    // IDXGISwapChain2::GetFrameLatencyWaitableObject returns an auto-reset event.
    // Once we've waited on the event, waiting on it again will block until the timeout elapses.
    // _waitForPresentation guards against this.
    if constexpr (!ATLAS_DEBUG_DISABLE_FRAME_LATENCY_WAITABLE_OBJECT)
    {
        if (_waitForPresentation)
        {
            WaitForSingleObjectEx(_frameLatencyWaitableObject.get(), 100, true);
            _waitForPresentation = false;
        }
    }
}

void SwapChainManager::_createSwapChain(const RenderingPayload& p, IUnknown* device)
{
    _swapChain.reset();
    _frameLatencyWaitableObject.reset();

    DXGI_SWAP_CHAIN_DESC1 desc{
        .Width = p.s->targetSize.x,
        .Height = p.s->targetSize.y,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc = { .Count = 1 },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        // Sometimes up to 2 buffers are locked, for instance during screen capture or when moving the window.
        // 3 buffers seems to guarantee a stable framerate at display frequency at all times.
        .BufferCount = 3,
        .Scaling = DXGI_SCALING_NONE,
        // DXGI_SWAP_EFFECT_FLIP_DISCARD is a mode that was created at a time were display drivers
        // lacked support for Multiplane Overlays (MPO) and were copying buffers was expensive.
        // This allowed DWM to quickly draw overlays (like gamebars) on top of rendered content.
        // With faster GPU memory in general and with support for MPO in particular this isn't
        // really an advantage anymore. Instead DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL allows for a
        // more "intelligent" composition and display updates to occur like Panel Self Refresh
        // (PSR) which requires dirty rectangles (Present1 API) to work correctly.
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
        // If our background is opaque we can enable "independent" flips by setting DXGI_ALPHA_MODE_IGNORE.
        // As our swap chain won't have to compose with DWM anymore it reduces the display latency dramatically.
        .AlphaMode = p.s->target->enableTransparentBackground ? DXGI_ALPHA_MODE_PREMULTIPLIED : DXGI_ALPHA_MODE_IGNORE,
        .Flags = flags,
    };

    wil::com_ptr<IDXGISwapChain1> swapChain1;

    if (p.s->target->hwnd)
    {
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        THROW_IF_FAILED(p.dxgi.factory->CreateSwapChainForHwnd(device, p.s->target->hwnd, &desc, nullptr, nullptr, swapChain1.addressof()));
    }
    else
    {
        const auto module = GetModuleHandleW(L"dcomp.dll");
        const auto DCompositionCreateSurfaceHandle = GetProcAddressByFunctionDeclaration(module, DCompositionCreateSurfaceHandle);
        THROW_LAST_ERROR_IF(!DCompositionCreateSurfaceHandle);

        // As per: https://docs.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-dcompositioncreatesurfacehandle
        static constexpr DWORD COMPOSITIONSURFACE_ALL_ACCESS = 0x0003L;
        THROW_IF_FAILED(DCompositionCreateSurfaceHandle(COMPOSITIONSURFACE_ALL_ACCESS, nullptr, _swapChainHandle.addressof()));
        THROW_IF_FAILED(p.dxgi.factory.query<IDXGIFactoryMedia>()->CreateSwapChainForCompositionSurfaceHandle(device, _swapChainHandle.get(), &desc, nullptr, swapChain1.addressof()));
    }

    _swapChain = swapChain1.query<IDXGISwapChain2>();
    _frameLatencyWaitableObject.reset(_swapChain->GetFrameLatencyWaitableObject());
    _targetGeneration = p.s->target.generation();
    _fontGeneration = {};
    _targetSize = p.s->targetSize;
    _waitForPresentation = true;

    WaitUntilCanRender();

    if (p.swapChainChangedCallback)
    {
        try
        {
            p.swapChainChangedCallback(_swapChainHandle.get());
        }
        CATCH_LOG()
    }
}

void SwapChainManager::_resizeBuffers(const RenderingPayload& p)
{
    THROW_IF_FAILED(_swapChain->ResizeBuffers(0, p.s->targetSize.x, p.s->targetSize.y, DXGI_FORMAT_UNKNOWN, flags));
    _targetSize = p.s->targetSize;
}

void SwapChainManager::_updateMatrixTransform(const RenderingPayload& p)
{
    if (!p.s->target->hwnd)
    {
        // XAML's SwapChainPanel combines the worst of both worlds and always applies a transform
        // to the swap chain to make it match the display scale. This undoes the damage.
        const DXGI_MATRIX_3X2_F matrix{
            ._11 = static_cast<f32>(USER_DEFAULT_SCREEN_DPI) / static_cast<f32>(p.s->font->dpi),
            ._22 = static_cast<f32>(USER_DEFAULT_SCREEN_DPI) / static_cast<f32>(p.s->font->dpi),
        };
        THROW_IF_FAILED(_swapChain->SetMatrixTransform(&matrix));
    }
    _fontGeneration = p.s->font.generation();
}

wil::com_ptr<IDWriteColorGlyphRunEnumerator1> Microsoft::Console::Render::Atlas::TranslateColorGlyphRun(IDWriteFactory4* dwriteFactory4, D2D_POINT_2F baselineOrigin, const DWRITE_GLYPH_RUN* glyphRun)
{
    static constexpr auto formats =
        DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE |
        DWRITE_GLYPH_IMAGE_FORMATS_CFF |
        DWRITE_GLYPH_IMAGE_FORMATS_COLR |
        DWRITE_GLYPH_IMAGE_FORMATS_SVG |
        DWRITE_GLYPH_IMAGE_FORMATS_PNG |
        DWRITE_GLYPH_IMAGE_FORMATS_JPEG |
        DWRITE_GLYPH_IMAGE_FORMATS_TIFF |
        DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8;

    wil::com_ptr<IDWriteColorGlyphRunEnumerator1> enumerator;

    if (dwriteFactory4)
    {
        THROW_IF_FAILED(dwriteFactory4->TranslateColorGlyphRun(baselineOrigin, glyphRun, nullptr, formats, DWRITE_MEASURING_MODE_NATURAL, nullptr, 0, enumerator.addressof()));
    }

    return enumerator;
}

// Draws a `DWRITE_GLYPH_RUN` at `baselineOrigin` into the given `ID2D1DeviceContext`.
// `d2dRenderTarget4` and `dwriteFactory4` are optional and used to draw colored glyphs.
// Returns true if the `DWRITE_GLYPH_RUN` contained a color glyph.
bool Microsoft::Console::Render::Atlas::DrawGlyphRun(ID2D1DeviceContext* d2dRenderTarget, ID2D1DeviceContext4* d2dRenderTarget4, IDWriteFactory4* dwriteFactory4, D2D_POINT_2F baselineOrigin, const DWRITE_GLYPH_RUN* glyphRun, ID2D1Brush* foregroundBrush)
{
    // Support for ID2D1DeviceContext4 implies support for IDWriteFactory4 and vice versa.
    if (const auto enumerator = TranslateColorGlyphRun(dwriteFactory4, baselineOrigin, glyphRun))
    {
        DrawColorGlyphRun(d2dRenderTarget4, enumerator.get(), foregroundBrush);
        return true;
    }
    else
    {
        DrawBasicGlyphRun(d2dRenderTarget, baselineOrigin, glyphRun, foregroundBrush);
        return false;
    }
}

void Microsoft::Console::Render::Atlas::DrawBasicGlyphRun(ID2D1DeviceContext* d2dRenderTarget, D2D_POINT_2F baselineOrigin, const DWRITE_GLYPH_RUN* glyphRun, ID2D1Brush* foregroundBrush)
{
    d2dRenderTarget->DrawGlyphRun(baselineOrigin, glyphRun, foregroundBrush, DWRITE_MEASURING_MODE_NATURAL);
}

void Microsoft::Console::Render::Atlas::DrawColorGlyphRun(ID2D1DeviceContext4* d2dRenderTarget4, IDWriteColorGlyphRunEnumerator1* enumerator, ID2D1Brush* foregroundBrush)
{
    const auto previousAntialiasingMode = d2dRenderTarget4->GetTextAntialiasMode();
    d2dRenderTarget4->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    const auto cleanup = wil::scope_exit([&]() {
        d2dRenderTarget4->SetTextAntialiasMode(previousAntialiasingMode);
    });

    wil::com_ptr<ID2D1SolidColorBrush> solidBrush;

    for (;;)
    {
        BOOL hasRun;
        THROW_IF_FAILED(enumerator->MoveNext(&hasRun));
        if (!hasRun)
        {
            break;
        }

        const DWRITE_COLOR_GLYPH_RUN1* colorGlyphRun;
        THROW_IF_FAILED(enumerator->GetCurrentRun(&colorGlyphRun));

        ID2D1Brush* runBrush = nullptr;
        if (colorGlyphRun->paletteIndex == /*DWRITE_NO_PALETTE_INDEX*/ 0xffff)
        {
            runBrush = foregroundBrush;
        }
        else
        {
            if (!solidBrush)
            {
                THROW_IF_FAILED(d2dRenderTarget4->CreateSolidColorBrush(colorGlyphRun->runColor, &solidBrush));
            }
            else
            {
                solidBrush->SetColor(colorGlyphRun->runColor);
            }
            runBrush = solidBrush.get();
        }

        const D2D1_POINT_2F runOrigin{
            colorGlyphRun->baselineOriginX,
            colorGlyphRun->baselineOriginY,
        };

        switch (colorGlyphRun->glyphImageFormat)
        {
        case DWRITE_GLYPH_IMAGE_FORMATS_NONE:
            break;
        case DWRITE_GLYPH_IMAGE_FORMATS_PNG:
        case DWRITE_GLYPH_IMAGE_FORMATS_JPEG:
        case DWRITE_GLYPH_IMAGE_FORMATS_TIFF:
        case DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8:
            d2dRenderTarget4->DrawColorBitmapGlyphRun(colorGlyphRun->glyphImageFormat, runOrigin, &colorGlyphRun->glyphRun, colorGlyphRun->measuringMode, D2D1_COLOR_BITMAP_GLYPH_SNAP_OPTION_DEFAULT);
            break;
        case DWRITE_GLYPH_IMAGE_FORMATS_SVG:
            d2dRenderTarget4->DrawSvgGlyphRun(runOrigin, &colorGlyphRun->glyphRun, runBrush, nullptr, 0, colorGlyphRun->measuringMode);
            break;
        default:
            d2dRenderTarget4->DrawGlyphRun(runOrigin, &colorGlyphRun->glyphRun, colorGlyphRun->glyphRunDescription, runBrush, colorGlyphRun->measuringMode);
            break;
        }
    }
}

TIL_FAST_MATH_END
