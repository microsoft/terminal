#include "pch.h"
#include "BackendD3D11.h"

#include <til/hash.h>

#include <custom_shader_ps.h>
#include <custom_shader_vs.h>
#include <shader_ps.h>
#include <shader_vs.h>

#include "dwrite.h"

TIL_FAST_MATH_BEGIN

// Disable a bunch of warnings which get in the way of writing performant code.
#pragma warning(disable : 26429) // Symbol 'data' is never tested for nullness, it can be marked as not_null (f.23).
#pragma warning(disable : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
#pragma warning(disable : 26459) // You called an STL function '...' with a raw pointer parameter at position '...' that may be unsafe [...].
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26482) // Only index into arrays using constant expressions (bounds.2).

using namespace Microsoft::Console::Render::Atlas;

BackendD3D11::GlyphCacheMap::~GlyphCacheMap()
{
    Clear();
}

BackendD3D11::GlyphCacheMap& BackendD3D11::GlyphCacheMap::operator=(GlyphCacheMap&& other) noexcept
{
    _map = std::exchange(other._map, {});
    _mapMask = std::exchange(other._mapMask, 0);
    _capacity = std::exchange(other._capacity, 0);
    _size = std::exchange(other._size, 0);
    return *this;
}

void BackendD3D11::GlyphCacheMap::Clear() noexcept
{
    if (_size)
    {
        for (auto& entry : _map)
        {
            if (entry.fontFace)
            {
                // I'm pretty sure Release() doesn't throw exceptions.
#pragma warning(suppress : 26447) // The function is declared 'noexcept' but calls function 'Release()' which may throw exceptions (f.6).
                entry.fontFace->Release();
                entry.fontFace = nullptr;
            }
        }
    }
}

BackendD3D11::GlyphCacheEntry& BackendD3D11::GlyphCacheMap::FindOrInsert(IDWriteFontFace* fontFace, u16 glyphIndex, bool& inserted)
{
    const auto hash = _hash(fontFace, glyphIndex);

    for (auto i = hash;; ++i)
    {
        auto& entry = _map[i & _mapMask];
        if (entry.fontFace == fontFace && entry.glyphIndex == glyphIndex)
        {
            inserted = false;
            return entry;
        }
        if (!entry.fontFace)
        {
            inserted = true;
            return _insert(fontFace, glyphIndex, hash);
        }
    }
}

size_t BackendD3D11::GlyphCacheMap::_hash(IDWriteFontFace* fontFace, u16 glyphIndex) noexcept
{
    // MSVC 19.33 produces surprisingly good assembly for this without stack allocation.
    const uintptr_t data[2]{ std::bit_cast<uintptr_t>(fontFace), glyphIndex };
    return til::hash(&data[0], sizeof(data));
}

BackendD3D11::GlyphCacheEntry& BackendD3D11::GlyphCacheMap::_insert(IDWriteFontFace* fontFace, u16 glyphIndex, size_t hash)
{
    if (_size >= _capacity)
    {
        _bumpSize();
    }

    ++_size;

    for (auto i = hash;; ++i)
    {
        auto& entry = _map[i & _mapMask];
        if (!entry.fontFace)
        {
            entry.fontFace = fontFace;
            entry.glyphIndex = glyphIndex;
            entry.fontFace->AddRef();
            return entry;
        }
    }
}

void BackendD3D11::GlyphCacheMap::_bumpSize()
{
    const auto newMapSize = _map.size() * 2;
    const auto newMapMask = newMapSize - 1;
    FAIL_FAST_IF(newMapSize >= INT32_MAX); // overflow/truncation protection

    auto newMap = Buffer<GlyphCacheEntry>(newMapSize);

    for (const auto& entry : _map)
    {
        const auto newHash = _hash(entry.fontFace, entry.glyphIndex);
        newMap[newHash & newMapMask] = entry;
    }

    _map = std::move(newMap);
    _mapMask = newMapMask;
    _capacity = newMapSize / 2;
}

BackendD3D11::BackendD3D11(wil::com_ptr<ID3D11Device2> device, wil::com_ptr<ID3D11DeviceContext2> deviceContext) :
    _device{ std::move(device) },
    _deviceContext{ std::move(deviceContext) }
{
    THROW_IF_FAILED(_device->CreateVertexShader(&shader_vs[0], sizeof(shader_vs), nullptr, _vertexShader.addressof()));
    THROW_IF_FAILED(_device->CreatePixelShader(&shader_ps[0], sizeof(shader_ps), nullptr, _pixelShader.addressof()));

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

void BackendD3D11::Render(RenderingPayload& p)
{
    _debugUpdateShaders();

    if (_generation != p.s.generation())
    {
        _handleSettingsUpdate(p);
    }

    // After a Present() the render target becomes unbound.
    _deviceContext->OMSetRenderTargets(1, _renderTargetView.addressof(), nullptr);

    _drawBackground(p);
    _drawCursorPart1(p);
    _drawText(p);
    _drawGridlines(p);
    _drawCursorPart2(p);
    _drawSelection(p);
    _flushQuads(p);

    if (_customPixelShader)
    {
        _executeCustomShader(p);
    }

    _swapChainManager.Present(p);
}

bool BackendD3D11::RequiresContinuousRedraw() noexcept
{
    return _requiresContinuousRedraw;
}

void BackendD3D11::WaitUntilCanRender() noexcept
{
    _swapChainManager.WaitUntilCanRender();
}

void BackendD3D11::_debugUpdateShaders() noexcept
try
{
#ifndef NDEBUG
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
        wil::com_ptr<ID3D11VertexShader> BackendD3D11::*target;
    };
    struct FilePS
    {
        std::wstring_view filename;
        wil::com_ptr<ID3D11PixelShader> BackendD3D11::*target;
    };

    static constexpr std::array filesVS{
        FileVS{ L"shader_vs.hlsl", &BackendD3D11::_vertexShader },
    };
    static constexpr std::array filesPS{
        FilePS{ L"shader_ps.hlsl", &BackendD3D11::_pixelShader },
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
#endif
}
CATCH_LOG()

void BackendD3D11::_handleSettingsUpdate(const RenderingPayload& p)
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
        DWrite_GetRenderParams(p.dwriteFactory.get(), &_gamma, &_cleartypeEnhancedContrast, &_grayscaleEnhancedContrast, _textRenderingParams.put());
        _resetGlyphAtlasNeeded = true;

        if (_d2dRenderTarget)
        {
            _d2dRenderTargetUpdateFontSettings(*p.s->font);
        }
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

void BackendD3D11::_recreateCustomShader(const RenderingPayload& p)
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
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = sizeof(CustomConstBuffer);
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            THROW_IF_FAILED(_device->CreateBuffer(&desc, nullptr, _customShaderConstantBuffer.put()));
        }

        {
            D3D11_SAMPLER_DESC desc{};
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
            desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
            desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
            desc.MaxAnisotropy = 1;
            desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
            desc.MaxLOD = D3D11_FLOAT32_MAX;
            THROW_IF_FAILED(_device->CreateSamplerState(&desc, _customShaderSamplerState.put()));
        }

        _customShaderStartTime = std::chrono::steady_clock::now();
    }
}

void BackendD3D11::_recreateCustomRenderTargetView(u16x2 targetSize)
{
    // Avoid memory usage spikes by releasing memory first.
    _customOffscreenTexture.reset();
    _customOffscreenTextureView.reset();

    // This causes our regular rendered contents to end up in the offscreen texture. We'll then use the
    // `_customRenderTargetView` to render into the swap chain using the custom (user provided) shader.
    _customRenderTargetView = std::move(_renderTargetView);

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = targetSize.x;
    desc.Height = targetSize.y;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc = { 1, 0 };
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    THROW_IF_FAILED(_device->CreateTexture2D(&desc, nullptr, _customOffscreenTexture.addressof()));
    THROW_IF_FAILED(_device->CreateShaderResourceView(_customOffscreenTexture.get(), nullptr, _customOffscreenTextureView.addressof()));
    THROW_IF_FAILED(_device->CreateRenderTargetView(_customOffscreenTexture.get(), nullptr, _renderTargetView.addressof()));
}

void BackendD3D11::_recreateBackgroundColorBitmap(u16x2 cellCount)
{
    // Avoid memory usage spikes by releasing memory first.
    _backgroundBitmap.reset();
    _backgroundBitmapView.reset();

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = cellCount.x;
    desc.Height = cellCount.y;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc = { 1, 0 };
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    THROW_IF_FAILED(_device->CreateTexture2D(&desc, nullptr, _backgroundBitmap.addressof()));
    THROW_IF_FAILED(_device->CreateShaderResourceView(_backgroundBitmap.get(), nullptr, _backgroundBitmapView.addressof()));
}

void BackendD3D11::_d2dRenderTargetUpdateFontSettings(const FontSettings& font) noexcept
{
    _d2dRenderTarget->SetDpi(font.dpi, font.dpi);
    _d2dRenderTarget->SetTextAntialiasMode(static_cast<D2D1_TEXT_ANTIALIAS_MODE>(font.antialiasingMode));
}

void BackendD3D11::_recreateConstBuffer(const RenderingPayload& p)
{
    {
        VSConstBuffer data;
        data.positionScale = { 2.0f / p.s->targetSize.x, -2.0f / p.s->targetSize.y };
        _deviceContext->UpdateSubresource(_vsConstantBuffer.get(), 0, nullptr, &data, 0, 0);
    }
    {
        PSConstBuffer data;
        data.backgroundColor = colorFromU32<f32x4>(p.s->misc->backgroundColor);
        data.cellCount = { static_cast<f32>(p.s->cellCount.x), static_cast<f32>(p.s->cellCount.y) };
        data.cellSize = { static_cast<f32>(p.s->font->cellSize.x), static_cast<f32>(p.s->font->cellSize.y) };
        DWrite_GetGammaRatios(_gamma, data.gammaRatios);
        data.enhancedContrast = p.s->font->antialiasingMode == D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE ? _cleartypeEnhancedContrast : _grayscaleEnhancedContrast;
        data.dashedLineLength = p.s->font->underlineWidth * 3.0f;
        _deviceContext->UpdateSubresource(_psConstantBuffer.get(), 0, nullptr, &data, 0, 0);
    }
}

void BackendD3D11::_setupDeviceContextState(const RenderingPayload& p)
{
    // IA: Input Assembler
    _deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    _deviceContext->IASetIndexBuffer(_indexBuffer.get(), _indicesFormat, 0);

    // VS: Vertex Shader
    _deviceContext->VSSetShader(_vertexShader.get(), nullptr, 0);
    _deviceContext->VSSetConstantBuffers(0, 1, _vsConstantBuffer.addressof());
    _deviceContext->VSSetShaderResources(0, 1, _instanceBufferView.addressof());

    // RS: Rasterizer Stage
    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<f32>(p.s->targetSize.x);
    viewport.Height = static_cast<f32>(p.s->targetSize.y);
    _deviceContext->RSSetViewports(1, &viewport);

    // PS: Pixel Shader
    ID3D11ShaderResourceView* const resources[]{ _backgroundBitmapView.get(), _glyphAtlasView.get() };
    _deviceContext->PSSetShader(_pixelShader.get(), nullptr, 0);
    _deviceContext->PSSetConstantBuffers(0, 1, _psConstantBuffer.addressof());
    _deviceContext->PSSetShaderResources(0, 2, &resources[0]);

    // OM: Output Merger
    _deviceContext->OMSetBlendState(_blendState.get(), nullptr, 0xffffffff);
    _deviceContext->OMSetRenderTargets(1, _renderTargetView.addressof(), nullptr);
}

void BackendD3D11::_d2dBeginDrawing() noexcept
{
    if (!_d2dBeganDrawing)
    {
        _d2dRenderTarget->BeginDraw();
        _d2dBeganDrawing = true;
    }
}

void BackendD3D11::_d2dEndDrawing()
{
    if (_d2dBeganDrawing)
    {
        THROW_IF_FAILED(_d2dRenderTarget->EndDraw());
        _d2dBeganDrawing = false;
    }
}

void BackendD3D11::_resetGlyphAtlasAndBeginDraw(const RenderingPayload& p)
{
    // This block of code calculates the size of a power-of-2 texture that has an area larger than the targetSize
    // of the swap chain. In other words for a 985x1946 pixel swap chain (area = 1916810) it would result in a u/v
    // of 2048x1024 (area = 2097152). This has 2 benefits: GPUs like power-of-2 textures and it ensures that we don't
    // resize the texture every time you resize the window by a pixel. Instead it only grows/shrinks by a factor of 2.
    auto area = static_cast<u32>(p.s->targetSize.x) * static_cast<u32>(p.s->targetSize.y);
    // The index returned by _BitScanReverse is undefined when the input is 0. We can simultaneously
    // guard against this and avoid unreasonably small textures, by clamping the min. texture size.
    area = std::max(uint32_t{ 256 * 256 }, area);
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
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = u;
            desc.Height = v;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc = { 1, 0 };
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            THROW_IF_FAILED(_device->CreateTexture2D(&desc, nullptr, _glyphAtlas.addressof()));
            THROW_IF_FAILED(_device->CreateShaderResourceView(_glyphAtlas.get(), nullptr, _glyphAtlasView.addressof()));
        }

        {
            const auto surface = _glyphAtlas.query<IDXGISurface>();

            D2D1_RENDER_TARGET_PROPERTIES props{};
            props.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
            props.pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED };
            wil::com_ptr<ID2D1RenderTarget> renderTarget;
            THROW_IF_FAILED(p.d2dFactory->CreateDxgiSurfaceRenderTarget(surface.get(), &props, renderTarget.addressof()));
            _d2dRenderTarget = renderTarget.query<ID2D1DeviceContext>();
            _d2dRenderTarget4 = renderTarget.try_query<ID2D1DeviceContext4>();

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

        ID3D11ShaderResourceView* const resources[]{ _backgroundBitmapView.get(), _glyphAtlasView.get() };
        _deviceContext->PSSetShaderResources(0, 2, &resources[0]);
    }

    _glyphCache.Clear();
    _rectPackerData = Buffer<stbrp_node>{ u };
    stbrp_init_target(&_rectPacker, u, v, _rectPackerData.data(), gsl::narrow_cast<int>(_rectPackerData.size()));

    _d2dBeginDrawing();
    _d2dRenderTarget->Clear();
}

BackendD3D11::QuadInstance& BackendD3D11::_getLastQuad() noexcept
{
    assert(_instancesSize != 0);
    return _instances[_instancesSize - 1];
}

void BackendD3D11::_appendQuad(i32r position, u32 color, ShadingType shadingType)
{
    _appendQuad(position, {}, color, shadingType);
}

void BackendD3D11::_appendQuad(i32r position, i32r texcoord, u32 color, ShadingType shadingType)
{
    if (_instancesSize >= _instances.size())
    {
        _bumpInstancesSize();
    }

    _instances[_instancesSize++] = QuadInstance{ position, texcoord, color, static_cast<u32>(shadingType) };
}

void BackendD3D11::_bumpInstancesSize()
{
    _instances = Buffer<QuadInstance>{ std::max<size_t>(1024, _instances.size() << 1) };
}

void BackendD3D11::_flushQuads(const RenderingPayload& p)
{
    if (!_instancesSize)
    {
        return;
    }

    if (_instancesSize > _instanceBufferSize)
    {
        _recreateInstanceBuffers(p);
    }

    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        THROW_IF_FAILED(_deviceContext->Map(_instanceBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
        memcpy(mapped.pData, _instances.data(), _instancesSize * sizeof(QuadInstance));
        _deviceContext->Unmap(_instanceBuffer.get(), 0);
    }

    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        THROW_IF_FAILED(_deviceContext->Map(_indexBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));

        if (_indicesFormat == DXGI_FORMAT_R16_UINT)
        {
            auto data = static_cast<u16*>(mapped.pData);
            const u16 vertices = gsl::narrow_cast<u16>(4 * _instancesSize);
            for (u16 off = 0; off < vertices; off += 4)
            {
                *data++ = off + 0;
                *data++ = off + 1;
                *data++ = off + 2;
                *data++ = off + 3;
                *data++ = off + 2;
                *data++ = off + 1;
            }
        }
        else
        {
            assert(_indicesFormat == DXGI_FORMAT_R32_UINT);
            auto data = static_cast<u32*>(mapped.pData);
            const u32 vertices = gsl::narrow_cast<u32>(4 * _instancesSize);
            for (u32 off = 0; off < vertices; off += 4)
            {
                *data++ = off + 0;
                *data++ = off + 1;
                *data++ = off + 2;
                *data++ = off + 3;
                *data++ = off + 2;
                *data++ = off + 1;
            }
        }

        _deviceContext->Unmap(_indexBuffer.get(), 0);
    }

    // I found 4 approaches to drawing lots of quads quickly.
    // They can often be found in discussions about "particle" or "point sprite" rendering in game development.
    // * Compute Shader: My understanding is that at the time of writing games are moving over to bucketing
    //   particles into "tiles" on the screen and drawing them with a compute shader. While this improves
    //   performance, it doesn't mix well with our goal of allowing arbitrary overlaps between glyphs.
    //   Additionally none of the next 3 approaches use any significant amount of GPU time in the first place.
    // * Geometry Shader: Geometry shaders can generate vertices on the fly, which would neatly replace
    //   our need for an index buffer. The reason this wasn't chosen is the same as for the next point.
    // * DrawInstanced: On my own hardware (Nvidia RTX 4090) this seems to perform ~50% better than the final point,
    //   but with no significant difference in power draw. However the popular "Vertex Shader Tricks" talk from
    //   Bill Bilodeau at GDC 2014 suggests that this at least doesn't apply to 2014ish hardware, which supposedly
    //   performs poorly with very small, instanced meshes. Furthermore, public feedback suggests that we still
    //   have a lot of users with older hardware, so I've chosen the following approach, suggested in the talk.
    // * DrawIndexed: This works about the same as DrawInstanced, but instead of using D3D11_INPUT_PER_INSTANCE_DATA,
    //   it uses a SRV (shader resource view) for instance data and maps each SV_VertexID to a SRV slot.
    _deviceContext->DrawIndexed(gsl::narrow_cast<UINT>(6 * _instancesSize), 0, 0);

    _instancesSize = 0;
}

void BackendD3D11::_recreateInstanceBuffers(const RenderingPayload& p)
{
    static constexpr size_t R16max = 1 << 16;
    // While the viewport size of the terminal is probably a good initial estimate for the amount of instances we'll see,
    // I feel like we should ensure that the estimate doesn't exceed the limit for a DXGI_FORMAT_R16_UINT index buffer.
    const auto estimatedInstances = std::min(R16max / 4, static_cast<size_t>(p.s->cellCount.x) * p.s->cellCount.y);
    const auto minSize = std::max(_instancesSize, estimatedInstances);
    // std::bit_ceil will result in a nice exponential growth curve. I don't know exactly how structured buffers are treated
    // by various drivers, but I'm assuming that they prefer buffer sizes that are close to power-of-2 sizes as well.
    const auto newInstancesSize = std::bit_ceil(minSize * sizeof(QuadInstance)) / sizeof(QuadInstance);
    const auto newIndicesSize = newInstancesSize * 6;
    const auto vertices = newInstancesSize * 4;
    const auto indicesFormat = vertices <= R16max ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    const auto indexSize = vertices <= R16max ? sizeof(u16) : sizeof(u32);

    _indexBuffer.reset();
    _instanceBuffer.reset();
    _instanceBufferView.reset();

    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = gsl::narrow<UINT>(newIndicesSize * indexSize);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        THROW_IF_FAILED(_device->CreateBuffer(&desc, nullptr, _indexBuffer.addressof()));
    }

    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = gsl::narrow<UINT>(newInstancesSize * sizeof(QuadInstance));
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        desc.StructureByteStride = sizeof(QuadInstance);
        THROW_IF_FAILED(_device->CreateBuffer(&desc, nullptr, _instanceBuffer.addressof()));
        THROW_IF_FAILED(_device->CreateShaderResourceView(_instanceBuffer.get(), nullptr, _instanceBufferView.addressof()));
    }

    _deviceContext->IASetIndexBuffer(_indexBuffer.get(), indicesFormat, 0);
    _deviceContext->VSSetShaderResources(0, 1, _instanceBufferView.addressof());

    _instanceBufferSize = newInstancesSize;
    _indicesFormat = indicesFormat;
}

void BackendD3D11::_drawBackground(const RenderingPayload& p)
{
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
    }
    {
        const i32r rect{ 0, 0, p.s->targetSize.x, p.s->targetSize.y };
        _appendQuad(rect, rect, 0, ShadingType::Background);
    }
}

void BackendD3D11::_drawText(RenderingPayload& p)
{
    if (_resetGlyphAtlasNeeded)
    {
        _resetGlyphAtlasAndBeginDraw(p);
        _resetGlyphAtlasNeeded = false;
    }

    u16 y = 0;

    for (auto& row : p.rows)
    {
        const auto baselineY = y * p.d.font.cellSizeDIP.y + p.s->font->baselineInDIP;
        f32 cumulativeAdvance = 0;

        for (const auto& m : row.mappings)
        {
            for (auto x = m.glyphsFrom; x < m.glyphsTo; ++x)
            {
                bool inserted = false;
                auto& entry = _glyphCache.FindOrInsert(m.fontFace.get(), row.glyphIndices[x], inserted);
                if (inserted)
                {
                    _d2dBeginDrawing();

                    if (!_drawGlyph(p, entry, m.fontEmSize))
                    {
                        _d2dEndDrawing();
                        _flushQuads(p);
                        _resetGlyphAtlasAndBeginDraw(p);
                        --x;
                        continue; // retry
                    }
                }

                if (entry.shadingType)
                {
                    const auto l = static_cast<i32>((cumulativeAdvance + row.glyphOffsets[x].advanceOffset) * p.d.font.pixelPerDIP + 0.5f) + entry.offset.x;
                    const auto t = static_cast<i32>((baselineY - row.glyphOffsets[x].ascenderOffset) * p.d.font.pixelPerDIP + 0.5f) + entry.offset.y;
                    const auto w = entry.texcoord.right - entry.texcoord.left;
                    const auto h = entry.texcoord.bottom - entry.texcoord.top;
                    const i32r rect{ l, t, l + w, t + h };
                    row.top = std::min(row.top, rect.top);
                    row.bottom = std::max(row.bottom, rect.bottom);
                    _appendQuad(rect, entry.texcoord, row.colors[x], static_cast<ShadingType>(entry.shadingType));
                }

                cumulativeAdvance += row.glyphAdvances[x];
            }
        }

        ++y;
    }

    _d2dEndDrawing();
}

bool BackendD3D11::_drawGlyph(const RenderingPayload& p, GlyphCacheEntry& entry, f32 fontEmSize)
{
    DWRITE_GLYPH_RUN glyphRun{};
    glyphRun.fontFace = entry.fontFace;
    glyphRun.fontEmSize = fontEmSize;
    glyphRun.glyphCount = 1;
    glyphRun.glyphIndices = &entry.glyphIndex;

    auto box = GetGlyphRunBlackBox(glyphRun, 0, 0);
    if (box.left >= box.right || box.top >= box.bottom)
    {
        entry = {};
        return true;
    }

    box.left *= p.d.font.pixelPerDIP;
    box.top *= p.d.font.pixelPerDIP;
    box.right *= p.d.font.pixelPerDIP;
    box.bottom *= p.d.font.pixelPerDIP;

    // We'll add a 1px padding on all 4 sides, by adding +2px to the width and +1px to the baseline origin.
    // We do this to avoid neighboring glyphs from overlapping, since the blackbox measurement is only an estimate.

    stbrp_rect rect{};
    rect.w = gsl::narrow_cast<int>(box.right - box.left + 2.5f);
    rect.h = gsl::narrow_cast<int>(box.bottom - box.top + 2.5f);
    if (!stbrp_pack_rects(&_rectPacker, &rect, 1))
    {
        return false;
    }

    const D2D1_POINT_2F baseline{
        roundf(rect.x - box.left + 1.0f) * p.d.font.dipPerPixel,
        roundf(rect.y - box.top + 1.0f) * p.d.font.dipPerPixel,
    };
    const auto colorGlyph = DrawGlyphRun(_d2dRenderTarget.get(), _d2dRenderTarget4.get(), p.dwriteFactory4.get(), baseline, &glyphRun, _brush.get());

    entry.shadingType = gsl::narrow_cast<u16>(colorGlyph ? ShadingType::Passthrough : (p.s->font->antialiasingMode == D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE ? ShadingType::TextClearType : ShadingType::TextGrayscale));
    entry.offset.x = gsl::narrow_cast<i32>(lround(box.left));
    entry.offset.y = gsl::narrow_cast<i32>(lround(box.top));
    entry.texcoord.left = rect.x;
    entry.texcoord.top = rect.y;
    entry.texcoord.right = rect.x + rect.w;
    entry.texcoord.bottom = rect.y + rect.h;
    return true;
}

void BackendD3D11::_drawGridlines(const RenderingPayload& p)
{
    u16 y = 0;
    for (const auto& row : p.rows)
    {
        if (!row.gridLineRanges.empty())
        {
            _drawGridlineRow(p, row, y);
        }
        y++;
    }
}

void BackendD3D11::_drawGridlineRow(const RenderingPayload& p, const ShapedRow& row, u16 y)
{
    const auto top = p.s->font->cellSize.y * y;
    const auto bottom = top + p.s->font->cellSize.y;

    for (const auto& r : row.gridLineRanges)
    {
        // AtlasEngine.cpp shouldn't add any gridlines if they don't do anything.
        assert(r.lines.any());

        i32r rect{ r.from * p.s->font->cellSize.x, top, r.to * p.s->font->cellSize.x, bottom };

        if (r.lines.test(GridLines::Left))
        {
            for (auto i = r.from; i < r.to; ++i)
            {
                rect.left = i * p.s->font->cellSize.x;
                rect.right = rect.left + p.s->font->thinLineWidth;
                _appendQuad(rect, r.color, ShadingType::SolidFill);
            }
        }
        if (r.lines.test(GridLines::Top))
        {
            rect.bottom = rect.top + p.s->font->thinLineWidth;
            _appendQuad(rect, r.color, ShadingType::SolidFill);
        }
        if (r.lines.test(GridLines::Right))
        {
            for (auto i = r.to; i > r.from; --i)
            {
                rect.right = i * p.s->font->cellSize.x;
                rect.left = rect.right - p.s->font->thinLineWidth;
                _appendQuad(rect, r.color, ShadingType::SolidFill);
            }
        }
        if (r.lines.test(GridLines::Bottom))
        {
            rect.top = rect.bottom - p.s->font->thinLineWidth;
            _appendQuad(rect, r.color, ShadingType::SolidFill);
        }
        if (r.lines.test(GridLines::Underline))
        {
            rect.top += p.s->font->underlinePos;
            rect.bottom = rect.top + p.s->font->underlineWidth;
            _appendQuad(rect, r.color, ShadingType::SolidFill);
        }
        if (r.lines.test(GridLines::HyperlinkUnderline))
        {
            rect.top += p.s->font->underlinePos;
            rect.bottom = rect.top + p.s->font->underlineWidth;
            _appendQuad(rect, r.color, ShadingType::DashedLine);
        }
        if (r.lines.test(GridLines::DoubleUnderline))
        {
            rect.top = top + p.s->font->doubleUnderlinePos.x;
            rect.bottom = rect.top + p.s->font->thinLineWidth;
            _appendQuad(rect, r.color, ShadingType::SolidFill);

            rect.top = top + p.s->font->doubleUnderlinePos.y;
            rect.bottom = rect.top + p.s->font->thinLineWidth;
            _appendQuad(rect, r.color, ShadingType::SolidFill);
        }
        if (r.lines.test(GridLines::Strikethrough))
        {
            rect.top = top + p.s->font->strikethroughPos;
            rect.bottom = rect.top + p.s->font->strikethroughWidth;
            _appendQuad(rect, r.color, ShadingType::SolidFill);
        }
    }
}

void BackendD3D11::_drawCursorPart1(const RenderingPayload& p)
{
    _cursorRects.clear();
    if (!p.cursorRect)
    {
        return;
    }

    const auto color = p.s->cursor->cursorColor;
    const auto offset = gsl::narrow_cast<size_t>(p.cursorRect.top * p.s->cellCount.x);

    for (auto x1 = p.cursorRect.left; x1 < p.cursorRect.right; ++x1)
    {
        const auto x0 = x1;
        const auto bg = p.backgroundBitmap[offset + x1] | 0xff000000;

        for (; x1 < p.cursorRect.right && (p.backgroundBitmap[offset + x1] | 0xff000000) == bg; ++x1)
        {
        }

        auto& c0 = _cursorRects.emplace_back(CursorRect{
            i32r{
                p.s->font->cellSize.x * x0,
                p.s->font->cellSize.y * p.cursorRect.top,
                p.s->font->cellSize.x * x1,
                p.s->font->cellSize.y * p.cursorRect.bottom,
            },
            color == 0xffffffff ? bg ^ 0x3f3f3f : color,
        });

        switch (static_cast<CursorType>(p.s->cursor->cursorType))
        {
        case CursorType::Legacy:
            c0.rect.top = c0.rect.bottom - ((c0.rect.bottom - c0.rect.top) * p.s->cursor->heightPercentage + 50) / 100;
            break;
        case CursorType::VerticalBar:
            c0.rect.right = c0.rect.left + p.s->font->thinLineWidth;
            break;
        case CursorType::Underscore:
            c0.rect.top += p.s->font->underlinePos;
            c0.rect.bottom = c0.rect.top + p.s->font->underlineWidth;
            break;
        case CursorType::EmptyBox:
        {
            auto& c1 = _cursorRects.emplace_back(c0);
            if (x0 == p.cursorRect.left)
            {
                auto& c = _cursorRects.emplace_back(c0);
                c.rect.top += p.s->font->thinLineWidth;
                c.rect.bottom -= p.s->font->thinLineWidth;
                c.rect.right = c.rect.left + p.s->font->thinLineWidth;
            }
            if (x1 == p.cursorRect.right)
            {
                auto& c = _cursorRects.emplace_back(c0);
                c.rect.top += p.s->font->thinLineWidth;
                c.rect.bottom -= p.s->font->thinLineWidth;
                c.rect.left = c.rect.right - p.s->font->thinLineWidth;
            }
            c0.rect.bottom = c0.rect.top + p.s->font->thinLineWidth;
            c1.rect.top = c1.rect.bottom - p.s->font->thinLineWidth;
            break;
        }
        case CursorType::FullBox:
            break;
        case CursorType::DoubleUnderscore:
        {
            auto& c1 = _cursorRects.emplace_back(c0);
            c0.rect.top += p.s->font->doubleUnderlinePos.x;
            c0.rect.bottom = c0.rect.top + p.s->font->thinLineWidth;
            c1.rect.top += p.s->font->doubleUnderlinePos.y;
            c1.rect.bottom = c1.rect.top + p.s->font->thinLineWidth;
            break;
        }
        default:
            break;
        }
    }

    if (color == 0xffffffff)
    {
        for (auto& c : _cursorRects)
        {
            _appendQuad(c.rect, c.color, ShadingType::SolidFill);
            c.color = 0xffffffff;
        }
    }
}

void BackendD3D11::_drawCursorPart2(const RenderingPayload& p)
{
    if (!p.cursorRect)
    {
        return;
    }

    const auto color = p.s->cursor->cursorColor;

    if (color == 0xffffffff)
    {
        _flushQuads(p);
        _deviceContext->OMSetBlendState(_blendStateInvert.get(), nullptr, 0xffffffff);
    }

    for (const auto& c : _cursorRects)
    {
        _appendQuad(c.rect, c.color, ShadingType::SolidFill);
    }

    if (color == 0xffffffff)
    {
        _flushQuads(p);
        _deviceContext->OMSetBlendState(_blendState.get(), nullptr, 0xffffffff);
    }
}

void BackendD3D11::_drawSelection(const RenderingPayload& p)
{
    u16 y = 0;
    u16 lastFrom = 0;
    u16 lastTo = 0;

    for (const auto& row : p.rows)
    {
        if (row.selectionTo > row.selectionFrom)
        {
            // If the current selection line matches the previous one, we can just extend the previous quad downwards.
            // The way this is implemented isn't very smart, but we also don't have very many rows to iterate through.
            if (row.selectionFrom == lastFrom && row.selectionTo == lastTo)
            {
                _getLastQuad().position.bottom = p.s->font->cellSize.y * (y + 1);
            }
            else
            {
                const i32r rect{
                    p.s->font->cellSize.x * row.selectionFrom,
                    p.s->font->cellSize.y * y,
                    p.s->font->cellSize.x * row.selectionTo,
                    p.s->font->cellSize.y * (y + 1),
                };
                _appendQuad(rect, p.s->misc->selectionColor, ShadingType::SolidFill);
                lastFrom = row.selectionFrom;
                lastTo = row.selectionTo;
            }
        }

        y++;
    }
}

void BackendD3D11::_executeCustomShader(RenderingPayload& p)
{
    {
        CustomConstBuffer data;
        data.time = std::chrono::duration<f32>(std::chrono::steady_clock::now() - _customShaderStartTime).count();
        data.scale = p.d.font.pixelPerDIP;
        data.resolution.x = static_cast<f32>(_cellCount.x * p.s->font->cellSize.x);
        data.resolution.y = static_cast<f32>(_cellCount.y * p.s->font->cellSize.y);
        data.background = colorFromU32<f32x4>(p.s->misc->backgroundColor);

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
        _deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        _deviceContext->IASetIndexBuffer(_indexBuffer.get(), _indicesFormat, 0);

        // VS: Vertex Shader
        _deviceContext->VSSetShader(_customVertexShader.get(), nullptr, 0);
        _deviceContext->VSSetConstantBuffers(0, 0, nullptr);
        _deviceContext->VSSetShaderResources(0, 0, nullptr);

        // RS: Rasterizer Stage
        D3D11_VIEWPORT viewport{};
        viewport.Width = static_cast<f32>(p.s->targetSize.x);
        viewport.Height = static_cast<f32>(p.s->targetSize.y);
        _deviceContext->RSSetViewports(1, &viewport);

        // PS: Pixel Shader
        _deviceContext->PSSetShader(_customPixelShader.get(), nullptr, 0);
        _deviceContext->PSSetConstantBuffers(0, 1, _customShaderConstantBuffer.addressof());
        _deviceContext->PSSetShaderResources(0, 1, _customOffscreenTextureView.addressof());
        _deviceContext->PSSetSamplers(0, 1, _customShaderSamplerState.addressof());

        // OM: Output Merger
        _deviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);

        _deviceContext->Draw(4, 0);
    }

    _setupDeviceContextState(p);

    // With custom shaders, everything might be invalidated, so we have to
    // indirectly disable Present1() and its dirty rects this way.
    p.dirtyRect = { 0, 0, p.s->cellCount.x, p.s->cellCount.y };
}

TIL_FAST_MATH_END
