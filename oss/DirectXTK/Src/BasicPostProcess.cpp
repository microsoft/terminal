//--------------------------------------------------------------------------------------
// File: BasicPostProcess.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "PostProcess.h"

#include "BufferHelpers.h"
#include "CommonStates.h"
#include "DirectXHelpers.h"
#include "AlignedNew.h"
#include "DemandCreate.h"
#include "SharedResourcePool.h"

using namespace DirectX;

using Microsoft::WRL::ComPtr;

namespace
{
    constexpr int c_MaxSamples = 16;

    constexpr int Dirty_ConstantBuffer = 0x01;
    constexpr int Dirty_Parameters = 0x02;

    // Constant buffer layout. Must match the shader!
    XM_ALIGNED_STRUCT(16) PostProcessConstants
    {
        XMVECTOR sampleOffsets[c_MaxSamples];
        XMVECTOR sampleWeights[c_MaxSamples];
    };

    static_assert((sizeof(PostProcessConstants) % 16) == 0, "CB size not padded correctly");

    // 2-parameter Gaussian distribution given standard deviation (rho)
    inline float GaussianDistribution(float x, float y, float rho) noexcept
    {
        return expf(-(x * x + y * y) / (2 * rho * rho)) / sqrtf(2 * XM_PI * rho * rho);
    }
}

#pragma region Shaders
// Include the precompiled shader code.
namespace
{
#if defined(_XBOX_ONE) && defined(_TITLE)
#include "XboxOnePostProcess_VSQuad.inc"

#include "XboxOnePostProcess_PSCopy.inc"
#include "XboxOnePostProcess_PSMonochrome.inc"
#include "XboxOnePostProcess_PSSepia.inc"
#include "XboxOnePostProcess_PSDownScale2x2.inc"
#include "XboxOnePostProcess_PSDownScale4x4.inc"
#include "XboxOnePostProcess_PSGaussianBlur5x5.inc"
#include "XboxOnePostProcess_PSBloomExtract.inc"
#include "XboxOnePostProcess_PSBloomBlur.inc"
#else
#include "PostProcess_VSQuad.inc"

#include "PostProcess_PSCopy.inc"
#include "PostProcess_PSMonochrome.inc"
#include "PostProcess_PSSepia.inc"
#include "PostProcess_PSDownScale2x2.inc"
#include "PostProcess_PSDownScale4x4.inc"
#include "PostProcess_PSGaussianBlur5x5.inc"
#include "PostProcess_PSBloomExtract.inc"
#include "PostProcess_PSBloomBlur.inc"
#endif

    struct ShaderBytecode
    {
        void const* code;
        size_t length;
    };

    const ShaderBytecode pixelShaders[] =
    {
        { PostProcess_PSCopy,                   sizeof(PostProcess_PSCopy) },
        { PostProcess_PSMonochrome,             sizeof(PostProcess_PSMonochrome) },
        { PostProcess_PSSepia,                  sizeof(PostProcess_PSSepia) },
        { PostProcess_PSDownScale2x2,           sizeof(PostProcess_PSDownScale2x2) },
        { PostProcess_PSDownScale4x4,           sizeof(PostProcess_PSDownScale4x4) },
        { PostProcess_PSGaussianBlur5x5,        sizeof(PostProcess_PSGaussianBlur5x5) },
        { PostProcess_PSBloomExtract,           sizeof(PostProcess_PSBloomExtract) },
        { PostProcess_PSBloomBlur,              sizeof(PostProcess_PSBloomBlur) },
    };

    static_assert(static_cast<unsigned int>(std::size(pixelShaders)) == BasicPostProcess::Effect_Max, "array/max mismatch");

    // Factory for lazily instantiating shaders.
    class DeviceResources
    {
    public:
        DeviceResources(_In_ ID3D11Device* device)
            : stateObjects(device),
            mDevice(device),
            mVertexShader{},
            mPixelShaders{},
            mMutex{}
        { }

        // Gets or lazily creates the vertex shader.
        ID3D11VertexShader* GetVertexShader()
        {
            return DemandCreate(mVertexShader, mMutex, [&](ID3D11VertexShader** pResult) -> HRESULT
                {
                    HRESULT hr = mDevice->CreateVertexShader(PostProcess_VSQuad, sizeof(PostProcess_VSQuad), nullptr, pResult);

                    if (SUCCEEDED(hr))
                        SetDebugObjectName(*pResult, "BasicPostProcess");

                    return hr;
                });
        }

        // Gets or lazily creates the specified pixel shader.
        ID3D11PixelShader* GetPixelShader(unsigned int shaderIndex)
        {
            assert(shaderIndex < BasicPostProcess::Effect_Max);
            _Analysis_assume_(shaderIndex < BasicPostProcess::Effect_Max);

            return DemandCreate(mPixelShaders[shaderIndex], mMutex, [&](ID3D11PixelShader** pResult) -> HRESULT
                {
                    HRESULT hr = mDevice->CreatePixelShader(pixelShaders[shaderIndex].code, pixelShaders[shaderIndex].length, nullptr, pResult);

                    if (SUCCEEDED(hr))
                        SetDebugObjectName(*pResult, "BasicPostProcess");

                    return hr;
                });
        }

        CommonStates                stateObjects;

    protected:
        ComPtr<ID3D11Device>        mDevice;
        ComPtr<ID3D11VertexShader>  mVertexShader;
        ComPtr<ID3D11PixelShader>   mPixelShaders[BasicPostProcess::Effect_Max];
        std::mutex                  mMutex;
    };
}
#pragma endregion


class BasicPostProcess::Impl : public AlignedNew<PostProcessConstants>
{
public:
    explicit Impl(_In_ ID3D11Device* device);

    void Process(_In_ ID3D11DeviceContext* deviceContext, const std::function<void __cdecl()>& setCustomState);

    void SetConstants(bool value = true) noexcept { mUseConstants = value; mDirtyFlags = INT_MAX; }
    void SetDirtyFlag() noexcept { mDirtyFlags = INT_MAX; }

    // Fields.
    PostProcessConstants                    constants;
    BasicPostProcess::Effect                fx;
    ComPtr<ID3D11ShaderResourceView>        texture;
    unsigned                                texWidth;
    unsigned                                texHeight;
    float                                   guassianMultiplier;
    float                                   bloomSize;
    float                                   bloomBrightness;
    float                                   bloomThreshold;
    bool                                    bloomHorizontal;

private:
    bool                                    mUseConstants;
    int                                     mDirtyFlags;

    void                                    DownScale2x2();
    void                                    DownScale4x4();
    void                                    GaussianBlur5x5(float multiplier);
    void                                    Bloom(bool horizontal, float size, float brightness);

    ConstantBuffer<PostProcessConstants>    mConstantBuffer;

    // Per-device resources.
    std::shared_ptr<DeviceResources>        mDeviceResources;

    static SharedResourcePool<ID3D11Device*, DeviceResources> deviceResourcesPool;
};


// Global pool of per-device BasicPostProcess resources.
SharedResourcePool<ID3D11Device*, DeviceResources> BasicPostProcess::Impl::deviceResourcesPool;


// Constructor.
BasicPostProcess::Impl::Impl(_In_ ID3D11Device* device)
    : constants{},
    fx(BasicPostProcess::Copy),
    texWidth(0),
    texHeight(0),
    guassianMultiplier(1.f),
    bloomSize(1.f),
    bloomBrightness(1.f),
    bloomThreshold(0.25f),
    bloomHorizontal(true),
    mUseConstants(false),
    mDirtyFlags(INT_MAX),
    mConstantBuffer(device),
    mDeviceResources(deviceResourcesPool.DemandCreate(device))
{
    if (device->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0)
    {
        throw std::runtime_error("BasicPostProcess requires Feature Level 10.0 or later");
    }

    SetDebugObjectName(mConstantBuffer.GetBuffer(), "BasicPostProcess");
}


// Sets our state onto the D3D device.
void BasicPostProcess::Impl::Process(
    _In_ ID3D11DeviceContext* deviceContext,
    const std::function<void __cdecl()>& setCustomState)
{
    // Set the texture.
    ID3D11ShaderResourceView* textures[1] = { texture.Get() };
    deviceContext->PSSetShaderResources(0, 1, textures);

    auto sampler = mDeviceResources->stateObjects.LinearClamp();
    deviceContext->PSSetSamplers(0, 1, &sampler);

    // Set state objects.
    deviceContext->OMSetBlendState(mDeviceResources->stateObjects.Opaque(), nullptr, 0xffffffff);
    deviceContext->OMSetDepthStencilState(mDeviceResources->stateObjects.DepthNone(), 0);
    deviceContext->RSSetState(mDeviceResources->stateObjects.CullNone());

    // Set shaders.
    auto vertexShader = mDeviceResources->GetVertexShader();
    auto pixelShader = mDeviceResources->GetPixelShader(fx);

    deviceContext->VSSetShader(vertexShader, nullptr, 0);
    deviceContext->PSSetShader(pixelShader, nullptr, 0);

    // Set constants.
    if (mUseConstants)
    {
        if (mDirtyFlags & Dirty_Parameters)
        {
            mDirtyFlags &= ~Dirty_Parameters;
            mDirtyFlags |= Dirty_ConstantBuffer;

            switch (fx)
            {
            case DownScale_2x2:
                DownScale2x2();
                break;

            case DownScale_4x4:
                DownScale4x4();
                break;

            case GaussianBlur_5x5:
                GaussianBlur5x5(guassianMultiplier);
                break;

            case BloomExtract:
                constants.sampleWeights[0] = XMVectorReplicate(bloomThreshold);
                break;

            case BloomBlur:
                Bloom(bloomHorizontal, bloomSize, bloomBrightness);
                break;

            default:
                break;
            }
        }

    #if defined(_XBOX_ONE) && defined(_TITLE)
        void *grfxMemory;
        mConstantBuffer.SetData(deviceContext, constants, &grfxMemory);

        ComPtr<ID3D11DeviceContextX> deviceContextX;
        ThrowIfFailed(deviceContext->QueryInterface(IID_GRAPHICS_PPV_ARGS(deviceContextX.GetAddressOf())));

        auto buffer = mConstantBuffer.GetBuffer();

        deviceContextX->PSSetPlacementConstantBuffer(0, buffer, grfxMemory);
    #else
        if (mDirtyFlags & Dirty_ConstantBuffer)
        {
            mDirtyFlags &= ~Dirty_ConstantBuffer;
            mConstantBuffer.SetData(deviceContext, constants);
        }

        // Set the constant buffer.
        auto buffer = mConstantBuffer.GetBuffer();

        deviceContext->PSSetConstantBuffers(0, 1, &buffer);
    #endif
    }

    if (setCustomState)
    {
        setCustomState();
    }

    // Draw quad.
    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    deviceContext->Draw(3, 0);
}


void BasicPostProcess::Impl::DownScale2x2()
{
    mUseConstants = true;

    if (!texWidth || !texHeight)
    {
        throw std::logic_error("Call SetSourceTexture before setting post-process effect");
    }

    const float tu = 1.0f / float(texWidth);
    const float tv = 1.0f / float(texHeight);

    // Sample from the 4 surrounding points. Since the center point will be in the exact
    // center of 4 texels, a 0.5f offset is needed to specify a texel center.
    auto ptr = reinterpret_cast<XMFLOAT4*>(constants.sampleOffsets);
    for (int y = 0; y < 2; ++y)
    {
        for (int x = 0; x < 2; ++x)
        {
            ptr->x = (float(x) - 0.5f) * tu;
            ptr->y = (float(y) - 0.5f) * tv;
            ++ptr;
        }
    }
}


void BasicPostProcess::Impl::DownScale4x4()
{
    mUseConstants = true;

    if (!texWidth || !texHeight)
    {
        throw std::logic_error("Call SetSourceTexture before setting post-process effect");
    }

    const float tu = 1.0f / float(texWidth);
    const float tv = 1.0f / float(texHeight);

    // Sample from the 16 surrounding points. Since the center point will be in the
    // exact center of 16 texels, a 1.5f offset is needed to specify a texel center.
    auto ptr = reinterpret_cast<XMFLOAT4*>(constants.sampleOffsets);
    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            ptr->x = (float(x) - 1.5f) * tu;
            ptr->y = (float(y) - 1.5f) * tv;
            ++ptr;
        }
    }

}


void BasicPostProcess::Impl::GaussianBlur5x5(float multiplier)
{
    mUseConstants = true;

    if (!texWidth || !texHeight)
    {
        throw std::logic_error("Call SetSourceTexture before setting post-process effect");
    }

    const float tu = 1.0f / float(texWidth);
    const float tv = 1.0f / float(texHeight);

    float totalWeight = 0.0f;
    size_t index = 0;
    auto offsets = reinterpret_cast<XMFLOAT4*>(constants.sampleOffsets);
    auto weights = constants.sampleWeights;
    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            // Exclude pixels with a block distance greater than 2. This will
            // create a kernel which approximates a 5x5 kernel using only 13
            // sample points instead of 25; this is necessary since 2.0 shaders
            // only support 16 texture grabs.
            if (fabsf(float(x)) + fabsf(float(y)) > 2.0f)
                continue;

            // Get the unscaled Gaussian intensity for this offset
            offsets[index].x = float(x) * tu;
            offsets[index].y = float(y) * tv;
            offsets[index].z = 0.0f;
            offsets[index].w = 0.0f;

            const float g = GaussianDistribution(float(x), float(y), 1.0f);
            weights[index] = XMVectorReplicate(g);

            totalWeight += XMVectorGetX(weights[index]);

            ++index;
        }
    }

    // Divide the current weight by the total weight of all the samples; Gaussian
    // blur kernels add to 1.0f to ensure that the intensity of the image isn't
    // changed when the blur occurs. An optional multiplier variable is used to
    // add or remove image intensity during the blur.
    const XMVECTOR vtw = XMVectorReplicate(totalWeight);
    const XMVECTOR vm = XMVectorReplicate(multiplier);
    for (size_t i = 0; i < index; ++i)
    {
        weights[i] = XMVectorDivide(weights[i], vtw);
        weights[i] = XMVectorMultiply(weights[i], vm);
    }
}


void  BasicPostProcess::Impl::Bloom(bool horizontal, float size, float brightness)
{
    mUseConstants = true;

    if (!texWidth || !texHeight)
    {
        throw std::logic_error("Call SetSourceTexture before setting post-process effect");
    }

    float tu = 0.f;
    float tv = 0.f;
    if (horizontal)
    {
        tu = 1.f / float(texWidth);
    }
    else
    {
        tv = 1.f / float(texHeight);
    }

    auto weights = reinterpret_cast<XMFLOAT4*>(constants.sampleWeights);
    auto offsets = reinterpret_cast<XMFLOAT4*>(constants.sampleOffsets);

    // Fill the center texel
    float weight = brightness * GaussianDistribution(0, 0, size);
    weights[0] = XMFLOAT4(weight, weight, weight, 1.0f);
    offsets[0].x = offsets[0].y = offsets[0].z = offsets[0].w = 0.f;

    // Fill the first half
    for (int i = 1; i < 8; ++i)
    {
        // Get the Gaussian intensity for this offset
        weight = brightness * GaussianDistribution(float(i), 0, size);
        weights[i] = XMFLOAT4(weight, weight, weight, 1.0f);
        offsets[i] = XMFLOAT4(float(i) * tu, float(i) * tv, 0.f, 0.f);
    }

    // Mirror to the second half
    for (int i = 8; i < 15; i++)
    {
        weights[i] = weights[i - 7];
        offsets[i] = XMFLOAT4(-offsets[i - 7].x, -offsets[i - 7].y, 0.f, 0.f);
    }
}


// Public constructor.
BasicPostProcess::BasicPostProcess(_In_ ID3D11Device* device)
    : pImpl(std::make_unique<Impl>(device))
{
}


BasicPostProcess::BasicPostProcess(BasicPostProcess&&) noexcept = default;
BasicPostProcess& BasicPostProcess::operator= (BasicPostProcess&&) noexcept = default;
BasicPostProcess::~BasicPostProcess() = default;


// IPostProcess methods.
void BasicPostProcess::Process(
    _In_ ID3D11DeviceContext* deviceContext,
    _In_opt_ std::function<void __cdecl()> setCustomState)
{
    pImpl->Process(deviceContext, setCustomState);
}


// Shader control.
void BasicPostProcess::SetEffect(Effect fx)
{
    if (fx >= Effect_Max)
        throw std::invalid_argument("Effect not defined");

    pImpl->fx = fx;

    switch (fx)
    {
    case Copy:
    case Monochrome:
    case Sepia:
        // These shaders don't use the constant buffer
        pImpl->SetConstants(false);
        break;

    default:
        pImpl->SetConstants(true);
        break;
    }
}


// Properties
void BasicPostProcess::SetSourceTexture(_In_opt_ ID3D11ShaderResourceView* value)
{
    pImpl->texture = value;

    if (value)
    {
        ComPtr<ID3D11Resource> res;
        value->GetResource(res.GetAddressOf());

        D3D11_RESOURCE_DIMENSION resType = D3D11_RESOURCE_DIMENSION_UNKNOWN;
        res->GetType(&resType);

        switch (resType)
        {
        case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
            {
                ComPtr<ID3D11Texture1D> tex;
                ThrowIfFailed(res.As(&tex));

                D3D11_TEXTURE1D_DESC desc = {};
                tex->GetDesc(&desc);
                pImpl->texWidth = desc.Width;
                pImpl->texHeight = 1;
                break;
            }

        case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
            {
                ComPtr<ID3D11Texture2D> tex;
                ThrowIfFailed(res.As(&tex));

                D3D11_TEXTURE2D_DESC desc = {};
                tex->GetDesc(&desc);
                pImpl->texWidth = desc.Width;
                pImpl->texHeight = desc.Height;
                break;
            }

        case D3D11_RESOURCE_DIMENSION_UNKNOWN:
        case D3D11_RESOURCE_DIMENSION_BUFFER:
        case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
        default:
            throw std::invalid_argument("Unsupported texture type");
        }
    }
    else
    {
        pImpl->texWidth = pImpl->texHeight = 0;
    }
}


void BasicPostProcess::SetGaussianParameter(float multiplier)
{
    pImpl->guassianMultiplier = multiplier;
    pImpl->SetDirtyFlag();
}


void BasicPostProcess::SetBloomExtractParameter(float threshold)
{
    pImpl->bloomThreshold = threshold;
    pImpl->SetDirtyFlag();
}


void BasicPostProcess::SetBloomBlurParameters(bool horizontal, float size, float brightness)
{
    pImpl->bloomSize = size;
    pImpl->bloomBrightness = brightness;
    pImpl->bloomHorizontal = horizontal;
    pImpl->SetDirtyFlag();
}
