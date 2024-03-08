// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "BackendD3D.h"

#include <til/unicode.h>

#include <custom_shader_ps.h>
#include <custom_shader_vs.h>
#include <shader_ps.h>
#include <shader_vs.h>

#include <wincodec.h>

#include "BuiltinGlyphs.h"
#include "dwrite.h"
#include "../../types/inc/ColorFix.hpp"

#if ATLAS_DEBUG_SHOW_DIRTY || ATLAS_DEBUG_COLORIZE_GLYPH_ATLAS
#include "colorbrewer.h"
#endif

#if ATLAS_DEBUG_DUMP_RENDER_TARGET
#include "wic.h"
#endif

TIL_FAST_MATH_BEGIN

#pragma warning(disable : 4100) // '...': unreferenced formal parameter
#pragma warning(disable : 26440) // Function '...' can be declared 'noexcept'(f.6).
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

// Initializing large arrays can be very costly compared to how cheap some of these functions are.
#define ALLOW_UNINITIALIZED_BEGIN _Pragma("warning(push)") _Pragma("warning(disable : 26494)")
#define ALLOW_UNINITIALIZED_END _Pragma("warning(pop)")

using namespace Microsoft::Console::Render::Atlas;

static constexpr D2D1_MATRIX_3X2_F identityTransform{ .m11 = 1, .m22 = 1 };
static constexpr D2D1_COLOR_F whiteColor{ 1, 1, 1, 1 };

BackendD3D::BackendD3D(const RenderingPayload& p)
{
    THROW_IF_FAILED(p.device->CreateVertexShader(&shader_vs[0], sizeof(shader_vs), nullptr, _vertexShader.addressof()));
    THROW_IF_FAILED(p.device->CreatePixelShader(&shader_ps[0], sizeof(shader_ps), nullptr, _pixelShader.addressof()));

    {
        static constexpr D3D11_INPUT_ELEMENT_DESC layout[]{
            { "SV_Position", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "shadingType", 0, DXGI_FORMAT_R16_UINT, 1, offsetof(QuadInstance, shadingType), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "renditionScale", 0, DXGI_FORMAT_R8G8_UINT, 1, offsetof(QuadInstance, renditionScale), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "position", 0, DXGI_FORMAT_R16G16_SINT, 1, offsetof(QuadInstance, position), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "size", 0, DXGI_FORMAT_R16G16_UINT, 1, offsetof(QuadInstance, size), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "texcoord", 0, DXGI_FORMAT_R16G16_UINT, 1, offsetof(QuadInstance, texcoord), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "color", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 1, offsetof(QuadInstance, color), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        };
        THROW_IF_FAILED(p.device->CreateInputLayout(&layout[0], std::size(layout), &shader_vs[0], sizeof(shader_vs), _inputLayout.addressof()));
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
        THROW_IF_FAILED(p.device->CreateBuffer(&desc, &initialData, _vertexBuffer.addressof()));
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
        THROW_IF_FAILED(p.device->CreateBuffer(&desc, &initialData, _indexBuffer.addressof()));
    }

    {
        static constexpr D3D11_BUFFER_DESC desc{
            .ByteWidth = sizeof(VSConstBuffer),
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        };
        THROW_IF_FAILED(p.device->CreateBuffer(&desc, nullptr, _vsConstantBuffer.addressof()));
    }

    {
        static constexpr D3D11_BUFFER_DESC desc{
            .ByteWidth = sizeof(PSConstBuffer),
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        };
        THROW_IF_FAILED(p.device->CreateBuffer(&desc, nullptr, _psConstantBuffer.addressof()));
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
        THROW_IF_FAILED(p.device->CreateBlendState(&desc, _blendState.addressof()));
    }

#if ATLAS_DEBUG_SHADER_HOT_RELOAD
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

void BackendD3D::ReleaseResources() noexcept
{
    _renderTargetView.reset();
    _customRenderTargetView.reset();
    // Ensure _handleSettingsUpdate() is called so that _renderTarget gets recreated.
    _generation = {};
}

void BackendD3D::Render(RenderingPayload& p)
{
    if (_generation != p.s.generation())
    {
        _handleSettingsUpdate(p);
    }

    _debugUpdateShaders(p);

    // After a Present() the render target becomes unbound.
    p.deviceContext->OMSetRenderTargets(1, _customRenderTargetView ? _customRenderTargetView.addressof() : _renderTargetView.addressof(), nullptr);

    // Invalidating the render target helps with spotting invalid quad instances and Present1() bugs.
#if ATLAS_DEBUG_SHOW_DIRTY || ATLAS_DEBUG_DUMP_RENDER_TARGET
    {
        static constexpr f32 clearColor[4]{};
        p.deviceContext->ClearView(_renderTargetView.get(), &clearColor[0], nullptr, 0);
    }
#endif

    _drawBackground(p);
    _drawCursorBackground(p);
    _drawText(p);
    _drawSelection(p);
    _debugShowDirty(p);
    _flushQuads(p);

    if (_customPixelShader)
    {
        _executeCustomShader(p);
    }

    _debugDumpRenderTarget(p);
}

bool BackendD3D::RequiresContinuousRedraw() noexcept
{
    return _requiresContinuousRedraw;
}

void BackendD3D::_handleSettingsUpdate(const RenderingPayload& p)
{
    if (!_renderTargetView)
    {
        wil::com_ptr<ID3D11Texture2D> buffer;
        THROW_IF_FAILED(p.swapChain.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(buffer.addressof())));
        THROW_IF_FAILED(p.device->CreateRenderTargetView(buffer.get(), nullptr, _renderTargetView.put()));
    }

    const auto fontChanged = _fontGeneration != p.s->font.generation();
    const auto miscChanged = _miscGeneration != p.s->misc.generation();
    const auto cellCountChanged = _viewportCellCount != p.s->viewportCellCount;

    if (fontChanged)
    {
        _updateFontDependents(p);
    }
    if (miscChanged)
    {
        _recreateCustomShader(p);
    }
    if (cellCountChanged)
    {
        _recreateBackgroundColorBitmap(p);
    }

    // Similar to _renderTargetView above, we might have to recreate the _customRenderTargetView whenever _swapChainManager
    // resets it. We only do it after calling _recreateCustomShader however, since that sets the _customPixelShader.
    if (_customPixelShader && !_customRenderTargetView)
    {
        _recreateCustomRenderTargetView(p);
    }

    _recreateConstBuffer(p);
    _setupDeviceContextState(p);

    _generation = p.s.generation();
    _fontGeneration = p.s->font.generation();
    _miscGeneration = p.s->misc.generation();
    _targetSize = p.s->targetSize;
    _viewportCellCount = p.s->viewportCellCount;
}

void BackendD3D::_updateFontDependents(const RenderingPayload& p)
{
    const auto& font = *p.s->font;

    // Curlyline is drawn with a desired height relative to the font size. The
    // baseline of curlyline is at the middle of singly underline. When there's
    // limited space to draw a curlyline, we apply a limit on the peak height.
    {
        const auto cellHeight = static_cast<f32>(font.cellSize.y);
        const auto duTop = static_cast<f32>(font.doubleUnderline[0].position);
        const auto duBottom = static_cast<f32>(font.doubleUnderline[1].position);
        const auto duHeight = static_cast<f32>(font.doubleUnderline[0].height);

        // This gives it the same position and height as our double-underline. There's no particular reason for that, apart from
        // it being simple to implement and robust against more peculiar fonts with unusually large/small descenders, etc.
        // We still need to ensure though that it doesn't clip out of the cellHeight at the bottom.
        const auto height = std::max(3.0f, duBottom + duHeight - duTop);
        const auto top = std::min(duTop, floorf(cellHeight - height - duHeight));

        _curlyLineHalfHeight = height * 0.5f;
        _curlyUnderline.position = gsl::narrow_cast<u16>(lrintf(top));
        _curlyUnderline.height = gsl::narrow_cast<u16>(lrintf(height));
    }

    DWrite_GetRenderParams(p.dwriteFactory.get(), &_gamma, &_cleartypeEnhancedContrast, &_grayscaleEnhancedContrast, _textRenderingParams.put());
    // Clearing the atlas requires BeginDraw(), which is expensive. Defer this until we need Direct2D anyways.
    _fontChangedResetGlyphAtlas = true;
    _textShadingType = font.antialiasingMode == AntialiasingMode::ClearType ? ShadingType::TextClearType : ShadingType::TextGrayscale;

    // _ligatureOverhangTriggerLeft/Right are essentially thresholds for a glyph's width at
    // which point we consider it wider than allowed and "this looks like a coding ligature".
    // See _drawTextOverlapSplit for more information about what this does.
    {
        // No ligatures -> No thresholds.
        auto ligaturesDisabled = false;
        for (const auto& feature : font.fontFeatures)
        {
            if (feature.nameTag == DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES)
            {
                ligaturesDisabled = !feature.parameter;
                break;
            }
        }

        if (ligaturesDisabled)
        {
            _ligatureOverhangTriggerLeft = til::CoordTypeMin;
            _ligatureOverhangTriggerRight = til::CoordTypeMax;
        }
        else
        {
            const auto halfCellWidth = font.cellSize.x / 2;
            _ligatureOverhangTriggerLeft = -halfCellWidth;
            _ligatureOverhangTriggerRight = font.advanceWidth + halfCellWidth;
        }
    }

    if (_d2dRenderTarget)
    {
        _d2dRenderTargetUpdateFontSettings(p);
    }

    _softFontBitmap.reset();
}

void BackendD3D::_d2dRenderTargetUpdateFontSettings(const RenderingPayload& p) const noexcept
{
    const auto& font = *p.s->font;
    _d2dRenderTarget->SetDpi(font.dpi, font.dpi);
    _d2dRenderTarget->SetTextAntialiasMode(static_cast<D2D1_TEXT_ANTIALIAS_MODE>(font.antialiasingMode));
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

    if (!p.s->misc->customPixelShaderImagePath.empty())
    {
        _customShaderTexture = LoadShaderTextureFromFile(
            p.device.get(),
            p.s->misc->customPixelShaderImagePath);
    }

    if (!p.s->misc->customPixelShaderPath.empty())
    {
        const char* target = nullptr;
        switch (p.device->GetFeatureLevel())
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

        if (SUCCEEDED(hr))
        {
            THROW_IF_FAILED(p.device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, _customPixelShader.addressof()));

            // Try to determine whether the shader uses the Time variable
            wil::com_ptr<ID3D11ShaderReflection> reflector;
            if (SUCCEEDED_LOG(D3DReflect(blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(reflector.addressof()))))
            {
                // Depending on the version of the d3dcompiler_*.dll, the next two functions either return nullptr
                // on failure or an instance of CInvalidSRConstantBuffer or CInvalidSRVariable respectively,
                // which cause GetDesc() to return E_FAIL. In other words, we have to assume that any failure in the
                // next few lines indicates that the cbuffer is entirely unused (--> _requiresContinuousRedraw=false).
                if (ID3D11ShaderReflectionConstantBuffer* constantBufferReflector = reflector->GetConstantBufferByIndex(0)) // shader buffer
                {
                    if (ID3D11ShaderReflectionVariable* variableReflector = constantBufferReflector->GetVariableByIndex(0)) // time
                    {
                        D3D11_SHADER_VARIABLE_DESC variableDescriptor;
                        if (SUCCEEDED(variableReflector->GetDesc(&variableDescriptor)))
                        {
                            // only if time is used
                            _requiresContinuousRedraw = WI_IsFlagSet(variableDescriptor.uFlags, D3D_SVF_USED);
                        }
                    }
                }
            }
            else
            {
                // Unless we can determine otherwise, assume this shader requires evaluation every frame
                _requiresContinuousRedraw = true;
            }
        }
        else
        {
            if (error)
            {
                LOG_HR_MSG(hr, "%.*hs", static_cast<int>(error->GetBufferSize()), static_cast<char*>(error->GetBufferPointer()));
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
        THROW_IF_FAILED(p.device->CreatePixelShader(&custom_shader_ps[0], sizeof(custom_shader_ps), nullptr, _customPixelShader.put()));
    }

    if (_customPixelShader)
    {
        THROW_IF_FAILED(p.device->CreateVertexShader(&custom_shader_vs[0], sizeof(custom_shader_vs), nullptr, _customVertexShader.put()));

        {
            static constexpr D3D11_BUFFER_DESC desc{
                .ByteWidth = sizeof(CustomConstBuffer),
                .Usage = D3D11_USAGE_DYNAMIC,
                .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
                .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
            };
            THROW_IF_FAILED(p.device->CreateBuffer(&desc, nullptr, _customShaderConstantBuffer.put()));
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
            THROW_IF_FAILED(p.device->CreateSamplerState(&desc, _customShaderSamplerState.put()));
        }

        _customShaderStartTime = std::chrono::steady_clock::now();
    }
}

void BackendD3D::_recreateCustomRenderTargetView(const RenderingPayload& p)
{
    // Avoid memory usage spikes by releasing memory first.
    _customOffscreenTexture.reset();
    _customOffscreenTextureView.reset();

    const D3D11_TEXTURE2D_DESC desc{
        .Width = p.s->targetSize.x,
        .Height = p.s->targetSize.y,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc = { 1, 0 },
        .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
    };
    THROW_IF_FAILED(p.device->CreateTexture2D(&desc, nullptr, _customOffscreenTexture.addressof()));
    THROW_IF_FAILED(p.device->CreateShaderResourceView(_customOffscreenTexture.get(), nullptr, _customOffscreenTextureView.addressof()));
    THROW_IF_FAILED(p.device->CreateRenderTargetView(_customOffscreenTexture.get(), nullptr, _customRenderTargetView.addressof()));
}

void BackendD3D::_recreateBackgroundColorBitmap(const RenderingPayload& p)
{
    // Avoid memory usage spikes by releasing memory first.
    _backgroundBitmap.reset();
    _backgroundBitmapView.reset();

    const D3D11_TEXTURE2D_DESC desc{
        .Width = p.s->viewportCellCount.x,
        .Height = p.s->viewportCellCount.y,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = { 1, 0 },
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    THROW_IF_FAILED(p.device->CreateTexture2D(&desc, nullptr, _backgroundBitmap.addressof()));
    THROW_IF_FAILED(p.device->CreateShaderResourceView(_backgroundBitmap.get(), nullptr, _backgroundBitmapView.addressof()));
    _backgroundBitmapGeneration = {};
}

void BackendD3D::_recreateConstBuffer(const RenderingPayload& p) const
{
    {
        VSConstBuffer data{};
        data.positionScale = { 2.0f / p.s->targetSize.x, -2.0f / p.s->targetSize.y };
        p.deviceContext->UpdateSubresource(_vsConstantBuffer.get(), 0, nullptr, &data, 0, 0);
    }
    {
        PSConstBuffer data{};
        data.backgroundColor = colorFromU32Premultiply<f32x4>(p.s->misc->backgroundColor);
        data.backgroundCellSize = { static_cast<f32>(p.s->font->cellSize.x), static_cast<f32>(p.s->font->cellSize.y) };
        data.backgroundCellCount = { static_cast<f32>(p.s->viewportCellCount.x), static_cast<f32>(p.s->viewportCellCount.y) };
        DWrite_GetGammaRatios(_gamma, data.gammaRatios);
        data.enhancedContrast = p.s->font->antialiasingMode == AntialiasingMode::ClearType ? _cleartypeEnhancedContrast : _grayscaleEnhancedContrast;
        data.underlineWidth = p.s->font->underline.height;
        data.doubleUnderlineWidth = p.s->font->doubleUnderline[0].height;
        data.curlyLineHalfHeight = _curlyLineHalfHeight;
        data.shadedGlyphDotSize = std::max(1.0f, std::roundf(std::max(p.s->font->cellSize.x / 16.0f, p.s->font->cellSize.y / 32.0f)));
        p.deviceContext->UpdateSubresource(_psConstantBuffer.get(), 0, nullptr, &data, 0, 0);
    }
}

void BackendD3D::_setupDeviceContextState(const RenderingPayload& p)
{
    // IA: Input Assembler
    ID3D11Buffer* vertexBuffers[]{ _vertexBuffer.get(), _instanceBuffer.get() };
    static constexpr UINT strides[]{ sizeof(f32x2), sizeof(QuadInstance) };
    static constexpr UINT offsets[]{ 0, 0 };
    p.deviceContext->IASetIndexBuffer(_indexBuffer.get(), DXGI_FORMAT_R16_UINT, 0);
    p.deviceContext->IASetInputLayout(_inputLayout.get());
    p.deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    p.deviceContext->IASetVertexBuffers(0, 2, &vertexBuffers[0], &strides[0], &offsets[0]);

    // VS: Vertex Shader
    p.deviceContext->VSSetShader(_vertexShader.get(), nullptr, 0);
    p.deviceContext->VSSetConstantBuffers(0, 1, _vsConstantBuffer.addressof());

    // RS: Rasterizer Stage
    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<f32>(p.s->targetSize.x);
    viewport.Height = static_cast<f32>(p.s->targetSize.y);
    p.deviceContext->RSSetViewports(1, &viewport);

    // PS: Pixel Shader
    ID3D11ShaderResourceView* resources[]{ _backgroundBitmapView.get(), _glyphAtlasView.get() };
    p.deviceContext->PSSetShader(_pixelShader.get(), nullptr, 0);
    p.deviceContext->PSSetConstantBuffers(0, 1, _psConstantBuffer.addressof());
    p.deviceContext->PSSetShaderResources(0, 2, &resources[0]);

    // OM: Output Merger
    p.deviceContext->OMSetBlendState(_blendState.get(), nullptr, 0xffffffff);
    p.deviceContext->OMSetRenderTargets(1, _customRenderTargetView ? _customRenderTargetView.addressof() : _renderTargetView.addressof(), nullptr);
}

void BackendD3D::_debugUpdateShaders(const RenderingPayload& p) noexcept
{
#if ATLAS_DEBUG_SHADER_HOT_RELOAD
    try
    {
        const auto invalidationTime = _sourceCodeInvalidationTime.load(std::memory_order_relaxed);

        if (invalidationTime == INT64_MAX || invalidationTime > std::chrono::steady_clock::now().time_since_epoch().count())
        {
            return;
        }

        _sourceCodeInvalidationTime.store(INT64_MAX, std::memory_order_relaxed);

        static constexpr auto flags =
            D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS
#ifndef NDEBUG
            | D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION
#endif
            ;

        static const auto compile = [](const std::filesystem::path& path, const char* target) {
            wil::com_ptr<ID3DBlob> error;
            wil::com_ptr<ID3DBlob> blob;
            const auto hr = D3DCompileFromFile(
                /* pFileName   */ path.c_str(),
                /* pDefines    */ nullptr,
                /* pInclude    */ D3D_COMPILE_STANDARD_FILE_INCLUDE,
                /* pEntrypoint */ "main",
                /* pTarget     */ target,
                /* Flags1      */ flags,
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
            THROW_IF_FAILED(p.device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, compiledVS[i].addressof()));
        }
        for (size_t i = 0; i < filesPS.size(); ++i)
        {
            const auto blob = compile(_sourceDirectory / filesPS[i].filename, "ps_4_0");
            THROW_IF_FAILED(p.device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, compiledPS[i].addressof()));
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
}

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

void BackendD3D::_resetGlyphAtlas(const RenderingPayload& p)
{
    // The index returned by _BitScanReverse is undefined when the input is 0. We can simultaneously guard
    // against that and avoid unreasonably small textures, by clamping the min. texture size to `minArea`.
    // `minArea` results in a 64kB RGBA texture which is the min. alignment for placed memory.
    static constexpr u32 minArea = 128 * 128;
    static constexpr u32 maxArea = D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION * D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;

    const auto cellArea = static_cast<u32>(p.s->font->cellSize.x) * p.s->font->cellSize.y;
    const auto targetArea = static_cast<u32>(p.s->targetSize.x) * p.s->targetSize.y;

    const auto minAreaByFont = cellArea * 95; // Covers all printable ASCII characters
    const auto minAreaByGrowth = static_cast<u32>(_rectPacker.width) * _rectPacker.height * 2;

    // It's hard to say what the max. size of the cache should be. Optimally I think we should use as much
    // memory as is available, but the rendering code in this project is a big mess and so integrating
    // memory pressure feedback (RegisterVideoMemoryBudgetChangeNotificationEvent) is rather difficult.
    // As an alternative I'm using 1.25x the size of the swap chain. The 1.25x is there to avoid situations, where
    // we're locked into a state, where on every render pass we're starting with a half full atlas, drawing once,
    // filling it with the remaining half and drawing again, requiring two rendering passes on each frame.
    const auto maxAreaByFont = targetArea + targetArea / 4;

    auto area = std::min(maxAreaByFont, std::max(minAreaByFont, minAreaByGrowth));
    area = clamp(area, minArea, maxArea);

    // This block of code calculates the size of a power-of-2 texture that has an area larger than the given `area`.
    // For instance, for an area of 985x1946 = 1916810 it would result in a u/v of 2048x1024 (area = 2097152).
    // This has 2 benefits: GPUs like power-of-2 textures and it ensures that we don't resize the texture
    // every time you resize the window by a pixel. Instead it only grows/shrinks by a factor of 2.
    unsigned long index;
    _BitScanReverse(&index, area - 1);
    const auto u = static_cast<u16>(1u << ((index + 2) / 2));
    const auto v = static_cast<u16>(1u << ((index + 1) / 2));

    if (u != _rectPacker.width || v != _rectPacker.height)
    {
        _resizeGlyphAtlas(p, u, v);
    }

    stbrp_init_target(&_rectPacker, u, v, _rectPackerData.data(), _rectPackerData.size());

    // This is a little imperfect, because it only releases the memory of the glyph mappings, not the memory held by
    // any DirectWrite fonts. On the other side, the amount of fonts on a system is always finite, where "finite"
    // is pretty low, relatively speaking. Additionally this allows us to cache the boxGlyphs map indefinitely.
    // It's not great, but it's not terrible.
    for (auto& slot : _glyphAtlasMap.container())
    {
        for (auto& glyphs : slot.glyphs)
        {
            glyphs.clear();
        }
    }
    for (auto& glyphs : _builtinGlyphs.glyphs)
    {
        glyphs.clear();
    }

    _d2dBeginDrawing();
    _d2dRenderTarget->Clear();

    _fontChangedResetGlyphAtlas = false;
}

void BackendD3D::_resizeGlyphAtlas(const RenderingPayload& p, const u16 u, const u16 v)
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
        THROW_IF_FAILED(p.device->CreateTexture2D(&desc, nullptr, _glyphAtlas.addressof()));
        THROW_IF_FAILED(p.device->CreateShaderResourceView(_glyphAtlas.get(), nullptr, _glyphAtlasView.addressof()));
    }

    {
        const auto surface = _glyphAtlas.query<IDXGISurface>();

        static constexpr D2D1_RENDER_TARGET_PROPERTIES props{
            .type = D2D1_RENDER_TARGET_TYPE_DEFAULT,
            .pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED },
        };
        // ID2D1RenderTarget and ID2D1DeviceContext are the same and I'm tired of pretending they're not.
        THROW_IF_FAILED(p.d2dFactory->CreateDxgiSurfaceRenderTarget(surface.get(), &props, reinterpret_cast<ID2D1RenderTarget**>(_d2dRenderTarget.addressof())));
        _d2dRenderTarget.try_query_to(_d2dRenderTarget4.addressof());

        _d2dRenderTarget->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
        // Ensure that D2D uses the exact same gamma as our shader uses.
        _d2dRenderTarget->SetTextRenderingParams(_textRenderingParams.get());

        _d2dRenderTargetUpdateFontSettings(p);
    }

    // We have our own glyph cache so Direct2D's cache doesn't help much.
    // This saves us 1MB of RAM, which is not much, but also not nothing.
    if (_d2dRenderTarget4)
    {
        wil::com_ptr<ID2D1Device> device;
        _d2dRenderTarget4->GetDevice(device.addressof());

        device->SetMaximumTextureMemory(0);
        if (const auto device4 = device.try_query<ID2D1Device4>())
        {
            device4->SetMaximumColorGlyphCacheMemory(0);
        }
    }

    {
        THROW_IF_FAILED(_d2dRenderTarget->CreateSolidColorBrush(&whiteColor, nullptr, _emojiBrush.put()));
        THROW_IF_FAILED(_d2dRenderTarget->CreateSolidColorBrush(&whiteColor, nullptr, _brush.put()));
    }

    ID3D11ShaderResourceView* resources[]{ _backgroundBitmapView.get(), _glyphAtlasView.get() };
    p.deviceContext->PSSetShaderResources(0, 2, &resources[0]);

    _rectPackerData = Buffer<stbrp_node>{ u };
}

BackendD3D::QuadInstance& BackendD3D::_getLastQuad() noexcept
{
    assert(_instancesCount != 0);
    return _instances[_instancesCount - 1];
}

// NOTE: Up to 5M calls per second -> no std::vector, no std::unordered_map.
// This function is an easy >100x faster than std::vector, can be
// inlined and reduces overall (!) renderer CPU usage by 5%.
BackendD3D::QuadInstance& BackendD3D::_appendQuad()
{
    if (_instancesCount >= _instances.size())
    {
        _bumpInstancesSize();
    }

    return _instances[_instancesCount++];
}

void BackendD3D::_bumpInstancesSize()
{
    auto newSize = std::max(_instancesCount, _instances.size() * 2);
    newSize = std::max(size_t{ 256 }, newSize);
    Expects(newSize > _instances.size());

    // Our render loop heavily relies on memcpy() which is up to between 1.5x (Intel)
    // and 40x (AMD) faster for allocations with an alignment of 32 or greater.
    auto newInstances = Buffer<QuadInstance, 32>{ newSize };
    std::copy_n(_instances.data(), _instances.size(), newInstances.data());

    _instances = std::move(newInstances);
}

void BackendD3D::_flushQuads(const RenderingPayload& p)
{
    if (!_instancesCount)
    {
        return;
    }

    if (!_cursorRects.empty())
    {
        _drawCursorForeground();
    }

    // TODO: Shrink instances buffer
    if (_instancesCount > _instanceBufferCapacity)
    {
        _recreateInstanceBuffers(p);
    }

    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        THROW_IF_FAILED(p.deviceContext->Map(_instanceBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
        memcpy(mapped.pData, _instances.data(), _instancesCount * sizeof(QuadInstance));
        p.deviceContext->Unmap(_instanceBuffer.get(), 0);
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

    p.deviceContext->DrawIndexedInstanced(6, static_cast<UINT>(_instancesCount), 0, 0, 0);
    _instancesCount = 0;
}

void BackendD3D::_recreateInstanceBuffers(const RenderingPayload& p)
{
    // We use the viewport size of the terminal as the initial estimate for the amount of instances we'll see.
    const auto minCapacity = static_cast<size_t>(p.s->viewportCellCount.x) * p.s->viewportCellCount.y;
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
        THROW_IF_FAILED(p.device->CreateBuffer(&desc, nullptr, _instanceBuffer.addressof()));
    }

    // IA: Input Assembler
    ID3D11Buffer* vertexBuffers[]{ _vertexBuffer.get(), _instanceBuffer.get() };
    static constexpr UINT strides[]{ sizeof(f32x2), sizeof(QuadInstance) };
    static constexpr UINT offsets[]{ 0, 0 };
    p.deviceContext->IASetVertexBuffers(0, 2, &vertexBuffers[0], &strides[0], &offsets[0]);

    _instanceBufferCapacity = newCapacity;
}

void BackendD3D::_drawBackground(const RenderingPayload& p)
{
    // Not uploading the bitmap halves (!) the GPU load for any given frame on 2023 hardware.
    if (_backgroundBitmapGeneration != p.colorBitmapGenerations[0])
    {
        _uploadBackgroundBitmap(p);
    }

    _appendQuad() = {
        .shadingType = static_cast<u16>(ShadingType::Background),
        .size = p.s->targetSize,
    };
}

void BackendD3D::_uploadBackgroundBitmap(const RenderingPayload& p)
{
    D3D11_MAPPED_SUBRESOURCE mapped{};
    THROW_IF_FAILED(p.deviceContext->Map(_backgroundBitmap.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));

    auto src = std::bit_cast<const char*>(p.backgroundBitmap.data());
    const auto srcEnd = std::bit_cast<const char*>(p.backgroundBitmap.data() + p.backgroundBitmap.size());
    const auto srcStride = p.colorBitmapRowStride * sizeof(u32);
    auto dst = static_cast<char*>(mapped.pData);

    while (src < srcEnd)
    {
        memcpy(dst, src, srcStride);
        src += srcStride;
        dst += mapped.RowPitch;
    }

    p.deviceContext->Unmap(_backgroundBitmap.get(), 0);
    _backgroundBitmapGeneration = p.colorBitmapGenerations[0];
}

void BackendD3D::_drawText(RenderingPayload& p)
{
    if (_fontChangedResetGlyphAtlas)
    {
        _resetGlyphAtlas(p);
    }

    til::CoordType dirtyTop = til::CoordTypeMax;
    til::CoordType dirtyBottom = til::CoordTypeMin;

    u16 y = 0;
    for (const auto row : p.rows)
    {
        f32 baselineX = 0;
        f32 baselineY = y * p.s->font->cellSize.y + p.s->font->baseline;
        f32 scaleX = 1;
        f32 scaleY = 1;

        if (row->lineRendition != LineRendition::SingleWidth)
        {
            scaleX = 2;

            if (row->lineRendition >= LineRendition::DoubleHeightTop)
            {
                scaleY = 2;
                baselineY /= 2;
            }
        }

        const u8x2 renditionScale{
            static_cast<u8>(row->lineRendition != LineRendition::SingleWidth ? 2 : 1),
            static_cast<u8>(row->lineRendition >= LineRendition::DoubleHeightTop ? 2 : 1),
        };

        for (const auto& m : row->mappings)
        {
            auto x = m.glyphsFrom;
            const auto glyphsTo = m.glyphsTo;
            const auto fontFace = m.fontFace.get();

            // The lack of a fontFace indicates a soft font.
            AtlasFontFaceEntry* fontFaceEntry = &_builtinGlyphs;
            if (fontFace) [[likely]]
            {
                fontFaceEntry = _glyphAtlasMap.insert(fontFace).first;
            }

            const auto& glyphs = fontFaceEntry->glyphs[WI_EnumValue(row->lineRendition)];

            while (x < glyphsTo)
            {
                size_t dx = 1;
                u32 glyphIndex = row->glyphIndices[x];

                // Note: !fontFace is only nullptr for builtin glyphs which then use glyphIndices for UTF16 code points.
                // In other words, this doesn't accidentally corrupt any actual glyph indices.
                if (!fontFace && til::is_leading_surrogate(glyphIndex))
                {
                    glyphIndex = til::combine_surrogates(glyphIndex, row->glyphIndices[x + 1]);
                    dx = 2;
                }

                auto glyphEntry = glyphs.lookup(glyphIndex);
                if (!glyphEntry)
                {
                    glyphEntry = _drawGlyph(p, *row, *fontFaceEntry, glyphIndex);
                }

                // A shadingType of 0 (ShadingType::Default) indicates a glyph that is whitespace.
                if (glyphEntry->shadingType != ShadingType::Default)
                {
                    auto l = static_cast<til::CoordType>(lrintf((baselineX + row->glyphOffsets[x].advanceOffset) * scaleX));
                    auto t = static_cast<til::CoordType>(lrintf((baselineY - row->glyphOffsets[x].ascenderOffset) * scaleY));

                    l += glyphEntry->offset.x;
                    t += glyphEntry->offset.y;

                    row->dirtyTop = std::min(row->dirtyTop, t);
                    row->dirtyBottom = std::max(row->dirtyBottom, t + glyphEntry->size.y);

                    _appendQuad() = {
                        .shadingType = static_cast<u16>(glyphEntry->shadingType),
                        .renditionScale = renditionScale,
                        .position = { static_cast<i16>(l), static_cast<i16>(t) },
                        .size = glyphEntry->size,
                        .texcoord = glyphEntry->texcoord,
                        .color = row->colors[x],
                    };

                    if (glyphEntry->overlapSplit)
                    {
                        _drawTextOverlapSplit(p, y);
                    }
                }

                baselineX += row->glyphAdvances[x];
                x += dx;
            }
        }

        if (!row->gridLineRanges.empty())
        {
            _drawGridlines(p, y);
        }

        if (p.invalidatedRows.contains(y))
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

// There are a number of coding-oriented fonts that feature ligatures which (for instance)
// translate text like "!=" into a glyph that looks like "≠" (just 2 columns wide and not 1).
// Glyphs like that still need to be colored in potentially multiple colors however, so this
// function will handle these ligatures by splitting them up into multiple QuadInstances.
//
// It works by iteratively splitting the wide glyph into shorter and shorter segments like so
// (whitespaces indicate that the glyph was split up in a leading and trailing half):
//   <!--
//   < !--
//   < ! --
//   < ! - -
void BackendD3D::_drawTextOverlapSplit(const RenderingPayload& p, u16 y)
{
    auto& originalQuad = _getLastQuad();

    // If the current row has a non-default line rendition then every glyph is scaled up by 2x horizontally.
    // This requires us to make some changes: For instance, if the ligature occupies columns 3, 4 and 5 (0-indexed)
    // then we need to get the foreground colors from columns 2 and 4, because columns 0,1 2,3 4,5 6,7 and so on form pairs.
    // A wide glyph would be a total of 4 actual columns wide! In other words, we need to properly round our clip rects and columns.
    i32 columnAdvance = 1;
    i32 columnAdvanceInPx = p.s->font->cellSize.x;
    i32 cellCount = p.s->viewportCellCount.x;

    if (p.rows[y]->lineRendition != LineRendition::SingleWidth)
    {
        columnAdvance = 2;
        columnAdvanceInPx <<= 1;
        cellCount >>= 1;
    }

    i32 originalLeft = originalQuad.position.x;
    i32 originalRight = originalQuad.position.x + originalQuad.size.x;
    originalLeft = std::max(originalLeft, 0);
    originalRight = std::min(originalRight, cellCount * columnAdvanceInPx);

    if (originalLeft >= originalRight)
    {
        return;
    }

    const auto colors = &p.foregroundBitmap[p.colorBitmapRowStride * y];

    // As explained in the beginning, column and clipLeft should be in multiples of columnAdvance
    // and columnAdvanceInPx respectively, because that's how line renditions work.
    auto column = originalLeft / columnAdvanceInPx;
    auto clipLeft = column * columnAdvanceInPx;
    column *= columnAdvance;

    // Our loop below will split the ligature by processing it from left to right.
    // Some fonts however implement ligatures by replacing a string like "&&" with a whitespace padding glyph,
    // followed by the actual "&&" glyph which has a 1 column advance width. In that case the originalQuad
    // will have the .color of the 2nd column and not of the 1st one. We need to fix that here.
    auto lastFg = colors[column];
    originalQuad.color = lastFg;
    column += columnAdvance;
    clipLeft += columnAdvanceInPx;

    // We must ensure to exit the loop while `column` is less than `cellCount.x`,
    // otherwise we cause a potential out of bounds access into foregroundBitmap.
    // This may happen with glyphs that are severely overlapping their cells,
    // outside of the viewport. The `clipLeft < originalRight` condition doubles
    // as a `column < cellCount.x` condition thanks to us std::min()ing it above.
    for (; clipLeft < originalRight; column += columnAdvance, clipLeft += columnAdvanceInPx)
    {
        const auto fg = colors[column];

        if (lastFg != fg)
        {
            // NOTE: _appendQuad might reallocate and any pointers
            // acquired before calling this function are now invalid.
            auto& next = _appendQuad();
            // The item at -1 is the quad we've just appended, which means
            // that the previous quad we want to split up is at -2.
            auto& prev = _instances[_instancesCount - 2];

            const auto prevWidth = clipLeft - prev.position.x;
            const auto nextWidth = prev.size.x - prevWidth;

            prev.size.x = gsl::narrow<u16>(prevWidth);

            next = prev;
            next.position.x = gsl::narrow<i16>(next.position.x + prevWidth);
            next.texcoord.x = gsl::narrow<u16>(next.texcoord.x + prevWidth);
            next.size.x = gsl::narrow<u16>(nextWidth);
            next.color = fg;

            lastFg = fg;
        }
    }
}

BackendD3D::AtlasGlyphEntry* BackendD3D::_drawGlyph(const RenderingPayload& p, const ShapedRow& row, AtlasFontFaceEntry& fontFaceEntry, u32 glyphIndex)
{
    // The lack of a fontFace indicates a soft font.
    if (!fontFaceEntry.fontFace)
    {
        return _drawBuiltinGlyph(p, row, fontFaceEntry, glyphIndex);
    }

    const auto glyphIndexU16 = static_cast<u16>(glyphIndex);
    const DWRITE_GLYPH_RUN glyphRun{
        .fontFace = fontFaceEntry.fontFace.get(),
        .fontEmSize = p.s->font->fontSize,
        .glyphCount = 1,
        .glyphIndices = &glyphIndexU16,
    };

    // It took me a while to figure out how to rasterize glyphs manually with DirectWrite without depending on Direct2D.
    // The benefits are a reduction in memory usage, an increase in performance under certain circumstances and most
    // importantly, the ability to debug the renderer more easily, because many graphics debuggers don't support Direct2D.
    // Direct2D has one big advantage however: It features a clever glyph uploader with a pool of D3D11_USAGE_STAGING textures,
    // which I was too short on time to implement myself. This makes rasterization with Direct2D roughly 2x faster.
    //
    // This code remains, because it features some parts that are slightly more and some parts that are outright difficult to figure out.
#if 0
    const auto wantsClearType = p.s->font->antialiasingMode == AntialiasingMode::ClearType;
    const auto wantsAliased = p.s->font->antialiasingMode == AntialiasingMode::Aliased;
    const auto antialiasMode = wantsClearType ? DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE : DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE;
    const auto outlineThreshold = wantsAliased ? DWRITE_OUTLINE_THRESHOLD_ALIASED : DWRITE_OUTLINE_THRESHOLD_ANTIALIASED;

    DWRITE_RENDERING_MODE renderingMode{};
    DWRITE_GRID_FIT_MODE gridFitMode{};
    THROW_IF_FAILED(fontFaceEntry.fontFace->GetRecommendedRenderingMode(
        /* fontEmSize       */ fontEmSize,
        /* dpiX             */ 1, // fontEmSize is already in pixel
        /* dpiY             */ 1, // fontEmSize is already in pixel
        /* transform        */ nullptr,
        /* isSideways       */ FALSE,
        /* outlineThreshold */ outlineThreshold,
        /* measuringMode    */ DWRITE_MEASURING_MODE_NATURAL,
        /* renderingParams  */ _textRenderingParams.get(),
        /* renderingMode    */ &renderingMode,
        /* gridFitMode      */ &gridFitMode));

    wil::com_ptr<IDWriteGlyphRunAnalysis> glyphRunAnalysis;
    THROW_IF_FAILED(p.dwriteFactory->CreateGlyphRunAnalysis(
        /* glyphRun         */ &glyphRun,
        /* transform        */ nullptr,
        /* renderingMode    */ renderingMode,
        /* measuringMode    */ DWRITE_MEASURING_MODE_NATURAL,
        /* gridFitMode      */ gridFitMode,
        /* antialiasMode    */ antialiasMode,
        /* baselineOriginX  */ 0,
        /* baselineOriginY  */ 0,
        /* glyphRunAnalysis */ glyphRunAnalysis.put()));

    RECT textureBounds{};

    if (wantsClearType)
    {
        THROW_IF_FAILED(glyphRunAnalysis->GetAlphaTextureBounds(DWRITE_TEXTURE_CLEARTYPE_3x1, &textureBounds));

        // Some glyphs cannot be drawn with ClearType, such as bitmap fonts. In that case
        // GetAlphaTextureBounds() supposedly returns an empty RECT, but I haven't tested that yet.
        if (!IsRectEmpty(&textureBounds))
        {
            // Allocate a buffer of `3 * width * height` bytes.
            THROW_IF_FAILED(glyphRunAnalysis->CreateAlphaTexture(DWRITE_TEXTURE_CLEARTYPE_3x1, &textureBounds, buffer.data(), buffer.size()));
            // The buffer contains RGB ClearType weights which can now be packed into RGBA and uploaded to the glyph atlas.
            return;
        }

        // --> Retry with grayscale AA.
    }

    // Even though it says "ALIASED" and even though the docs for the flag still say:
    // > [...] that is, each pixel is either fully opaque or fully transparent [...]
    // don't be confused: It's grayscale antialiased. lol
    THROW_IF_FAILED(glyphRunAnalysis->GetAlphaTextureBounds(DWRITE_TEXTURE_ALIASED_1x1, &textureBounds));

    // Allocate a buffer of `width * height` bytes.
    THROW_IF_FAILED(glyphRunAnalysis->CreateAlphaTexture(DWRITE_TEXTURE_ALIASED_1x1, &textureBounds, buffer.data(), buffer.size()));
    // The buffer now contains a grayscale alpha mask.
#endif

    const int scale = row.lineRendition != LineRendition::SingleWidth;
    D2D1_MATRIX_3X2_F transform = identityTransform;

    if (scale)
    {
        transform.m11 = 2.0f;
        transform.m22 = row.lineRendition >= LineRendition::DoubleHeightTop ? 2.0f : 1.0f;
        _d2dRenderTarget->SetTransform(&transform);
    }

    const auto restoreTransform = wil::scope_exit([&]() noexcept {
        _d2dRenderTarget->SetTransform(&identityTransform);
    });

    // This calculates the black box of the glyph, or in other words,
    // it's extents/size relative to its baseline origin (at 0,0).
    //
    // bounds.top ------++-----######--+
    //   (-7)           ||  ############
    //                  ||####      ####
    //                  |###       #####
    //  baseline ______ |###      #####|
    //   origin        \|############# |
    //  (= 0,0)         \|###########  |
    //                  ++-------###---+
    //                  ##      ###    |
    // bounds.bottom ---+#########-----+
    //    (+2)          |              |
    //             bounds.left     bounds.right
    //                 (-1)           (+14)
    //

    bool isColorGlyph = false;
    D2D1_RECT_F bounds = GlyphRunEmptyBounds;

    const auto antialiasingCleanup = wil::scope_exit([&]() {
        if (isColorGlyph)
        {
            _d2dRenderTarget4->SetTextAntialiasMode(static_cast<D2D1_TEXT_ANTIALIAS_MODE>(p.s->font->antialiasingMode));
        }
    });

    {
        const auto enumerator = TranslateColorGlyphRun(p.dwriteFactory4.get(), {}, &glyphRun);

        if (!enumerator)
        {
            THROW_IF_FAILED(_d2dRenderTarget->GetGlyphRunWorldBounds({}, &glyphRun, DWRITE_MEASURING_MODE_NATURAL, &bounds));
        }
        else
        {
            isColorGlyph = true;
            _d2dRenderTarget4->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

            while (ColorGlyphRunMoveNext(enumerator.get()))
            {
                const auto colorGlyphRun = ColorGlyphRunGetCurrentRun(enumerator.get());
                ColorGlyphRunAccumulateBounds(_d2dRenderTarget.get(), colorGlyphRun, bounds);
            }
        }
    }

    // The bounds may be empty if the glyph is whitespace.
    if (bounds.left >= bounds.right || bounds.top >= bounds.bottom)
    {
        return _drawGlyphAllocateEntry(row, fontFaceEntry, glyphIndex);
    }

    const auto bl = lrintf(bounds.left);
    const auto bt = lrintf(bounds.top);
    const auto br = lrintf(bounds.right);
    const auto bb = lrintf(bounds.bottom);

    stbrp_rect rect{
        .w = br - bl,
        .h = bb - bt,
    };
    _drawGlyphAtlasAllocate(p, rect);
    _d2dBeginDrawing();

    const D2D1_POINT_2F baselineOrigin{
        static_cast<f32>(rect.x - bl),
        static_cast<f32>(rect.y - bt),
    };

    if (scale)
    {
        transform.dx = (1.0f - transform.m11) * baselineOrigin.x;
        transform.dy = (1.0f - transform.m22) * baselineOrigin.y;
        _d2dRenderTarget->SetTransform(&transform);
    }

    if (!isColorGlyph)
    {
        _d2dRenderTarget->DrawGlyphRun(baselineOrigin, &glyphRun, _brush.get(), DWRITE_MEASURING_MODE_NATURAL);
    }
    else
    {
        const auto enumerator = TranslateColorGlyphRun(p.dwriteFactory4.get(), baselineOrigin, &glyphRun);
        while (ColorGlyphRunMoveNext(enumerator.get()))
        {
            const auto colorGlyphRun = ColorGlyphRunGetCurrentRun(enumerator.get());
            ColorGlyphRunDraw(_d2dRenderTarget4.get(), _emojiBrush.get(), _brush.get(), colorGlyphRun);
        }
    }

    // Ligatures are drawn with strict cell-wise foreground color, while other text allows colors to overhang
    // their cells. This makes sure that italics and such retain their color and don't look "cut off".
    //
    // The former condition makes sure to exclude diacritics and such from being considered a ligature,
    // while the latter condition-pair makes sure to exclude regular BMP wide glyphs that overlap a little.
    const auto triggerLeft = _ligatureOverhangTriggerLeft << scale;
    const auto triggerRight = _ligatureOverhangTriggerRight << scale;
    const auto overlapSplit = rect.w >= p.s->font->cellSize.x && (bl <= triggerLeft || br >= triggerRight);

    const auto glyphEntry = _drawGlyphAllocateEntry(row, fontFaceEntry, glyphIndex);
    glyphEntry->shadingType = isColorGlyph ? ShadingType::TextPassthrough : _textShadingType;
    glyphEntry->overlapSplit = overlapSplit;
    glyphEntry->offset.x = bl;
    glyphEntry->offset.y = bt;
    glyphEntry->size.x = rect.w;
    glyphEntry->size.y = rect.h;
    glyphEntry->texcoord.x = rect.x;
    glyphEntry->texcoord.y = rect.y;

    if (row.lineRendition >= LineRendition::DoubleHeightTop)
    {
        _splitDoubleHeightGlyph(p, row, fontFaceEntry, glyphEntry);
    }

    return glyphEntry;
}

BackendD3D::AtlasGlyphEntry* BackendD3D::_drawBuiltinGlyph(const RenderingPayload& p, const ShapedRow& row, AtlasFontFaceEntry& fontFaceEntry, u32 glyphIndex)
{
    auto baseline = p.s->font->baseline;
    stbrp_rect rect{
        .w = p.s->font->cellSize.x,
        .h = p.s->font->cellSize.y,
    };
    if (row.lineRendition != LineRendition::SingleWidth)
    {
        const auto heightShift = static_cast<u8>(row.lineRendition >= LineRendition::DoubleHeightTop);
        rect.w <<= 1;
        rect.h <<= heightShift;
        baseline <<= heightShift;
    }

    _drawGlyphAtlasAllocate(p, rect);
    _d2dBeginDrawing();

    auto shadingType = ShadingType::TextGrayscale;

    if (BuiltinGlyphs::IsSoftFontChar(glyphIndex))
    {
        _drawSoftFontGlyph(p, rect, glyphIndex);
    }
    else
    {
        const D2D1_RECT_F r{
            static_cast<f32>(rect.x),
            static_cast<f32>(rect.y),
            static_cast<f32>(rect.x + rect.w),
            static_cast<f32>(rect.y + rect.h),
        };
        BuiltinGlyphs::DrawBuiltinGlyph(p.d2dFactory.get(), _d2dRenderTarget.get(), _brush.get(), r, glyphIndex);
        shadingType = ShadingType::TextBuiltinGlyph;
    }

    const auto glyphEntry = _drawGlyphAllocateEntry(row, fontFaceEntry, glyphIndex);
    glyphEntry->shadingType = shadingType;
    glyphEntry->overlapSplit = 0;
    glyphEntry->offset.x = 0;
    glyphEntry->offset.y = -baseline;
    glyphEntry->size.x = rect.w;
    glyphEntry->size.y = rect.h;
    glyphEntry->texcoord.x = rect.x;
    glyphEntry->texcoord.y = rect.y;

    if (row.lineRendition >= LineRendition::DoubleHeightTop)
    {
        _splitDoubleHeightGlyph(p, row, fontFaceEntry, glyphEntry);
    }

    return glyphEntry;
}

void BackendD3D::_drawSoftFontGlyph(const RenderingPayload& p, const stbrp_rect& rect, u32 glyphIndex)
{
    if (!_softFontBitmap)
    {
        // Allocating such a tiny texture is very wasteful (min. texture size on GPUs
        // right now is 64kB), but this is a seldomly used feature so it's fine...
        const D2D1_SIZE_U size{
            static_cast<UINT32>(p.s->font->softFontCellSize.width),
            static_cast<UINT32>(p.s->font->softFontCellSize.height),
        };
        const D2D1_BITMAP_PROPERTIES1 bitmapProperties{
            .pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED },
            .dpiX = static_cast<f32>(p.s->font->dpi),
            .dpiY = static_cast<f32>(p.s->font->dpi),
        };
        THROW_IF_FAILED(_d2dRenderTarget->CreateBitmap(size, nullptr, 0, &bitmapProperties, _softFontBitmap.addressof()));
    }

    {
        const auto width = static_cast<size_t>(p.s->font->softFontCellSize.width);
        const auto height = static_cast<size_t>(p.s->font->softFontCellSize.height);

        auto bitmapData = Buffer<u32>{ width * height };
        const auto softFontIndex = glyphIndex - 0xEF20u;
        auto src = p.s->font->softFontPattern.begin() + height * softFontIndex;
        auto dst = bitmapData.begin();

        for (size_t y = 0; y < height; y++)
        {
            auto srcBits = *src++;
            for (size_t x = 0; x < width; x++)
            {
                const auto srcBitIsSet = (srcBits & 0x8000) != 0;
                *dst++ = srcBitIsSet ? 0xffffffff : 0x00000000;
                srcBits <<= 1;
            }
        }

        const auto pitch = static_cast<UINT32>(width * sizeof(u32));
        THROW_IF_FAILED(_softFontBitmap->CopyFromMemory(nullptr, bitmapData.data(), pitch));
    }

    const auto interpolation = p.s->font->antialiasingMode == AntialiasingMode::Aliased ? D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR : D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC;
    const D2D1_RECT_F dest{
        static_cast<f32>(rect.x),
        static_cast<f32>(rect.y),
        static_cast<f32>(rect.x + rect.w),
        static_cast<f32>(rect.y + rect.h),
    };

    _d2dBeginDrawing();
    _d2dRenderTarget->DrawBitmap(_softFontBitmap.get(), &dest, 1, interpolation, nullptr, nullptr);
}

void BackendD3D::_drawGlyphAtlasAllocate(const RenderingPayload& p, stbrp_rect& rect)
{
    if (stbrp_pack_rects(&_rectPacker, &rect, 1))
    {
        return;
    }

    _d2dEndDrawing();
    _flushQuads(p);
    _resetGlyphAtlas(p);

    if (!stbrp_pack_rects(&_rectPacker, &rect, 1))
    {
        THROW_HR(HRESULT_FROM_WIN32(ERROR_POSSIBLE_DEADLOCK));
    }
}

BackendD3D::AtlasGlyphEntry* BackendD3D::_drawGlyphAllocateEntry(const ShapedRow& row, AtlasFontFaceEntry& fontFaceEntry, u32 glyphIndex)
{
    const auto glyphEntry = fontFaceEntry.glyphs[WI_EnumValue(row.lineRendition)].insert(glyphIndex).first;
    glyphEntry->shadingType = ShadingType::Default;
    return glyphEntry;
}

// If this is a double-height glyph (DECDHL), we need to split it into 2 glyph entries:
// One for the top/bottom half each, because that's how DECDHL works. This will clip the
// `glyphEntry` to only contain the one specified by `fontFaceEntry.lineRendition`
// and create a second entry in our glyph cache hashmap that contains the other half.
void BackendD3D::_splitDoubleHeightGlyph(const RenderingPayload& p, const ShapedRow& row, AtlasFontFaceEntry& fontFaceEntry, AtlasGlyphEntry* glyphEntry)
{
    // Twice the line height, twice the descender gap. For both.
    glyphEntry->offset.y -= p.s->font->descender;

    const auto isTop = row.lineRendition == LineRendition::DoubleHeightTop;
    const auto otherLineRendition = isTop ? LineRendition::DoubleHeightBottom : LineRendition::DoubleHeightTop;
    const auto entry2 = fontFaceEntry.glyphs[WI_EnumValue(otherLineRendition)].insert(glyphEntry->glyphIndex).first;

    *entry2 = *glyphEntry;

    const auto top = isTop ? glyphEntry : entry2;
    const auto bottom = isTop ? entry2 : glyphEntry;
    const auto topSize = clamp(-glyphEntry->offset.y - p.s->font->baseline, 0, static_cast<int>(glyphEntry->size.y));

    top->offset.y += p.s->font->cellSize.y;
    top->size.y = topSize;
    bottom->offset.y += topSize;
    bottom->size.y = std::max(0, bottom->size.y - topSize);
    bottom->texcoord.y += topSize;

    // Things like diacritics might be so small that they only exist on either half of the
    // double-height row. This effectively turns the other (unneeded) side into whitespace.
    if (!top->size.y)
    {
        top->shadingType = ShadingType::Default;
    }
    if (!bottom->size.y)
    {
        bottom->shadingType = ShadingType::Default;
    }
}

void BackendD3D::_drawGridlines(const RenderingPayload& p, u16 y)
{
    const auto row = p.rows[y];

    const auto horizontalShift = static_cast<u8>(row->lineRendition != LineRendition::SingleWidth);
    const auto verticalShift = static_cast<u8>(row->lineRendition >= LineRendition::DoubleHeightTop);

    const auto cellSize = p.s->font->cellSize;
    const auto rowTop = static_cast<i16>(cellSize.y * y);
    const auto rowBottom = static_cast<i16>(rowTop + cellSize.y);

    auto textCellTop = rowTop;
    if (row->lineRendition == LineRendition::DoubleHeightBottom)
    {
        textCellTop -= cellSize.y;
    }

    const i32 clipTop = row->lineRendition == LineRendition::DoubleHeightBottom ? rowTop : 0;
    const i32 clipBottom = row->lineRendition == LineRendition::DoubleHeightTop ? rowBottom : p.s->targetSize.y;

    const auto appendVerticalLines = [&](const GridLineRange& r, FontDecorationPosition pos) {
        const auto textCellWidth = cellSize.x << horizontalShift;
        const auto offset = pos.position << horizontalShift;
        const auto width = static_cast<u16>(pos.height << horizontalShift);

        auto posX = r.from * cellSize.x + offset;
        const auto end = r.to * cellSize.x;

        for (; posX < end; posX += textCellWidth)
        {
            _appendQuad() = {
                .shadingType = static_cast<u16>(ShadingType::SolidLine),
                .position = { static_cast<i16>(posX), rowTop },
                .size = { width, p.s->font->cellSize.y },
                .color = r.gridlineColor,
            };
        }
    };
    const auto appendHorizontalLine = [&](const GridLineRange& r, FontDecorationPosition pos, ShadingType shadingType, const u32 color) {
        const auto offset = pos.position << verticalShift;
        const auto height = static_cast<u16>(pos.height << verticalShift);

        const auto left = static_cast<i16>(r.from * cellSize.x);
        const auto width = static_cast<u16>((r.to - r.from) * cellSize.x);

        i32 rt = textCellTop + offset;
        i32 rb = rt + height;
        rt = clamp(rt, clipTop, clipBottom);
        rb = clamp(rb, clipTop, clipBottom);

        if (rt < rb)
        {
            _appendQuad() = {
                .shadingType = static_cast<u16>(shadingType),
                .renditionScale = { static_cast<u8>(1 << horizontalShift), static_cast<u8>(1 << verticalShift) },
                .position = { left, static_cast<i16>(rt) },
                .size = { width, static_cast<u16>(rb - rt) },
                .color = color,
            };
        }
    };

    for (const auto& r : row->gridLineRanges)
    {
        // AtlasEngine.cpp shouldn't add any gridlines if they don't do anything.
        assert(r.lines.any());

        if (r.lines.test(GridLines::Left))
        {
            appendVerticalLines(r, p.s->font->gridLeft);
        }
        if (r.lines.test(GridLines::Right))
        {
            appendVerticalLines(r, p.s->font->gridRight);
        }
        if (r.lines.test(GridLines::Top))
        {
            appendHorizontalLine(r, p.s->font->gridTop, ShadingType::SolidLine, r.gridlineColor);
        }
        if (r.lines.test(GridLines::Bottom))
        {
            appendHorizontalLine(r, p.s->font->gridBottom, ShadingType::SolidLine, r.gridlineColor);
        }
        if (r.lines.test(GridLines::Strikethrough))
        {
            appendHorizontalLine(r, p.s->font->strikethrough, ShadingType::SolidLine, r.gridlineColor);
        }

        if (r.lines.test(GridLines::Underline))
        {
            appendHorizontalLine(r, p.s->font->underline, ShadingType::SolidLine, r.underlineColor);
        }
        else if (r.lines.any(GridLines::DottedUnderline, GridLines::HyperlinkUnderline))
        {
            appendHorizontalLine(r, p.s->font->underline, ShadingType::DottedLine, r.underlineColor);
        }
        else if (r.lines.test(GridLines::DashedUnderline))
        {
            appendHorizontalLine(r, p.s->font->underline, ShadingType::DashedLine, r.underlineColor);
        }
        else if (r.lines.test(GridLines::CurlyUnderline))
        {
            appendHorizontalLine(r, _curlyUnderline, ShadingType::CurlyLine, r.underlineColor);
        }
        else if (r.lines.test(GridLines::DoubleUnderline))
        {
            for (const auto pos : p.s->font->doubleUnderline)
            {
                appendHorizontalLine(r, pos, ShadingType::SolidLine, r.underlineColor);
            }
        }
    }
}

void BackendD3D::_drawCursorBackground(const RenderingPayload& p)
{
    _cursorRects.clear();

    if (p.cursorRect.empty())
    {
        return;
    }

    _cursorPosition = {
        p.s->font->cellSize.x * p.cursorRect.left,
        p.s->font->cellSize.y * p.cursorRect.top,
        p.s->font->cellSize.x * p.cursorRect.right,
        p.s->font->cellSize.y * p.cursorRect.bottom,
    };

    const auto cursorColor = p.s->cursor->cursorColor;
    const auto offset = p.cursorRect.top * p.colorBitmapRowStride;

    for (auto x1 = p.cursorRect.left; x1 < p.cursorRect.right; ++x1)
    {
        const auto x0 = x1;
        const auto bg = p.backgroundBitmap[offset + x1] | 0xff000000;

        for (; x1 < p.cursorRect.right && (p.backgroundBitmap[offset + x1] | 0xff000000) == bg; ++x1)
        {
        }

        const i16x2 position{
            static_cast<i16>(p.s->font->cellSize.x * x0),
            static_cast<i16>(p.s->font->cellSize.y * p.cursorRect.top),
        };
        const u16x2 size{
            static_cast<u16>(p.s->font->cellSize.x * (x1 - x0)),
            p.s->font->cellSize.y,
        };
        auto background = cursorColor;
        auto foreground = bg;

        if (cursorColor == 0xffffffff)
        {
            background = bg ^ 0xffffff;
            foreground = 0xffffffff;
        }

        // The legacy console used to invert colors by just doing `bg ^ 0xc0c0c0`. This resulted
        // in a minimum squared distance of just 0.029195 across all possible color combinations.
        background = ColorFix::GetPerceivableColor(background, bg, 0.25f * 0.25f);

        auto& c0 = _cursorRects.emplace_back(position, size, background, foreground);

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
            c0.position.y += p.s->font->underline.position;
            c0.size.y = p.s->font->underline.height;
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
            c0.position.y += p.s->font->doubleUnderline[0].position;
            c0.size.y = p.s->font->thinLineWidth;
            c1.position.y += p.s->font->doubleUnderline[1].position;
            c1.size.y = p.s->font->thinLineWidth;
            break;
        }
        default:
            break;
        }
    }

    for (const auto& c : _cursorRects)
    {
        _appendQuad() = {
            .shadingType = static_cast<u16>(ShadingType::Cursor),
            .position = c.position,
            .size = c.size,
            .color = c.background,
        };
    }
}

void BackendD3D::_drawCursorForeground()
{
    // NOTE: _appendQuad() may reallocate the _instances vector. It's important to iterate
    // by index, because pointers (or iterators) would get invalidated. It's also important
    // to cache the original _instancesCount since it'll get changed with each append.
    auto instancesCount = _instancesCount;
    size_t instancesOffset = 0;

    assert(instancesCount != 0);

    // All of the text drawing primitives are drawn as a single block, after drawing
    // the background and cursor background and before drawing the selection overlay.
    // To avoid having to check the shadingType in the loop below, we'll find the
    // start and end of this "block" here in advance.
    for (; instancesOffset < instancesCount; ++instancesOffset)
    {
        const auto shadingType = static_cast<ShadingType>(_instances[instancesOffset].shadingType);
        if (shadingType >= ShadingType::TextDrawingFirst && shadingType <= ShadingType::TextDrawingLast)
        {
            break;
        }
    }
    // We can also skip any instances (= any rows) at the beginning that are clearly not overlapping with
    // the cursor. This reduces the CPU cost of this function by roughly half (a few microseconds).
    for (; instancesOffset < instancesCount; ++instancesOffset)
    {
        const auto& it = _instances[instancesOffset];
        if ((it.position.y + it.size.y) > _cursorPosition.top)
        {
            break;
        }
    }

    // Now do the same thing as above, but backwards from the end.
    for (; instancesCount > instancesOffset; --instancesCount)
    {
        const auto shadingType = static_cast<ShadingType>(_instances[instancesCount - 1].shadingType);
        if (shadingType >= ShadingType::TextDrawingFirst && shadingType <= ShadingType::TextDrawingLast)
        {
            break;
        }
    }
    for (; instancesCount > instancesOffset; --instancesCount)
    {
        const auto& it = _instances[instancesCount - 1];
        if (it.position.y < _cursorPosition.bottom)
        {
            break;
        }
    }

    // For cursors with multiple rectangles this really isn't all that fast, because it iterates
    // over the instances vector multiple times. But I also don't really care, because the
    // double-underline and empty-box cursors are pretty annoying to deal with in any case.
    //
    // It would definitely help if instead of position & size QuadInstances would use left/top/right/bottom
    // with f32, because then computing the intersection would be much faster via SIMD. But that would
    // make the struct size larger and cost more power to transmit more data to the GPU. ugh.
    for (const auto& c : _cursorRects)
    {
        const int cursorL = c.position.x;
        const int cursorT = c.position.y;
        const int cursorR = cursorL + c.size.x;
        const int cursorB = cursorT + c.size.y;

        for (size_t i = instancesOffset; i < instancesCount; ++i)
        {
            const auto& it = _instances[i];
            const int instanceL = it.position.x;
            const int instanceT = it.position.y;
            const int instanceR = instanceL + it.size.x;
            const int instanceB = instanceT + it.size.y;

            if (instanceL < cursorR && instanceR > cursorL && instanceT < cursorB && instanceB > cursorT)
            {
                // The _instances vector is _huge_ (easily up to 50k items) whereas only 1-2 items will actually overlap
                // with the cursor. --> Make this loop more compact by putting as much as possible into a function call.
                const auto added = _drawCursorForegroundSlowPath(c, i);
                i += added;
                instancesCount += added;
            }
        }
    }
}

size_t BackendD3D::_drawCursorForegroundSlowPath(const CursorRect& c, size_t offset)
{
    // We won't die from copying 24 bytes. It simplifies the code below especially in
    // respect to when/if we overwrite the _instances[offset] slot with a cutout.
#pragma warning(suppress : 26820) // This is a potentially expensive copy operation. Consider using a reference unless a copy is required (p.9).
    const auto it = _instances[offset];

    // There's one special exception to the rule: Emojis. We currently don't really support inverting
    // (or reversing) colored glyphs like that, so we can return early here and avoid cutting them up.
    // It'd be too expensive to check for these rare glyph types inside the _drawCursorForeground() loop.
    if (static_cast<ShadingType>(it.shadingType) == ShadingType::TextPassthrough)
    {
        return 0;
    }

    const int cursorL = c.position.x;
    const int cursorT = c.position.y;
    const int cursorR = cursorL + c.size.x;
    const int cursorB = cursorT + c.size.y;

    const int instanceL = it.position.x;
    const int instanceT = it.position.y;
    const int instanceR = instanceL + it.size.x;
    const int instanceB = instanceT + it.size.y;

    const auto intersectionL = std::max(cursorL, instanceL);
    const auto intersectionT = std::max(cursorT, instanceT);
    const auto intersectionR = std::min(cursorR, instanceR);
    const auto intersectionB = std::min(cursorB, instanceB);

    // We should only get called if there's actually an intersection.
    assert(intersectionL < intersectionR && intersectionT < intersectionB);

    // We need to ensure that the glyph doesn't "dirty" the cursor background with its un-inverted/un-reversed color.
    // If it did, and we'd draw the inverted/reversed glyph on top, it would look smudged.
    // As such, this cuts a cursor-sized hole into the original glyph and splits it up.
    //
    // > Always initialize an object
    // I would pay money if this warning was a little smarter. The array can remain uninitialized,
    // because it acts like a tiny small_vector, but without the assertions.
#pragma warning(suppress : 26494) // Variable 'cutouts' is uninitialized. Always initialize an object (type.5).
    rect<int> cutouts[4];
    size_t cutoutCount = 0;

    if (instanceT < intersectionT)
    {
        cutouts[cutoutCount++] = { instanceL, instanceT, instanceR, intersectionT };
    }
    if (instanceB > intersectionB)
    {
        cutouts[cutoutCount++] = { instanceL, intersectionB, instanceR, instanceB };
    }
    if (instanceL < intersectionL)
    {
        cutouts[cutoutCount++] = { instanceL, intersectionT, intersectionL, intersectionB };
    }
    if (instanceR > intersectionR)
    {
        cutouts[cutoutCount++] = { intersectionR, intersectionT, instanceR, intersectionB };
    }

    const auto addedInstances = cutoutCount ? cutoutCount - 1 : 0;

    // Make place for cutoutCount-many items at position.
    // NOTE: _bumpInstancesSize() reallocates the vector and all references to _instances will now be invalid.
    if (addedInstances)
    {
        const auto instancesCount = _instancesCount;

        _instancesCount += addedInstances;
        if (_instancesCount >= _instances.size())
        {
            _bumpInstancesSize();
        }

        const auto src = _instances.data() + offset;
        const auto dst = src + addedInstances;
        const auto count = instancesCount - offset;
        assert(src >= _instances.begin() && (src + count) < _instances.end());
        assert(dst >= _instances.begin() && (dst + count) < _instances.end());
        memmove(dst, src, count * sizeof(QuadInstance));
    }

    // Now that there's space we can write the glyph cutouts back into the instances vector.
    for (size_t i = 0; i < cutoutCount; ++i)
    {
        const auto& cutout = cutouts[i];
        auto& target = _instances[offset + i];

        target.shadingType = it.shadingType;
        target.renditionScale.x = it.renditionScale.x;
        target.renditionScale.y = it.renditionScale.y;
        target.position.x = static_cast<i16>(cutout.left);
        target.position.y = static_cast<i16>(cutout.top);
        target.size.x = static_cast<u16>(cutout.right - cutout.left);
        target.size.y = static_cast<u16>(cutout.bottom - cutout.top);
        target.texcoord.x = static_cast<u16>(it.texcoord.x + cutout.left - instanceL);
        target.texcoord.y = static_cast<u16>(it.texcoord.y + cutout.top - instanceT);
        target.color = it.color;
    }

    auto color = c.foreground == 0xffffffff ? it.color ^ 0xffffff : c.foreground;
    color = ColorFix::GetPerceivableColor(color, c.background, 0.5f * 0.5f);

    // If the cursor covers the entire glyph (like, let's say, a full-box cursor with an ASCII character),
    // we don't append a new quad, but rather reuse the one that already exists (cutoutCount == 0).
    auto& target = cutoutCount ? _appendQuad() : _instances[offset];

    target.shadingType = it.shadingType;
    target.renditionScale.x = it.renditionScale.x;
    target.renditionScale.y = it.renditionScale.y;
    target.position.x = static_cast<i16>(intersectionL);
    target.position.y = static_cast<i16>(intersectionT);
    target.size.x = static_cast<u16>(intersectionR - intersectionL);
    target.size.y = static_cast<u16>(intersectionB - intersectionT);
    target.texcoord.x = static_cast<u16>(it.texcoord.x + intersectionL - instanceL);
    target.texcoord.y = static_cast<u16>(it.texcoord.y + intersectionT - instanceT);
    target.color = color;

    return addedInstances;
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
                _appendQuad() = {
                    .shadingType = static_cast<u16>(ShadingType::Selection),
                    .position = {
                        static_cast<i16>(p.s->font->cellSize.x * row->selectionFrom),
                        static_cast<i16>(p.s->font->cellSize.y * y),
                    },
                    .size = {
                        static_cast<u16>(p.s->font->cellSize.x * (row->selectionTo - row->selectionFrom)),
                        static_cast<u16>(p.s->font->cellSize.y),
                    },
                    .color = p.s->misc->selectionColor,
                };
                lastFrom = row->selectionFrom;
                lastTo = row->selectionTo;
            }
        }

        y++;
    }
}

void BackendD3D::_debugShowDirty(const RenderingPayload& p)
{
#if ATLAS_DEBUG_SHOW_DIRTY
    _presentRects[_presentRectsPos] = p.dirtyRectInPx;
    _presentRectsPos = (_presentRectsPos + 1) % std::size(_presentRects);

    for (size_t i = 0; i < std::size(_presentRects); ++i)
    {
        const auto& rect = _presentRects[(_presentRectsPos + i) % std::size(_presentRects)];
        if (rect.non_empty())
        {
            _appendQuad() = {
                .shadingType = ShadingType::Selection,
                .position = {
                    static_cast<i16>(rect.left),
                    static_cast<i16>(rect.top),
                },
                .size = {
                    static_cast<u16>(rect.right - rect.left),
                    static_cast<u16>(rect.bottom - rect.top),
                },
                .color = colorbrewer::pastel1[i] | 0x1f000000,
            };
        }
    }
#endif
}

void BackendD3D::_debugDumpRenderTarget(const RenderingPayload& p)
{
#if ATLAS_DEBUG_DUMP_RENDER_TARGET
    if (_dumpRenderTargetCounter == 0)
    {
        ExpandEnvironmentStringsW(ATLAS_DEBUG_DUMP_RENDER_TARGET_PATH, &_dumpRenderTargetBasePath[0], gsl::narrow_cast<DWORD>(std::size(_dumpRenderTargetBasePath)));
        std::filesystem::create_directories(_dumpRenderTargetBasePath);
    }

    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\%u_%08zu.png", &_dumpRenderTargetBasePath[0], GetCurrentProcessId(), _dumpRenderTargetCounter);
    SaveTextureToPNG(p.deviceContext.get(), _swapChainManager.GetBuffer().get(), p.s->font->dpi, &path[0]);
    _dumpRenderTargetCounter++;
#endif
}

void BackendD3D::_executeCustomShader(RenderingPayload& p)
{
    {
        const CustomConstBuffer data{
            .time = std::chrono::duration<f32>(std::chrono::steady_clock::now() - _customShaderStartTime).count(),
            .scale = static_cast<f32>(p.s->font->dpi) / static_cast<f32>(USER_DEFAULT_SCREEN_DPI),
            .resolution = {
                static_cast<f32>(_viewportCellCount.x * p.s->font->cellSize.x),
                static_cast<f32>(_viewportCellCount.y * p.s->font->cellSize.y),
            },
            .background = colorFromU32Premultiply<f32x4>(p.s->misc->backgroundColor),
        };

        D3D11_MAPPED_SUBRESOURCE mapped{};
        THROW_IF_FAILED(p.deviceContext->Map(_customShaderConstantBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
        memcpy(mapped.pData, &data, sizeof(data));
        p.deviceContext->Unmap(_customShaderConstantBuffer.get(), 0);
    }

    {
        // Before we do anything else we have to unbound _renderTargetView from being
        // a render target, otherwise we can't use it as a shader resource below.
        p.deviceContext->OMSetRenderTargets(1, _renderTargetView.addressof(), nullptr);

        // IA: Input Assembler
        p.deviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
        p.deviceContext->IASetInputLayout(nullptr);
        p.deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        p.deviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);

        // VS: Vertex Shader
        p.deviceContext->VSSetShader(_customVertexShader.get(), nullptr, 0);
        p.deviceContext->VSSetConstantBuffers(0, 0, nullptr);

        // PS: Pixel Shader
        p.deviceContext->PSSetShader(_customPixelShader.get(), nullptr, 0);
        p.deviceContext->PSSetConstantBuffers(0, 1, _customShaderConstantBuffer.addressof());
        ID3D11ShaderResourceView* const resourceViews[]{
            _customOffscreenTextureView.get(), // The temrinal contents
            _customShaderTexture.TextureView.get(), // the experimental.pixelShaderImagePath, if there is one
        };
        // Checking if customer shader texture is set
        const UINT numViews = resourceViews[1] ? 2 : 1;
        p.deviceContext->PSSetShaderResources(0, numViews, &resourceViews[0]);

        p.deviceContext->PSSetSamplers(0, 1, _customShaderSamplerState.addressof());

        // OM: Output Merger
        p.deviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    }

    p.deviceContext->Draw(4, 0);

    {
        // IA: Input Assembler
        ID3D11Buffer* vertexBuffers[]{ _vertexBuffer.get(), _instanceBuffer.get() };
        static constexpr UINT strides[]{ sizeof(f32x2), sizeof(QuadInstance) };
        static constexpr UINT offsets[]{ 0, 0 };
        p.deviceContext->IASetIndexBuffer(_indexBuffer.get(), DXGI_FORMAT_R16_UINT, 0);
        p.deviceContext->IASetInputLayout(_inputLayout.get());
        p.deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        p.deviceContext->IASetVertexBuffers(0, 2, &vertexBuffers[0], &strides[0], &offsets[0]);

        // VS: Vertex Shader
        p.deviceContext->VSSetShader(_vertexShader.get(), nullptr, 0);
        p.deviceContext->VSSetConstantBuffers(0, 1, _vsConstantBuffer.addressof());

        // PS: Pixel Shader
        ID3D11ShaderResourceView* resources[]{ _backgroundBitmapView.get(), _glyphAtlasView.get() };
        p.deviceContext->PSSetShader(_pixelShader.get(), nullptr, 0);
        p.deviceContext->PSSetConstantBuffers(0, 1, _psConstantBuffer.addressof());
        p.deviceContext->PSSetShaderResources(0, 2, &resources[0]);
        p.deviceContext->PSSetSamplers(0, 0, nullptr);

        // OM: Output Merger
        p.deviceContext->OMSetBlendState(_blendState.get(), nullptr, 0xffffffff);
        p.deviceContext->OMSetRenderTargets(1, _customRenderTargetView.addressof(), nullptr);
    }

    // With custom shaders, everything might be invalidated, so we have to
    // indirectly disable Present1() and its dirty rects this way.
    p.dirtyRectInPx = { 0, 0, p.s->targetSize.x, p.s->targetSize.y };
}

TIL_FAST_MATH_END
