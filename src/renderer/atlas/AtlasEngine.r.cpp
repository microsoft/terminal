// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AtlasEngine.h"

#include "dwrite.h"

// #### NOTE ####
// If you see any code in here that contains "_api." you might be seeing a race condition.
// The AtlasEngine::Present() method is called on a background thread without any locks,
// while any of the API methods (like AtlasEngine::Invalidate) might be called concurrently.
// The usage of the _r field is safe as its members are in practice
// only ever written to by the caller of Present() (the "Renderer" class).
// The _api fields on the other hand are concurrently written to by others.

#pragma warning(disable : 4100) // '...': unreferenced formal parameter
// Disable a bunch of warnings which get in the way of writing performant code.
#pragma warning(disable : 26429) // Symbol 'data' is never tested for nullness, it can be marked as not_null (f.23).
#pragma warning(disable : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
#pragma warning(disable : 26459) // You called an STL function '...' with a raw pointer parameter at position '...' that may be unsafe [...].
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26482) // Only index into arrays using constant expressions (bounds.2).

// https://en.wikipedia.org/wiki/Inversion_list
template<size_t N>
constexpr bool isInInversionList(const std::array<wchar_t, N>& ranges, wchar_t needle)
{
    const auto beg = ranges.begin();
    const auto end = ranges.end();
    decltype(ranges.begin()) it;

    // Linear search is faster than binary search for short inputs.
    if constexpr (N < 16)
    {
        it = std::find_if(beg, end, [=](wchar_t v) { return needle < v; });
    }
    else
    {
        it = std::upper_bound(beg, end, needle);
    }

    const auto idx = it - beg;
    return (idx & 1) != 0;
}

using namespace Microsoft::Console::Render;

#pragma region IRenderEngine

// Present() is called without the console buffer lock being held.
// --> Put as much in here as possible.
[[nodiscard]] HRESULT AtlasEngine::Present() noexcept
try
{
    _adjustAtlasSize();
    _processGlyphQueue();

    if (WI_IsFlagSet(_r.invalidations, RenderInvalidations::Cursor))
    {
        _drawCursor();
        WI_ClearFlag(_r.invalidations, RenderInvalidations::Cursor);
    }

    // The values the constant buffer depends on are potentially updated after BeginPaint().
    if (WI_IsFlagSet(_r.invalidations, RenderInvalidations::ConstBuffer))
    {
        _updateConstantBuffer();
        WI_ClearFlag(_r.invalidations, RenderInvalidations::ConstBuffer);
    }

    {
#pragma warning(suppress : 26494) // Variable 'mapped' is uninitialized. Always initialize an object (type.5).
        D3D11_MAPPED_SUBRESOURCE mapped;
        THROW_IF_FAILED(_r.deviceContext->Map(_r.cellBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
        assert(mapped.RowPitch >= _r.cells.size() * sizeof(Cell));
        memcpy(mapped.pData, _r.cells.data(), _r.cells.size() * sizeof(Cell));
        _r.deviceContext->Unmap(_r.cellBuffer.get(), 0);
    }

    // After Present calls, the back buffer needs to explicitly be
    // re-bound to the D3D11 immediate context before it can be used again.
    _r.deviceContext->OMSetRenderTargets(1, _r.renderTargetView.addressof(), nullptr);
    _r.deviceContext->Draw(3, 0);

    // See documentation for IDXGISwapChain2::GetFrameLatencyWaitableObject method:
    // > For every frame it renders, the app should wait on this handle before starting any rendering operations.
    // > Note that this requirement includes the first frame the app renders with the swap chain.
    assert(debugGeneralPerformance || _r.frameLatencyWaitableObjectUsed);

    // > IDXGISwapChain::Present: Partial Presentation (using a dirty rects or scroll) is not supported
    // > for SwapChains created with DXGI_SWAP_EFFECT_DISCARD or DXGI_SWAP_EFFECT_FLIP_DISCARD.
    // ---> No need to call IDXGISwapChain1::Present1.
    //      TODO: Would IDXGISwapChain1::Present1 and its dirty rects have benefits for remote desktop?
    THROW_IF_FAILED(_r.swapChain->Present(1, 0));

    // On some GPUs with tile based deferred rendering (TBDR) architectures, binding
    // RenderTargets that already have contents in them (from previous rendering) incurs a
    // cost for having to copy the RenderTarget contents back into tile memory for rendering.
    //
    // On Windows 10 with DXGI_SWAP_EFFECT_FLIP_DISCARD we get this for free.
    if (!_sr.isWindows10OrGreater)
    {
        _r.deviceContext->DiscardView(_r.renderTargetView.get());
    }

    return S_OK;
}
catch (const wil::ResultException& exception)
{
    // TODO: this writes to _api.
    return _handleException(exception);
}
CATCH_RETURN()

#pragma endregion

void AtlasEngine::_setShaderResources() const
{
    _r.deviceContext->VSSetShader(_r.vertexShader.get(), nullptr, 0);
    _r.deviceContext->PSSetShader(_r.pixelShader.get(), nullptr, 0);

    // Our vertex shader uses a trick from Bill Bilodeau published in
    // "Vertex Shader Tricks" at GDC14 to draw a fullscreen triangle
    // without vertex/index buffers. This prepares our context for this.
    _r.deviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    _r.deviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    _r.deviceContext->IASetInputLayout(nullptr);
    _r.deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    _r.deviceContext->PSSetConstantBuffers(0, 1, _r.constantBuffer.addressof());

    const std::array resources{ _r.cellView.get(), _r.atlasView.get() };
    _r.deviceContext->PSSetShaderResources(0, gsl::narrow_cast<UINT>(resources.size()), resources.data());
}

void AtlasEngine::_updateConstantBuffer() const noexcept
{
    const auto useClearType = _api.realizedAntialiasingMode == D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE;

    ConstBuffer data;
    data.viewport.x = 0;
    data.viewport.y = 0;
    data.viewport.z = static_cast<float>(_r.cellCount.x * _r.fontMetrics.cellSize.x);
    data.viewport.w = static_cast<float>(_r.cellCount.y * _r.fontMetrics.cellSize.y);
    DWrite_GetGammaRatios(_r.gamma, data.gammaRatios);
    data.enhancedContrast = useClearType ? _r.cleartypeEnhancedContrast : _r.grayscaleEnhancedContrast;
    data.cellCountX = _r.cellCount.x;
    data.cellSize.x = _r.fontMetrics.cellSize.x;
    data.cellSize.y = _r.fontMetrics.cellSize.y;
    data.underlinePos = _r.fontMetrics.underlinePos;
    data.underlineWidth = _r.fontMetrics.underlineWidth;
    data.strikethroughPos = _r.fontMetrics.strikethroughPos;
    data.strikethroughWidth = _r.fontMetrics.strikethroughWidth;
    data.doubleUnderlinePos.x = _r.fontMetrics.doubleUnderlinePos.x;
    data.doubleUnderlinePos.y = _r.fontMetrics.doubleUnderlinePos.y;
    data.thinLineWidth = _r.fontMetrics.thinLineWidth;
    data.backgroundColor = _r.backgroundColor;
    data.cursorColor = _r.cursorOptions.cursorColor;
    data.selectionColor = _r.selectionColor;
    data.useClearType = useClearType;
#pragma warning(suppress : 26447) // The function is declared 'noexcept' but calls function '...' which may throw exceptions (f.6).
    _r.deviceContext->UpdateSubresource(_r.constantBuffer.get(), 0, nullptr, &data, 0, 0);
}

void AtlasEngine::_adjustAtlasSize()
{
    // Only grow the atlas texture if our tileAllocator needs it to be larger.
    // We have no way of shrinking our tileAllocator at the moment,
    // so technically a `requiredSize != _r.atlasSizeInPixel`
    // comparison would be sufficient, but better safe than sorry.
    const auto requiredSize = _r.tileAllocator.size();
    if (requiredSize.y <= _r.atlasSizeInPixel.y && requiredSize.x <= _r.atlasSizeInPixel.x)
    {
        return;
    }

    wil::com_ptr<ID3D11Texture2D> atlasBuffer;
    wil::com_ptr<ID3D11ShaderResourceView> atlasView;
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = requiredSize.x;
        desc.Height = requiredSize.y;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc = { 1, 0 };
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        THROW_IF_FAILED(_r.device->CreateTexture2D(&desc, nullptr, atlasBuffer.addressof()));
        THROW_IF_FAILED(_r.device->CreateShaderResourceView(atlasBuffer.get(), nullptr, atlasView.addressof()));
    }

    // If a _r.atlasBuffer already existed, we can copy its glyphs
    // over to the new texture without re-rendering everything.
    const auto copyFromExisting = _r.atlasSizeInPixel != u16x2{};
    if (copyFromExisting)
    {
        D3D11_BOX box;
        box.left = 0;
        box.top = 0;
        box.front = 0;
        box.right = _r.atlasSizeInPixel.x;
        box.bottom = _r.atlasSizeInPixel.y;
        box.back = 1;
        _r.deviceContext->CopySubresourceRegion1(atlasBuffer.get(), 0, 0, 0, 0, _r.atlasBuffer.get(), 0, &box, D3D11_COPY_NO_OVERWRITE);
    }

    _r.atlasSizeInPixel = requiredSize;
    _r.atlasBuffer = std::move(atlasBuffer);
    _r.atlasView = std::move(atlasView);
    _setShaderResources();

    {
        const auto surface = _r.atlasBuffer.query<IDXGISurface>();

        wil::com_ptr<IDWriteRenderingParams1> renderingParams;
        DWrite_GetRenderParams(_sr.dwriteFactory.get(), &_r.gamma, &_r.cleartypeEnhancedContrast, &_r.grayscaleEnhancedContrast, renderingParams.addressof());

        D2D1_RENDER_TARGET_PROPERTIES props{};
        props.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
        props.pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED };
        props.dpiX = static_cast<float>(_r.dpi);
        props.dpiY = static_cast<float>(_r.dpi);
        THROW_IF_FAILED(_sr.d2dFactory->CreateDxgiSurfaceRenderTarget(surface.get(), &props, _r.d2dRenderTarget.put()));

        // We don't really use D2D for anything except DWrite, but it
        // can't hurt to ensure that everything it does is pixel aligned.
        _r.d2dRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
        // In case _api.realizedAntialiasingMode is D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE we'll
        // continuously adjust it in AtlasEngine::_drawGlyph. See _drawGlyph.
        _r.d2dRenderTarget->SetTextAntialiasMode(static_cast<D2D1_TEXT_ANTIALIAS_MODE>(_api.realizedAntialiasingMode));
        // Ensure that D2D uses the exact same gamma as our shader uses.
        _r.d2dRenderTarget->SetTextRenderingParams(renderingParams.get());
    }
    {
        static constexpr D2D1_COLOR_F color{ 1, 1, 1, 1 };
        wil::com_ptr<ID2D1SolidColorBrush> brush;
        THROW_IF_FAILED(_r.d2dRenderTarget->CreateSolidColorBrush(&color, nullptr, brush.addressof()));
        _r.brush = brush.query<ID2D1Brush>();
    }

    WI_SetAllFlags(_r.invalidations, RenderInvalidations::ConstBuffer);
    WI_SetFlagIf(_r.invalidations, RenderInvalidations::Cursor, !copyFromExisting);
}

void AtlasEngine::_processGlyphQueue()
{
    if (_r.glyphQueue.empty())
    {
        return;
    }

    _r.d2dRenderTarget->BeginDraw();
    for (const auto& pair : _r.glyphQueue)
    {
        _drawGlyph(pair);
    }
    THROW_IF_FAILED(_r.d2dRenderTarget->EndDraw());

    _r.glyphQueue.clear();
}

void AtlasEngine::_drawGlyph(const AtlasQueueItem& item) const
{
    const auto key = item.key->data();
    const auto value = item.value->data();
    const auto coords = &value->coords[0];
    const auto charsLength = key->charCount;
    const auto cellCount = static_cast<u32>(key->attributes.cellCount);
    const auto textFormat = _getTextFormat(key->attributes.bold, key->attributes.italic);
    const auto coloredGlyph = WI_IsFlagSet(value->flags, CellFlags::ColoredGlyph);
    const f32x2 layoutBox{ cellCount * _r.cellSizeDIP.x, _r.cellSizeDIP.y };

    // See D2DFactory::DrawText
    wil::com_ptr<IDWriteTextLayout> textLayout;
    THROW_IF_FAILED(_sr.dwriteFactory->CreateTextLayout(&key->chars[0], charsLength, textFormat, layoutBox.x, layoutBox.y, textLayout.addressof()));
    if (_r.typography)
    {
        textLayout->SetTypography(_r.typography.get(), { 0, charsLength });
    }

    auto options = D2D1_DRAW_TEXT_OPTIONS_NONE;
    // D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT enables a bunch of internal machinery
    // which doesn't have to run if we know we can't use it anyways in the shader.
    WI_SetFlagIf(options, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT, coloredGlyph);

    // Colored glyphs cannot be drawn in linear gamma.
    // That's why we're simply alpha-blending them in the shader.
    // In order for this to work correctly we have to prevent them from being drawn
    // with ClearType, because we would then lack the alpha channel for the glyphs.
    if (_api.realizedAntialiasingMode == D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE)
    {
        _r.d2dRenderTarget->SetTextAntialiasMode(coloredGlyph ? D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE : D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    }

    const f32x2 halfSize{ layoutBox.x * 0.5f, layoutBox.y * 0.5f };
    bool scalingRequired = false;
    f32x2 offset{ 0, 0 };
    f32x2 scale{ 1, 1 };

    // Block Element and Box Drawing characters need to be handled separately,
    // because unlike regular ones they're supposed to fill the entire layout box.
    //
    // Ranges:
    // * 0x2500-0x257F: Box Drawing
    // * 0x2580-0x259F: Block Elements
    // * 0xE0A0-0xE0A3,0xE0B0-0xE0C8,0xE0CA-0xE0CA,0xE0CC-0xE0D4: PowerLine
    //   (https://github.com/ryanoasis/nerd-fonts/wiki/Glyph-Sets-and-Code-Points#powerline-symbols)
    //
    // The following `blockCharacters` forms a so called "inversion list".
    static constexpr std::array blockCharacters{
        // clang-format off
        L'\u2500', L'\u2580',
        L'\u2580', L'\u25A0',
        L'\uE0A0', L'\uE0A4',
        L'\uE0B0', L'\uE0C9',
        L'\uE0CA', L'\uE0CB',
        L'\uE0CC', L'\uE0D5',
        // clang-format on
    };

    if (charsLength == 1 && isInInversionList(blockCharacters, key->chars[0]))
    {
        wil::com_ptr<IDWriteFontCollection> fontCollection;
        THROW_IF_FAILED(textFormat->GetFontCollection(fontCollection.addressof()));
        const auto baseWeight = textFormat->GetFontWeight();
        const auto baseStyle = textFormat->GetFontStyle();

        TextAnalysisSource analysisSource{ &key->chars[0], 1 };
        UINT32 mappedLength = 0;
        wil::com_ptr<IDWriteFont> mappedFont;
        FLOAT mappedScale = 0;
        THROW_IF_FAILED(_sr.systemFontFallback->MapCharacters(
            /* analysisSource     */ &analysisSource,
            /* textPosition       */ 0,
            /* textLength         */ 1,
            /* baseFontCollection */ fontCollection.get(),
            /* baseFamilyName     */ _r.fontMetrics.fontName.data(),
            /* baseWeight         */ baseWeight,
            /* baseStyle          */ baseStyle,
            /* baseStretch        */ DWRITE_FONT_STRETCH_NORMAL,
            /* mappedLength       */ &mappedLength,
            /* mappedFont         */ mappedFont.addressof(),
            /* scale              */ &mappedScale));

        if (mappedFont)
        {
            wil::com_ptr<IDWriteFontFace> fontFace;
            THROW_IF_FAILED(mappedFont->CreateFontFace(fontFace.addressof()));

            DWRITE_FONT_METRICS metrics;
            fontFace->GetMetrics(&metrics);

            const u32 codePoint = key->chars[0];
            u16 glyphIndex;
            THROW_IF_FAILED(fontFace->GetGlyphIndicesW(&codePoint, 1, &glyphIndex));

            DWRITE_GLYPH_METRICS glyphMetrics;
            THROW_IF_FAILED(fontFace->GetDesignGlyphMetrics(&glyphIndex, 1, &glyphMetrics));

            const f32x2 boxSize{
                static_cast<f32>(glyphMetrics.advanceWidth) / static_cast<f32>(metrics.designUnitsPerEm) * _r.fontMetrics.fontSizeInDIP,
                static_cast<f32>(glyphMetrics.advanceHeight) / static_cast<f32>(metrics.designUnitsPerEm) * _r.fontMetrics.fontSizeInDIP,
            };

            // We always want box drawing glyphs to exactly match the size of a terminal cell.
            // So for safe measure we'll always scale them to the exact size.
            // But add 1px to the destination size, so that we don't end up with fractional pixels.
            scalingRequired = true;
            scale.x = layoutBox.x / boxSize.x;
            scale.y = layoutBox.y / boxSize.y;
        }
    }
    else
    {
        DWRITE_OVERHANG_METRICS overhang;
        THROW_IF_FAILED(textLayout->GetOverhangMetrics(&overhang));

        const DWRITE_OVERHANG_METRICS clampedOverhang{
            std::max(0.0f, overhang.left),
            std::max(0.0f, overhang.top),
            std::max(0.0f, overhang.right),
            std::max(0.0f, overhang.bottom),
        };
        f32x2 actualSize{
            layoutBox.x + overhang.left + overhang.right,
            layoutBox.y + overhang.top + overhang.bottom,
        };

        // Long glyphs should be drawn with their proper design size, even if that makes them a bit blurry,
        // because otherwise we fail to support "pseudo" block characters like the "===" ligature in Cascadia Code.
        // If we didn't force upscale that ligatures it would seemingly shrink shorter and shorter, as its
        // glyph advance is often slightly shorter by a fractional pixel or two compared to our terminal's cells.
        // It's a trade off that keeps most glyphs "crisp" while retaining support for things like "===".
        // At least I can't think of any better heuristic for this at the moment...
        if (cellCount > 2)
        {
            const auto advanceScale = _r.fontMetrics.advanceScale;
            scalingRequired = true;
            scale = { advanceScale, advanceScale };
            actualSize.x *= advanceScale;
            actualSize.y *= advanceScale;
        }

        // We need to offset glyphs that are simply outside of our layout box (layoutBox.x/.y)
        // and additionally downsize glyphs that are entirely too large to fit in.
        // The DWRITE_OVERHANG_METRICS will tell us how many DIPs the layout box is too large/small.
        // It contains a positive number if the glyph is outside and a negative one if it's inside
        // the layout box. For example, given a layoutBox.x/.y (and cell size) of 20/30:
        // * "M" is the "largest" ASCII character and might be:
        //     left:    -0.6f
        //     right:   -0.6f
        //     top:     -7.6f
        //     bottom:  -7.4f
        //   "M" doesn't fill the layout box at all!
        //   This is because we've rounded up the Terminal's cell size to whole pixels in
        //   _resolveFontMetrics. top/bottom margins are fairly large because we added the
        //   chosen font's ascender, descender and line gap metrics to get our line height.
        //   --> offsetX = 0
        //   --> offsetY = 0
        //   --> scale   = 1
        // * The bar diacritic (U+0336 combining long stroke overlay)
        //     left:    -9.0f
        //     top:    -16.3f
        //     right:    5.6f
        //     bottom: -11.7f
        //   right is positive! Our glyph is 5.6 DIPs outside of the layout box and would
        //   appear cut off during rendering. left is negative at -9, which indicates that
        //   we can simply shift the glyph by 5.6 DIPs to the left to fit it into our bounds.
        //   --> offsetX = -5.6f
        //   --> offsetY = 0
        //   --> scale   = 1
        // * Any wide emoji in a narrow cell (U+26A0 warning sign)
        //     left:     6.7f
        //     top:     -4.1f
        //     right:    6.7f
        //     bottom:  -3.0f
        //   Our emoji is outside the bounds on both the left and right side and we need to shrink it.
        //   --> offsetX = 0
        //   --> offsetY = 0
        //   --> scale   = layoutBox.y / (layoutBox.y + left + right)
        //               = 0.69f
        offset.x = clampedOverhang.left - clampedOverhang.right;
        offset.y = clampedOverhang.top - clampedOverhang.bottom;

        if ((actualSize.x - layoutBox.x) > _r.dipPerPixel)
        {
            scalingRequired = true;
            offset.x = (overhang.left - overhang.right) * 0.5f;
            scale.x = layoutBox.x / actualSize.x;
            scale.y = scale.x;
        }
        if ((actualSize.y - layoutBox.y) > _r.dipPerPixel)
        {
            scalingRequired = true;
            offset.y = (overhang.top - overhang.bottom) * 0.5f;
            scale.x = std::min(scale.x, layoutBox.y / actualSize.y);
            scale.y = scale.x;
        }

        // As explained below, we use D2D1_DRAW_TEXT_OPTIONS_NO_SNAP to prevent a weird issue with baseline snapping.
        // But we do want it technically, so this re-implements baseline snapping... I think?
        // It calculates the new `baseline` height after transformation by `scale.y` relative to the center point `halfSize.y`.
        //
        // This works even if `scale.y == 1`, because then `baseline == baselineInDIP + offset.y` and `baselineInDIP`
        // is always measured in full pixels. So rounding it will be equivalent to just rounding `offset.y` itself.
        const auto baseline = halfSize.y + (_r.fontMetrics.baselineInDIP + offset.y - halfSize.y) * scale.y;
        // This rounds to the nearest multiple of _r.dipPerPixel.
        const auto baselineFixed = roundf(baseline * _r.pixelPerDIP) * _r.dipPerPixel;
        offset.y += (baselineFixed - baseline) / scale.y;
    }

    // !!! IMPORTANT !!!
    // DirectWrite/2D snaps the baseline to whole pixels, which is something we technically
    // want (it makes text look crisp), but fails in weird ways if `scalingRequired` is true.
    // As our scaling matrix's dx/dy (center point) is based on the `origin` coordinates
    // each cell we draw gets a unique, fractional baseline which gets rounded differently.
    // I'm not 100% sure why that happens, since `origin` is always in full pixels...
    // But this causes wide glyphs to draw as tiles that are potentially misaligned vertically by a pixel.
    // The resulting text rendering looks especially bad for ligatures like "====" in Cascadia Code,
    // where every single "=" might be blatantly misaligned vertically (same for any box drawings).
    WI_SetFlagIf(options, D2D1_DRAW_TEXT_OPTIONS_NO_SNAP, scalingRequired);

    const f32x2 inverseScale{ 1.0f - scale.x, 1.0f - scale.y };

    for (u32 i = 0; i < cellCount; ++i)
    {
        const auto coord = coords[i];

        D2D1_RECT_F rect;
        rect.left = static_cast<float>(coord.x) * _r.dipPerPixel;
        rect.top = static_cast<float>(coord.y) * _r.dipPerPixel;
        rect.right = rect.left + _r.cellSizeDIP.x;
        rect.bottom = rect.top + _r.cellSizeDIP.y;

        D2D1_POINT_2F origin;
        origin.x = rect.left - i * _r.cellSizeDIP.x;
        origin.y = rect.top;

        {
            _r.d2dRenderTarget->PushAxisAlignedClip(&rect, D2D1_ANTIALIAS_MODE_ALIASED);
            _r.d2dRenderTarget->Clear();
        }
        if (scalingRequired)
        {
            const D2D1_MATRIX_3X2_F transform{
                scale.x,
                0,
                0,
                scale.y,
                (origin.x + halfSize.x) * inverseScale.x,
                (origin.y + halfSize.y) * inverseScale.y,
            };
            _r.d2dRenderTarget->SetTransform(&transform);
        }
        {
            // Now that we're done using origin to calculate the center point for our transformation
            // we can use it for its intended purpose to slightly shift the glyph around.
            origin.x += offset.x;
            origin.y += offset.y;
            _r.d2dRenderTarget->DrawTextLayout(origin, textLayout.get(), _r.brush.get(), options);
        }
        if (scalingRequired)
        {
            static constexpr D2D1_MATRIX_3X2_F identity{ 1, 0, 0, 1, 0, 0 };
            _r.d2dRenderTarget->SetTransform(&identity);
        }
        {
            _r.d2dRenderTarget->PopAxisAlignedClip();
        }
    }
}

void AtlasEngine::_drawCursor()
{
    // lineWidth is in D2D's DIPs. For instance if we have a 150-200% zoom scale we want to draw a 2px wide line.
    // At 150% scale lineWidth thus needs to be 1.33333... because at a zoom scale of 1.5 this results in a 2px wide line.
    const auto lineWidth = std::max(1.0f, static_cast<float>((_r.dpi + USER_DEFAULT_SCREEN_DPI / 2) / USER_DEFAULT_SCREEN_DPI * USER_DEFAULT_SCREEN_DPI) / static_cast<float>(_r.dpi));
    const auto cursorType = static_cast<CursorType>(_r.cursorOptions.cursorType);

    // `clip` is the rectangle within our texture atlas that's reserved for our cursor texture, ...
    D2D1_RECT_F clip;
    clip.left = 0.0f;
    clip.top = 0.0f;
    clip.right = _r.cellSizeDIP.x;
    clip.bottom = _r.cellSizeDIP.y;

    // ... whereas `rect` is just the visible (= usually white) portion of our cursor.
    auto rect = clip;

    switch (cursorType)
    {
    case CursorType::Legacy:
        rect.top = _r.cellSizeDIP.y * static_cast<float>(100 - _r.cursorOptions.heightPercentage) / 100.0f;
        break;
    case CursorType::VerticalBar:
        rect.right = lineWidth;
        break;
    case CursorType::EmptyBox:
    {
        // EmptyBox is drawn as a line and unlike filled rectangles those are drawn centered on their
        // coordinates in such a way that the line border extends half the width to each side.
        // --> Our coordinates have to be 0.5 DIP off in order to draw a 2px line on a 200% scaling.
        const auto halfWidth = lineWidth / 2.0f;
        rect.left = halfWidth;
        rect.top = halfWidth;
        rect.right -= halfWidth;
        rect.bottom -= halfWidth;
        break;
    }
    case CursorType::Underscore:
    case CursorType::DoubleUnderscore:
        rect.top = _r.cellSizeDIP.y - lineWidth;
        break;
    default:
        break;
    }

    _r.d2dRenderTarget->BeginDraw();
    // We need to clip the area we draw in to ensure we don't
    // accidentally draw into any neighboring texture atlas tiles.
    _r.d2dRenderTarget->PushAxisAlignedClip(&clip, D2D1_ANTIALIAS_MODE_ALIASED);
    _r.d2dRenderTarget->Clear();

    if (cursorType == CursorType::EmptyBox)
    {
        _r.d2dRenderTarget->DrawRectangle(&rect, _r.brush.get(), lineWidth);
    }
    else
    {
        _r.d2dRenderTarget->FillRectangle(&rect, _r.brush.get());
    }

    if (cursorType == CursorType::DoubleUnderscore)
    {
        rect.top -= 2.0f;
        rect.bottom -= 2.0f;
        _r.d2dRenderTarget->FillRectangle(&rect, _r.brush.get());
    }

    _r.d2dRenderTarget->PopAxisAlignedClip();
    THROW_IF_FAILED(_r.d2dRenderTarget->EndDraw());
}
