// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "BackendD3D.h"

#include <til/hash.h>

#include <custom_shader_ps.h>
#include <custom_shader_vs.h>
#include <shader_ps.h>
#include <shader_vs.h>

#include "dwrite.h"

#if ATLAS_DEBUG_SHOW_DIRTY || ATLAS_DEBUG_COLORIZE_GLYPH_ATLAS
#include "colorbrewer.h"
#endif

#if ATLAS_DEBUG_DUMP_RENDER_TARGET
#include "wic.h"
#endif

TIL_FAST_MATH_BEGIN

// This code packs various data into smaller-than-int types to save both CPU and GPU memory. This warning would force
// us to add dozens upon dozens of gsl::narrow_cast<>s throughout the file which is more annoying than helpful.
#pragma warning(disable : 4242) // '=': conversion from '...' to '...', possible loss of data
#pragma warning(disable : 4244) // 'initializing': conversion from '...' to '...', possible loss of data
#pragma warning(disable : 4267) // 'argument': conversion from '...' to '...', possible loss of data
#pragma warning(disable : 4838) // conversion from '...' to '...' requires a narrowing conversion
#pragma warning(disable : 26472) // Don't use a static_cast for arithmetic conversions. Use brace initialization, gsl::narrow_cast or gsl::narrow (type.1).
// Disable a bunch of warnings which get in the way of writing performant code.
#pragma warning(disable : 26429) // Symbol 'data' is never tested for nullness, it can be marked as not_null (f.23).
#pragma warning(disable : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
#pragma warning(disable : 26459) // You called an STL function '...' with a raw pointer parameter at position '...' that may be unsafe [...].
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26482) // Only index into arrays using constant expressions (bounds.2).

using namespace Microsoft::Console::Render::Atlas;

BackendD3D::GlyphCacheMap::~GlyphCacheMap()
{
    Clear();
}

BackendD3D::GlyphCacheMap& BackendD3D::GlyphCacheMap::operator=(GlyphCacheMap&& other) noexcept
{
    _map = std::exchange(other._map, {});
    _mask = std::exchange(other._mask, 0);
    _capacity = std::exchange(other._capacity, 0);
    _size = std::exchange(other._size, 0);
    return *this;
}

void BackendD3D::GlyphCacheMap::Clear() noexcept
{
    for (auto& entry : _map)
    {
        if (entry.key.fontFace)
        {
            // I'm pretty sure Release() doesn't throw exceptions.
#pragma warning(suppress : 26447) // The function is declared 'noexcept' but calls function 'Release()' which may throw exceptions (f.6).
            entry.key.fontFace->Release();
            entry.key.fontFace = nullptr;
        }
    }

    _size = 0;
}

BackendD3D::GlyphCacheEntry& BackendD3D::GlyphCacheMap::FindOrInsert(const GlyphCacheKey& key, bool& inserted)
{
    // Putting this into the Find() path is a little pessimistic, but it
    // allows us to default-construct this hashmap with a size of 0.
    if (_size >= _capacity)
    {
        _bumpSize();
    }

    const auto hash = _hash(key);
    for (auto i = hash;; ++i)
    {
        auto& entry = _map[i & _mask];
        if (entry.key == key)
        {
            inserted = false;
            return entry;
        }
        if (!entry.key.fontFace)
        {
            ++_size;
            entry.key = key;
            entry.key.fontFace->AddRef();
            inserted = true;
            return entry;
        }
    }
}

size_t BackendD3D::GlyphCacheMap::_hash(const GlyphCacheKey& key) noexcept
{
    //auto h = UINT64_C(0xcafef00dd15ea5e5);
    //h = (h ^ key.hashField1) * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
    //h = (h ^ key.hashField2) * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
    //const int r = h & 63;
    //const auto x = static_cast<uint32_t>(h >> 32) ^ static_cast<uint32_t>(h);
    //return _rotl(x, r);
    return til::hash(&key, GlyphCacheKeyDataSize);
}

void BackendD3D::GlyphCacheMap::_bumpSize()
{
    // The following block of code may be used to assess the quality of the hash function.
    // The displacement is the distance between the ideal slot the hash value points at to
    // the slot the value actually ended up in. A low displacement is not everything however,
    // and the size and performance of the hash function is just as important.
#if 0
    if (_size)
    {
        size_t displacementMax = 0;
        size_t displacementTotal = 0;
        size_t actualSlot = 0;

        for (const auto& entry : _map)
        {
            if (entry.key.fontFace)
            {
                const auto idealSlot = _hash(entry.key) & _mask;
                size_t displacement = actualSlot - idealSlot;

                // A hash near the end of the map may wrap around to the beginning.
                // This if condition will fix the displacement in that case.
                if (actualSlot < idealSlot)
                {
                    displacement += _map.size();
                }

                if (displacement > displacementMax)
                {
                    displacementMax = displacement;
                    displacementTotal += displacement;
                }
            }

            actualSlot++;
        }

        const auto displacementAvg = static_cast<float>(displacementTotal) / static_cast<float>(_size);
        wchar_t buffer[128];
        swprintf_s(buffer, L"GlyphCacheMap resize at %zu, max. displacement: %zu, avg. displacement: %f%%\r\n", _map.size(), displacementMax, displacementAvg);
        OutputDebugStringW(&buffer[0]);
    }
#endif

    const auto newSize = std::max<size_t>(256, _map.size() * 2);
    const auto newMask = newSize - 1;

    static constexpr auto sizeLimit = std::numeric_limits<size_t>::max() / 2;
    THROW_HR_IF_MSG(E_OUTOFMEMORY, newSize >= sizeLimit, "GlyphCacheMap overflow");

    auto newMap = Buffer<GlyphCacheEntry>(newSize);

    for (const auto& oldEntry : _map)
    {
        const auto hash = _hash(oldEntry.key);
        for (auto i = hash;; ++i)
        {
            auto& newEntry = newMap[i & newMask];
            if (!newEntry.key.fontFace)
            {
                newEntry = oldEntry;
                break;
            }
        }
    }

    _map = std::move(newMap);
    _mask = newMask;
    _capacity = newSize / 2;
}

BackendD3D::BackendD3D(wil::com_ptr<ID3D11Device2> device, wil::com_ptr<ID3D11DeviceContext2> deviceContext) :
    _device{ std::move(device) },
    _deviceContext{ std::move(deviceContext) }
{
    THROW_IF_FAILED(_device->CreateVertexShader(&shader_vs[0], sizeof(shader_vs), nullptr, _vertexShader.addressof()));
    THROW_IF_FAILED(_device->CreatePixelShader(&shader_ps[0], sizeof(shader_ps), nullptr, _pixelShader.addressof()));

    {
        static constexpr D3D11_INPUT_ELEMENT_DESC layout[]{
            { "SV_Position", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "position", 0, DXGI_FORMAT_R16G16_SINT, 1, offsetof(QuadInstance, position), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "size", 0, DXGI_FORMAT_R16G16_UINT, 1, offsetof(QuadInstance, size), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "texcoord", 0, DXGI_FORMAT_R16G16_UINT, 1, offsetof(QuadInstance, texcoord), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "shadingType", 0, DXGI_FORMAT_R32_UINT, 1, offsetof(QuadInstance, shadingType), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "color", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 1, offsetof(QuadInstance, color), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        };
        THROW_IF_FAILED(_device->CreateInputLayout(&layout[0], std::size(layout), &shader_vs[0], sizeof(shader_vs), _inputLayout.addressof()));
    }

    {
        static constexpr f32x2 vertices[]{
            { 0, 0 },
            { 1, 0 },
            { 1, 1 },
            { 0, 1 },
        };
        static constexpr D3D11_SUBRESOURCE_DATA initialData{ &vertices[0] };

        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(vertices);
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        THROW_IF_FAILED(_device->CreateBuffer(&desc, &initialData, _vertexBuffer.addressof()));
    }

    {
        static constexpr u16 indices[]{
            0, // { 0, 0 }
            1, // { 1, 0 }
            2, // { 1, 1 }
            2, // { 1, 1 }
            3, // { 0, 1 }
            0, // { 0, 0 }
        };
        static constexpr D3D11_SUBRESOURCE_DATA initialData{ &indices[0] };

        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(indices);
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        THROW_IF_FAILED(_device->CreateBuffer(&desc, &initialData, _indexBuffer.addressof()));
    }

    {
        static constexpr D3D11_BUFFER_DESC desc{
            .ByteWidth = sizeof(VSConstBuffer),
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        };
        THROW_IF_FAILED(_device->CreateBuffer(&desc, nullptr, _vsConstantBuffer.addressof()));
    }

    {
        static constexpr D3D11_BUFFER_DESC desc{
            .ByteWidth = sizeof(PSConstBuffer),
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        };
        THROW_IF_FAILED(_device->CreateBuffer(&desc, nullptr, _psConstantBuffer.addressof()));
    }

    {
        // The final step of the ClearType blending algorithm is a lerp() between the premultiplied alpha
        // background color and straight alpha foreground color given the 3 RGB weights in alphaCorrected:
        //   lerp(background, foreground, weights)
        // Which is equivalent to:
        //   background * (1 - weights) + foreground * weights
        //
        // This COULD be implemented using dual source color blending like so:
        //   .SrcBlend = D3D11_BLEND_SRC1_COLOR
        //   .DestBlend = D3D11_BLEND_INV_SRC1_COLOR
        //   .BlendOp = D3D11_BLEND_OP_ADD
        // Because:
        //   background * (1 - weights) + foreground * weights
        //       ^             ^        ^     ^           ^
        //      Dest     INV_SRC1_COLOR |    Src      SRC1_COLOR
        //                            OP_ADD
        //
        // BUT we need simultaneous support for regular "source over" alpha blending
        // (SHADING_TYPE_PASSTHROUGH)  like this:
        //   background * (1 - alpha) + foreground
        //
        // This is why we set:
        //   .SrcBlend = D3D11_BLEND_ONE
        //
        // --> We need to multiply the foreground with the weights ourselves.
        static constexpr D3D11_BLEND_DESC desc{
            .RenderTarget = { {
                .BlendEnable = TRUE,
                .SrcBlend = D3D11_BLEND_ONE,
                .DestBlend = D3D11_BLEND_INV_SRC1_COLOR,
                .BlendOp = D3D11_BLEND_OP_ADD,
                .SrcBlendAlpha = D3D11_BLEND_ONE,
                .DestBlendAlpha = D3D11_BLEND_INV_SRC1_ALPHA,
                .BlendOpAlpha = D3D11_BLEND_OP_ADD,
                .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
            } },
        };
        THROW_IF_FAILED(_device->CreateBlendState(&desc, _blendState.addressof()));
    }

    {
        static constexpr D3D11_BLEND_DESC desc{
            .RenderTarget = { {
                .BlendEnable = TRUE,
                .SrcBlend = D3D11_BLEND_ONE,
                .DestBlend = D3D11_BLEND_ONE,
                .BlendOp = D3D11_BLEND_OP_SUBTRACT,
                // In order for D3D to be okay with us using dual source blending in the shader, we need to use dual
                // source blending in the blend state. Alternatively we could write an extra shader for these cursors.
                .SrcBlendAlpha = D3D11_BLEND_SRC1_ALPHA,
                .DestBlendAlpha = D3D11_BLEND_ZERO,
                .BlendOpAlpha = D3D11_BLEND_OP_ADD,
                .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
            } },
        };
        THROW_IF_FAILED(_device->CreateBlendState(&desc, _blendStateInvert.addressof()));
    }

#ifndef NDEBUG
    _sourceDirectory = std::filesystem::path{ __FILE__ }.parent_path();
    _sourceCodeWatcher = wil::make_folder_change_reader_nothrow(_sourceDirectory.c_str(), false, wil::FolderChangeEvents::FileName | wil::FolderChangeEvents::LastWriteTime, [this](wil::FolderChangeEvent, PCWSTR path) {
        if (til::ends_with(path, L".hlsl"))
        {
            auto expected = INT64_MAX;
            const auto invalidationTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
            _sourceCodeInvalidationTime.compare_exchange_strong(expected, invalidationTime.time_since_epoch().count(), std::memory_order_relaxed);
        }
    });
#endif
}

void BackendD3D::Render(RenderingPayload& p)
{
    if (_generation != p.s.generation())
    {
        _handleSettingsUpdate(p);
    }

#ifndef NDEBUG
    _debugUpdateShaders(p);
#endif

    // After a Present() the render target becomes unbound.
    _deviceContext->OMSetRenderTargets(1, _renderTargetView.addressof(), nullptr);

    // Invalidating the render target helps with spotting invalid quad instances and Present1() bugs.
#if ATLAS_DEBUG_SHOW_DIRTY || ATLAS_DEBUG_DUMP_RENDER_TARGET
    {
        static constexpr f32 clearColor[4]{};
        _deviceContext->ClearView(_renderTargetView.get(), &clearColor[0], nullptr, 0);
    }
#endif

    _drawBackground(p);
    _drawCursorPart1(p);
    _drawText(p);
    _drawGridlines(p);
    _drawCursorPart2(p);
    _drawSelection(p);
#if ATLAS_DEBUG_SHOW_DIRTY
    _debugShowDirty(p);
#endif
    _flushQuads(p);

    if (_customPixelShader)
    {
        _executeCustomShader(p);
    }

#if ATLAS_DEBUG_DUMP_RENDER_TARGET
    _debugDumpRenderTarget(p);
#endif
    _swapChainManager.Present(p);
}

bool BackendD3D::RequiresContinuousRedraw() noexcept
{
    return _requiresContinuousRedraw;
}

void BackendD3D::WaitUntilCanRender() noexcept
{
    _swapChainManager.WaitUntilCanRender();
}

void BackendD3D::_handleSettingsUpdate(const RenderingPayload& p)
{
    _swapChainManager.UpdateSwapChainSettings(
        p,
        _device.get(),
        [this]() {
            _renderTargetView.reset();
            _customRenderTargetView.reset();
            _deviceContext->ClearState();
            _deviceContext->Flush();
        },
        [this]() {
            _renderTargetView.reset();
            _customRenderTargetView.reset();
            _deviceContext->ClearState();
        });

    if (!_renderTargetView)
    {
        const auto buffer = _swapChainManager.GetBuffer();
        THROW_IF_FAILED(_device->CreateRenderTargetView(buffer.get(), nullptr, _renderTargetView.put()));
    }

    const auto fontChanged = _fontGeneration != p.s->font.generation();
    const auto miscChanged = _miscGeneration != p.s->misc.generation();
    const auto cellCountChanged = _cellCount != p.s->cellCount;

    if (fontChanged)
    {
        _updateFontDependents(p);
    }

    if (cellCountChanged)
    {
        _recreateBackgroundColorBitmap(p.s->cellCount);
    }

    if (miscChanged)
    {
        _recreateCustomShader(p);
    }

    if (_customPixelShader && !_customRenderTargetView)
    {
        _recreateCustomRenderTargetView(p.s->targetSize);
    }

    _recreateConstBuffer(p);
    _setupDeviceContextState(p);

    _generation = p.s.generation();
    _fontGeneration = p.s->font.generation();
    _miscGeneration = p.s->misc.generation();
    _targetSize = p.s->targetSize;
    _cellCount = p.s->cellCount;
}

void BackendD3D::_updateFontDependents(const RenderingPayload& p)
{
    DWrite_GetRenderParams(p.dwriteFactory.get(), &_gamma, &_cleartypeEnhancedContrast, &_grayscaleEnhancedContrast, _textRenderingParams.put());
    // Clearing the atlas requires BeginDraw(), which is expensive. Defer this until we need Direct2D anyways.
    _fontChangedResetGlyphAtlas = true;
    _textShadingType = p.s->font->antialiasingMode == D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE ? ShadingType::TextClearType : ShadingType::TextGrayscale;

    if (_d2dRenderTarget)
    {
        _d2dRenderTargetUpdateFontSettings(*p.s->font);
    }
}

void BackendD3D::_recreateCustomShader(const RenderingPayload& p)
{
    _customRenderTargetView.reset();
    _customOffscreenTexture.reset();
    _customOffscreenTextureView.reset();
    _customVertexShader.reset();
    _customPixelShader.reset();
    _customShaderConstantBuffer.reset();
    _customShaderSamplerState.reset();
    _requiresContinuousRedraw = false;

    if (!p.s->misc->customPixelShaderPath.empty())
    {
        const char* target = nullptr;
        switch (_device->GetFeatureLevel())
        {
        case D3D_FEATURE_LEVEL_10_0:
            target = "ps_4_0";
            break;
        case D3D_FEATURE_LEVEL_10_1:
            target = "ps_4_1";
            break;
        default:
            target = "ps_5_0";
            break;
        }

        static constexpr auto flags =
            D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR
#ifdef NDEBUG
            | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#else
            // Only enable strictness and warnings in DEBUG mode
            //  as these settings makes it very difficult to develop
            //  shaders as windows terminal is not telling the user
            //  what's wrong, windows terminal just fails.
            //  Keep it in DEBUG mode to catch errors in shaders
            //  shipped with windows terminal
            | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        wil::com_ptr<ID3DBlob> error;
        wil::com_ptr<ID3DBlob> blob;
        const auto hr = D3DCompileFromFile(
            /* pFileName   */ p.s->misc->customPixelShaderPath.c_str(),
            /* pDefines    */ nullptr,
            /* pInclude    */ D3D_COMPILE_STANDARD_FILE_INCLUDE,
            /* pEntrypoint */ "main",
            /* pTarget     */ target,
            /* Flags1      */ flags,
            /* Flags2      */ 0,
            /* ppCode      */ blob.addressof(),
            /* ppErrorMsgs */ error.addressof());

        // Unless we can determine otherwise, assume this shader requires evaluation every frame
        _requiresContinuousRedraw = true;

        if (SUCCEEDED(hr))
        {
            THROW_IF_FAILED(_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, _customPixelShader.put()));

            // Try to determine whether the shader uses the Time variable
            wil::com_ptr<ID3D11ShaderReflection> reflector;
            if (SUCCEEDED_LOG(D3DReflect(blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(reflector.put()))))
            {
                if (ID3D11ShaderReflectionConstantBuffer* constantBufferReflector = reflector->GetConstantBufferByIndex(0)) // shader buffer
                {
                    if (ID3D11ShaderReflectionVariable* variableReflector = constantBufferReflector->GetVariableByIndex(0)) // time
                    {
                        D3D11_SHADER_VARIABLE_DESC variableDescriptor;
                        if (SUCCEEDED_LOG(variableReflector->GetDesc(&variableDescriptor)))
                        {
                            // only if time is used
                            _requiresContinuousRedraw = WI_IsFlagSet(variableDescriptor.uFlags, D3D_SVF_USED);
                        }
                    }
                }
            }
        }
        else
        {
            if (error)
            {
                LOG_HR_MSG(hr, "%*hs", error->GetBufferSize(), error->GetBufferPointer());
            }
            else
            {
                LOG_HR(hr);
            }
            if (p.warningCallback)
            {
                p.warningCallback(D2DERR_SHADER_COMPILE_FAILED);
            }
        }
    }
    else if (p.s->misc->useRetroTerminalEffect)
    {
        THROW_IF_FAILED(_device->CreatePixelShader(&custom_shader_ps[0], sizeof(custom_shader_ps), nullptr, _customPixelShader.put()));
        // We know the built-in retro shader doesn't require continuous redraw.
        _requiresContinuousRedraw = false;
    }

    if (_customPixelShader)
    {
        THROW_IF_FAILED(_device->CreateVertexShader(&custom_shader_vs[0], sizeof(custom_shader_vs), nullptr, _customVertexShader.put()));

        {
            static constexpr D3D11_BUFFER_DESC desc{
                .ByteWidth = sizeof(CustomConstBuffer),
                .Usage = D3D11_USAGE_DYNAMIC,
                .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
                .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
            };
            THROW_IF_FAILED(_device->CreateBuffer(&desc, nullptr, _customShaderConstantBuffer.put()));
        }

        {
            static constexpr D3D11_SAMPLER_DESC desc{
                .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
                .AddressU = D3D11_TEXTURE_ADDRESS_BORDER,
                .AddressV = D3D11_TEXTURE_ADDRESS_BORDER,
                .AddressW = D3D11_TEXTURE_ADDRESS_BORDER,
                .MaxAnisotropy = 1,
                .ComparisonFunc = D3D11_COMPARISON_ALWAYS,
                .MaxLOD = D3D11_FLOAT32_MAX,
            };
            THROW_IF_FAILED(_device->CreateSamplerState(&desc, _customShaderSamplerState.put()));
        }

        _customShaderStartTime = std::chrono::steady_clock::now();
    }
}

void BackendD3D::_recreateCustomRenderTargetView(u16x2 targetSize)
{
    // Avoid memory usage spikes by releasing memory first.
    _customOffscreenTexture.reset();
    _customOffscreenTextureView.reset();

    // This causes our regular rendered contents to end up in the offscreen texture. We'll then use the
    // `_customRenderTargetView` to render into the swap chain using the custom (user provided) shader.
    _customRenderTargetView = std::move(_renderTargetView);

    const D3D11_TEXTURE2D_DESC desc{
        .Width = targetSize.x,
        .Height = targetSize.y,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc = { 1, 0 },
        .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
    };
    THROW_IF_FAILED(_device->CreateTexture2D(&desc, nullptr, _customOffscreenTexture.addressof()));
    THROW_IF_FAILED(_device->CreateShaderResourceView(_customOffscreenTexture.get(), nullptr, _customOffscreenTextureView.addressof()));
    THROW_IF_FAILED(_device->CreateRenderTargetView(_customOffscreenTexture.get(), nullptr, _renderTargetView.addressof()));
}

void BackendD3D::_recreateBackgroundColorBitmap(u16x2 cellCount)
{
    // Avoid memory usage spikes by releasing memory first.
    _backgroundBitmap.reset();
    _backgroundBitmapView.reset();

    const D3D11_TEXTURE2D_DESC desc{
        .Width = cellCount.x,
        .Height = cellCount.y,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = { 1, 0 },
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    THROW_IF_FAILED(_device->CreateTexture2D(&desc, nullptr, _backgroundBitmap.addressof()));
    THROW_IF_FAILED(_device->CreateShaderResourceView(_backgroundBitmap.get(), nullptr, _backgroundBitmapView.addressof()));
    _backgroundBitmapGeneration = {};
}

void BackendD3D::_d2dRenderTargetUpdateFontSettings(const FontSettings& font) const noexcept
{
    _d2dRenderTarget->SetDpi(font.dpi, font.dpi);
    _d2dRenderTarget->SetTextAntialiasMode(static_cast<D2D1_TEXT_ANTIALIAS_MODE>(font.antialiasingMode));
}

void BackendD3D::_recreateConstBuffer(const RenderingPayload& p) const
{
    {
        VSConstBuffer data;
        data.positionScale = { 2.0f / p.s->targetSize.x, -2.0f / p.s->targetSize.y };
        _deviceContext->UpdateSubresource(_vsConstantBuffer.get(), 0, nullptr, &data, 0, 0);
    }
    {
        PSConstBuffer data;
        data.backgroundColor = colorFromU32<f32x4>(p.s->misc->backgroundColor);
        data.cellSize = { static_cast<f32>(p.s->font->cellSize.x), static_cast<f32>(p.s->font->cellSize.y) };
        data.cellCount = { static_cast<f32>(p.s->cellCount.x), static_cast<f32>(p.s->cellCount.y) };
        DWrite_GetGammaRatios(_gamma, data.gammaRatios);
        data.enhancedContrast = p.s->font->antialiasingMode == D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE ? _cleartypeEnhancedContrast : _grayscaleEnhancedContrast;
        data.dashedLineLength = p.s->font->underlineWidth * 3.0f;
        _deviceContext->UpdateSubresource(_psConstantBuffer.get(), 0, nullptr, &data, 0, 0);
    }
}

void BackendD3D::_setupDeviceContextState(const RenderingPayload& p)
{
    // IA: Input Assembler
    ID3D11Buffer* vertexBuffers[]{ _vertexBuffer.get(), _instanceBuffer.get() };
    static constexpr UINT strides[]{ sizeof(f32x2), sizeof(QuadInstance) };
    static constexpr UINT offsets[]{ 0, 0 };
    _deviceContext->IASetIndexBuffer(_indexBuffer.get(), DXGI_FORMAT_R16_UINT, 0);
    _deviceContext->IASetInputLayout(_inputLayout.get());
    _deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    _deviceContext->IASetVertexBuffers(0, 2, &vertexBuffers[0], &strides[0], &offsets[0]);

    // VS: Vertex Shader
    _deviceContext->VSSetShader(_vertexShader.get(), nullptr, 0);
    _deviceContext->VSSetConstantBuffers(0, 1, _vsConstantBuffer.addressof());

    // RS: Rasterizer Stage
    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<f32>(p.s->targetSize.x);
    viewport.Height = static_cast<f32>(p.s->targetSize.y);
    _deviceContext->RSSetViewports(1, &viewport);

    // PS: Pixel Shader
    ID3D11ShaderResourceView* resources[]{ _backgroundBitmapView.get(), _glyphAtlasView.get() };
    _deviceContext->PSSetShader(_pixelShader.get(), nullptr, 0);
    _deviceContext->PSSetConstantBuffers(0, 1, _psConstantBuffer.addressof());
    _deviceContext->PSSetShaderResources(0, 2, &resources[0]);

    // OM: Output Merger
    _deviceContext->OMSetBlendState(_blendState.get(), nullptr, 0xffffffff);
    _deviceContext->OMSetRenderTargets(1, _renderTargetView.addressof(), nullptr);
}

#ifndef NDEBUG
void BackendD3D::_debugUpdateShaders(const RenderingPayload& p) noexcept
try
{
    const auto invalidationTime = _sourceCodeInvalidationTime.load(std::memory_order_relaxed);

    if (invalidationTime == INT64_MAX || invalidationTime > std::chrono::steady_clock::now().time_since_epoch().count())
    {
        return;
    }

    _sourceCodeInvalidationTime.store(INT64_MAX, std::memory_order_relaxed);

    static const auto compile = [](const std::filesystem::path& path, const char* target) {
        wil::com_ptr<ID3DBlob> error;
        wil::com_ptr<ID3DBlob> blob;
        const auto hr = D3DCompileFromFile(
            /* pFileName   */ path.c_str(),
            /* pDefines    */ nullptr,
            /* pInclude    */ D3D_COMPILE_STANDARD_FILE_INCLUDE,
            /* pEntrypoint */ "main",
            /* pTarget     */ target,
            /* Flags1      */ D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS,
            /* Flags2      */ 0,
            /* ppCode      */ blob.addressof(),
            /* ppErrorMsgs */ error.addressof());

        if (error)
        {
            std::thread t{ [error = std::move(error)]() noexcept {
                MessageBoxA(nullptr, static_cast<const char*>(error->GetBufferPointer()), "Compilation error", MB_ICONERROR | MB_OK);
            } };
            t.detach();
        }

        THROW_IF_FAILED(hr);
        return blob;
    };

    struct FileVS
    {
        std::wstring_view filename;
        wil::com_ptr<ID3D11VertexShader> BackendD3D::*target;
    };
    struct FilePS
    {
        std::wstring_view filename;
        wil::com_ptr<ID3D11PixelShader> BackendD3D::*target;
    };

    static constexpr std::array filesVS{
        FileVS{ L"shader_vs.hlsl", &BackendD3D::_vertexShader },
    };
    static constexpr std::array filesPS{
        FilePS{ L"shader_ps.hlsl", &BackendD3D::_pixelShader },
    };

    std::array<wil::com_ptr<ID3D11VertexShader>, filesVS.size()> compiledVS;
    std::array<wil::com_ptr<ID3D11PixelShader>, filesPS.size()> compiledPS;

    // Compile our files before moving them into `this` below to ensure we're
    // always in a consistent state where all shaders are seemingly valid.
    for (size_t i = 0; i < filesVS.size(); ++i)
    {
        const auto blob = compile(_sourceDirectory / filesVS[i].filename, "vs_4_0");
        THROW_IF_FAILED(_device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, compiledVS[i].addressof()));
    }
    for (size_t i = 0; i < filesPS.size(); ++i)
    {
        const auto blob = compile(_sourceDirectory / filesPS[i].filename, "ps_4_0");
        THROW_IF_FAILED(_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, compiledPS[i].addressof()));
    }

    for (size_t i = 0; i < filesVS.size(); ++i)
    {
        this->*filesVS[i].target = std::move(compiledVS[i]);
    }
    for (size_t i = 0; i < filesPS.size(); ++i)
    {
        this->*filesPS[i].target = std::move(compiledPS[i]);
    }

    _setupDeviceContextState(p);
}
CATCH_LOG()
#endif

void BackendD3D::_d2dBeginDrawing() noexcept
{
    if (!_d2dBeganDrawing)
    {
        _d2dRenderTarget->BeginDraw();
        _d2dBeganDrawing = true;
    }
}

void BackendD3D::_d2dEndDrawing()
{
    if (_d2dBeganDrawing)
    {
        THROW_IF_FAILED(_d2dRenderTarget->EndDraw());
        _d2dBeganDrawing = false;
    }
}

void BackendD3D::_handleFontChangedResetGlyphAtlas(const RenderingPayload& p)
{
    _fontChangedResetGlyphAtlas = false;
    _resetGlyphAtlasAndBeginDraw(p);
}

void BackendD3D::_resetGlyphAtlasAndBeginDraw(const RenderingPayload& p)
{
    // The index returned by _BitScanReverse is undefined when the input is 0. We can simultaneously guard
    // against that and avoid unreasonably small textures, by clamping the min. texture size to `minArea`.
    static constexpr u32 minArea = 128 * 128;
    const auto minAreaByFont = static_cast<u32>(p.s->font->cellSize.x) * p.s->font->cellSize.y * 64;
    const auto minAreaByGrowth = static_cast<u32>(_rectPacker.width) * _rectPacker.height * 2;
    const auto maxArea = static_cast<u32>(p.s->targetSize.x) * static_cast<u32>(p.s->targetSize.y);
    const auto area = std::min(maxArea, std::max(minArea, std::max(minAreaByFont, minAreaByGrowth)));
    // This block of code calculates the size of a power-of-2 texture that has an area larger than the given `area`.
    // For instance, for an area of 985x1946 = 1916810 it would result in a u/v of 2048x1024 (area = 2097152).
    // This has 2 benefits: GPUs like power-of-2 textures and it ensures that we don't resize the texture
    // every time you resize the window by a pixel. Instead it only grows/shrinks by a factor of 2.
    unsigned long index;
    _BitScanReverse(&index, area - 1);
    const auto u = ::base::saturated_cast<u16>(1u << ((index + 2) / 2));
    const auto v = ::base::saturated_cast<u16>(1u << ((index + 1) / 2));

    if (u != _rectPacker.width || v != _rectPacker.height)
    {
        _d2dRenderTarget.reset();
        _d2dRenderTarget4.reset();
        _glyphAtlas.reset();
        _glyphAtlasView.reset();

        {
            const D3D11_TEXTURE2D_DESC desc{
                .Width = u,
                .Height = v,
                .MipLevels = 1,
                .ArraySize = 1,
                .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
                .SampleDesc = { 1, 0 },
                .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
            };
            THROW_IF_FAILED(_device->CreateTexture2D(&desc, nullptr, _glyphAtlas.addressof()));
            THROW_IF_FAILED(_device->CreateShaderResourceView(_glyphAtlas.get(), nullptr, _glyphAtlasView.addressof()));
        }

        {
            const auto surface = _glyphAtlas.query<IDXGISurface>();

            static constexpr D2D1_RENDER_TARGET_PROPERTIES props{
                .type = D2D1_RENDER_TARGET_TYPE_DEFAULT,
                .pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED },
            };
            wil::com_ptr<ID2D1RenderTarget> renderTarget;
            THROW_IF_FAILED(p.d2dFactory->CreateDxgiSurfaceRenderTarget(surface.get(), &props, renderTarget.addressof()));
            _d2dRenderTarget = renderTarget.query<ID2D1DeviceContext>();
            _d2dRenderTarget4 = renderTarget.try_query<ID2D1DeviceContext4>();

            _d2dRenderTarget->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
            // We don't really use D2D for anything except DWrite, but it
            // can't hurt to ensure that everything it does is pixel aligned.
            _d2dRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
            // Ensure that D2D uses the exact same gamma as our shader uses.
            _d2dRenderTarget->SetTextRenderingParams(_textRenderingParams.get());

            _d2dRenderTargetUpdateFontSettings(*p.s->font);
        }

        {
            static constexpr D2D1_COLOR_F color{ 1, 1, 1, 1 };
            THROW_IF_FAILED(_d2dRenderTarget->CreateSolidColorBrush(&color, nullptr, _brush.put()));
        }

        ID3D11ShaderResourceView* resources[]{ _backgroundBitmapView.get(), _glyphAtlasView.get() };
        _deviceContext->PSSetShaderResources(0, 2, &resources[0]);
    }

    _glyphCache.Clear();
    _rectPackerData = Buffer<stbrp_node>{ u };
    stbrp_init_target(&_rectPacker, u, v, _rectPackerData.data(), _rectPackerData.size());

    _d2dBeginDrawing();
    _d2dRenderTarget->Clear();
}

void BackendD3D::_markStateChange(ID3D11BlendState* blendState)
{
    _instancesStateChanges.emplace_back(StateChange{
        .blendState = blendState,
        .offset = _instancesCount,
    });
}

BackendD3D::QuadInstance& BackendD3D::_getLastQuad() noexcept
{
    assert(_instancesCount != 0);
    return _instances[_instancesCount - 1];
}

void BackendD3D::_appendQuad(i16x2 position, u16x2 size, u32 color, ShadingType shadingType)
{
    _appendQuad(position, size, {}, color, shadingType);
}

void BackendD3D::_appendQuad(i16x2 position, u16x2 size, u16x2 texcoord, u32 color, ShadingType shadingType)
{
    if (_instancesCount >= _instances.size())
    {
        _bumpInstancesSize();
    }

    _instances[_instancesCount++] = QuadInstance{ position, size, texcoord, shadingType, color };
}

void BackendD3D::_bumpInstancesSize()
{
    const auto newSize = std::max<size_t>(256, _instances.size() * 2);
    Expects(newSize > _instances.size());

    auto newInstances = Buffer<QuadInstance>{ newSize };
    std::copy_n(_instances.data(), _instances.size(), newInstances.data());

    _instances = std::move(newInstances);
}

void BackendD3D::_flushQuads(const RenderingPayload& p)
{
    if (!_instancesCount)
    {
        return;
    }

    // TODO: Shrink instances buffer
    if (_instancesCount > _instanceBufferCapacity)
    {
        _recreateInstanceBuffers(p);
    }

    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        THROW_IF_FAILED(_deviceContext->Map(_instanceBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
        memcpy(mapped.pData, _instances.data(), _instancesCount * sizeof(QuadInstance));
        _deviceContext->Unmap(_instanceBuffer.get(), 0);
    }

    // I found 4 approaches to drawing lots of quads quickly. There are probably even more.
    // They can often be found in discussions about "particle" or "point sprite" rendering in game development.
    // * Compute Shader: My understanding is that at the time of writing games are moving over to bucketing
    //   particles into "tiles" on the screen and drawing them with a compute shader. While this improves
    //   performance, it doesn't mix well with our goal of allowing arbitrary overlaps between glyphs.
    //   Additionally none of the next 3 approaches use any significant amount of GPU time in the first place.
    // * Geometry Shader: Geometry shaders can generate vertices on the fly, which would neatly replace our need
    //   for an index buffer. However, many sources claim they're significantly slower than the following approaches.
    // * DrawIndexed & DrawInstanced: Again, many sources claim that GPU instancing (Draw(Indexed)Instanced) performs
    //   poorly for small meshes, and instead indexed vertices with a SRV (shader resource view) should be used.
    //   The popular "Vertex Shader Tricks" talk from Bill Bilodeau at GDC 2014 suggests this approach, explains
    //   how it works (you divide the `SV_VertexID` by 4 and index into the SRV that contains the per-instance data;
    //   it's basically manual instancing inside the vertex shader) and shows how it outperforms regular instancing.
    //   However on my own limited test hardware (built around ~2020), I found that for at least our use case,
    //   GPU instancing matches the performance of using a custom buffer. In fact on my Nvidia GPU in particular,
    //   instancing with ~10k instances appears to be about 50% faster and so DrawInstanced was chosen.
    //   Instead I found that packing instance data as tightly as possible made the biggest performance difference,
    //   and packing 16 bit integers with ID3D11InputLayout is quite a bit more convenient too.

    // This will cause the loop below to emit one final DrawIndexedInstanced() for the remainder of instances.
    _markStateChange(nullptr);

    size_t previousOffset = 0;
    for (const auto& state : _instancesStateChanges)
    {
        if (const auto count = state.offset - previousOffset)
        {
            _deviceContext->DrawIndexedInstanced(6, count, 0, 0, previousOffset);
        }
        if (state.blendState)
        {
            _deviceContext->OMSetBlendState(state.blendState, nullptr, 0xffffffff);
        }
        previousOffset = state.offset;
    }

    _instancesStateChanges.clear();
    _instancesCount = 0;
}

void BackendD3D::_recreateInstanceBuffers(const RenderingPayload& p)
{
    // We use the viewport size of the terminal as the initial estimate for the amount of instances we'll see.
    const auto minCapacity = static_cast<size_t>(p.s->cellCount.x) * p.s->cellCount.y;
    auto newCapacity = std::max(_instancesCount, minCapacity);
    auto newSize = newCapacity * sizeof(QuadInstance);
    // Round up to multiples of 64kB to avoid reallocating too often.
    // 64kB is the minimum alignment for committed resources in D3D12.
    newSize = (newSize + 0xffff) & ~size_t{ 0xffff };
    newCapacity = newSize / sizeof(QuadInstance);

    _instanceBuffer.reset();

    {
        const D3D11_BUFFER_DESC desc{
            .ByteWidth = gsl::narrow<UINT>(newSize),
            .Usage = D3D11_USAGE_DYNAMIC,
            .BindFlags = D3D11_BIND_VERTEX_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
            .StructureByteStride = sizeof(QuadInstance),
        };
        THROW_IF_FAILED(_device->CreateBuffer(&desc, nullptr, _instanceBuffer.addressof()));
    }

    // IA: Input Assembler
    ID3D11Buffer* vertexBuffers[]{ _vertexBuffer.get(), _instanceBuffer.get() };
    static constexpr UINT strides[]{ sizeof(f32x2), sizeof(QuadInstance) };
    static constexpr UINT offsets[]{ 0, 0 };
    _deviceContext->IASetVertexBuffers(0, 2, &vertexBuffers[0], &strides[0], &offsets[0]);

    _instanceBufferCapacity = newCapacity;
}

void BackendD3D::_drawBackground(const RenderingPayload& p)
{
    if (_backgroundBitmapGeneration != p.backgroundBitmapGeneration)
    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        THROW_IF_FAILED(_deviceContext->Map(_backgroundBitmap.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));

        auto data = static_cast<char*>(mapped.pData);
        for (size_t i = 0; i < p.s->cellCount.y; ++i)
        {
            memcpy(data, p.backgroundBitmap.data() + i * p.s->cellCount.x, p.s->cellCount.x * sizeof(u32));
            data += mapped.RowPitch;
        }

        _deviceContext->Unmap(_backgroundBitmap.get(), 0);
        _backgroundBitmapGeneration = p.backgroundBitmapGeneration;
    }

    _appendQuad({}, p.s->targetSize, 0, ShadingType::Background);
}

void BackendD3D::_drawText(RenderingPayload& p)
{
    if (_fontChangedResetGlyphAtlas)
    {
        _handleFontChangedResetGlyphAtlas(p);
    }

    til::CoordType dirtyTop = til::CoordTypeMax;
    til::CoordType dirtyBottom = til::CoordTypeMin;

    u16 y = 0;
    for (const auto row : p.rows)
    {
        f32 baselineX = 0;
        const auto baselineY = y * p.s->font->cellSize.y + p.s->font->baseline;
        GlyphCacheKey key{ .lineRendition = static_cast<u16>(row->lineRendition) };
        const auto lineRenditionScale = static_cast<uint8_t>(row->lineRendition != LineRendition::SingleWidth);

        for (const auto& m : row->mappings)
        {
            key.fontFace = m.fontFace.get();

            for (auto x = m.glyphsFrom; x < m.glyphsTo; ++x)
            {
                key.glyphIndex = row->glyphIndices[x];
                bool inserted = false;
                auto& entry = _glyphCache.FindOrInsert(key, inserted);
                if (inserted)
                {
                    _drawGlyph(p, entry, m.fontEmSize);
                }

                if (entry.data.shadingType != ShadingType::Default)
                {
                    auto l = static_cast<i32>(baselineX + row->glyphOffsets[x].advanceOffset + 0.5f) + entry.data.offset.x;
                    const auto t = static_cast<i32>(baselineY - row->glyphOffsets[x].ascenderOffset + 0.5f) + entry.data.offset.y;

                    l <<= lineRenditionScale;

                    row->dirtyTop = std::min(row->dirtyTop, t);
                    row->dirtyBottom = std::max(row->dirtyBottom, t + entry.data.size.y);

                    _appendQuad({ static_cast<i16>(l), static_cast<i16>(t) }, entry.data.size, entry.data.texcoord, row->colors[x], entry.data.shadingType);
                }

                baselineX += row->glyphAdvances[x];
            }
        }

        if (y >= p.invalidatedRows.x && y < p.invalidatedRows.y)
        {
            dirtyTop = std::min(dirtyTop, row->dirtyTop);
            dirtyBottom = std::max(dirtyBottom, row->dirtyBottom);
        }

        ++y;
    }

    if (dirtyTop < dirtyBottom)
    {
        p.dirtyRectInPx.top = std::min(p.dirtyRectInPx.top, dirtyTop);
        p.dirtyRectInPx.bottom = std::max(p.dirtyRectInPx.bottom, dirtyBottom);
    }

    _d2dEndDrawing();
}

void BackendD3D::_drawGlyph(const RenderingPayload& p, GlyphCacheEntry& entry, f32 fontEmSize)
{
    const DWRITE_GLYPH_RUN glyphRun{
        .fontFace = entry.key.fontFace,
        .fontEmSize = fontEmSize,
        .glyphCount = 1,
        .glyphIndices = &entry.key.glyphIndex,
    };

    DWRITE_FONT_METRICS fontMetrics;
    glyphRun.fontFace->GetMetrics(&fontMetrics);

    DWRITE_GLYPH_METRICS glyphMetrics;
    glyphRun.fontFace->GetDesignGlyphMetrics(glyphRun.glyphIndices, glyphRun.glyphCount, &glyphMetrics, false);

    // This calculates the black box of the glyph, or in other words, it's extents/size relative to its baseline
    // origin (at 0,0). The algorithm below is a reverse engineered variant of `IDWriteTextLayout::GetMetrics`.
    // The coordinates will be in pixels and the positive direction will be bottom/right.
    //
    //  box.top --------++-----######--+
    //   (-7)           ||  ############
    //                  ||####      ####
    //                  |###       #####
    //  baseline _____  |###      #####|
    //   origin       \ |############# |
    //  (= 0,0)        \||###########  |
    //                  ++-------###---+
    //                  ##      ###    |
    //  box.bottom -----+#########-----+
    //    (+2)          |              |
    //               box.left       box.right
    //                 (-1)           (+14)
    //
    const f32 fontScale = glyphRun.fontEmSize / fontMetrics.designUnitsPerEm;
    f32r box{
        static_cast<f32>(glyphMetrics.leftSideBearing) * fontScale,
        static_cast<f32>(glyphMetrics.topSideBearing - glyphMetrics.verticalOriginY) * fontScale,
        static_cast<f32>(static_cast<INT32>(glyphMetrics.advanceWidth) - glyphMetrics.rightSideBearing) * fontScale,
        static_cast<f32>(static_cast<INT32>(glyphMetrics.advanceHeight) - glyphMetrics.bottomSideBearing - glyphMetrics.verticalOriginY) * fontScale,
    };

    // box may be empty if the glyph is whitespace.
    if (box.empty())
    {
        // This will indicate to `BackendD3D::_drawText` that this glyph is whitespace. It's important to set this member,
        // because `GlyphCacheMap` does not zero out inserted entries and `shadingType` might still contain "garbage".
        entry.data.shadingType = ShadingType::Default;
        return;
    }

    const auto isDoubleHeight = static_cast<LineRendition>(entry.key.lineRendition) >= LineRendition::DoubleHeightTop;

    std::optional<D2D1_MATRIX_3X2_F> transform;
    if (entry.key.lineRendition)
    {
        auto& t = transform.emplace();
        t.m11 = 2.0f;
        t.m22 = isDoubleHeight ? 2.0f : 1.0f;

        box.left *= t.m11;
        box.top *= t.m22;
        box.right *= t.m11;
        box.bottom *= t.m22;
    }

    // To take anti-aliasing of the borders into account, we'll add a 1px padding on all 4 sides.
    // This doesn't work however if font hinting causes the pixels to be offset from the design outline.
    // We need to use round (and not ceil/floor) to ensure we pixel-snap individual
    // glyphs correctly and form a consistent baseline across an entire run of glyphs.
    const auto bl = lround(box.left) - 1;
    const auto bt = lround(box.top) - 1;
    const auto br = lround(box.right) + 1;
    const auto bb = lround(box.bottom) + 1;

    stbrp_rect rect{
        .w = br - bl,
        .h = bb - bt,
    };
    if (!stbrp_pack_rects(&_rectPacker, &rect, 1))
    {
        _drawGlyphRetry(p, rect);
    }

    _d2dBeginDrawing();

    D2D1_POINT_2F baseline{
        static_cast<f32>(rect.x - bl),
        static_cast<f32>(rect.y - bt),
    };
    D2D1_RECT_F clipRect{
        static_cast<f32>(rect.x),
        static_cast<f32>(rect.y),
        static_cast<f32>(rect.x + rect.w),
        static_cast<f32>(rect.y + rect.h),
    };
    _d2dRenderTarget->PushAxisAlignedClip(&clipRect, D2D1_ANTIALIAS_MODE_ALIASED);

#if ATLAS_DEBUG_COLORIZE_GLYPH_ATLAS
    {
        const auto d2dColor = colorFromU32(colorbrewer::pastel1[_colorizeGlyphAtlasCounter] | 0x3f000000);
        _colorizeGlyphAtlasCounter = (_colorizeGlyphAtlasCounter + 1) % std::size(colorbrewer::pastel1);
        wil::com_ptr<ID2D1SolidColorBrush> brush;
        THROW_IF_FAILED(_d2dRenderTarget->CreateSolidColorBrush(&d2dColor, nullptr, brush.addressof()));
        _d2dRenderTarget->FillRectangle(&clipRect, brush.get());
    }
#endif

    if (transform)
    {
        auto& t = *transform;
        t.dx = (1.0f - t.m11) * baseline.x;
        t.dy = (1.0f - t.m22) * baseline.y;
        _d2dRenderTarget->SetTransform(&t);
    }

    const auto colorGlyph = DrawGlyphRun(_d2dRenderTarget.get(), _d2dRenderTarget4.get(), p.dwriteFactory4.get(), baseline, &glyphRun, _brush.get());

    entry.data.offset.x = bl;
    entry.data.offset.y = bt;
    entry.data.size.x = rect.w;
    entry.data.size.y = rect.h;
    entry.data.texcoord.x = rect.x;
    entry.data.texcoord.y = rect.y;
    entry.data.shadingType = colorGlyph ? ShadingType::Passthrough : _textShadingType;

    if (transform)
    {
        static constexpr D2D1_MATRIX_3X2_F identity{ .m11 = 1, .m22 = 1 };
        _d2dRenderTarget->SetTransform(&identity);

        if (isDoubleHeight)
        {
            _splitDoubleHeightGlyph(p, entry);
        }
    }

    _d2dRenderTarget->PopAxisAlignedClip();
}

void BackendD3D::_drawGlyphRetry(const RenderingPayload& p, stbrp_rect& rect)
{
    _d2dEndDrawing();
    _flushQuads(p);
    _resetGlyphAtlasAndBeginDraw(p);

    if (!stbrp_pack_rects(&_rectPacker, &rect, 1))
    {
        THROW_HR_MSG(E_UNEXPECTED, "BackendD3D::_drawGlyph deadlock");
    }
}

// If this is a double-height glyph (DECDHL), we need to split it into 2 glyph entries:
// One for the top half and one for the bottom half, because that's how DECDHL works.
// This will clip `entry` to only contain the top/bottom half (as specified by `entry.key.lineRendition`)
// and create a second entry in our glyph cache hashmap that contains the other half.
void BackendD3D::_splitDoubleHeightGlyph(const RenderingPayload& p, GlyphCacheEntry& entry)
{
    static constexpr auto lrTop = static_cast<u16>(LineRendition::DoubleHeightTop);
    static constexpr auto lrBottom = static_cast<u16>(LineRendition::DoubleHeightBottom);

    // Twice the line height, twice the descender gap. For both.
    entry.data.offset.y -= p.s->font->descender;

    const auto isTop = entry.key.lineRendition == lrTop;
    const auto altRendition = isTop ? lrBottom : lrTop;
    const auto topSize = clamp(-entry.data.offset.y - p.s->font->baseline, 0, static_cast<int>(entry.data.size.y));

    bool inserted;
    auto key2 = entry.key;
    key2.lineRendition = altRendition;
    auto& entry2 = _glyphCache.FindOrInsert(key2, inserted);
    entry2.data = entry.data;

    auto& top = isTop ? entry : entry2;
    auto& bottom = isTop ? entry2 : entry;

    top.data.offset.y += p.s->font->cellSize.y;
    top.data.size.y = topSize;

    bottom.data.offset.y += topSize;
    bottom.data.size.y = std::max(0, bottom.data.size.y - topSize);
    bottom.data.texcoord.y += topSize;

    // Things like diacritics might be so small that they only exist on either half of the
    // double-height row. This effectively turns the other (unneeded) side into whitespace.
    if (!top.data.size.y)
    {
        top.data.shadingType = ShadingType::Default;
    }
    if (!bottom.data.size.y)
    {
        bottom.data.shadingType = ShadingType::Default;
    }
}

void BackendD3D::_drawGridlines(const RenderingPayload& p)
{
    u16 y = 0;
    for (const auto row : p.rows)
    {
        if (!row->gridLineRanges.empty())
        {
            _drawGridlineRow(p, row, y);
        }
        y++;
    }
}

void BackendD3D::_drawGridlineRow(const RenderingPayload& p, const ShapedRow* row, u16 y)
{
    const auto top = p.s->font->cellSize.y * y;

    for (const auto& r : row->gridLineRanges)
    {
        // AtlasEngine.cpp shouldn't add any gridlines if they don't do anything.
        assert(r.lines.any());

        const auto left = r.from * p.s->font->cellSize.x;
        const auto width = (r.to - r.from) * p.s->font->cellSize.x;
        i16x2 position;
        u16x2 size;

        if (r.lines.test(GridLines::Left))
        {
            for (auto i = r.from; i < r.to; ++i)
            {
                position.x = i * p.s->font->cellSize.x;
                position.y = top;
                size.x = p.s->font->thinLineWidth;
                size.y = p.s->font->cellSize.y;
                _appendQuad(position, size, r.color, ShadingType::SolidFill);
            }
        }
        if (r.lines.test(GridLines::Top))
        {
            position.x = left;
            position.y = top;
            size.x = width;
            size.y = p.s->font->thinLineWidth;
            _appendQuad(position, size, r.color, ShadingType::SolidFill);
        }
        if (r.lines.test(GridLines::Right))
        {
            for (auto i = r.to; i > r.from; --i)
            {
                position.x = i * p.s->font->cellSize.x;
                position.y = top;
                size.x = p.s->font->thinLineWidth;
                size.y = p.s->font->cellSize.y;
                _appendQuad(position, size, r.color, ShadingType::SolidFill);
            }
        }
        if (r.lines.test(GridLines::Bottom))
        {
            position.x = left;
            position.y = top + p.s->font->cellSize.y - p.s->font->thinLineWidth;
            size.x = width;
            size.y = p.s->font->thinLineWidth;
            _appendQuad(position, size, r.color, ShadingType::SolidFill);
        }
        if (r.lines.test(GridLines::Underline))
        {
            position.x = left;
            position.y = top + p.s->font->underlinePos;
            size.x = width;
            size.y = p.s->font->underlineWidth;
            _appendQuad(position, size, r.color, ShadingType::SolidFill);
        }
        if (r.lines.test(GridLines::HyperlinkUnderline))
        {
            position.x = left;
            position.y = top + p.s->font->underlinePos;
            size.x = width;
            size.y = p.s->font->underlineWidth;
            _appendQuad(position, size, r.color, ShadingType::DashedLine);
        }
        if (r.lines.test(GridLines::DoubleUnderline))
        {
            position.x = left;
            position.y = top + p.s->font->doubleUnderlinePos.x;
            size.x = width;
            size.y = p.s->font->thinLineWidth;
            _appendQuad(position, size, r.color, ShadingType::SolidFill);

            position.x = left;
            position.y = top + p.s->font->doubleUnderlinePos.y;
            size.x = width;
            size.y = p.s->font->thinLineWidth;
            _appendQuad(position, size, r.color, ShadingType::SolidFill);
        }
        if (r.lines.test(GridLines::Strikethrough))
        {
            position.x = left;
            position.y = top + p.s->font->strikethroughPos;
            size.x = width;
            size.y = p.s->font->strikethroughWidth;
            _appendQuad(position, size, r.color, ShadingType::SolidFill);
        }
    }
}

void BackendD3D::_drawCursorPart1(const RenderingPayload& p)
{
    _cursorRects.clear();

    if (p.cursorRect.empty())
    {
        return;
    }

    const auto cursorColor = p.s->cursor->cursorColor;
    const auto offset = p.cursorRect.top * static_cast<size_t>(p.s->cellCount.x);

    for (auto x1 = p.cursorRect.left; x1 < p.cursorRect.right; ++x1)
    {
        const auto x0 = x1;
        const auto bg = p.backgroundBitmap[offset + x1] | 0xff000000;

        for (; x1 < p.cursorRect.right && (p.backgroundBitmap[offset + x1] | 0xff000000) == bg; ++x1)
        {
        }

        const i16x2 position{
            p.s->font->cellSize.x * x0,
            p.s->font->cellSize.y * p.cursorRect.top,
        };
        const u16x2 size{
            static_cast<u16>(p.s->font->cellSize.x * (x1 - x0)),
            p.s->font->cellSize.y,
        };
        const auto color = cursorColor == 0xffffffff ? bg ^ 0x3f3f3f : cursorColor;
        auto& c0 = _cursorRects.emplace_back(CursorRect{ position, size, color });

        switch (static_cast<CursorType>(p.s->cursor->cursorType))
        {
        case CursorType::Legacy:
        {
            const auto height = (c0.size.y * p.s->cursor->heightPercentage + 50) / 100;
            c0.position.y += c0.size.y - height;
            c0.size.y = height;
            break;
        }
        case CursorType::VerticalBar:
            c0.size.x = p.s->font->thinLineWidth;
            break;
        case CursorType::Underscore:
            c0.position.y += p.s->font->underlinePos;
            c0.size.y = p.s->font->underlineWidth;
            break;
        case CursorType::EmptyBox:
        {
            auto& c1 = _cursorRects.emplace_back(c0);
            if (x0 == p.cursorRect.left)
            {
                auto& c = _cursorRects.emplace_back(c0);
                // Make line a little shorter vertically so it doesn't overlap with the top/bottom horizontal lines.
                c.position.y += p.s->font->thinLineWidth;
                c.size.y -= 2 * p.s->font->thinLineWidth;
                // The actual adjustment...
                c.size.x = p.s->font->thinLineWidth;
            }
            if (x1 == p.cursorRect.right)
            {
                auto& c = _cursorRects.emplace_back(c0);
                // Make line a little shorter vertically so it doesn't overlap with the top/bottom horizontal lines.
                c.position.y += p.s->font->thinLineWidth;
                c.size.y -= 2 * p.s->font->thinLineWidth;
                // The actual adjustment...
                c.position.x += c.size.x - p.s->font->thinLineWidth;
                c.size.x = p.s->font->thinLineWidth;
            }
            c0.size.y = p.s->font->thinLineWidth;
            c1.position.y += c1.size.y - p.s->font->thinLineWidth;
            c1.size.y = p.s->font->thinLineWidth;
            break;
        }
        case CursorType::FullBox:
            break;
        case CursorType::DoubleUnderscore:
        {
            auto& c1 = _cursorRects.emplace_back(c0);
            c0.position.y += p.s->font->doubleUnderlinePos.x;
            c0.size.y = p.s->font->thinLineWidth;
            c1.position.y += p.s->font->doubleUnderlinePos.y;
            c1.size.y = p.s->font->thinLineWidth;
            break;
        }
        default:
            break;
        }
    }

    if (cursorColor == 0xffffffff)
    {
        for (auto& c : _cursorRects)
        {
            _appendQuad(c.position, c.size, c.color, ShadingType::SolidFill);
            c.color = 0xffffffff;
        }
    }
}

void BackendD3D::_drawCursorPart2(const RenderingPayload& p)
{
    if (_cursorRects.empty())
    {
        return;
    }

    const auto color = p.s->cursor->cursorColor;

    if (color == 0xffffffff)
    {
        _markStateChange(_blendStateInvert.get());
    }

    for (const auto& c : _cursorRects)
    {
        _appendQuad(c.position, c.size, c.color, ShadingType::SolidFill);
    }

    if (color == 0xffffffff)
    {
        _markStateChange(_blendState.get());
    }
}

void BackendD3D::_drawSelection(const RenderingPayload& p)
{
    u16 y = 0;
    u16 lastFrom = 0;
    u16 lastTo = 0;

    for (const auto& row : p.rows)
    {
        if (row->selectionTo > row->selectionFrom)
        {
            // If the current selection line matches the previous one, we can just extend the previous quad downwards.
            // The way this is implemented isn't very smart, but we also don't have very many rows to iterate through.
            if (row->selectionFrom == lastFrom && row->selectionTo == lastTo)
            {
                _getLastQuad().size.y += p.s->font->cellSize.y;
            }
            else
            {
                const i16x2 position{
                    p.s->font->cellSize.x * row->selectionFrom,
                    p.s->font->cellSize.y * y,
                };
                const u16x2 size{
                    (p.s->font->cellSize.x * (row->selectionTo - row->selectionFrom)),
                    p.s->font->cellSize.y,
                };
                _appendQuad(position, size, p.s->misc->selectionColor, ShadingType::SolidFill);
                lastFrom = row->selectionFrom;
                lastTo = row->selectionTo;
            }
        }

        y++;
    }
}

#if ATLAS_DEBUG_SHOW_DIRTY
void BackendD3D::_debugShowDirty(const RenderingPayload& p)
{
    _presentRects[_presentRectsPos] = p.dirtyRectInPx;
    _presentRectsPos = (_presentRectsPos + 1) % std::size(_presentRects);

    for (size_t i = 0; i < std::size(_presentRects); ++i)
    {
        if (const auto& rect = _presentRects[i])
        {
            const i16x2 position{
                static_cast<i16>(rect.left),
                static_cast<i16>(rect.top),
            };
            const u16x2 size{
                static_cast<u16>(rect.right - rect.left),
                static_cast<u16>(rect.bottom - rect.top),
            };
            const auto color = colorbrewer::pastel1[i] | 0x1f000000;
            _appendQuad(position, size, color, ShadingType::SolidFill);
        }
    }
}
#endif

#if ATLAS_DEBUG_DUMP_RENDER_TARGET
void BackendD3D::_debugDumpRenderTarget(const RenderingPayload& p)
{
    if (_dumpRenderTargetCounter == 0)
    {
        ExpandEnvironmentStringsW(ATLAS_DEBUG_DUMP_RENDER_TARGET_PATH, &_dumpRenderTargetBasePath[0], gsl::narrow_cast<DWORD>(std::size(_dumpRenderTargetBasePath)));
        std::filesystem::create_directories(_dumpRenderTargetBasePath);
    }

    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\%u_%08zu.png", &_dumpRenderTargetBasePath[0], GetCurrentProcessId(), _dumpRenderTargetCounter);
    SaveTextureToPNG(_deviceContext.get(), _swapChainManager.GetBuffer().get(), p.s->font->dpi, &path[0]);
    _dumpRenderTargetCounter++;
}
#endif

void BackendD3D::_executeCustomShader(RenderingPayload& p)
{
    {
        const CustomConstBuffer data{
            .time = std::chrono::duration<f32>(std::chrono::steady_clock::now() - _customShaderStartTime).count(),
            .scale = static_cast<f32>(p.s->font->dpi) / static_cast<f32>(USER_DEFAULT_SCREEN_DPI),
            .resolution = {
                static_cast<f32>(_cellCount.x * p.s->font->cellSize.x),
                static_cast<f32>(_cellCount.y * p.s->font->cellSize.y),
            },
            .background = colorFromU32<f32x4>(p.s->misc->backgroundColor),
        };

        D3D11_MAPPED_SUBRESOURCE mapped{};
        THROW_IF_FAILED(_deviceContext->Map(_customShaderConstantBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
        memcpy(mapped.pData, &data, sizeof(data));
        _deviceContext->Unmap(_customShaderConstantBuffer.get(), 0);
    }

    {
        // Before we do anything else we have to unbound _renderTargetView from being
        // a render target, otherwise we can't use it as a shader resource below.
        _deviceContext->OMSetRenderTargets(1, _customRenderTargetView.addressof(), nullptr);

        // IA: Input Assembler
        _deviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
        _deviceContext->IASetInputLayout(nullptr);
        _deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        _deviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);

        // VS: Vertex Shader
        _deviceContext->VSSetShader(_customVertexShader.get(), nullptr, 0);
        _deviceContext->VSSetConstantBuffers(0, 0, nullptr);

        // PS: Pixel Shader
        _deviceContext->PSSetShader(_customPixelShader.get(), nullptr, 0);
        _deviceContext->PSSetConstantBuffers(0, 1, _customShaderConstantBuffer.addressof());
        _deviceContext->PSSetShaderResources(0, 1, _customOffscreenTextureView.addressof());
        _deviceContext->PSSetSamplers(0, 1, _customShaderSamplerState.addressof());

        // OM: Output Merger
        _deviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    }

    _deviceContext->Draw(4, 0);

    {
        // IA: Input Assembler
        ID3D11Buffer* vertexBuffers[]{ _vertexBuffer.get(), _instanceBuffer.get() };
        static constexpr UINT strides[]{ sizeof(f32x2), sizeof(QuadInstance) };
        static constexpr UINT offsets[]{ 0, 0 };
        _deviceContext->IASetIndexBuffer(_indexBuffer.get(), DXGI_FORMAT_R16_UINT, 0);
        _deviceContext->IASetInputLayout(_inputLayout.get());
        _deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        _deviceContext->IASetVertexBuffers(0, 2, &vertexBuffers[0], &strides[0], &offsets[0]);

        // VS: Vertex Shader
        _deviceContext->VSSetShader(_vertexShader.get(), nullptr, 0);
        _deviceContext->VSSetConstantBuffers(0, 1, _vsConstantBuffer.addressof());

        // PS: Pixel Shader
        ID3D11ShaderResourceView* resources[]{ _backgroundBitmapView.get(), _glyphAtlasView.get() };
        _deviceContext->PSSetShader(_pixelShader.get(), nullptr, 0);
        _deviceContext->PSSetConstantBuffers(0, 1, _psConstantBuffer.addressof());
        _deviceContext->PSSetShaderResources(0, 2, &resources[0]);
        _deviceContext->PSSetSamplers(0, 0, nullptr);

        // OM: Output Merger
        _deviceContext->OMSetBlendState(_blendState.get(), nullptr, 0xffffffff);
        _deviceContext->OMSetRenderTargets(1, _renderTargetView.addressof(), nullptr);
    }

    // With custom shaders, everything might be invalidated, so we have to
    // indirectly disable Present1() and its dirty rects this way.
    p.dirtyRectInPx = { 0, 0, p.s->targetSize.x, p.s->targetSize.y };
}

TIL_FAST_MATH_END
