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
    data.viewport.z = static_cast<float>(_r.cellCount.x * _r.cellSize.x);
    data.viewport.w = static_cast<float>(_r.cellCount.y * _r.cellSize.y);
    DWrite_GetGammaRatios(_r.gamma, data.gammaRatios);
    data.enhancedContrast = useClearType ? _r.cleartypeEnhancedContrast : _r.grayscaleEnhancedContrast;
    data.cellCountX = _r.cellCount.x;
    data.cellSize.x = _r.cellSize.x;
    data.cellSize.y = _r.cellSize.y;
    data.underlinePos.x = _r.underlinePos;
    data.underlinePos.y = _r.underlinePos + _r.lineThickness;
    data.strikethroughPos.x = _r.strikethroughPos;
    data.strikethroughPos.y = _r.strikethroughPos + _r.lineThickness;
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
    const auto cells = static_cast<u32>(key->attributes.cellCount);
    const auto textFormat = _getTextFormat(key->attributes.bold, key->attributes.italic);
    const auto coloredGlyph = WI_IsFlagSet(value->flags, CellFlags::ColoredGlyph);

    // See D2DFactory::DrawText
    wil::com_ptr<IDWriteTextLayout> textLayout;
    THROW_IF_FAILED(_sr.dwriteFactory->CreateTextLayout(&key->chars[0], charsLength, textFormat, cells * _r.cellSizeDIP.x, _r.cellSizeDIP.y, textLayout.addressof()));
    if (_r.typography)
    {
        textLayout->SetTypography(_r.typography.get(), { 0, charsLength });
    }

    auto options = D2D1_DRAW_TEXT_OPTIONS_CLIP;
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

    for (u32 i = 0; i < cells; ++i)
    {
        const auto coord = coords[i];

        D2D1_RECT_F rect;
        rect.left = static_cast<float>(coord.x) * static_cast<float>(USER_DEFAULT_SCREEN_DPI) / static_cast<float>(_r.dpi);
        rect.top = static_cast<float>(coord.y) * static_cast<float>(USER_DEFAULT_SCREEN_DPI) / static_cast<float>(_r.dpi);
        rect.right = rect.left + _r.cellSizeDIP.x;
        rect.bottom = rect.top + _r.cellSizeDIP.y;

        D2D1_POINT_2F origin;
        origin.x = rect.left - i * _r.cellSizeDIP.x;
        origin.y = rect.top;

        _r.d2dRenderTarget->PushAxisAlignedClip(&rect, D2D1_ANTIALIAS_MODE_ALIASED);
        _r.d2dRenderTarget->Clear();
        _r.d2dRenderTarget->DrawTextLayout(origin, textLayout.get(), _r.brush.get(), options);
        _r.d2dRenderTarget->PopAxisAlignedClip();
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
