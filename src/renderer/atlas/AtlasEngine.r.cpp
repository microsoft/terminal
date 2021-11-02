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
// DO NOT put stuff in here
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

void AtlasEngine::_setShaderResources() const noexcept
{
    const std::array resources{ _r.cellView.get(), _r.atlasView.get() };
#pragma warning(suppress : 26447) // The function is declared 'noexcept' but calls function '...' which may throw exceptions (f.6).
    _r.deviceContext->PSSetShaderResources(0, gsl::narrow_cast<UINT>(resources.size()), resources.data());
}

void AtlasEngine::_updateConstantBuffer() const noexcept
{
    ConstBuffer data;
    data.viewport.x = 0;
    data.viewport.y = 0;
    data.viewport.z = static_cast<float>(_r.cellCount.x * _r.cellSize.x);
    data.viewport.w = static_cast<float>(_r.cellCount.y * _r.cellSize.y);
    data.cellSize.x = _r.cellSize.x;
    data.cellSize.y = _r.cellSize.y;
    data.cellCountX = _r.cellCount.x;
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

    if (!copyFromExisting)
    {
        _drawCursor();
    }
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

void AtlasEngine::_drawGlyph(const til::pair<AtlasKey, AtlasValue>& pair) const
{
    const auto& key = pair.first;
    const auto& value = pair.second;
    const auto charsLength = wcsnlen_s(&key.chars[0], std::size(key.chars));
    const auto cells = key.attributes.cells + UINT32_C(1);
    const auto textFormat = _getTextFormat(key.attributes.bold, key.attributes.italic);

    // See D2DFactory::DrawText
    wil::com_ptr<IDWriteTextLayout> textLayout;
    THROW_IF_FAILED(_sr.dwriteFactory->CreateTextLayout(&key.chars[0], gsl::narrow_cast<UINT32>(charsLength), textFormat, cells * _r.cellSizeDIP.x, _r.cellSizeDIP.y, textLayout.addressof()));
    if (_r.typography)
    {
        textLayout->SetTypography(_r.typography.get(), { 0, gsl::narrow_cast<UINT32>(charsLength) });
    }

    _r.d2dRenderTarget->BeginDraw();
    // We could call
    //   _r.d2dRenderTarget->PushAxisAlignedClip(&rect, D2D1_ANTIALIAS_MODE_ALIASED);
    // now to reduce the surface that needs to be cleared, but this decreases
    // performance by 10% (tested using debugGlyphGenerationPerformance).
    _r.d2dRenderTarget->Clear();
    _r.d2dRenderTarget->DrawTextLayout({}, textLayout.get(), _r.brush.get(), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    THROW_IF_FAILED(_r.d2dRenderTarget->EndDraw());

    for (uint32_t i = 0; i < cells; ++i)
    {
        // Specifying NO_OVERWRITE means that the system can assume that existing references to the surface that
        // may be in flight on the GPU will not be affected by the update, so the copy can proceed immediately
        // (avoiding either a batch flush or the system maintaining multiple copies of the resource behind the scenes).
        //
        // Since our shader only draws whatever is in the atlas, and since we don't replace glyph tiles that are in use,
        // we can safely (?) tell the GPU that we don't overwrite parts of our atlas that are in use.
        _copyScratchpadTile(i, value.coords[i], D3D11_COPY_NO_OVERWRITE);
    }
}

void AtlasEngine::_drawCursor() const
{
    const auto cursorType = static_cast<CursorType>(_r.cursorOptions.cursorType);
    D2D1_RECT_F rect;
    rect.left = 0.0f;
    rect.top = 0.0f;
    rect.right = _r.cellSizeDIP.x;
    rect.bottom = _r.cellSizeDIP.y;

    switch (cursorType)
    {
    case CursorType::Legacy:
        rect.top = _r.cellSizeDIP.y * static_cast<float>(100 - _r.cursorOptions.ulCursorHeightPercent) / 100.0f;
        break;
    case CursorType::VerticalBar:
        rect.right = 1.0f;
        break;
    case CursorType::EmptyBox:
        // EmptyBox is drawn as a line and unlike filled rectangles those are drawn centered on their
        // coordinates in such a way that the line border extends half the width to each side.
        // --> Our coordinates have to be 0.5 DIP off in order to draw a 2px line on a 200% scaling.
        rect.left = 0.5f;
        rect.top = 0.5f;
        rect.right -= 0.5f;
        rect.bottom -= 0.5f;
        break;
    case CursorType::Underscore:
    case CursorType::DoubleUnderscore:
        rect.top = _r.cellSizeDIP.y - 1.0f;
        break;
    default:
        break;
    }

    _r.d2dRenderTarget->BeginDraw();
    _r.d2dRenderTarget->Clear();

    if (cursorType == CursorType::EmptyBox)
    {
        _r.d2dRenderTarget->DrawRectangle(&rect, _r.brush.get());
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
