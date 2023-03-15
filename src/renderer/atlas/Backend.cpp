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
    const til::rect fullRect{ 0, 0, p.s->cellCount.x, p.s->cellCount.y };

    DXGI_PRESENT_PARAMETERS params{};
    RECT dirtyRect{};
    RECT scrollRect{};
    POINT scrollOffset{};

    if (p.dirtyRect != fullRect)
    {
        dirtyRect = p.dirtyRect.to_win32_rect();
        dirtyRect.left *= p.s->font->cellSize.x;
        dirtyRect.top *= p.s->font->cellSize.y;
        dirtyRect.right *= p.s->font->cellSize.x;
        dirtyRect.bottom *= p.s->font->cellSize.y;

        // This block will enlarge the dirtyRect to handle glyphs that overlap their rows vertically.
        const auto actualDirtyTop = gsl::at(p.rows, p.dirtyRect.top).top;
        const auto actualDirtyBottom = gsl::at(p.rows, gsl::narrow_cast<size_t>(p.dirtyRect.bottom) - 1).bottom;
        // Since rows might be taller than their cells, they might have drawn outside of the viewport.
        // FYI using std::clamp() here would be dangerous. If std::clamp() is given a "min" that is greater
        // than "max" it'll return min, but our calculation of .bottom wants to do the exact opposite.
        dirtyRect.top = std::max(std::min(dirtyRect.top, LONG{ actualDirtyTop }), 0l);
        dirtyRect.bottom = std::min(std::max(dirtyRect.bottom, LONG{ actualDirtyBottom }), LONG{ _targetSize.y });
        // The swap chain might have a different size than the TextBuffer (due to the renderer running asynchronously) and so
        // we have to ensure to clamp the bottom/right coordinates into _targetSize. The above already did so for bottom.
        dirtyRect.right = std::min(dirtyRect.right, LONG{ _targetSize.x });

        // If a row of text has been changed, it's width will equal the full rect. In that case we should
        // also redraw the margin on the right, as overlapping glyphs might have previously drawn into it.
        if (p.dirtyRect.left == fullRect.left && p.dirtyRect.right == fullRect.right)
        {
            dirtyRect.right = _targetSize.x;
        }

        params.DirtyRectsCount = 1;
        params.pDirtyRects = &dirtyRect;

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

    THROW_IF_FAILED(_swapChain->Present1(1, 0, &params));
    _waitForPresentation = true;
}

void SwapChainManager::WaitUntilCanRender() noexcept
{
    // IDXGISwapChain2::GetFrameLatencyWaitableObject returns an auto-reset event.
    // Once we've waited on the event, waiting on it again will block until the timeout elapses.
    // _waitForPresentation guards against this.
    if constexpr (!debugDisableFrameLatencyWaitableObject)
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

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = p.s->targetSize.x;
    desc.Height = p.s->targetSize.y;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    // Sometimes up to 2 buffers are locked, for instance during screen capture or when moving the window.
    // 3 buffers seems to guarantee a stable framerate at display frequency at all times.
    desc.BufferCount = 3;
    desc.Scaling = DXGI_SCALING_NONE;
    // DXGI_SWAP_EFFECT_FLIP_DISCARD is a mode that was created at a time were display drivers
    // lacked support for Multiplane Overlays (MPO) and were copying buffers was expensive.
    // This allowed DWM to quickly draw overlays (like gamebars) on top of rendered content.
    // With faster GPU memory in general and with support for MPO in particular this isn't
    // really an advantage anymore. Instead DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL allows for a
    // more "intelligent" composition and display updates to occur like Panel Self Refresh
    // (PSR) which requires dirty rectangles (Present1 API) to work correctly.
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    // If our background is opaque we can enable "independent" flips by setting DXGI_ALPHA_MODE_IGNORE.
    // As our swap chain won't have to compose with DWM anymore it reduces the display latency dramatically.
    desc.AlphaMode = p.s->target->enableTransparentBackground ? DXGI_ALPHA_MODE_PREMULTIPLIED : DXGI_ALPHA_MODE_IGNORE;
    desc.Flags = flags;

    wil::com_ptr<IDXGISwapChain1> swapChain0;

    if (p.s->target->hwnd)
    {
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        THROW_IF_FAILED(p.dxgiFactory->CreateSwapChainForHwnd(device, p.s->target->hwnd, &desc, nullptr, nullptr, swapChain0.addressof()));
    }
    else
    {
        const auto module = GetModuleHandleW(L"dcomp.dll");
        const auto DCompositionCreateSurfaceHandle = GetProcAddressByFunctionDeclaration(module, DCompositionCreateSurfaceHandle);
        THROW_LAST_ERROR_IF(!DCompositionCreateSurfaceHandle);

        // As per: https://docs.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-dcompositioncreatesurfacehandle
        static constexpr DWORD COMPOSITIONSURFACE_ALL_ACCESS = 0x0003L;
        THROW_IF_FAILED(DCompositionCreateSurfaceHandle(COMPOSITIONSURFACE_ALL_ACCESS, nullptr, _swapChainHandle.addressof()));
        THROW_IF_FAILED(p.dxgiFactory.query<IDXGIFactoryMedia>()->CreateSwapChainForCompositionSurfaceHandle(device, _swapChainHandle.get(), &desc, nullptr, swapChain0.addressof()));
    }

    _swapChain = swapChain0.query<IDXGISwapChain2>();
    _frameLatencyWaitableObject.reset(_swapChain->GetFrameLatencyWaitableObject());
    _targetGeneration = p.s->target.generation();
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

void SwapChainManager::_updateMatrixTransform(const RenderingPayload& p) const
{
    // XAML's SwapChainPanel combines the worst of both worlds and always applies a transform to
    // the swap chain to make it match the display scale. This if condition undoes the damage.
    if (_fontGeneration != p.s->font.generation() && !p.s->target->hwnd)
    {
        const DXGI_MATRIX_3X2_F matrix{
            ._11 = p.d.font.dipPerPixel,
            ._22 = p.d.font.dipPerPixel,
        };
        THROW_IF_FAILED(_swapChain->SetMatrixTransform(&matrix));
    }
}

// Returns the theoretical/design design size of the given `DWRITE_GLYPH_RUN`, relative the the given baseline origin.
f32r Microsoft::Console::Render::Atlas::GetGlyphRunBlackBox(const DWRITE_GLYPH_RUN& glyphRun, f32 baselineX, f32 baselineY)
{
    DWRITE_FONT_METRICS fontMetrics;
    glyphRun.fontFace->GetMetrics(&fontMetrics);

    std::unique_ptr<DWRITE_GLYPH_METRICS[]> glyphRunMetricsHeap;
    std::array<DWRITE_GLYPH_METRICS, 8> glyphRunMetricsStack;
    DWRITE_GLYPH_METRICS* glyphRunMetrics = glyphRunMetricsStack.data();

    if (glyphRun.glyphCount > glyphRunMetricsStack.size())
    {
        glyphRunMetricsHeap = std::make_unique_for_overwrite<DWRITE_GLYPH_METRICS[]>(glyphRun.glyphCount);
        glyphRunMetrics = glyphRunMetricsHeap.get();
    }

    glyphRun.fontFace->GetDesignGlyphMetrics(glyphRun.glyphIndices, glyphRun.glyphCount, glyphRunMetrics, false);

    f32 const fontScale = glyphRun.fontEmSize / fontMetrics.designUnitsPerEm;
    f32r accumulatedBounds{
        FLT_MAX,
        FLT_MAX,
        FLT_MIN,
        FLT_MIN,
    };

    for (uint32_t i = 0; i < glyphRun.glyphCount; ++i)
    {
        const auto& glyphMetrics = glyphRunMetrics[i];
        const auto glyphAdvance = glyphRun.glyphAdvances ? glyphRun.glyphAdvances[i] : glyphMetrics.advanceWidth * fontScale;

        const auto left = static_cast<f32>(glyphMetrics.leftSideBearing) * fontScale;
        const auto top = static_cast<f32>(glyphMetrics.topSideBearing - glyphMetrics.verticalOriginY) * fontScale;
        const auto right = static_cast<f32>(gsl::narrow_cast<INT32>(glyphMetrics.advanceWidth) - glyphMetrics.rightSideBearing) * fontScale;
        const auto bottom = static_cast<f32>(gsl::narrow_cast<INT32>(glyphMetrics.advanceHeight) - glyphMetrics.bottomSideBearing - glyphMetrics.verticalOriginY) * fontScale;

        if (left < right && top < bottom)
        {
            auto glyphX = baselineX;
            auto glyphY = baselineY;
            if (glyphRun.glyphOffsets)
            {
                glyphX += glyphRun.glyphOffsets[i].advanceOffset;
                glyphY -= glyphRun.glyphOffsets[i].ascenderOffset;
            }

            accumulatedBounds.left = std::min(accumulatedBounds.left, left + glyphX);
            accumulatedBounds.top = std::min(accumulatedBounds.top, top + glyphY);
            accumulatedBounds.right = std::max(accumulatedBounds.right, right + glyphX);
            accumulatedBounds.bottom = std::max(accumulatedBounds.bottom, bottom + glyphY);
        }

        baselineX += glyphAdvance;
    }

    return accumulatedBounds;
}

// Draws a `DWRITE_GLYPH_RUN` at `baselineOrigin` into the given `ID2D1DeviceContext`.
// `d2dRenderTarget4` and `dwriteFactory4` are optional and used to draw colored glyphs.
// Returns true if the `DWRITE_GLYPH_RUN` contained a color glyph.
bool Microsoft::Console::Render::Atlas::DrawGlyphRun(ID2D1DeviceContext* d2dRenderTarget, ID2D1DeviceContext4* d2dRenderTarget4, IDWriteFactory4* dwriteFactory4, D2D_POINT_2F baselineOrigin, const DWRITE_GLYPH_RUN* glyphRun, ID2D1Brush* foregroundBrush)
{
    static constexpr auto measuringMode = DWRITE_MEASURING_MODE_NATURAL;
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

    // If ID2D1DeviceContext4 isn't supported, we'll exit early below.
    auto hr = DWRITE_E_NOCOLOR;

    if (d2dRenderTarget4)
    {
        D2D_MATRIX_3X2_F transform;
        d2dRenderTarget4->GetTransform(&transform);
        f32 dpiX, dpiY;
        d2dRenderTarget4->GetDpi(&dpiX, &dpiY);
        transform = transform * D2D1::Matrix3x2F::Scale(dpiX, dpiY);

        // Support for ID2D1DeviceContext4 implies support for IDWriteFactory4.
        // ID2D1DeviceContext4 is required for drawing below.
        hr = dwriteFactory4->TranslateColorGlyphRun(baselineOrigin, glyphRun, nullptr, formats, measuringMode, nullptr, 0, &enumerator);
    }

    if (hr == DWRITE_E_NOCOLOR)
    {
        d2dRenderTarget->DrawGlyphRun(baselineOrigin, glyphRun, foregroundBrush, measuringMode);
        return false;
    }

    THROW_IF_FAILED(hr);

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

        switch (colorGlyphRun->glyphImageFormat)
        {
        case DWRITE_GLYPH_IMAGE_FORMATS_NONE:
            break;
        case DWRITE_GLYPH_IMAGE_FORMATS_PNG:
        case DWRITE_GLYPH_IMAGE_FORMATS_JPEG:
        case DWRITE_GLYPH_IMAGE_FORMATS_TIFF:
        case DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8:
            d2dRenderTarget4->DrawColorBitmapGlyphRun(colorGlyphRun->glyphImageFormat, baselineOrigin, &colorGlyphRun->glyphRun, colorGlyphRun->measuringMode, D2D1_COLOR_BITMAP_GLYPH_SNAP_OPTION_DEFAULT);
            break;
        case DWRITE_GLYPH_IMAGE_FORMATS_SVG:
            d2dRenderTarget4->DrawSvgGlyphRun(baselineOrigin, &colorGlyphRun->glyphRun, runBrush, nullptr, 0, colorGlyphRun->measuringMode);
            break;
        default:
            d2dRenderTarget4->DrawGlyphRun(baselineOrigin, &colorGlyphRun->glyphRun, colorGlyphRun->glyphRunDescription, runBrush, colorGlyphRun->measuringMode);
            break;
        }
    }

    return true;
}

TIL_FAST_MATH_END
