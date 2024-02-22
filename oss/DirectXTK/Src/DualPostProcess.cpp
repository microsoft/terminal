//--------------------------------------------------------------------------------------
// File: DualPostProcess.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "PostProcess.h"

#include "AlignedNew.h"
#include "CommonStates.h"
#include "BufferHelpers.h"
#include "DemandCreate.h"
#include "DirectXHelpers.h"
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
}


#pragma region Shaders
// Include the precompiled shader code.
namespace
{
#if defined(_XBOX_ONE) && defined(_TITLE)
#include "XboxOnePostProcess_VSQuad.inc"

#include "XboxOnePostProcess_PSMerge.inc"
#include "XboxOnePostProcess_PSBloomCombine.inc"
#else
#include "PostProcess_VSQuad.inc"

#include "PostProcess_PSMerge.inc"
#include "PostProcess_PSBloomCombine.inc"
#endif

    struct ShaderBytecode
    {
        void const* code;
        size_t length;
    };

    const ShaderBytecode pixelShaders[] =
    {
        { PostProcess_PSMerge,              sizeof(PostProcess_PSMerge) },
        { PostProcess_PSBloomCombine,       sizeof(PostProcess_PSBloomCombine) },
    };

    static_assert(static_cast<unsigned int>(std::size(pixelShaders)) == DualPostProcess::Effect_Max, "array/max mismatch");

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
                        SetDebugObjectName(*pResult, "DualPostProcess");

                    return hr;
                });
        }

        // Gets or lazily creates the specified pixel shader.
        ID3D11PixelShader* GetPixelShader(unsigned int shaderIndex)
        {
            assert(shaderIndex < DualPostProcess::Effect_Max);
            _Analysis_assume_(shaderIndex < DualPostProcess::Effect_Max);

            return DemandCreate(mPixelShaders[shaderIndex], mMutex, [&](ID3D11PixelShader** pResult) -> HRESULT
                {
                    HRESULT hr = mDevice->CreatePixelShader(pixelShaders[shaderIndex].code, pixelShaders[shaderIndex].length, nullptr, pResult);

                    if (SUCCEEDED(hr))
                        SetDebugObjectName(*pResult, "DualPostProcess");

                    return hr;
                });
        }

        CommonStates                stateObjects;

    protected:
        ComPtr<ID3D11Device>        mDevice;
        ComPtr<ID3D11VertexShader>  mVertexShader;
        ComPtr<ID3D11PixelShader>   mPixelShaders[DualPostProcess::Effect_Max];
        std::mutex                  mMutex;
    };
}
#pragma endregion


class DualPostProcess::Impl : public AlignedNew<PostProcessConstants>
{
public:
    explicit Impl(_In_ ID3D11Device* device);

    void Process(_In_ ID3D11DeviceContext* deviceContext, const std::function<void __cdecl()>& setCustomState);

    void SetDirtyFlag() noexcept { mDirtyFlags = INT_MAX; }

    // Fields.
    PostProcessConstants                    constants;
    DualPostProcess::Effect                 fx;
    ComPtr<ID3D11ShaderResourceView>        texture;
    ComPtr<ID3D11ShaderResourceView>        texture2;
    float                                   mergeWeight1;
    float                                   mergeWeight2;
    float                                   bloomIntensity;
    float                                   bloomBaseIntensity;
    float                                   bloomSaturation;
    float                                   bloomBaseSaturation;

private:
    int                                     mDirtyFlags;

    ConstantBuffer<PostProcessConstants>    mConstantBuffer;

    // Per-device resources.
    std::shared_ptr<DeviceResources>        mDeviceResources;

    static SharedResourcePool<ID3D11Device*, DeviceResources> deviceResourcesPool;
};


// Global pool of per-device DualPostProcess resources.
SharedResourcePool<ID3D11Device*, DeviceResources> DualPostProcess::Impl::deviceResourcesPool;


// Constructor.
DualPostProcess::Impl::Impl(_In_ ID3D11Device* device)
    : constants{},
    fx(DualPostProcess::Merge),
    mergeWeight1(0.5f),
    mergeWeight2(0.5f),
    bloomIntensity(1.25f),
    bloomBaseIntensity(1.f),
    bloomSaturation(1.f),
    bloomBaseSaturation(1.f),
    mDirtyFlags(INT_MAX),
    mConstantBuffer(device),
    mDeviceResources(deviceResourcesPool.DemandCreate(device))
{
    if (device->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0)
    {
        throw std::runtime_error("DualPostProcess requires Feature Level 10.0 or later");
    }

    SetDebugObjectName(mConstantBuffer.GetBuffer(), "DualPostProcess");
}


// Sets our state onto the D3D device.
void DualPostProcess::Impl::Process(
    _In_ ID3D11DeviceContext* deviceContext,
    const std::function<void __cdecl()>& setCustomState)
{
    // Set the texture.
    ID3D11ShaderResourceView* textures[2] = { texture.Get(), texture2.Get() };
    deviceContext->PSSetShaderResources(0, 2, textures);

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
    if (mDirtyFlags & Dirty_Parameters)
    {
        mDirtyFlags &= ~Dirty_Parameters;
        mDirtyFlags |= Dirty_ConstantBuffer;

        switch (fx)
        {
        case Merge:
            constants.sampleWeights[0] = XMVectorReplicate(mergeWeight1);
            constants.sampleWeights[1] = XMVectorReplicate(mergeWeight2);
            break;

        case BloomCombine:
            constants.sampleWeights[0] = XMVectorSet(bloomBaseSaturation, bloomSaturation, 0.f, 0.f);
            constants.sampleWeights[1] = XMVectorReplicate(bloomBaseIntensity);
            constants.sampleWeights[2] = XMVectorReplicate(bloomIntensity);
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

    if (setCustomState)
    {
        setCustomState();
    }

    // Draw quad.
    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    deviceContext->Draw(3, 0);
}


// Public constructor.
DualPostProcess::DualPostProcess(_In_ ID3D11Device* device)
    : pImpl(std::make_unique<Impl>(device))
{
}


DualPostProcess::DualPostProcess(DualPostProcess&&) noexcept = default;
DualPostProcess& DualPostProcess::operator= (DualPostProcess&&) noexcept = default;
DualPostProcess::~DualPostProcess() = default;


// IPostProcess methods.
void DualPostProcess::Process(
    _In_ ID3D11DeviceContext* deviceContext,
    _In_opt_ std::function<void __cdecl()> setCustomState)
{
    pImpl->Process(deviceContext, setCustomState);
}


// Shader control.
void DualPostProcess::SetEffect(Effect fx)
{
    if (fx >= Effect_Max)
        throw std::invalid_argument("Effect not defined");

    pImpl->fx = fx;
    pImpl->SetDirtyFlag();
}


// Properties
void DualPostProcess::SetSourceTexture(_In_opt_ ID3D11ShaderResourceView* value)
{
    pImpl->texture = value;
}


void DualPostProcess::SetSourceTexture2(_In_opt_ ID3D11ShaderResourceView* value)
{
    pImpl->texture2 = value;
}


void DualPostProcess::SetMergeParameters(float weight1, float weight2)
{
    pImpl->mergeWeight1 = weight1;
    pImpl->mergeWeight2 = weight2;
    pImpl->SetDirtyFlag();
}


void DualPostProcess::SetBloomCombineParameters(float bloom, float base, float bloomSaturation, float baseSaturation)
{
    pImpl->bloomIntensity = bloom;
    pImpl->bloomBaseIntensity = base;
    pImpl->bloomSaturation = bloomSaturation;
    pImpl->bloomBaseSaturation = baseSaturation;
    pImpl->SetDirtyFlag();
}
