// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AtlasEngine.h"

#include <til/small_vector.h>

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
#pragma warning(disable : 4127)
// Disable a bunch of warnings which get in the way of writing performant code.
#pragma warning(disable : 26429) // Symbol 'data' is never tested for nullness, it can be marked as not_null (f.23).
#pragma warning(disable : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
#pragma warning(disable : 26459) // You called an STL function '...' with a raw pointer parameter at position '...' that may be unsafe [...].
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26482) // Only index into arrays using constant expressions (bounds.2).

using namespace Microsoft::Console::Render;

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

static AtlasEngine::f32r getGlyphRunBlackBox(const DWRITE_GLYPH_RUN& glyphRun, float baselineX, float baselineY)
{
    DWRITE_FONT_METRICS fontMetrics;
    glyphRun.fontFace->GetMetrics(&fontMetrics);

    til::small_vector<DWRITE_GLYPH_METRICS, 16> glyphRunMetrics;
    glyphRunMetrics.resize(glyphRun.glyphCount);
    glyphRun.fontFace->GetDesignGlyphMetrics(glyphRun.glyphIndices, glyphRun.glyphCount, glyphRunMetrics.data(), false);

    float const fontScale = glyphRun.fontEmSize / fontMetrics.designUnitsPerEm;
    AtlasEngine::f32r accumulatedBounds{
        FLT_MAX,
        FLT_MAX,
        FLT_MIN,
        FLT_MIN,
    };

    for (uint32_t i = 0; i < glyphRun.glyphCount; ++i)
    {
        const auto& glyphMetrics = glyphRunMetrics[i];
        const auto glyphAdvance = glyphRun.glyphAdvances ? glyphRun.glyphAdvances[i] : glyphMetrics.advanceWidth * fontScale;

        const auto left = static_cast<float>(glyphMetrics.leftSideBearing) * fontScale;
        const auto top = static_cast<float>(glyphMetrics.topSideBearing - glyphMetrics.verticalOriginY) * fontScale;
        const auto right = static_cast<float>(gsl::narrow_cast<INT32>(glyphMetrics.advanceWidth) - glyphMetrics.rightSideBearing) * fontScale;
        const auto bottom = static_cast<float>(gsl::narrow_cast<INT32>(glyphMetrics.advanceHeight) - glyphMetrics.bottomSideBearing - glyphMetrics.verticalOriginY) * fontScale;

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

    if (_r.d2dMode)
    {
        if (!_r.d2dRenderTarget)
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
                _r.d2dRenderTarget4 = renderTarget.query<ID2D1DeviceContext4>();

                // In case _api.realizedAntialiasingMode is D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE we'll
                // continuously adjust it in AtlasEngine::_drawGlyph. See _drawGlyph.
                _r.d2dRenderTarget->SetTextAntialiasMode(static_cast<D2D1_TEXT_ANTIALIAS_MODE>(_api.realizedAntialiasingMode));
            }
            {
                static constexpr D2D1_COLOR_F color{ 1, 1, 1, 1 };
                THROW_IF_FAILED(_r.d2dRenderTarget->CreateSolidColorBrush(&color, nullptr, _r.brush.put()));
                _r.brushColor = 0xffffffff;
            }
            {
                D2D1_BITMAP_PROPERTIES props{};
                props.pixelFormat = { DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED };
                props.dpiX = static_cast<float>(_r.dpi);
                props.dpiY = static_cast<float>(_r.dpi);
                THROW_IF_FAILED(_r.d2dRenderTarget->CreateBitmap({ _r.cellCount.x, _r.cellCount.y }, props, _r.d2dBackgroundBitmap.put()));
                THROW_IF_FAILED(_r.d2dRenderTarget->CreateBitmapBrush(_r.d2dBackgroundBitmap.get(), _r.d2dBackgroundBrush.put()));
                _r.d2dBackgroundBrush->SetInterpolationMode(D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
                _r.d2dBackgroundBrush->SetTransform(D2D1::Matrix3x2F::Scale(_r.fontMetrics.cellSize.x, _r.fontMetrics.cellSize.y));
            }
        }

        _r.d2dRenderTarget->BeginDraw();

        _r.d2dBackgroundBitmap->CopyFromMemory(nullptr, _r.backgroundBitmap.data(), _r.cellCount.x * 4);
        _r.d2dRenderTarget->FillRectangle({ 0, 0, _r.cellCount.x * _r.cellSizeDIP.x, _r.cellCount.y * _r.cellSizeDIP.y }, _r.d2dBackgroundBrush.get());

        size_t y = 0;
        for (const auto& row : _r.rows)
        {
            for (const auto& m : row.mappings)
            {
                DWRITE_GLYPH_RUN glyphRun{};
                glyphRun.fontFace = m.fontFace.get();
                glyphRun.fontEmSize = m.fontEmSize;
                glyphRun.glyphCount = m.glyphsTo - m.glyphsFrom;
                glyphRun.glyphIndices = &row.glyphIndices[m.glyphsFrom];
                glyphRun.glyphAdvances = &row.glyphAdvances[m.glyphsFrom];
                glyphRun.glyphOffsets = &row.glyphOffsets[m.glyphsFrom];

                const D2D1_POINT_2F baseline{
                    0, // TODO
                    _r.cellSizeDIP.y * y + _r.fontMetrics.baselineInDIP,
                };

                _drawGlyphRun(baseline, &glyphRun, _r.brush.get());
            }

            y++;
        }

        THROW_IF_FAILED(_r.d2dRenderTarget->EndDraw());
    }
    else
    {
        if (!_r.atlasBuffer)
        {
            {
                D3D11_TEXTURE2D_DESC desc{};
                desc.Width = 1024;
                desc.Height = 1024;
                desc.MipLevels = 1;
                desc.ArraySize = 1;
                desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                desc.SampleDesc = { 1, 0 };
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
                THROW_IF_FAILED(_r.device->CreateTexture2D(&desc, nullptr, _r.atlasBuffer.addressof()));
                THROW_IF_FAILED(_r.device->CreateShaderResourceView(_r.atlasBuffer.get(), nullptr, _r.atlasView.addressof()));
            }

            {
                _r.glyphCache.Clear();
                _r.rectPackerData.resize(1024);
                stbrp_init_target(&_r.rectPacker, 1024, 1024, _r.rectPackerData.data(), gsl::narrow_cast<int>(_r.rectPackerData.size()));
            }

            {
                const auto surface = _r.atlasBuffer.query<IDXGISurface>();

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
                _r.d2dRenderTarget4 = renderTarget.query<ID2D1DeviceContext4>();

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

            switch (_api.realizedAntialiasingMode)
            {
            case D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE:
                _r.textPixelShader = _r.cleartypePixelShader;
                _r.textBlendState = _r.cleartypeBlendState;
                break;
            default:
                _r.textPixelShader = _r.grayscalePixelShader;
                _r.textBlendState = _r.alphaBlendState;
                break;
            }
        }

        vec2<size_t> textRange;
        vec2<size_t> cursorRange;
        vec2<size_t> selectionRange;

        {
            _r.vertexInstanceData.clear();

            // Background
            {
                auto& ref = _r.vertexInstanceData.emplace_back();
                ref.rect = { 0.0f, 0.0f, static_cast<f32>(_api.sizeInPixel.x), static_cast<f32>(_api.sizeInPixel.y) };
                ref.tex = { 0.0f, 0.0f, static_cast<f32>(_api.sizeInPixel.x) / static_cast<f32>(_r.fontMetrics.cellSize.x), static_cast<f32>(_api.sizeInPixel.y) / static_cast<f32>(_r.fontMetrics.cellSize.y) };
                ref.color = 0;
                ref.shadingType = 1;
            }

            // Text
            {
                textRange.x = _r.vertexInstanceData.size();

                bool beganDrawing = false;

                size_t y = 0;
                for (const auto& row : _r.rows)
                {
                    const auto baselineY = _r.cellSizeDIP.y * y + _r.fontMetrics.baselineInDIP;
                    f32 cumulativeAdvance = 0;

                    for (const auto& m : row.mappings)
                    {
                        for (auto i = m.glyphsFrom; i < m.glyphsTo; ++i)
                        {
                            bool inserted = false;
                            auto& entry = _r.glyphCache.FindOrInsert(m.fontFace.get(), row.glyphIndices[i], inserted);
                            if (inserted)
                            {
                                if (!beganDrawing)
                                {
                                    beganDrawing = true;
                                    _r.d2dRenderTarget->BeginDraw();
                                }

                                _drawGlyph(entry, m.fontEmSize);
                            }

                            if (entry.wh != u16x2{})
                            {
                                auto& ref = _r.vertexInstanceData.emplace_back();
                                ref.rect = {
                                    (cumulativeAdvance + row.glyphOffsets[i].advanceOffset) * _r.pixelPerDIP + entry.offset.x,
                                    (baselineY - row.glyphOffsets[i].ascenderOffset) * _r.pixelPerDIP + entry.offset.y,
                                    static_cast<f32>(entry.wh.x),
                                    static_cast<f32>(entry.wh.y),
                                };
                                ref.tex = {
                                    static_cast<f32>(entry.xy.x),
                                    static_cast<f32>(entry.xy.y),
                                    static_cast<f32>(entry.wh.x),
                                    static_cast<f32>(entry.wh.y),
                                };
                                ref.color = row.colors[i];
                                ref.shadingType = entry.colorGlyph ? 1 : 0;
                            }

                            cumulativeAdvance += row.glyphAdvances[i];
                        }
                    }

                    y++;
                }

                if (beganDrawing)
                {
                    THROW_IF_FAILED(_r.d2dRenderTarget->EndDraw());
                }

                if constexpr (false)
                {
                    auto& ref = _r.vertexInstanceData.emplace_back();
                    ref.rect = { 0.0f, 0.0f, 100.0f, 100.0f };
                    ref.color = _r.selectionColor;
                    ref.shadingType = 2;
                }
                if constexpr (false)
                {
                    auto& ref = _r.vertexInstanceData.emplace_back();
                    ref.rect = { 50.0f, 50.0f, 100.0f, 100.0f };
                    ref.color = _r.selectionColor;
                    ref.shadingType = 2;
                }

                textRange.y = _r.vertexInstanceData.size() - textRange.x;
            }

            if (_r.cursorRect.non_empty())
            {
                cursorRange.x = _r.vertexInstanceData.size();

                auto& ref = _r.vertexInstanceData.emplace_back();
                ref.rect = {
                    static_cast<f32>(_r.fontMetrics.cellSize.x * _r.cursorRect.left),
                    static_cast<f32>(_r.fontMetrics.cellSize.y * _r.cursorRect.top),
                    static_cast<f32>(_r.fontMetrics.cellSize.x * (_r.cursorRect.right - _r.cursorRect.left)),
                    static_cast<f32>(_r.fontMetrics.cellSize.y * (_r.cursorRect.bottom - _r.cursorRect.top)),
                };

                cursorRange.y = _r.vertexInstanceData.size() - cursorRange.x;
            }

            // Selection
            {
                selectionRange.x = _r.vertexInstanceData.size();

                size_t y = 0;
                for (const auto& row : _r.rows)
                {
                    if (row.selectionTo > row.selectionFrom)
                    {
                        auto& ref = _r.vertexInstanceData.emplace_back();
                        ref.rect = {
                            static_cast<f32>(_r.fontMetrics.cellSize.x * row.selectionFrom),
                            static_cast<f32>(_r.fontMetrics.cellSize.y * y),
                            static_cast<f32>(_r.fontMetrics.cellSize.x * (row.selectionTo - row.selectionFrom)),
                            static_cast<f32>(_r.fontMetrics.cellSize.y),
                        };
                        ref.color = _r.selectionColor;
                        ref.shadingType = 2;
                    }

                    y++;
                }

                selectionRange.y = _r.vertexInstanceData.size() - selectionRange.x;
            }
        }

        if (WI_IsFlagSet(_r.invalidations, RenderInvalidations::ConstBuffer))
        {
            ConstBuffer data;
            data.positionScale = { 2.0f / _api.sizeInPixel.x, -2.0f / _api.sizeInPixel.y, 1, 1 };
            DWrite_GetGammaRatios(_r.gamma, data.gammaRatios);
            data.cleartypeEnhancedContrast = _r.cleartypeEnhancedContrast;
            data.grayscaleEnhancedContrast = _r.grayscaleEnhancedContrast;
#pragma warning(suppress : 26447) // The function is declared 'noexcept' but calls function '...' which may throw exceptions (f.6).
            _r.deviceContext->UpdateSubresource(_r.constantBuffer.get(), 0, nullptr, &data, 0, 0);
            WI_ClearFlag(_r.invalidations, RenderInvalidations::ConstBuffer);
        }

        if (_r.vertexInstanceData.size() > _r.vertexBuffers1Size)
        {
            const auto totalCellCount = static_cast<size_t>(_r.cellCount.x) * static_cast<size_t>(_r.cellCount.y);
            const auto growthSize = _r.vertexBuffers1Size + _r.vertexBuffers1Size / 2;
            const auto newSize = std::max(totalCellCount, growthSize);

            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = gsl::narrow<UINT>(sizeof(VertexInstanceData) * newSize);
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            THROW_IF_FAILED(_r.device->CreateBuffer(&desc, nullptr, _r.vertexBuffers[1].put()));

            _r.vertexBuffers1Size = newSize;
        }

        {
#pragma warning(suppress : 26494) // Variable 'mapped' is uninitialized. Always initialize an object (type.5).
            D3D11_MAPPED_SUBRESOURCE mapped;
            THROW_IF_FAILED(_r.deviceContext->Map(_r.perCellColor.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
            for (auto i = 0; i < _r.cellCount.y; ++i)
            {
                memcpy(mapped.pData, _r.backgroundBitmap.data() + i * _r.cellCount.x, _r.cellCount.x * sizeof(u32));
                mapped.pData = static_cast<void*>(static_cast<std::byte*>(mapped.pData) + mapped.RowPitch);
            }
            _r.deviceContext->Unmap(_r.perCellColor.get(), 0);
        }

        {
#pragma warning(suppress : 26494) // Variable 'mapped' is uninitialized. Always initialize an object (type.5).
            D3D11_MAPPED_SUBRESOURCE mapped;
            THROW_IF_FAILED(_r.deviceContext->Map(_r.vertexBuffers[1].get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
            memcpy(mapped.pData, _r.vertexInstanceData.data(), _r.vertexInstanceData.size() * sizeof(VertexInstanceData));
            _r.deviceContext->Unmap(_r.vertexBuffers[1].get(), 0);
        }

        {
            {
                // IA: Input Assembler
                static constexpr UINT strides[2]{ sizeof(f32x2), sizeof(VertexInstanceData) };
                static constexpr UINT offsets[2]{ 0, 0 };
                _r.deviceContext->IASetInputLayout(_r.textInputLayout.get());
                _r.deviceContext->IASetVertexBuffers(0, 2, _r.vertexBuffers[0].addressof(), &strides[0], &offsets[0]);
                _r.deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                // VS: Vertex Shader
                _r.deviceContext->VSSetShader(_r.vertexShader.get(), nullptr, 0);
                _r.deviceContext->VSSetConstantBuffers(0, 1, _r.constantBuffer.addressof());

                // RS: Rasterizer Stage
                D3D11_VIEWPORT viewport{};
                viewport.Width = static_cast<float>(_api.sizeInPixel.x);
                viewport.Height = static_cast<float>(_api.sizeInPixel.y);
                _r.deviceContext->RSSetViewports(1, &viewport);
                _r.deviceContext->RSSetState(nullptr);

                // PS: Pixel Shader
                _r.deviceContext->PSSetShader(_r.textPixelShader.get(), nullptr, 0);
                _r.deviceContext->PSSetConstantBuffers(0, 1, _r.constantBuffer.addressof());
                _r.deviceContext->PSSetShaderResources(0, 1, _r.perCellColorView.addressof());

                // OM: Output Merger
                _r.deviceContext->OMSetRenderTargets(1, _r.renderTargetView.addressof(), nullptr);
                _r.deviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);

                _r.deviceContext->DrawInstanced(6, 1, 0, 0);
            }

            // Inverted cursors use D3D11 Logic Ops with D3D11_LOGIC_OP_XOR (see GH#).
            // But unfortunately this poses two problems:
            // * Cursors are drawn "in between" text and selection
            // * all RenderTargets bound must have a UINT format
            // --> We have to draw in 3 passes.
            if (cursorRange.y)
            {
                _r.deviceContext->PSSetShader(_r.textPixelShader.get(), nullptr, 0);
                _r.deviceContext->PSSetShaderResources(0, 1, _r.atlasView.addressof());
                _r.deviceContext->OMSetBlendState(_r.textBlendState.get(), nullptr, 0xffffffff);
                _r.deviceContext->DrawInstanced(6, gsl::narrow_cast<UINT>(textRange.y), 0, gsl::narrow_cast<UINT>(textRange.x));

                _r.deviceContext->PSSetShader(_r.invertCursorPixelShader.get(), nullptr, 0);
                _r.deviceContext->OMSetRenderTargets(1, _r.renderTargetViewUInt.addressof(), nullptr);
                _r.deviceContext->OMSetBlendState(_r.invertCursorBlendState.get(), nullptr, 0xffffffff);
                _r.deviceContext->DrawInstanced(6, gsl::narrow_cast<UINT>(cursorRange.y), 0, gsl::narrow_cast<UINT>(cursorRange.x));

                if (selectionRange.y)
                {
                    _r.deviceContext->PSSetShader(_r.textPixelShader.get(), nullptr, 0);
                    _r.deviceContext->PSSetShaderResources(0, 1, _r.atlasView.addressof());
                    _r.deviceContext->OMSetRenderTargets(1, _r.renderTargetView.addressof(), nullptr);
                    _r.deviceContext->OMSetBlendState(_r.textBlendState.get(), nullptr, 0xffffffff);
                    _r.deviceContext->DrawInstanced(6, gsl::narrow_cast<UINT>(selectionRange.y), 0, gsl::narrow_cast<UINT>(selectionRange.x));
                }
            }
            else
            {
                _r.deviceContext->PSSetShader(_r.textPixelShader.get(), nullptr, 0);
                _r.deviceContext->PSSetShaderResources(0, 1, _r.atlasView.addressof());
                _r.deviceContext->OMSetBlendState(_r.textBlendState.get(), nullptr, 0xffffffff);
                _r.deviceContext->DrawInstanced(6, gsl::narrow_cast<UINT>(textRange.y + selectionRange.y), 0, gsl::narrow_cast<UINT>(textRange.x));
            }
        }

        if constexpr (false)
        {
            _r.deviceContext->RSSetState(_r.wireframeRasterizerState.get());
            _r.deviceContext->PSSetShader(_r.wireframePixelShader.get(), nullptr, 0);
            _r.deviceContext->OMSetBlendState(_r.alphaBlendState.get(), nullptr, 0xffffffff);
            _r.deviceContext->DrawInstanced(6, gsl::narrow<UINT>(_r.vertexInstanceData.size()), 0, 0);
        }
    }

    if (false && _r.dirtyRect != fullRect)
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
    if (std::exchange(_r.waitForPresentation, false))
    {
        WaitForSingleObjectEx(_r.frameLatencyWaitableObject.get(), 100, true);
#ifndef NDEBUG
        _r.frameLatencyWaitableObjectUsed = true;
#endif
    }
}

#pragma endregion

bool AtlasEngine::_drawGlyphRun(D2D_POINT_2F baselineOrigin, const DWRITE_GLYPH_RUN* glyphRun, ID2D1SolidColorBrush* foregroundBrush) const noexcept
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

    if (_r.d2dRenderTarget4)
    {
        D2D_MATRIX_3X2_F transform;
        _r.d2dRenderTarget4->GetTransform(&transform);
        float dpiX, dpiY;
        _r.d2dRenderTarget4->GetDpi(&dpiX, &dpiY);
        transform = transform * D2D1::Matrix3x2F::Scale(dpiX, dpiY);

        // Support for ID2D1DeviceContext4 implies support for IDWriteFactory4.
        // ID2D1DeviceContext4 is required for drawing below.
        hr = _sr.dwriteFactory4->TranslateColorGlyphRun(baselineOrigin, glyphRun, nullptr, formats, measuringMode, nullptr, 0, &enumerator);
    }

    if (hr == DWRITE_E_NOCOLOR)
    {
        _r.d2dRenderTarget->DrawGlyphRun(baselineOrigin, glyphRun, foregroundBrush, measuringMode);
        return false;
    }

    THROW_IF_FAILED(hr);

    const auto previousAntialiasingMode = _r.d2dRenderTarget4->GetTextAntialiasMode();
    _r.d2dRenderTarget4->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    const auto cleanup = wil::scope_exit([&]() {
        _r.d2dRenderTarget4->SetTextAntialiasMode(previousAntialiasingMode);
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

        ID2D1Brush* runBrush;
        if (colorGlyphRun->paletteIndex == /*DWRITE_NO_PALETTE_INDEX*/ 0xffff)
        {
            runBrush = foregroundBrush;
        }
        else
        {
            if (!solidBrush)
            {
                THROW_IF_FAILED(_r.d2dRenderTarget4->CreateSolidColorBrush(colorGlyphRun->runColor, &solidBrush));
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
            _r.d2dRenderTarget4->DrawColorBitmapGlyphRun(colorGlyphRun->glyphImageFormat, baselineOrigin, &colorGlyphRun->glyphRun, colorGlyphRun->measuringMode, D2D1_COLOR_BITMAP_GLYPH_SNAP_OPTION_DEFAULT);
            break;
        case DWRITE_GLYPH_IMAGE_FORMATS_SVG:
            _r.d2dRenderTarget4->DrawSvgGlyphRun(baselineOrigin, &colorGlyphRun->glyphRun, runBrush, nullptr, 0, colorGlyphRun->measuringMode);
            break;
        default:
            _r.d2dRenderTarget4->DrawGlyphRun(baselineOrigin, &colorGlyphRun->glyphRun, colorGlyphRun->glyphRunDescription, runBrush, colorGlyphRun->measuringMode);
            break;
        }
    }

    return true;
}

void AtlasEngine::_drawGlyph(GlyphCacheEntry& entry, f32 fontEmSize)
{
    DWRITE_GLYPH_RUN glyphRun{};
    glyphRun.fontFace = entry.fontFace;
    glyphRun.fontEmSize = fontEmSize;
    glyphRun.glyphCount = 1;
    glyphRun.glyphIndices = &entry.glyphIndex;

    auto box = getGlyphRunBlackBox(glyphRun, 0, 0);
    if (box.left >= box.right || box.top >= box.bottom)
    {
        return;
    }

    box.left = roundf(box.left * _r.pixelPerDIP) - 1.0f;
    box.top = roundf(box.top * _r.pixelPerDIP) - 1.0f;
    box.right = roundf(box.right * _r.pixelPerDIP) + 1.0f;
    box.bottom = roundf(box.bottom * _r.pixelPerDIP) + 1.0f;

    stbrp_rect rect{};
    rect.w = gsl::narrow_cast<int>(box.right - box.left);
    rect.h = gsl::narrow_cast<int>(box.bottom - box.top);
    if (!stbrp_pack_rects(&_r.rectPacker, &rect, 1))
    {
        __debugbreak();
        return;
    }

    const D2D1_POINT_2F baseline{
        (rect.x - box.left) * _r.dipPerPixel,
        (rect.y - box.top) * _r.dipPerPixel,
    };
    const auto colorGlyph = _drawGlyphRun(baseline, &glyphRun, _r.brush.get());

    entry.xy.x = gsl::narrow_cast<u16>(rect.x);
    entry.xy.y = gsl::narrow_cast<u16>(rect.y);
    entry.wh.x = gsl::narrow_cast<u16>(rect.w);
    entry.wh.y = gsl::narrow_cast<u16>(rect.h);
    entry.offset.x = gsl::narrow_cast<u16>(box.left);
    entry.offset.y = gsl::narrow_cast<u16>(box.top);
    entry.colorGlyph = colorGlyph;
}
