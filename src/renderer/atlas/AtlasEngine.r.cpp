// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AtlasEngine.h"

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
    _reserveScratchpadSize(_r.maxEncounteredCellCount);
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
    assert(_r.frameLatencyWaitableObjectUsed);

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

AtlasEngine::f32x4 AtlasEngine::_getGammaRatios(float gamma) noexcept
{
    static constexpr f32x4 gammaIncorrectTargetRatios[13]{
        { 0.0000f / 4.f, 0.0000f / 4.f, 0.0000f / 4.f, 0.0000f / 4.f }, // gamma = 1.0
        { 0.0166f / 4.f, -0.0807f / 4.f, 0.2227f / 4.f, -0.0751f / 4.f }, // gamma = 1.1
        { 0.0350f / 4.f, -0.1760f / 4.f, 0.4325f / 4.f, -0.1370f / 4.f }, // gamma = 1.2
        { 0.0543f / 4.f, -0.2821f / 4.f, 0.6302f / 4.f, -0.1876f / 4.f }, // gamma = 1.3
        { 0.0739f / 4.f, -0.3963f / 4.f, 0.8167f / 4.f, -0.2287f / 4.f }, // gamma = 1.4
        { 0.0933f / 4.f, -0.5161f / 4.f, 0.9926f / 4.f, -0.2616f / 4.f }, // gamma = 1.5
        { 0.1121f / 4.f, -0.6395f / 4.f, 1.1588f / 4.f, -0.2877f / 4.f }, // gamma = 1.6
        { 0.1300f / 4.f, -0.7649f / 4.f, 1.3159f / 4.f, -0.3080f / 4.f }, // gamma = 1.7
        { 0.1469f / 4.f, -0.8911f / 4.f, 1.4644f / 4.f, -0.3234f / 4.f }, // gamma = 1.8
        { 0.1627f / 4.f, -1.0170f / 4.f, 1.6051f / 4.f, -0.3347f / 4.f }, // gamma = 1.9
        { 0.1773f / 4.f, -1.1420f / 4.f, 1.7385f / 4.f, -0.3426f / 4.f }, // gamma = 2.0
        { 0.1908f / 4.f, -1.2652f / 4.f, 1.8650f / 4.f, -0.3476f / 4.f }, // gamma = 2.1
        { 0.2031f / 4.f, -1.3864f / 4.f, 1.9851f / 4.f, -0.3501f / 4.f }, // gamma = 2.2
    };
    static constexpr auto norm13 = static_cast<float>(static_cast<double>(0x10000) / (255 * 255) * 4);
    static constexpr auto norm24 = static_cast<float>(static_cast<double>(0x100) / (255) * 4);

    gamma = clamp(gamma, 1.0f, 2.2f);

    const size_t index = gsl::narrow_cast<size_t>(std::round((gamma - 1.0f) / 1.2f * 12.0f));
    const auto& ratios = gammaIncorrectTargetRatios[index];
    return { norm13 * ratios.x, norm24 * ratios.y, norm13 * ratios.z, norm24 * ratios.w };
}

void AtlasEngine::_updateConstantBuffer() const noexcept
{
    ConstBuffer data;
    data.viewport.x = 0;
    data.viewport.y = 0;
    data.viewport.z = static_cast<float>(_r.cellCount.x * _r.cellSize.x);
    data.viewport.w = static_cast<float>(_r.cellCount.y * _r.cellSize.y);
    data.gammaRatios = _getGammaRatios(_r.gamma);
    data.grayscaleEnhancedContrast = _r.grayscaleEnhancedContrast;
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
#pragma warning(suppress : 26447) // The function is declared 'noexcept' but calls function '...' which may throw exceptions (f.6).
    _r.deviceContext->UpdateSubresource(_r.constantBuffer.get(), 0, nullptr, &data, 0, 0);
}

void AtlasEngine::_adjustAtlasSize()
{
    if (_r.atlasPosition.y < _r.atlasSizeInPixel.y && _r.atlasPosition.x < _r.atlasSizeInPixel.x)
    {
        return;
    }

    const u32 limitX = _r.atlasSizeInPixelLimit.x;
    const u32 limitY = _r.atlasSizeInPixelLimit.y;
    const u32 posX = _r.atlasPosition.x;
    const u32 posY = _r.atlasPosition.y;
    const u32 cellX = _r.cellSize.x;
    const u32 cellY = _r.cellSize.y;
    const auto perCellArea = cellX * cellY;

    // The texture atlas is filled like this:
    //   x →
    // y +--------------+
    // ↓ |XXXXXXXXXXXXXX|
    //   |XXXXXXXXXXXXXX|
    //   |XXXXX↖        |
    //   |      |       |
    //   +------|-------+
    // This is where _r.atlasPosition points at.
    //
    // Each X is a glyph texture tile that's occupied.
    // We can compute the area of pixels consumed by adding the first
    // two lines of X (rectangular) together with the last line of X.
    const auto currentArea = posY * limitX + posX * cellY;
    // minArea reserves enough room for 64 cells in all cases (mainly during startup).
    const auto minArea = 64 * perCellArea;
    auto newArea = std::max(minArea, currentArea);

    // I want the texture to grow exponentially similar to std::vector, as this
    // ensures we don't need to resize the texture again right after having done.
    // This rounds newArea up to the next power of 2.
    unsigned long int index;
    _BitScanReverse(&index, newArea); // newArea can't be 0
    newArea = u32{ 1 } << (index + 1);

    const auto pixelPerRow = limitX * cellY;
    // newArea might be just large enough that it spans N full rows of cells and one additional row
    // just barely. This algorithm rounds up newArea to the _next_ multiple of cellY.
    const auto wantedHeight = (newArea + pixelPerRow - 1) / pixelPerRow * cellY;
    // The atlas might either be a N rows of full width (xLimit) or just one
    // row (where wantedHeight == cellY) that doesn't quite fill it's maximum width yet.
    const auto wantedWidth = wantedHeight != cellY ? limitX : newArea / perCellArea * cellX;

    // We know that limitX/limitY were u16 originally, and thus it's safe to narrow_cast it back.
    const auto height = gsl::narrow_cast<u16>(std::min(limitY, wantedHeight));
    const auto width = gsl::narrow_cast<u16>(std::min(limitX, wantedWidth));

    assert(width != 0);
    assert(height != 0);

    wil::com_ptr<ID3D11Texture2D> atlasBuffer;
    wil::com_ptr<ID3D11ShaderResourceView> atlasView;
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc = { 1, 0 };
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
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

    _r.atlasSizeInPixel = u16x2{ width, height };
    _r.atlasBuffer = std::move(atlasBuffer);
    _r.atlasView = std::move(atlasView);
    _setShaderResources();

    WI_SetFlagIf(_r.invalidations, RenderInvalidations::Cursor, !copyFromExisting);
}

void AtlasEngine::_reserveScratchpadSize(u16 minWidth)
{
    if (minWidth <= _r.scratchpadCellWidth)
    {
        return;
    }

    // The new size is the greater of ... cells wide:
    // * 2
    // * minWidth
    // * current size * 1.5
    const auto newWidth = std::max<UINT>(std::max<UINT>(2, minWidth), _r.scratchpadCellWidth + (_r.scratchpadCellWidth >> 1));

    _r.d2dRenderTarget.reset();
    _r.atlasScratchpad.reset();

    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = _r.cellSize.x * newWidth;
        desc.Height = _r.cellSize.y;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc = { 1, 0 };
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        THROW_IF_FAILED(_r.device->CreateTexture2D(&desc, nullptr, _r.atlasScratchpad.put()));
    }
    {
        const auto surface = _r.atlasScratchpad.query<IDXGISurface>();

        wil::com_ptr<IDWriteRenderingParams1> defaultParams;
        THROW_IF_FAILED(_sr.dwriteFactory->CreateRenderingParams(reinterpret_cast<IDWriteRenderingParams**>(defaultParams.addressof())));
        wil::com_ptr<IDWriteRenderingParams1> renderingParams;
        THROW_IF_FAILED(_sr.dwriteFactory->CreateCustomRenderingParams(1.0f, 0.0f, 0.0f, defaultParams->GetClearTypeLevel(), defaultParams->GetPixelGeometry(), defaultParams->GetRenderingMode(), renderingParams.addressof()));

        _r.gamma = defaultParams->GetGamma();
        _r.grayscaleEnhancedContrast = defaultParams->GetGrayscaleEnhancedContrast();

        D2D1_RENDER_TARGET_PROPERTIES props{};
        props.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
        props.pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED };
        props.dpiX = static_cast<float>(_r.dpi);
        props.dpiY = static_cast<float>(_r.dpi);
        THROW_IF_FAILED(_sr.d2dFactory->CreateDxgiSurfaceRenderTarget(surface.get(), &props, _r.d2dRenderTarget.put()));

        // We don't really use D2D for anything except DWrite, but it
        // can't hurt to ensure that everything it does is pixel aligned.
        _r.d2dRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
        // Ensure that D2D uses the exact same gamma as our shader uses.
        _r.d2dRenderTarget->SetTextRenderingParams(renderingParams.get());
        // We can't set the antialiasingMode here, as D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE
        // will force the alpha channel to be 0 for _all_ text.
        //_r.d2dRenderTarget->SetTextAntialiasMode(static_cast<D2D1_TEXT_ANTIALIAS_MODE>(_api.antialiasingMode));
    }
    {
        static constexpr D2D1_COLOR_F color{ 1, 1, 1, 1 };
        wil::com_ptr<ID2D1SolidColorBrush> brush;
        THROW_IF_FAILED(_r.d2dRenderTarget->CreateSolidColorBrush(&color, nullptr, brush.addressof()));
        _r.brush = brush.query<ID2D1Brush>();
    }

    _r.scratchpadCellWidth = _r.maxEncounteredCellCount;
    WI_SetAllFlags(_r.invalidations, RenderInvalidations::ConstBuffer);
}

void AtlasEngine::_processGlyphQueue()
{
    if (_r.glyphQueue.empty())
    {
        return;
    }

    for (const auto& pair : _r.glyphQueue)
    {
        _drawGlyph(pair);
    }

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

    // See D2DFactory::DrawText
    wil::com_ptr<IDWriteTextLayout> textLayout;
    THROW_IF_FAILED(_sr.dwriteFactory->CreateTextLayout(&key->chars[0], charsLength, textFormat, cells * _r.cellSizeDIP.x, _r.cellSizeDIP.y, textLayout.addressof()));
    if (item.scale != 1.0f)
    {
        const auto f = textFormat->GetFontSize();
        textLayout->SetFontSize(f * item.scale, { 0, charsLength });
    }
    if (_r.typography)
    {
        textLayout->SetTypography(_r.typography.get(), { 0, charsLength });
    }

    auto options = D2D1_DRAW_TEXT_OPTIONS_NONE;
    // D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT enables a bunch of internal machinery
    // which doesn't have to run if we know we can't use it anyways in the shader.
    WI_SetFlagIf(options, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT, WI_IsFlagSet(value->flags, CellFlags::ColoredGlyph));

    _r.d2dRenderTarget->BeginDraw();
    // We could call
    //   _r.d2dRenderTarget->PushAxisAlignedClip(&rect, D2D1_ANTIALIAS_MODE_ALIASED);
    // now to reduce the surface that needs to be cleared, but this decreases
    // performance by 10% (tested using debugGlyphGenerationPerformance).
    _r.d2dRenderTarget->Clear();
    _r.d2dRenderTarget->DrawTextLayout({}, textLayout.get(), _r.brush.get(), options);
    THROW_IF_FAILED(_r.d2dRenderTarget->EndDraw());

    for (uint32_t i = 0; i < cells; ++i)
    {
        // Specifying NO_OVERWRITE means that the system can assume that existing references to the surface that
        // may be in flight on the GPU will not be affected by the update, so the copy can proceed immediately
        // (avoiding either a batch flush or the system maintaining multiple copies of the resource behind the scenes).
        //
        // Since our shader only draws whatever is in the atlas, and since we don't replace glyph tiles that are in use,
        // we can safely (?) tell the GPU that we don't overwrite parts of our atlas that are in use.
        _copyScratchpadTile(i, coords[i], D3D11_COPY_NO_OVERWRITE);
    }
}

void AtlasEngine::_drawCursor()
{
    _reserveScratchpadSize(1);

    // lineWidth is in D2D's DIPs. For instance if we have a 150-200% zoom scale we want to draw a 2px wide line.
    // At 150% scale lineWidth thus needs to be 1.33333... because at a zoom scale of 1.5 this results in a 2px wide line.
    const auto lineWidth = std::max(1.0f, static_cast<float>((_r.dpi + USER_DEFAULT_SCREEN_DPI / 2) / USER_DEFAULT_SCREEN_DPI * USER_DEFAULT_SCREEN_DPI) / static_cast<float>(_r.dpi));
    const auto cursorType = static_cast<CursorType>(_r.cursorOptions.cursorType);
    D2D1_RECT_F rect;
    rect.left = 0.0f;
    rect.top = 0.0f;
    rect.right = _r.cellSizeDIP.x;
    rect.bottom = _r.cellSizeDIP.y;

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

    THROW_IF_FAILED(_r.d2dRenderTarget->EndDraw());

    _copyScratchpadTile(0, {});
}

void AtlasEngine::_copyScratchpadTile(uint32_t scratchpadIndex, u16x2 target, uint32_t copyFlags) const noexcept
{
    D3D11_BOX box;
    box.left = scratchpadIndex * _r.cellSize.x;
    box.top = 0;
    box.front = 0;
    box.right = box.left + _r.cellSize.x;
    box.bottom = _r.cellSize.y;
    box.back = 1;
#pragma warning(suppress : 26447) // The function is declared 'noexcept' but calls function '...' which may throw exceptions (f.6).
    _r.deviceContext->CopySubresourceRegion1(_r.atlasBuffer.get(), 0, target.x, target.y, 0, _r.atlasScratchpad.get(), 0, &box, copyFlags);
}
