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

template<typename T = D2D1_COLOR_F>
constexpr T colorFromU32(uint32_t rgba)
{
    const auto r = static_cast<float>((rgba >> 0) & 0xff) / 255.0f;
    const auto g = static_cast<float>((rgba >> 8) & 0xff) / 255.0f;
    const auto b = static_cast<float>((rgba >> 16) & 0xff) / 255.0f;
    const auto a = static_cast<float>((rgba >> 24) & 0xff) / 255.0f;
    return { r, g, b, a };
}

using namespace Microsoft::Console::Render;

#pragma region IRenderEngine

// Present() is called without the console buffer lock being held.
// --> Put as much in here as possible.
[[nodiscard]] HRESULT AtlasEngine::Present() noexcept
try
{
    const til::rect fullRect{ 0, 0, _r.cellCount.x, _r.cellCount.y };

    // A change in the selection or background color (etc.) forces a full redraw.
    if (WI_IsFlagSet(_r.invalidations, RenderInvalidations::ConstBuffer) || _r.customPixelShader)
    {
        _r.dirtyRect = fullRect;
    }

    if (!_r.dirtyRect)
    {
        return S_OK;
    }

    // See documentation for IDXGISwapChain2::GetFrameLatencyWaitableObject method:
    // > For every frame it renders, the app should wait on this handle before starting any rendering operations.
    // > Note that this requirement includes the first frame the app renders with the swap chain.
    assert(debugGeneralPerformance || _r.frameLatencyWaitableObjectUsed);

    if (_r.d2dMode) [[unlikely]]
    {
        _d2dPresent();
    }
    else
    {
        _adjustAtlasSize();
        _processGlyphQueue();

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

        if (_r.customPixelShader) [[unlikely]]
        {
            _renderWithCustomShader();
        }
        else
        {
            _r.deviceContext->OMSetRenderTargets(1, _r.renderTargetView.addressof(), nullptr);
            _r.deviceContext->Draw(3, 0);
        }
    }

    // See documentation for IDXGISwapChain2::GetFrameLatencyWaitableObject method:
    // > For every frame it renders, the app should wait on this handle before starting any rendering operations.
    // > Note that this requirement includes the first frame the app renders with the swap chain.
    assert(debugGeneralPerformance || _r.frameLatencyWaitableObjectUsed);

    if (_r.dirtyRect != fullRect)
    {
        auto dirtyRectInPx = _r.dirtyRect;
        dirtyRectInPx.left *= _r.fontMetrics.cellSize.x;
        dirtyRectInPx.top *= _r.fontMetrics.cellSize.y;
        dirtyRectInPx.right *= _r.fontMetrics.cellSize.x;
        dirtyRectInPx.bottom *= _r.fontMetrics.cellSize.y;

        RECT scrollRect{};
        POINT scrollOffset{};
        DXGI_PRESENT_PARAMETERS params{
            .DirtyRectsCount = 1,
            .pDirtyRects = dirtyRectInPx.as_win32_rect(),
        };

        if (_r.scrollOffset)
        {
            scrollRect = {
                0,
                std::max<til::CoordType>(0, _r.scrollOffset),
                _r.cellCount.x,
                _r.cellCount.y + std::min<til::CoordType>(0, _r.scrollOffset),
            };
            scrollOffset = {
                0,
                _r.scrollOffset,
            };

            scrollRect.top *= _r.fontMetrics.cellSize.y;
            scrollRect.right *= _r.fontMetrics.cellSize.x;
            scrollRect.bottom *= _r.fontMetrics.cellSize.y;

            scrollOffset.y *= _r.fontMetrics.cellSize.y;

            params.pScrollRect = &scrollRect;
            params.pScrollOffset = &scrollOffset;
        }

        THROW_IF_FAILED(_r.swapChain->Present1(1, 0, &params));
    }
    else
    {
        THROW_IF_FAILED(_r.swapChain->Present(1, 0));
    }

    _r.waitForPresentation = true;

    if (!_r.dxgiFactory->IsCurrent())
    {
        WI_SetFlag(_api.invalidations, ApiInvalidations::Device);
    }

    return S_OK;
}
catch (const wil::ResultException& exception)
{
    // TODO: this writes to _api.
    return _handleException(exception);
}
CATCH_RETURN()

[[nodiscard]] bool AtlasEngine::RequiresContinuousRedraw() noexcept
{
    return debugGeneralPerformance || _r.requiresContinuousRedraw;
}

void AtlasEngine::WaitUntilCanRender() noexcept
{
    // IDXGISwapChain2::GetFrameLatencyWaitableObject returns an auto-reset event.
    // Once we've waited on the event, waiting on it again will block until the timeout elapses.
    // _r.waitForPresentation guards against this.
    if (!debugGeneralPerformance && std::exchange(_r.waitForPresentation, false))
    {
        WaitForSingleObjectEx(_r.frameLatencyWaitableObject.get(), 100, true);
#ifndef NDEBUG
        _r.frameLatencyWaitableObjectUsed = true;
#endif
    }
}

#pragma endregion

void AtlasEngine::_renderWithCustomShader() const
{
    // Render with our main shader just like Present().
    {
        // OM: Output Merger
        _r.deviceContext->OMSetRenderTargets(1, _r.customOffscreenTextureTargetView.addressof(), nullptr);
        _r.deviceContext->Draw(3, 0);
    }

    // Update the custom shader's constant buffer.
    {
        CustomConstBuffer data;
        data.time = std::chrono::duration<float>(std::chrono::steady_clock::now() - _r.customShaderStartTime).count();
        data.scale = _r.pixelPerDIP;
        data.resolution.x = static_cast<float>(_r.cellCount.x * _r.fontMetrics.cellSize.x);
        data.resolution.y = static_cast<float>(_r.cellCount.y * _r.fontMetrics.cellSize.y);
        data.background = colorFromU32<f32x4>(_r.backgroundColor);

#pragma warning(suppress : 26494) // Variable 'mapped' is uninitialized. Always initialize an object (type.5).
        D3D11_MAPPED_SUBRESOURCE mapped;
        THROW_IF_FAILED(_r.deviceContext->Map(_r.customShaderConstantBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
        assert(mapped.RowPitch >= sizeof(data));
        memcpy(mapped.pData, &data, sizeof(data));
        _r.deviceContext->Unmap(_r.customShaderConstantBuffer.get(), 0);
    }

    // Render with the custom shader.
    {
        // OM: Output Merger
        // customOffscreenTextureView was just rendered to via customOffscreenTextureTargetView and is
        // set as the output target. Before we can use it as an input we have to remove it as an output.
        _r.deviceContext->OMSetRenderTargets(1, _r.renderTargetView.addressof(), nullptr);

        // VS: Vertex Shader
        _r.deviceContext->VSSetShader(_r.customVertexShader.get(), nullptr, 0);

        // PS: Pixel Shader
        _r.deviceContext->PSSetShader(_r.customPixelShader.get(), nullptr, 0);
        _r.deviceContext->PSSetConstantBuffers(0, 1, _r.customShaderConstantBuffer.addressof());
        _r.deviceContext->PSSetShaderResources(0, 1, _r.customOffscreenTextureView.addressof());
        _r.deviceContext->PSSetSamplers(0, 1, _r.customShaderSamplerState.addressof());

        _r.deviceContext->Draw(4, 0);
    }

    // For the next frame we need to restore our context state.
    {
        // VS: Vertex Shader
        _r.deviceContext->VSSetShader(_r.vertexShader.get(), nullptr, 0);

        // PS: Pixel Shader
        _r.deviceContext->PSSetShader(_r.pixelShader.get(), nullptr, 0);
        _r.deviceContext->PSSetConstantBuffers(0, 1, _r.constantBuffer.addressof());
        const std::array resources{ _r.cellView.get(), _r.atlasView.get() };
        _r.deviceContext->PSSetShaderResources(0, gsl::narrow_cast<UINT>(resources.size()), resources.data());
        _r.deviceContext->PSSetSamplers(0, 0, nullptr);
    }
}

void AtlasEngine::_setShaderResources() const
{
    // IA: Input Assembler
    // Our vertex shader uses a trick from Bill Bilodeau published in
    // "Vertex Shader Tricks" at GDC14 to draw a fullscreen triangle
    // without vertex/index buffers. This prepares our context for this.
    _r.deviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    _r.deviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    _r.deviceContext->IASetInputLayout(nullptr);
    _r.deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // VS: Vertex Shader
    _r.deviceContext->VSSetShader(_r.vertexShader.get(), nullptr, 0);

    // PS: Pixel Shader
    _r.deviceContext->PSSetShader(_r.pixelShader.get(), nullptr, 0);
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

    {
        const auto surface = atlasBuffer.query<IDXGISurface>();

        wil::com_ptr<IDWriteRenderingParams1> renderingParams;
        DWrite_GetRenderParams(_sr.dwriteFactory.get(), &_r.gamma, &_r.cleartypeEnhancedContrast, &_r.grayscaleEnhancedContrast, renderingParams.addressof());

        D2D1_RENDER_TARGET_PROPERTIES props{};
        props.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
        props.pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED };
        props.dpiX = static_cast<float>(_r.dpi);
        props.dpiY = static_cast<float>(_r.dpi);
        wil::com_ptr<ID2D1RenderTarget> renderTarget;
        THROW_IF_FAILED(_sr.d2dFactory->CreateDxgiSurfaceRenderTarget(surface.get(), &props, renderTarget.addressof()));
        _r.d2dRenderTarget = renderTarget.query<ID2D1DeviceContext>();

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
        THROW_IF_FAILED(_r.d2dRenderTarget->CreateSolidColorBrush(&color, nullptr, _r.brush.put()));
        _r.brushColor = 0xffffffff;
    }

    _r.atlasSizeInPixel = requiredSize;
    _r.atlasBuffer = std::move(atlasBuffer);
    _r.atlasView = std::move(atlasView);
    _setShaderResources();

    WI_SetAllFlags(_r.invalidations, RenderInvalidations::ConstBuffer);
    WI_SetFlagIf(_r.invalidations, RenderInvalidations::Cursor, !copyFromExisting);
}

void AtlasEngine::_processGlyphQueue()
{
    if (_r.glyphQueue.empty() && WI_IsFlagClear(_r.invalidations, RenderInvalidations::Cursor))
    {
        return;
    }

    _r.d2dRenderTarget->BeginDraw();

    if (WI_IsFlagSet(_r.invalidations, RenderInvalidations::Cursor))
    {
        _drawCursor({ 0, 0, 1, 1 }, 0xffffffff, true);
        WI_ClearFlag(_r.invalidations, RenderInvalidations::Cursor);
    }

    for (const auto& it : _r.glyphQueue)
    {
        _drawGlyph(it);
    }
    _r.glyphQueue.clear();

    THROW_IF_FAILED(_r.d2dRenderTarget->EndDraw());
}

void AtlasEngine::_drawGlyph(const TileHashMap::iterator& it) const
{
    const auto key = it->first.data();
    const auto value = it->second.data();
    const auto coords = &value->coords[0];
    const auto charsLength = key->charCount;
    const auto cellCount = key->attributes.cellCount;
    const auto textFormat = _getTextFormat(key->attributes.bold, key->attributes.italic);
    const auto coloredGlyph = WI_IsFlagSet(value->flags, CellFlags::ColoredGlyph);
    const auto cachedLayout = _getCachedGlyphLayout(&key->chars[0], charsLength, cellCount, textFormat, coloredGlyph);

    // Colored glyphs cannot be drawn in linear gamma.
    // That's why we're simply alpha-blending them in the shader.
    // In order for this to work correctly we have to prevent them from being drawn
    // with ClearType, because we would then lack the alpha channel for the glyphs.
    if (_api.realizedAntialiasingMode == D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE)
    {
        _r.d2dRenderTarget->SetTextAntialiasMode(coloredGlyph ? D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE : D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    }

    for (u16 i = 0; i < cellCount; ++i)
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

        _r.d2dRenderTarget->PushAxisAlignedClip(&rect, D2D1_ANTIALIAS_MODE_ALIASED);
        _r.d2dRenderTarget->Clear();

        cachedLayout.applyScaling(_r.d2dRenderTarget.get(), origin);

        // Now that we're done using origin to calculate the center point for our transformation
        // we can use it for its intended purpose to slightly shift the glyph around.
        origin.x += cachedLayout.offset.x;
        origin.y += cachedLayout.offset.y;
        _r.d2dRenderTarget->DrawTextLayout(origin, cachedLayout.textLayout.get(), _r.brush.get(), cachedLayout.options);

        cachedLayout.undoScaling(_r.d2dRenderTarget.get());

        _r.d2dRenderTarget->PopAxisAlignedClip();
    }
}

AtlasEngine::CachedGlyphLayout AtlasEngine::_getCachedGlyphLayout(const wchar_t* chars, u16 charsLength, u16 cellCount, IDWriteTextFormat* textFormat, bool coloredGlyph) const
{
    const f32x2 layoutBox{ cellCount * _r.cellSizeDIP.x, _r.cellSizeDIP.y };
    bool scalingRequired = false;
    f32x2 offset{ 0, 0 };
    f32x2 scale{ 1, 1 };
    f32x2 scaleCenter;

    // See D2DFactory::DrawText
    wil::com_ptr<IDWriteTextLayout> textLayout;
    THROW_IF_FAILED(_sr.dwriteFactory->CreateTextLayout(chars, charsLength, textFormat, layoutBox.x, layoutBox.y, textLayout.addressof()));
    if (_r.typography)
    {
        textLayout->SetTypography(_r.typography.get(), { 0, charsLength });
    }

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

    if (charsLength == 1 && isInInversionList(blockCharacters, chars[0]))
    {
        wil::com_ptr<IDWriteFontCollection> fontCollection;
        THROW_IF_FAILED(textFormat->GetFontCollection(fontCollection.addressof()));
        const auto baseWeight = textFormat->GetFontWeight();
        const auto baseStyle = textFormat->GetFontStyle();

        TextAnalysisSource analysisSource{ chars, 1 };
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

            // Don't adjust the size of block glyphs that are part of the user's chosen font.
            if (std::ranges::find(_r.fontFaces, fontFace) == std::end(_r.fontFaces))
            {
                DWRITE_FONT_METRICS metrics;
                fontFace->GetMetrics(&metrics);

                static constexpr u32 codePoint = L'\u2588'; // Full Block character
                u16 glyphIndex;
                THROW_IF_FAILED(fontFace->GetGlyphIndicesW(&codePoint, 1, &glyphIndex));

                if (glyphIndex)
                {
                    DWRITE_GLYPH_METRICS glyphMetrics;
                    THROW_IF_FAILED(fontFace->GetDesignGlyphMetrics(&glyphIndex, 1, &glyphMetrics));

                    const auto fontScale = _r.fontMetrics.fontSizeInDIP / metrics.designUnitsPerEm;

                    // How-to-DWRITE_OVERHANG_METRICS given a single glyph:
                    DWRITE_OVERHANG_METRICS overhang;
                    overhang.left = static_cast<f32>(glyphMetrics.leftSideBearing) * fontScale;
                    overhang.top = static_cast<f32>(glyphMetrics.verticalOriginY - glyphMetrics.topSideBearing) * fontScale - _r.fontMetrics.baselineInDIP;
                    overhang.right = static_cast<f32>(gsl::narrow_cast<INT32>(glyphMetrics.advanceWidth) - glyphMetrics.rightSideBearing) * fontScale - layoutBox.x;
                    overhang.bottom = static_cast<f32>(gsl::narrow_cast<INT32>(glyphMetrics.advanceHeight) - glyphMetrics.verticalOriginY - glyphMetrics.bottomSideBearing) * fontScale + _r.fontMetrics.baselineInDIP - layoutBox.y;

                    scalingRequired = true;
                    // Center glyphs.
                    offset.x = (overhang.left - overhang.right) * 0.5f;
                    offset.y = (overhang.top - overhang.bottom) * 0.5f;
                    // We always want box drawing glyphs to exactly match the size of a terminal cell.
                    // But add 1px to the destination size, so that we don't end up with fractional pixels.
                    scale.x = (layoutBox.x + _r.pixelPerDIP) / (layoutBox.x + overhang.left + overhang.right);
                    scale.y = (layoutBox.y + _r.pixelPerDIP) / (layoutBox.y + overhang.top + overhang.bottom);
                    // Now that the glyph is in the center of the cell thanks
                    // to the offset, the scaleCenter is center of the cell.
                    scaleCenter.x = layoutBox.x * 0.5f;
                    scaleCenter.y = layoutBox.y * 0.5f;
                }
            }
        }
    }
    else
    {
        DWRITE_OVERHANG_METRICS overhang;
        THROW_IF_FAILED(textLayout->GetOverhangMetrics(&overhang));

        auto actualSizeX = layoutBox.x + overhang.left + overhang.right;

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
            actualSizeX *= advanceScale;
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
        offset.x = std::max(0.0f, overhang.left) - std::max(0.0f, overhang.right);
        scaleCenter.x = offset.x;
        scaleCenter.y = _r.fontMetrics.baselineInDIP;

        if ((actualSizeX - layoutBox.x) > _r.dipPerPixel)
        {
            scalingRequired = true;
            offset.x = (overhang.left - overhang.right) * 0.5f;
            scale.x = layoutBox.x / actualSizeX;
            scale.y = scale.x;
            scaleCenter.x = layoutBox.x * 0.5f;
        }
        if (overhang.top > _r.dipPerPixel || overhang.bottom > _r.dipPerPixel)
        {
            const auto descend = _r.cellSizeDIP.y - _r.fontMetrics.baselineInDIP;
            const auto scaleTop = _r.fontMetrics.baselineInDIP / (_r.fontMetrics.baselineInDIP + overhang.top);
            const auto scaleBottom = descend / (descend + overhang.bottom);
            scalingRequired = true;
            scale.x = std::min(scale.x, std::min(scaleTop, scaleBottom));
            scale.y = scale.x;
        }
    }

    auto options = D2D1_DRAW_TEXT_OPTIONS_NONE;
    // D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT enables a bunch of internal machinery
    // which doesn't have to run if we know we can't use it anyways in the shader.
    WI_SetFlagIf(options, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT, coloredGlyph);
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

    // ClearType basically has a 3x higher horizontal resolution. To make our glyphs render the same everywhere,
    // it's probably for the best to ensure we initially rasterize them on a whole pixel boundary.
    // (https://en.wikipedia.org/wiki/ClearType#How_ClearType_works)
    offset.x = roundf(offset.x * _r.pixelPerDIP) * _r.dipPerPixel;
    // As explained below, we use D2D1_DRAW_TEXT_OPTIONS_NO_SNAP to prevent a weird issue with baseline snapping.
    // But we do want it technically, so this re-implements baseline snapping... I think?
    offset.y = roundf(offset.y * _r.pixelPerDIP) * _r.dipPerPixel;

    return CachedGlyphLayout{
        .textLayout = textLayout,
        .offset = offset,
        .scale = scale,
        .scaleCenter = scaleCenter,
        .options = options,
        .scalingRequired = scalingRequired,
    };
}

void AtlasEngine::_drawCursor(u16r rect, u32 color, bool clear)
{
    // lineWidth is in D2D's DIPs. For instance if we have a 150-200% zoom scale we want to draw a 2px wide line.
    // At 150% scale lineWidth thus needs to be 1.33333... because at a zoom scale of 1.5 this results in a 2px wide line.
    const auto lineWidth = std::max(1.0f, static_cast<float>((_r.dpi + USER_DEFAULT_SCREEN_DPI / 2) / USER_DEFAULT_SCREEN_DPI * USER_DEFAULT_SCREEN_DPI) / static_cast<float>(_r.dpi));
    const auto cursorType = static_cast<CursorType>(_r.cursorOptions.cursorType);

    // `clip` is the rectangle within our texture atlas that's reserved for our cursor texture, ...
    D2D1_RECT_F clip;
    clip.left = static_cast<float>(rect.left) * _r.cellSizeDIP.x;
    clip.top = static_cast<float>(rect.top) * _r.cellSizeDIP.y;
    clip.right = static_cast<float>(rect.right) * _r.cellSizeDIP.x;
    clip.bottom = static_cast<float>(rect.bottom) * _r.cellSizeDIP.y;

    // ... whereas `rect` is just the visible (= usually white) portion of our cursor.
    auto box = clip;

    switch (cursorType)
    {
    case CursorType::Legacy:
        box.top = box.bottom - _r.cellSizeDIP.y * static_cast<float>(_r.cursorOptions.heightPercentage) / 100.0f;
        break;
    case CursorType::VerticalBar:
        box.right = box.left + lineWidth;
        break;
    case CursorType::EmptyBox:
    {
        // EmptyBox is drawn as a line and unlike filled rectangles those are drawn centered on their
        // coordinates in such a way that the line border extends half the width to each side.
        // --> Our coordinates have to be 0.5 DIP off in order to draw a 2px line on a 200% scaling.
        const auto halfWidth = lineWidth / 2.0f;
        box.left += halfWidth;
        box.top += halfWidth;
        box.right -= halfWidth;
        box.bottom -= halfWidth;
        break;
    }
    case CursorType::Underscore:
    case CursorType::DoubleUnderscore:
        box.top = box.bottom - lineWidth;
        break;
    default:
        break;
    }

    const auto brush = _brushWithColor(color);

    // We need to clip the area we draw in to ensure we don't
    // accidentally draw into any neighboring texture atlas tiles.
    _r.d2dRenderTarget->PushAxisAlignedClip(&clip, D2D1_ANTIALIAS_MODE_ALIASED);

    if (clear)
    {
        _r.d2dRenderTarget->Clear();
    }

    if (cursorType == CursorType::EmptyBox)
    {
        _r.d2dRenderTarget->DrawRectangle(&box, brush, lineWidth);
    }
    else
    {
        _r.d2dRenderTarget->FillRectangle(&box, brush);
    }

    if (cursorType == CursorType::DoubleUnderscore)
    {
        const auto offset = lineWidth * 2.0f;
        box.top -= offset;
        box.bottom -= offset;
        _r.d2dRenderTarget->FillRectangle(&box, brush);
    }

    _r.d2dRenderTarget->PopAxisAlignedClip();
}

ID2D1Brush* AtlasEngine::_brushWithColor(u32 color)
{
    if (_r.brushColor != color)
    {
        const auto d2dColor = colorFromU32(color);
        THROW_IF_FAILED(_r.d2dRenderTarget->CreateSolidColorBrush(&d2dColor, nullptr, _r.brush.put()));
        _r.brushColor = color;
    }
    return _r.brush.get();
}

AtlasEngine::CachedGlyphLayout::operator bool() const noexcept
{
    return static_cast<bool>(textLayout);
}

void AtlasEngine::CachedGlyphLayout::reset() noexcept
{
    textLayout.reset();
}

void AtlasEngine::CachedGlyphLayout::applyScaling(ID2D1RenderTarget* d2dRenderTarget, D2D1_POINT_2F origin) const noexcept
{
    __assume(d2dRenderTarget != nullptr);

    if (scalingRequired)
    {
        const D2D1_MATRIX_3X2_F transform{
            scale.x,
            0,
            0,
            scale.y,
            (origin.x + scaleCenter.x) * (1.0f - scale.x),
            (origin.y + scaleCenter.y) * (1.0f - scale.y),
        };
        d2dRenderTarget->SetTransform(&transform);
    }
}

void AtlasEngine::CachedGlyphLayout::undoScaling(ID2D1RenderTarget* d2dRenderTarget) const noexcept
{
    __assume(d2dRenderTarget != nullptr);

    if (scalingRequired)
    {
        static constexpr D2D1_MATRIX_3X2_F identity{ 1, 0, 0, 1, 0, 0 };
        d2dRenderTarget->SetTransform(&identity);
    }
}

void AtlasEngine::_d2dPresent()
{
    if (!_r.d2dRenderTarget)
    {
        _d2dCreateRenderTarget();
    }

    _d2dDrawDirtyArea();

    _r.glyphQueue.clear();
    WI_ClearAllFlags(_r.invalidations, RenderInvalidations::Cursor | RenderInvalidations::ConstBuffer);
}

void AtlasEngine::_d2dCreateRenderTarget()
{
    {
        wil::com_ptr<ID3D11Texture2D> buffer;
        THROW_IF_FAILED(_r.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), buffer.put_void()));

        const auto surface = buffer.query<IDXGISurface>();

        D2D1_RENDER_TARGET_PROPERTIES props{};
        props.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
        props.pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED };
        props.dpiX = static_cast<float>(_r.dpi);
        props.dpiY = static_cast<float>(_r.dpi);
        wil::com_ptr<ID2D1RenderTarget> renderTarget;
        THROW_IF_FAILED(_sr.d2dFactory->CreateDxgiSurfaceRenderTarget(surface.get(), &props, renderTarget.addressof()));
        _r.d2dRenderTarget = renderTarget.query<ID2D1DeviceContext>();

        // In case _api.realizedAntialiasingMode is D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE we'll
        // continuously adjust it in AtlasEngine::_drawGlyph. See _drawGlyph.
        _r.d2dRenderTarget->SetTextAntialiasMode(static_cast<D2D1_TEXT_ANTIALIAS_MODE>(_api.realizedAntialiasingMode));
    }
    {
        static constexpr D2D1_COLOR_F color{ 1, 1, 1, 1 };
        THROW_IF_FAILED(_r.d2dRenderTarget->CreateSolidColorBrush(&color, nullptr, _r.brush.put()));
        _r.brushColor = 0xffffffff;
    }
}

void AtlasEngine::_d2dDrawDirtyArea()
{
    struct CellFlagHandler
    {
        CellFlags filter;
        decltype(&AtlasEngine::_d2dCellFlagRendererCursor) func;
    };

    static constexpr std::array cellFlagHandlers{
        // Ordered by lowest to highest "layer".
        // The selection for instance is drawn on top of underlines, not under them.
        CellFlagHandler{ CellFlags::Underline, &AtlasEngine::_d2dCellFlagRendererUnderline },
        CellFlagHandler{ CellFlags::UnderlineDotted, &AtlasEngine::_d2dCellFlagRendererUnderlineDotted },
        CellFlagHandler{ CellFlags::UnderlineDouble, &AtlasEngine::_d2dCellFlagRendererUnderlineDouble },
        CellFlagHandler{ CellFlags::Strikethrough, &AtlasEngine::_d2dCellFlagRendererStrikethrough },
        CellFlagHandler{ CellFlags::Cursor, &AtlasEngine::_d2dCellFlagRendererCursor },
        CellFlagHandler{ CellFlags::Selected, &AtlasEngine::_d2dCellFlagRendererSelected },
    };

    auto left = gsl::narrow<u16>(_r.dirtyRect.left);
    auto top = gsl::narrow<u16>(_r.dirtyRect.top);
    auto right = gsl::narrow<u16>(_r.dirtyRect.right);
    auto bottom = gsl::narrow<u16>(_r.dirtyRect.bottom);
    if constexpr (debugGlyphGenerationPerformance)
    {
        left = 0;
        top = 0;
        right = _r.cellCount.x;
        bottom = _r.cellCount.y;
    }

    _r.d2dRenderTarget->BeginDraw();

    if (WI_IsFlagSet(_r.invalidations, RenderInvalidations::ConstBuffer))
    {
        _r.d2dRenderTarget->Clear(colorFromU32(_r.backgroundColor));
    }

    for (u16 y = top; y < bottom; ++y)
    {
        const Cell* cells = _getCell(0, y);
        const TileHashMap::iterator* cellGlyphMappings = _getCellGlyphMapping(0, y);

        // left/right might intersect a wide glyph. We have to extend left/right
        // to include the entire glyph so that we can properly render it.
        // Since a series of identical narrow glyphs (2 spaces for instance) are stored in cellGlyphMappings
        // just like a single wide glyph (2 references to the same glyph in a row), the only way for us to
        // know where wide glyphs begin and end is to iterate the entire row and use the stored `cellCount`.
        u16 beg = 0;
        for (;;)
        {
            const auto cellCount = cellGlyphMappings[beg]->first.data()->attributes.cellCount;
            const auto begNext = gsl::narrow_cast<u16>(beg + cellCount);

            if (begNext > left)
            {
                break;
            }

            beg = begNext;
        }
        auto end = beg;
        for (;;)
        {
            const auto cellCount = cellGlyphMappings[end]->first.data()->attributes.cellCount;
            end += cellCount;

            if (end >= right)
            {
                break;
            }
        }

        // Draw background.
        {
            _r.d2dRenderTarget->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);

            auto x1 = beg;
            auto x2 = gsl::narrow_cast<u16>(x1 + 1);
            auto currentColor = cells[x1].color.y;

            for (; x2 < end; ++x2)
            {
                const auto color = cells[x2].color.y;

                if (currentColor != color)
                {
                    const u16r rect{ x1, y, x2, gsl::narrow_cast<u16>(y + 1) };
                    _d2dFillRectangle(rect, currentColor);
                    x1 = x2;
                    currentColor = color;
                }
            }

            {
                const u16r rect{ x1, y, x2, gsl::narrow_cast<u16>(y + 1) };
                _d2dFillRectangle(rect, currentColor);
            }

            _r.d2dRenderTarget->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
        }

        // Draw text.
        for (auto x = beg; x < end;)
        {
            const auto& it = cellGlyphMappings[x];
            const u16x2 coord{ x, y };
            const auto color = cells[x].color.x;
            x += _d2dDrawGlyph(it, coord, color);
        }

        // Draw underlines, cursors, selections, etc.
        for (const auto& handler : cellFlagHandlers)
        {
            auto x1 = beg;
            auto currentFlags = CellFlags::None;

            for (auto x2 = beg; x2 < end; ++x2)
            {
                const auto flags = cells[x2].flags & handler.filter;

                if (currentFlags != flags)
                {
                    if (currentFlags != CellFlags::None)
                    {
                        const u16r rect{ x1, y, x2, gsl::narrow_cast<u16>(y + 1) };
                        const auto color = cells[x1].color.x;
                        (this->*handler.func)(rect, color);
                    }

                    x1 = x2;
                    currentFlags = flags;
                }
            }

            if (currentFlags != CellFlags::None)
            {
                const u16r rect{ x1, y, right, gsl::narrow_cast<u16>(y + 1) };
                const auto color = cells[x1].color.x;
                (this->*handler.func)(rect, color);
            }
        }
    }

    THROW_IF_FAILED(_r.d2dRenderTarget->EndDraw());
}

// See _drawGlyph() for reference.
AtlasEngine::u16 AtlasEngine::_d2dDrawGlyph(const TileHashMap::iterator& it, const u16x2 coord, const u32 color)
{
    const auto key = it->first.data();
    const auto value = it->second.data();
    const auto charsLength = key->charCount;
    const auto cellCount = key->attributes.cellCount;
    const auto textFormat = _getTextFormat(key->attributes.bold, key->attributes.italic);
    const auto coloredGlyph = WI_IsFlagSet(value->flags, CellFlags::ColoredGlyph);

    auto& cachedLayout = it->second.cachedLayout;
    if (!cachedLayout)
    {
        cachedLayout = _getCachedGlyphLayout(&key->chars[0], charsLength, cellCount, textFormat, coloredGlyph);
    }

    D2D1_RECT_F rect;
    rect.left = static_cast<float>(coord.x) * _r.cellSizeDIP.x;
    rect.top = static_cast<float>(coord.y) * _r.cellSizeDIP.y;
    rect.right = static_cast<float>(coord.x + cellCount) * _r.cellSizeDIP.x;
    rect.bottom = rect.top + _r.cellSizeDIP.y;

    D2D1_POINT_2F origin;
    origin.x = rect.left;
    origin.y = rect.top;

    _r.d2dRenderTarget->PushAxisAlignedClip(&rect, D2D1_ANTIALIAS_MODE_ALIASED);

    cachedLayout.applyScaling(_r.d2dRenderTarget.get(), origin);

    origin.x += cachedLayout.offset.x;
    origin.y += cachedLayout.offset.y;
    _r.d2dRenderTarget->DrawTextLayout(origin, cachedLayout.textLayout.get(), _brushWithColor(color), cachedLayout.options);

    cachedLayout.undoScaling(_r.d2dRenderTarget.get());

    _r.d2dRenderTarget->PopAxisAlignedClip();

    return cellCount;
}

void AtlasEngine::_d2dDrawLine(u16r rect, u16 pos, u16 width, u32 color, ID2D1StrokeStyle* strokeStyle)
{
    const auto w = static_cast<float>(width) * _r.dipPerPixel;
    const auto y1 = static_cast<float>(rect.top) * _r.cellSizeDIP.y + static_cast<float>(pos) * _r.dipPerPixel + w * 0.5f;
    const auto x1 = static_cast<float>(rect.left) * _r.cellSizeDIP.x;
    const auto x2 = static_cast<float>(rect.right) * _r.cellSizeDIP.x;
    const auto brush = _brushWithColor(color);
    _r.d2dRenderTarget->DrawLine({ x1, y1 }, { x2, y1 }, brush, w, strokeStyle);
}

void AtlasEngine::_d2dFillRectangle(u16r rect, u32 color)
{
    const D2D1_RECT_F r{
        .left = static_cast<float>(rect.left) * _r.cellSizeDIP.x,
        .top = static_cast<float>(rect.top) * _r.cellSizeDIP.y,
        .right = static_cast<float>(rect.right) * _r.cellSizeDIP.x,
        .bottom = static_cast<float>(rect.bottom) * _r.cellSizeDIP.y,
    };
    const auto brush = _brushWithColor(color);
    _r.d2dRenderTarget->FillRectangle(r, brush);
}

void AtlasEngine::_d2dCellFlagRendererCursor(u16r rect, u32 color)
{
    _drawCursor(rect, _r.cursorOptions.cursorColor, false);
}

void AtlasEngine::_d2dCellFlagRendererSelected(u16r rect, u32 color)
{
    _d2dFillRectangle(rect, _r.selectionColor);
}

void AtlasEngine::_d2dCellFlagRendererUnderline(u16r rect, u32 color)
{
    _d2dDrawLine(rect, _r.fontMetrics.underlinePos, _r.fontMetrics.underlineWidth, color);
}

void AtlasEngine::_d2dCellFlagRendererUnderlineDotted(u16r rect, u32 color)
{
    if (!_r.dottedStrokeStyle)
    {
        static constexpr D2D1_STROKE_STYLE_PROPERTIES props{ .dashStyle = D2D1_DASH_STYLE_CUSTOM };
        static constexpr FLOAT dashes[2]{ 1, 2 };
        THROW_IF_FAILED(_sr.d2dFactory->CreateStrokeStyle(&props, &dashes[0], 2, _r.dottedStrokeStyle.addressof()));
    }

    _d2dDrawLine(rect, _r.fontMetrics.underlinePos, _r.fontMetrics.underlineWidth, color, _r.dottedStrokeStyle.get());
}

void AtlasEngine::_d2dCellFlagRendererUnderlineDouble(u16r rect, u32 color)
{
    _d2dDrawLine(rect, _r.fontMetrics.doubleUnderlinePos.x, _r.fontMetrics.thinLineWidth, color);
    _d2dDrawLine(rect, _r.fontMetrics.doubleUnderlinePos.y, _r.fontMetrics.thinLineWidth, color);
}

void AtlasEngine::_d2dCellFlagRendererStrikethrough(u16r rect, u32 color)
{
    _d2dDrawLine(rect, _r.fontMetrics.strikethroughPos, _r.fontMetrics.strikethroughWidth, color);
}
