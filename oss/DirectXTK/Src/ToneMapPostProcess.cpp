//--------------------------------------------------------------------------------------
// File: ToneMapPostProcess.cpp
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
    constexpr int Dirty_ConstantBuffer = 0x01;
    constexpr int Dirty_Parameters = 0x02;

#if defined(_XBOX_ONE) && defined(_TITLE)
    constexpr int PixelShaderCount = 15;
    constexpr int ShaderPermutationCount = 24;
#else
    constexpr int PixelShaderCount = 9;
    constexpr int ShaderPermutationCount = 12;
#endif

    // Constant buffer layout. Must match the shader!
    XM_ALIGNED_STRUCT(16) ToneMapConstants
    {
        // linearExposure is .x
        // paperWhiteNits is .y
        XMVECTOR parameters;
        XMVECTOR colorRotation[3];
    };

    static_assert((sizeof(ToneMapConstants) % 16) == 0, "CB size not padded correctly");

    // HDTV to UHDTV (Rec.709 color primaries into Rec.2020)
    constexpr float c_from709to2020[12] =
    {
          0.6274040f, 0.3292820f, 0.0433136f, 0.f,
          0.0690970f, 0.9195400f, 0.0113612f, 0.f,
          0.0163916f, 0.0880132f, 0.8955950f, 0.f,
    };

    // DCI-P3-D65 https://en.wikipedia.org/wiki/DCI-P3 to UHDTV (DCI-P3-D65 color primaries into Rec.2020)
    constexpr float c_fromP3D65to2020[12] =
    {
           0.753845f,  0.198593f,  0.047562f, 0.f,
          0.0457456f,  0.941777f, 0.0124772f, 0.f,
        -0.00121055f, 0.0176041f,  0.983607f, 0.f,
    };

    // HDTV to DCI-P3-D65 (a.k.a. Display P3 or P3D65)
    constexpr float c_from709toP3D65[12] =
    {
        0.822461969f, 0.1775380f,        0.f, 0.f,
        0.033194199f, 0.9668058f,        0.f, 0.f,
        0.017082631f, 0.0723974f, 0.9105199f, 0.f,
    };
}


#pragma region Shaders
// Include the precompiled shader code.
namespace
{
#if defined(_XBOX_ONE) && defined(_TITLE)
#include "XboxOneToneMap_VSQuad.inc"

#include "XboxOneToneMap_PSCopy.inc"
#include "XboxOneToneMap_PSSaturate.inc"
#include "XboxOneToneMap_PSReinhard.inc"
#include "XboxOneToneMap_PSACESFilmic.inc"
#include "XboxOneToneMap_PS_SRGB.inc"
#include "XboxOneToneMap_PSSaturate_SRGB.inc"
#include "XboxOneToneMap_PSReinhard_SRGB.inc"
#include "XboxOneToneMap_PSACESFilmic_SRGB.inc"
#include "XboxOneToneMap_PSHDR10.inc"
#include "XboxOneToneMap_PSHDR10_Saturate.inc"
#include "XboxOneToneMap_PSHDR10_Reinhard.inc"
#include "XboxOneToneMap_PSHDR10_ACESFilmic.inc"
#include "XboxOneToneMap_PSHDR10_Saturate_SRGB.inc"
#include "XboxOneToneMap_PSHDR10_Reinhard_SRGB.inc"
#include "XboxOneToneMap_PSHDR10_ACESFilmic_SRGB.inc"
#else
#include "ToneMap_VSQuad.inc"

#include "ToneMap_PSCopy.inc"
#include "ToneMap_PSSaturate.inc"
#include "ToneMap_PSReinhard.inc"
#include "ToneMap_PSACESFilmic.inc"
#include "ToneMap_PS_SRGB.inc"
#include "ToneMap_PSSaturate_SRGB.inc"
#include "ToneMap_PSReinhard_SRGB.inc"
#include "ToneMap_PSACESFilmic_SRGB.inc"
#include "ToneMap_PSHDR10.inc"
#endif
}

namespace
{
    struct ShaderBytecode
    {
        void const* code;
        size_t length;
    };

    const ShaderBytecode pixelShaders[] =
    {
        { ToneMap_PSCopy,                   sizeof(ToneMap_PSCopy) },
        { ToneMap_PSSaturate,               sizeof(ToneMap_PSSaturate) },
        { ToneMap_PSReinhard,               sizeof(ToneMap_PSReinhard) },
        { ToneMap_PSACESFilmic,             sizeof(ToneMap_PSACESFilmic) },
        { ToneMap_PS_SRGB,                  sizeof(ToneMap_PS_SRGB) },
        { ToneMap_PSSaturate_SRGB,          sizeof(ToneMap_PSSaturate_SRGB) },
        { ToneMap_PSReinhard_SRGB,          sizeof(ToneMap_PSReinhard_SRGB) },
        { ToneMap_PSACESFilmic_SRGB,        sizeof(ToneMap_PSACESFilmic_SRGB) },
        { ToneMap_PSHDR10,                  sizeof(ToneMap_PSHDR10) },

#if defined(_XBOX_ONE) && defined(_TITLE)
        // Shaders that generate both HDR10 and GameDVR SDR signals via Multiple Render Targets.
        { ToneMap_PSHDR10_Saturate,         sizeof(ToneMap_PSHDR10_Saturate) },
        { ToneMap_PSHDR10_Reinhard,         sizeof(ToneMap_PSHDR10_Reinhard) },
        { ToneMap_PSHDR10_ACESFilmic,       sizeof(ToneMap_PSHDR10_ACESFilmic) },
        { ToneMap_PSHDR10_Saturate_SRGB,    sizeof(ToneMap_PSHDR10_Saturate_SRGB) },
        { ToneMap_PSHDR10_Reinhard_SRGB,    sizeof(ToneMap_PSHDR10_Reinhard_SRGB) },
        { ToneMap_PSHDR10_ACESFilmic_SRGB,  sizeof(ToneMap_PSHDR10_ACESFilmic_SRGB) },
#endif
    };

    static_assert(static_cast<int>(std::size(pixelShaders)) == PixelShaderCount, "array/max mismatch");

    const int pixelShaderIndices[] =
    {
        // Linear EOTF
        0,  // Copy
        1,  // Saturate
        2,  // Reinhard
        3,  // ACES Filmic

        // Gamam22 EOTF
        4,  // SRGB
        5,  // Saturate_SRGB
        6,  // Reinhard_SRGB
        7,  // ACES Filmic

        // ST.2084 EOTF
        8,  // HDR10
        8,  // HDR10
        8,  // HDR10
        8,  // HDR10

#if defined(_XBOX_ONE) && defined(_TITLE)
        // MRT Linear EOTF
        9,  // HDR10+Saturate
        9,  // HDR10+Saturate
        10, // HDR10+Reinhard
        11, // HDR10+ACESFilmic

        // MRT Gamma22 EOTF
        12, // HDR10+Saturate_SRGB
        12, // HDR10+Saturate_SRGB
        13, // HDR10+Reinhard_SRGB
        14,  // HDR10+ACESFilmic

        // MRT ST.2084 EOTF
        9,  // HDR10+Saturate
        9,  // HDR10+Saturate
        10, // HDR10+Reinhard
        11, // HDR10+ACESFilmic
#endif
    };

    static_assert(static_cast<int>(std::size(pixelShaderIndices)) == ShaderPermutationCount, "array/max mismatch");

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
                    HRESULT hr = mDevice->CreateVertexShader(ToneMap_VSQuad, sizeof(ToneMap_VSQuad), nullptr, pResult);

                    if (SUCCEEDED(hr))
                        SetDebugObjectName(*pResult, "ToneMapPostProcess");

                    return hr;
                });
        }

        // Gets or lazily creates the specified pixel shader.
        ID3D11PixelShader* GetPixelShader(int permutation)
        {
            assert(permutation >= 0 && permutation < ShaderPermutationCount);
            _Analysis_assume_(permutation >= 0 && permutation < ShaderPermutationCount);
            int shaderIndex = pixelShaderIndices[permutation];
            assert(shaderIndex >= 0 && shaderIndex < PixelShaderCount);
            _Analysis_assume_(shaderIndex >= 0 && shaderIndex < PixelShaderCount);

            return DemandCreate(mPixelShaders[shaderIndex], mMutex, [&](ID3D11PixelShader** pResult) -> HRESULT
                {
                    HRESULT hr = mDevice->CreatePixelShader(pixelShaders[shaderIndex].code, pixelShaders[shaderIndex].length, nullptr, pResult);

                    if (SUCCEEDED(hr))
                        SetDebugObjectName(*pResult, "ToneMapPostProcess");

                    return hr;
                });
        }

        CommonStates                stateObjects;

    protected:
        ComPtr<ID3D11Device>        mDevice;
        ComPtr<ID3D11VertexShader>  mVertexShader;
        ComPtr<ID3D11PixelShader>   mPixelShaders[PixelShaderCount];
        std::mutex                  mMutex;
    };
}
#pragma endregion


class ToneMapPostProcess::Impl : public AlignedNew<ToneMapConstants>
{
public:
    explicit Impl(_In_ ID3D11Device* device);

    void Process(_In_ ID3D11DeviceContext* deviceContext, const std::function<void __cdecl()>& setCustomState);

    void SetDirtyFlag() noexcept { mDirtyFlags = INT_MAX; }

    int GetCurrentShaderPermutation() const noexcept;

    // Fields.
    ToneMapConstants                        constants;
    ComPtr<ID3D11ShaderResourceView>        hdrTexture;
    float                                   linearExposure;
    float                                   paperWhiteNits;

    Operator                                op;
    TransferFunction                        func;
    bool                                    mrt;

private:
    int                                     mDirtyFlags;

    ConstantBuffer<ToneMapConstants>        mConstantBuffer;

    // Per-device resources.
    std::shared_ptr<DeviceResources>        mDeviceResources;

    static SharedResourcePool<ID3D11Device*, DeviceResources> deviceResourcesPool;
};


// Global pool of per-device ToneMapPostProcess resources.
SharedResourcePool<ID3D11Device*, DeviceResources> ToneMapPostProcess::Impl::deviceResourcesPool;


// Constructor.
ToneMapPostProcess::Impl::Impl(_In_ ID3D11Device* device)
    : constants{},
    linearExposure(1.f),
    paperWhiteNits(200.f),
    op(None),
    func(Linear),
    mrt(false),
    mDirtyFlags(INT_MAX),
    mConstantBuffer(device),
    mDeviceResources(deviceResourcesPool.DemandCreate(device))
{
    if (device->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0)
    {
        throw std::runtime_error("ToneMapPostProcess requires Feature Level 10.0 or later");
    }

    memcpy(constants.colorRotation, c_from709to2020, sizeof(c_from709to2020));

    SetDebugObjectName(mConstantBuffer.GetBuffer(), "ToneMapPostProcess");
}


// Sets our state onto the D3D device.
void ToneMapPostProcess::Impl::Process(
    _In_ ID3D11DeviceContext* deviceContext,
    const std::function<void __cdecl()>& setCustomState)
{
    // Set the texture.
    ID3D11ShaderResourceView* textures[1] = { hdrTexture.Get() };
    deviceContext->PSSetShaderResources(0, 1, textures);

    auto sampler = mDeviceResources->stateObjects.PointClamp();
    deviceContext->PSSetSamplers(0, 1, &sampler);

    // Set state objects.
    deviceContext->OMSetBlendState(mDeviceResources->stateObjects.Opaque(), nullptr, 0xffffffff);
    deviceContext->OMSetDepthStencilState(mDeviceResources->stateObjects.DepthNone(), 0);
    deviceContext->RSSetState(mDeviceResources->stateObjects.CullNone());

    // Set shaders.
    auto vertexShader = mDeviceResources->GetVertexShader();
    auto pixelShader = mDeviceResources->GetPixelShader(GetCurrentShaderPermutation());

    deviceContext->VSSetShader(vertexShader, nullptr, 0);
    deviceContext->PSSetShader(pixelShader, nullptr, 0);

    // Set constants.
    if (mDirtyFlags & Dirty_Parameters)
    {
        mDirtyFlags &= ~Dirty_Parameters;
        mDirtyFlags |= Dirty_ConstantBuffer;

        constants.parameters = XMVectorSet(linearExposure, paperWhiteNits, 0.f, 0.f);
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


int ToneMapPostProcess::Impl::GetCurrentShaderPermutation() const noexcept
{
#if defined(_XBOX_ONE) && defined(_TITLE)
    int permutation = (mrt) ? 12 : 0;
    return permutation + (static_cast<int>(func) * static_cast<int>(Operator_Max)) + static_cast<int>(op);
#else
    return (static_cast<int>(func) * static_cast<int>(Operator_Max)) + static_cast<int>(op);
#endif
}


// Public constructor.
ToneMapPostProcess::ToneMapPostProcess(_In_ ID3D11Device* device)
    : pImpl(std::make_unique<Impl>(device))
{
}


ToneMapPostProcess::ToneMapPostProcess(ToneMapPostProcess&&) noexcept = default;
ToneMapPostProcess& ToneMapPostProcess::operator= (ToneMapPostProcess&&) noexcept = default;
ToneMapPostProcess::~ToneMapPostProcess() = default;


// IPostProcess methods.
void ToneMapPostProcess::Process(
    _In_ ID3D11DeviceContext* deviceContext,
    _In_opt_ std::function<void __cdecl()> setCustomState)
{
    pImpl->Process(deviceContext, setCustomState);
}


// Shader control.
void ToneMapPostProcess::SetOperator(Operator op)
{
    if (op >= Operator_Max)
        throw std::invalid_argument("Tonemap operator not defined");

    pImpl->op = op;
}


void ToneMapPostProcess::SetTransferFunction(TransferFunction func)
{
    if (func >= TransferFunction_Max)
        throw std::invalid_argument("Electro-optical transfer function not defined");

    pImpl->func = func;
}


#if defined(_XBOX_ONE) && defined(_TITLE)
void ToneMapPostProcess::SetMRTOutput(bool value)
{
    pImpl->mrt = value;
}
#endif


// Properties
void ToneMapPostProcess::SetHDRSourceTexture(_In_opt_ ID3D11ShaderResourceView* value)
{
    pImpl->hdrTexture = value;
}


void ToneMapPostProcess::SetColorRotation(ColorPrimaryRotation value)
{
    switch (value)
    {
    case DCI_P3_D65_to_UHDTV:   memcpy(pImpl->constants.colorRotation, c_fromP3D65to2020, sizeof(c_fromP3D65to2020)); break;
    case HDTV_to_DCI_P3_D65:    memcpy(pImpl->constants.colorRotation, c_from709toP3D65, sizeof(c_from709toP3D65)); break;
    default:                    memcpy(pImpl->constants.colorRotation, c_from709to2020, sizeof(c_from709to2020)); break;
    }

    pImpl->SetDirtyFlag();
}


void ToneMapPostProcess::SetColorRotation(CXMMATRIX value)
{
    const XMMATRIX transpose = XMMatrixTranspose(value);
    pImpl->constants.colorRotation[0] = transpose.r[0];
    pImpl->constants.colorRotation[1] = transpose.r[1];
    pImpl->constants.colorRotation[2] = transpose.r[2];
    pImpl->SetDirtyFlag();
}


void ToneMapPostProcess::SetExposure(float exposureValue)
{
    pImpl->linearExposure = powf(2.f, exposureValue);
    pImpl->SetDirtyFlag();
}


void ToneMapPostProcess::SetST2084Parameter(float paperWhiteNits)
{
    pImpl->paperWhiteNits = paperWhiteNits;
    pImpl->SetDirtyFlag();
}
