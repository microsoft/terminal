//--------------------------------------------------------------------------------------
// File: NormalMapEffect.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "EffectCommon.h"

namespace DirectX
{
    namespace EffectDirtyFlags
    {
        constexpr int ConstantBufferBones = 0x100000;
    }
}

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace
{
    // Constant buffer layout. Must match the shader!
    struct NormalMapEffectConstants
    {
        XMVECTOR diffuseColor;
        XMVECTOR emissiveColor;
        XMVECTOR specularColorAndPower;

        XMVECTOR lightDirection[IEffectLights::MaxDirectionalLights];
        XMVECTOR lightDiffuseColor[IEffectLights::MaxDirectionalLights];
        XMVECTOR lightSpecularColor[IEffectLights::MaxDirectionalLights];

        XMVECTOR eyePosition;

        XMVECTOR fogColor;
        XMVECTOR fogVector;

        XMMATRIX world;
        XMVECTOR worldInverseTranspose[3];
        XMMATRIX worldViewProj;
    };

    static_assert((sizeof(NormalMapEffectConstants) % 16) == 0, "CB size not padded correctly");

    XM_ALIGNED_STRUCT(16) BoneConstants
    {
        XMVECTOR Bones[SkinnedNormalMapEffect::MaxBones][3];
    };

    static_assert((sizeof(BoneConstants) % 16) == 0, "CB size not padded correctly");

    // Traits type describes our characteristics to the EffectBase template.
    struct NormalMapEffectTraits
    {
        using ConstantBufferType = NormalMapEffectConstants;

        static constexpr int VertexShaderCount = 10;
        static constexpr int PixelShaderCount = 4;
        static constexpr int ShaderPermutationCount = 40;
    };
}

// Internal NormalMapEffect implementation class.
class NormalMapEffect::Impl : public EffectBase<NormalMapEffectTraits>
{
public:
    explicit Impl(_In_ ID3D11Device* device);

    void Initialize(_In_ ID3D11Device* device, bool enableSkinning);

    ComPtr<ID3D11ShaderResourceView> normalTexture;
    ComPtr<ID3D11ShaderResourceView> specularTexture;

    bool vertexColorEnabled;
    bool biasedVertexNormals;
    bool instancing;
    int weightsPerVertex;

    EffectLights lights;

    int GetCurrentShaderPermutation() const noexcept;

    void Apply(_In_ ID3D11DeviceContext* deviceContext);

    BoneConstants boneConstants;

private:
    ConstantBuffer<BoneConstants> mBones;
};


#pragma region Shaders
// Include the precompiled shader code.
namespace
{
#if defined(_XBOX_ONE) && defined(_TITLE)
#include "XboxOneNormalMapEffect_VSNormalPixelLightingTx.inc"
#include "XboxOneNormalMapEffect_VSNormalPixelLightingTxInst.inc"

#include "XboxOneNormalMapEffect_VSNormalPixelLightingTxVc.inc"
#include "XboxOneNormalMapEffect_VSNormalPixelLightingTxVcInst.inc"

#include "XboxOneNormalMapEffect_VSNormalPixelLightingTxBn.inc"
#include "XboxOneNormalMapEffect_VSNormalPixelLightingTxBnInst.inc"

#include "XboxOneNormalMapEffect_VSNormalPixelLightingTxVcBn.inc"
#include "XboxOneNormalMapEffect_VSNormalPixelLightingTxVcBnInst.inc"

#include "XboxOneNormalMapEffect_VSSkinnedPixelLightingTx.inc"
#include "XboxOneNormalMapEffect_VSSkinnedPixelLightingTxBn.inc"

#include "XboxOneNormalMapEffect_PSNormalPixelLightingTx.inc"
#include "XboxOneNormalMapEffect_PSNormalPixelLightingTxNoFog.inc"
#include "XboxOneNormalMapEffect_PSNormalPixelLightingTxNoSpec.inc"
#include "XboxOneNormalMapEffect_PSNormalPixelLightingTxNoFogSpec.inc"
#else
#include "NormalMapEffect_VSNormalPixelLightingTx.inc"
#include "NormalMapEffect_VSNormalPixelLightingTxInst.inc"

#include "NormalMapEffect_VSNormalPixelLightingTxVc.inc"
#include "NormalMapEffect_VSNormalPixelLightingTxVcInst.inc"

#include "NormalMapEffect_VSNormalPixelLightingTxBn.inc"
#include "NormalMapEffect_VSNormalPixelLightingTxBnInst.inc"

#include "NormalMapEffect_VSNormalPixelLightingTxVcBn.inc"
#include "NormalMapEffect_VSNormalPixelLightingTxVcBnInst.inc"

#include "NormalMapEffect_VSSkinnedPixelLightingTx.inc"
#include "NormalMapEffect_VSSkinnedPixelLightingTxBn.inc"

#include "NormalMapEffect_PSNormalPixelLightingTx.inc"
#include "NormalMapEffect_PSNormalPixelLightingTxNoFog.inc"
#include "NormalMapEffect_PSNormalPixelLightingTxNoSpec.inc"
#include "NormalMapEffect_PSNormalPixelLightingTxNoFogSpec.inc"
#endif
}


template<>
const ShaderBytecode EffectBase<NormalMapEffectTraits>::VertexShaderBytecode[] =
{
    { NormalMapEffect_VSNormalPixelLightingTx,         sizeof(NormalMapEffect_VSNormalPixelLightingTx)         },
    { NormalMapEffect_VSNormalPixelLightingTxVc,       sizeof(NormalMapEffect_VSNormalPixelLightingTxVc)       },

    { NormalMapEffect_VSNormalPixelLightingTxBn,       sizeof(NormalMapEffect_VSNormalPixelLightingTxBn)       },
    { NormalMapEffect_VSNormalPixelLightingTxVcBn,     sizeof(NormalMapEffect_VSNormalPixelLightingTxVcBn)     },

    { NormalMapEffect_VSNormalPixelLightingTxInst,     sizeof(NormalMapEffect_VSNormalPixelLightingTxInst)     },
    { NormalMapEffect_VSNormalPixelLightingTxVcInst,   sizeof(NormalMapEffect_VSNormalPixelLightingTxVcInst)   },

    { NormalMapEffect_VSNormalPixelLightingTxBnInst,   sizeof(NormalMapEffect_VSNormalPixelLightingTxBnInst)   },
    { NormalMapEffect_VSNormalPixelLightingTxVcBnInst, sizeof(NormalMapEffect_VSNormalPixelLightingTxVcBnInst) },

    { NormalMapEffect_VSSkinnedPixelLightingTx,        sizeof(NormalMapEffect_VSSkinnedPixelLightingTx)        },
    { NormalMapEffect_VSSkinnedPixelLightingTxBn,      sizeof(NormalMapEffect_VSSkinnedPixelLightingTxBn)      },
};


template<>
const int EffectBase<NormalMapEffectTraits>::VertexShaderIndices[] =
{
    0,      // pixel lighting + texture
    0,      // pixel lighting + texture, no fog
    0,      // pixel lighting + texture, no specular
    0,      // pixel lighting + texture, no fog or specular

    2,      // pixel lighting (biased vertex normal) + texture
    2,      // pixel lighting (biased vertex normal) + texture, no fog
    2,      // pixel lighting (biased vertex normal) + texture, no specular
    2,      // pixel lighting (biased vertex normal) + texture, no fog or specular

    1,      // pixel lighting + texture + vertex color
    1,      // pixel lighting + texture + vertex color, no fog
    1,      // pixel lighting + texture + vertex color, no specular
    1,      // pixel lighting + texture + vertex color, no fog or specular

    3,      // pixel lighting (biased vertex normal) + texture + vertex color
    3,      // pixel lighting (biased vertex normal) + texture + vertex color, no fog
    3,      // pixel lighting (biased vertex normal) + texture + vertex color, no specular
    3,      // pixel lighting (biased vertex normal) + texture + vertex color, no fog or specular

    4,      // instancing + pixel lighting + texture
    4,      // instancing + pixel lighting + texture, no fog
    4,      // instancing + pixel lighting + texture, no specular
    4,      // instancing + pixel lighting + texture, no fog or specular

    6,      // instancing + pixel lighting (biased vertex normal) + texture
    6,      // instancing + pixel lighting (biased vertex normal) + texture, no fog
    6,      // instancing + pixel lighting (biased vertex normal) + texture, no specular
    6,      // instancing + pixel lighting (biased vertex normal) + texture, no fog or specular

    5,      // instancing + pixel lighting + texture + vertex color
    5,      // instancing + pixel lighting + texture + vertex color, no fog
    5,      // instancing + pixel lighting + texture + vertex color, no specular
    5,      // instancing + pixel lighting + texture + vertex color, no fog or specular

    7,      // instancing + pixel lighting (biased vertex normal) + texture + vertex color
    7,      // instancing + pixel lighting (biased vertex normal) + texture + vertex color, no fog
    7,      // instancing + pixel lighting (biased vertex normal) + texture + vertex color, no specular
    7,      // instancing + pixel lighting (biased vertex normal) + texture + vertex color, no fog or specular

    8,      // skinning + pixel lighting + texture
    8,      // skinning + pixel lighting + texture, no fog
    8,      // skinning + pixel lighting + texture, no specular
    8,      // skinning + pixel lighting + texture, no fog or specular

    9,      // skinning + pixel lighting (biased vertex normal) + texture
    9,      // skinning + pixel lighting (biased vertex normal) + texture, no fog
    9,      // skinning + pixel lighting (biased vertex normal) + texture, no specular
    9,      // skinning + pixel lighting (biased vertex normal) + texture, no fog or specular
};


template<>
const ShaderBytecode EffectBase<NormalMapEffectTraits>::PixelShaderBytecode[] =
{
    { NormalMapEffect_PSNormalPixelLightingTx,          sizeof(NormalMapEffect_PSNormalPixelLightingTx)          },
    { NormalMapEffect_PSNormalPixelLightingTxNoFog,     sizeof(NormalMapEffect_PSNormalPixelLightingTxNoFog)     },
    { NormalMapEffect_PSNormalPixelLightingTxNoSpec,    sizeof(NormalMapEffect_PSNormalPixelLightingTxNoSpec)    },
    { NormalMapEffect_PSNormalPixelLightingTxNoFogSpec, sizeof(NormalMapEffect_PSNormalPixelLightingTxNoFogSpec) },
};


template<>
const int EffectBase<NormalMapEffectTraits>::PixelShaderIndices[] =
{
    0,      // pixel lighting + texture
    1,      // pixel lighting + texture, no fog
    2,      // pixel lighting + texture, no specular
    3,      // pixel lighting + texture, no fog or specular

    0,      // pixel lighting (biased vertex normal) + texture
    1,      // pixel lighting (biased vertex normal) + texture, no fog
    2,      // pixel lighting (biased vertex normal) + texture, no specular
    3,      // pixel lighting (biased vertex normal) + texture, no fog or specular

    0,      // pixel lighting + texture + vertex color
    1,      // pixel lighting + texture + vertex color, no fog
    2,      // pixel lighting + texture + vertex color, no specular
    3,      // pixel lighting + texture + vertex color, no fog or specular

    0,      // pixel lighting (biased vertex normal) + texture + vertex color
    1,      // pixel lighting (biased vertex normal) + texture + vertex color, no fog
    2,      // pixel lighting (biased vertex normal) + texture + vertex color, no specular
    3,      // pixel lighting (biased vertex normal) + texture + vertex color, no fog or specular

    0,      // instancing + pixel lighting + texture
    1,      // instancing + pixel lighting + texture, no fog
    2,      // instancing + pixel lighting + texture, no specular
    3,      // instancing + pixel lighting + texture, no fog or specular

    0,      // instancing + pixel lighting (biased vertex normal) + texture
    1,      // instancing + pixel lighting (biased vertex normal) + texture, no fog
    2,      // instancing + pixel lighting (biased vertex normal) + texture, no specular
    3,      // instancing + pixel lighting (biased vertex normal) + texture, no fog or specular

    0,      // instancing + pixel lighting + texture + vertex color
    1,      // instancing + pixel lighting + texture + vertex color, no fog
    2,      // instancing + pixel lighting + texture + vertex color, no specular
    3,      // instancing + pixel lighting + texture + vertex color, no fog or specular

    0,      // instancing + pixel lighting (biased vertex normal) + texture + vertex color
    1,      // instancing + pixel lighting (biased vertex normal) + texture + vertex color, no fog
    2,      // instancing + pixel lighting (biased vertex normal) + texture + vertex color, no specular
    3,      // instancing + pixel lighting (biased vertex normal) + texture + vertex color, no fog or specular

    0,      // skinning + pixel lighting + texture
    1,      // skinning + pixel lighting + texture, no fog
    2,      // skinning + pixel lighting + texture, no specular
    3,      // skinning + pixel lighting + texture, no fog or specular

    0,      // skinning + pixel lighting (biased vertex normal) + texture
    1,      // skinning + pixel lighting (biased vertex normal) + texture, no fog
    2,      // skinning + pixel lighting (biased vertex normal) + texture, no specular
    3,      // skinning + pixel lighting (biased vertex normal) + texture, no fog or specular
};
#pragma endregion

// Global pool of per-device NormalMapEffect resources.
template<>
SharedResourcePool<ID3D11Device*, EffectBase<NormalMapEffectTraits>::DeviceResources> EffectBase<NormalMapEffectTraits>::deviceResourcesPool = {};


// Constructor.
NormalMapEffect::Impl::Impl(_In_ ID3D11Device* device)
    : EffectBase(device),
    vertexColorEnabled(false),
    biasedVertexNormals(false),
    instancing(false),
    weightsPerVertex(0),
    boneConstants{}
{
    if (device->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0)
    {
        throw std::runtime_error("NormalMapEffect requires Feature Level 10.0 or later");
    }

    static_assert(static_cast<int>(std::size(EffectBase<NormalMapEffectTraits>::VertexShaderIndices)) == NormalMapEffectTraits::ShaderPermutationCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<NormalMapEffectTraits>::VertexShaderBytecode)) == NormalMapEffectTraits::VertexShaderCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<NormalMapEffectTraits>::PixelShaderBytecode)) == NormalMapEffectTraits::PixelShaderCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<NormalMapEffectTraits>::PixelShaderIndices)) == NormalMapEffectTraits::ShaderPermutationCount, "array/max mismatch");
}

void NormalMapEffect::Impl::Initialize(_In_ ID3D11Device* device, bool enableSkinning)
{
    lights.InitializeConstants(constants.specularColorAndPower, constants.lightDirection, constants.lightDiffuseColor, constants.lightSpecularColor);

    if (enableSkinning)
    {
        weightsPerVertex = 4;

        mBones.Create(device);

        for (size_t j = 0; j < SkinnedNormalMapEffect::MaxBones; ++j)
        {
            boneConstants.Bones[j][0] = g_XMIdentityR0;
            boneConstants.Bones[j][1] = g_XMIdentityR1;
            boneConstants.Bones[j][2] = g_XMIdentityR2;
        }
    }
}

int NormalMapEffect::Impl::GetCurrentShaderPermutation() const noexcept
{
    int permutation = 0;

    // Use optimized shaders if fog is disabled.
    if (!fog.enabled)
    {
        permutation += 1;
    }

    // Specular map?
    if (!specularTexture)
    {
        permutation += 2;
    }

    if (biasedVertexNormals)
    {
        // Compressed normals need to be scaled and biased in the vertex shader.
        permutation += 4;
    }

    if (weightsPerVertex > 0)
    {
        // Vertex skinning.
        permutation += 32;
    }
    else
    {
        // Support vertex coloring?
        if (vertexColorEnabled)
        {
            permutation += 8;
        }

        if (instancing)
        {
            // Vertex shader needs to use vertex matrix transform.
            permutation += 16;
        }
    }

    return permutation;
}


// Sets our state onto the D3D device.
void NormalMapEffect::Impl::Apply(_In_ ID3D11DeviceContext* deviceContext)
{
    assert(deviceContext != nullptr);

    // Compute derived parameter values.
    matrices.SetConstants(dirtyFlags, constants.worldViewProj);

    fog.SetConstants(dirtyFlags, matrices.worldView, constants.fogVector);

    lights.SetConstants(dirtyFlags, matrices, constants.world, constants.worldInverseTranspose, constants.eyePosition, constants.diffuseColor, constants.emissiveColor, true);

    if (weightsPerVertex > 0)
    {
    #if defined(_XBOX_ONE) && defined(_TITLE)
        void* grfxMemoryBone;
        mBones.SetData(deviceContext, boneConstants, &grfxMemoryBone);

        ComPtr<ID3D11DeviceContextX> deviceContextX;
        ThrowIfFailed(deviceContext->QueryInterface(IID_GRAPHICS_PPV_ARGS(deviceContextX.GetAddressOf())));

        deviceContextX->VSSetPlacementConstantBuffer(1, mBones.GetBuffer(), grfxMemoryBone);
    #else
        if (dirtyFlags & EffectDirtyFlags::ConstantBufferBones)
        {
            mBones.SetData(deviceContext, boneConstants);
            dirtyFlags &= ~EffectDirtyFlags::ConstantBufferBones;
        }

        ID3D11Buffer* buffer = mBones.GetBuffer();
        deviceContext->VSSetConstantBuffers(1, 1, &buffer);
    #endif
    }

    // Set the textures
    ID3D11ShaderResourceView* textures[3] =
    {
        (texture) ? texture.Get() : GetDefaultTexture(),
        (normalTexture) ? normalTexture.Get() : GetDefaultNormalTexture(),
        specularTexture.Get()
    };
    deviceContext->PSSetShaderResources(0, 3, textures);

    // Set shaders and constant buffers.
    ApplyShaders(deviceContext, GetCurrentShaderPermutation());
}


//--------------------------------------------------------------------------------------
// NormalMapEffect
//--------------------------------------------------------------------------------------

NormalMapEffect::NormalMapEffect(_In_ ID3D11Device* device, bool skinningEnabled)
    : pImpl(std::make_unique<Impl>(device))
{
    pImpl->Initialize(device, skinningEnabled);
}

NormalMapEffect::NormalMapEffect(NormalMapEffect&&) noexcept = default;
NormalMapEffect& NormalMapEffect::operator= (NormalMapEffect&&) noexcept = default;
NormalMapEffect::~NormalMapEffect() = default;


// IEffect methods.
void NormalMapEffect::Apply(_In_ ID3D11DeviceContext* deviceContext)
{
    pImpl->Apply(deviceContext);
}


void NormalMapEffect::GetVertexShaderBytecode(_Out_ void const** pShaderByteCode, _Out_ size_t* pByteCodeLength)
{
    pImpl->GetVertexShaderBytecode(pImpl->GetCurrentShaderPermutation(), pShaderByteCode, pByteCodeLength);
}


// Camera settings.
void XM_CALLCONV NormalMapEffect::SetWorld(FXMMATRIX value)
{
    pImpl->matrices.world = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::WorldInverseTranspose | EffectDirtyFlags::FogVector;
}


void XM_CALLCONV NormalMapEffect::SetView(FXMMATRIX value)
{
    pImpl->matrices.view = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::EyePosition | EffectDirtyFlags::FogVector;
}


void XM_CALLCONV NormalMapEffect::SetProjection(FXMMATRIX value)
{
    pImpl->matrices.projection = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj;
}


void XM_CALLCONV NormalMapEffect::SetMatrices(FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection)
{
    pImpl->matrices.world = world;
    pImpl->matrices.view = view;
    pImpl->matrices.projection = projection;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::WorldInverseTranspose | EffectDirtyFlags::EyePosition | EffectDirtyFlags::FogVector;
}


// Material settings.
void XM_CALLCONV NormalMapEffect::SetDiffuseColor(FXMVECTOR value)
{
    pImpl->lights.diffuseColor = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


void XM_CALLCONV NormalMapEffect::SetEmissiveColor(FXMVECTOR value)
{
    pImpl->lights.emissiveColor = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


void XM_CALLCONV NormalMapEffect::SetSpecularColor(FXMVECTOR value)
{
    // Set xyz to new value, but preserve existing w (specular power).
    pImpl->constants.specularColorAndPower = XMVectorSelect(pImpl->constants.specularColorAndPower, value, g_XMSelect1110);

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


void NormalMapEffect::SetSpecularPower(float value)
{
    // Set w to new value, but preserve existing xyz (specular color).
    pImpl->constants.specularColorAndPower = XMVectorSetW(pImpl->constants.specularColorAndPower, value);

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


void NormalMapEffect::DisableSpecular()
{
    // Set specular color to black, power to 1
    // Note: Don't use a power of 0 or the shader will generate strange highlights on non-specular materials

    pImpl->constants.specularColorAndPower = g_XMIdentityR3;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


void NormalMapEffect::SetAlpha(float value)
{
    pImpl->lights.alpha = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


void XM_CALLCONV NormalMapEffect::SetColorAndAlpha(FXMVECTOR value)
{
    pImpl->lights.diffuseColor = value;
    pImpl->lights.alpha = XMVectorGetW(value);

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


// Light settings.
void NormalMapEffect::SetLightingEnabled(bool value)
{
    if (!value)
    {
        throw std::invalid_argument("NormalMapEffect does not support turning off lighting");
    }
}


void NormalMapEffect::SetPerPixelLighting(bool)
{
    // Unsupported interface method.
}


void XM_CALLCONV NormalMapEffect::SetAmbientLightColor(FXMVECTOR value)
{
    pImpl->lights.ambientLightColor = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


void NormalMapEffect::SetLightEnabled(int whichLight, bool value)
{
    pImpl->dirtyFlags |= pImpl->lights.SetLightEnabled(whichLight, value, pImpl->constants.lightDiffuseColor, pImpl->constants.lightSpecularColor);
}


void XM_CALLCONV NormalMapEffect::SetLightDirection(int whichLight, FXMVECTOR value)
{
    EffectLights::ValidateLightIndex(whichLight);

    pImpl->constants.lightDirection[whichLight] = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


void XM_CALLCONV NormalMapEffect::SetLightDiffuseColor(int whichLight, FXMVECTOR value)
{
    pImpl->dirtyFlags |= pImpl->lights.SetLightDiffuseColor(whichLight, value, pImpl->constants.lightDiffuseColor);
}


void XM_CALLCONV NormalMapEffect::SetLightSpecularColor(int whichLight, FXMVECTOR value)
{
    pImpl->dirtyFlags |= pImpl->lights.SetLightSpecularColor(whichLight, value, pImpl->constants.lightSpecularColor);
}


void NormalMapEffect::EnableDefaultLighting()
{
    EffectLights::EnableDefaultLighting(this);
}


// Fog settings.
void NormalMapEffect::SetFogEnabled(bool value)
{
    pImpl->fog.enabled = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::FogEnable;
}


void NormalMapEffect::SetFogStart(float value)
{
    pImpl->fog.start = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::FogVector;
}


void NormalMapEffect::SetFogEnd(float value)
{
    pImpl->fog.end = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::FogVector;
}


void XM_CALLCONV NormalMapEffect::SetFogColor(FXMVECTOR value)
{
    pImpl->constants.fogColor = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


// Vertex color setting.
void NormalMapEffect::SetVertexColorEnabled(bool value)
{
    if (value && (pImpl->weightsPerVertex > 0))
    {
        throw std::invalid_argument("Per-vertex color is not supported for SkinnedNormalMapEffect");
    }

    pImpl->vertexColorEnabled = value;
}


// Texture settings.
void NormalMapEffect::SetTexture(_In_opt_ ID3D11ShaderResourceView* value)
{
    pImpl->texture = value;
}


void NormalMapEffect::SetNormalTexture(_In_opt_ ID3D11ShaderResourceView* value)
{
    pImpl->normalTexture = value;
}


void NormalMapEffect::SetSpecularTexture(_In_opt_ ID3D11ShaderResourceView* value)
{
    pImpl->specularTexture = value;
}


// Normal compression settings.
void NormalMapEffect::SetBiasedVertexNormals(bool value)
{
    pImpl->biasedVertexNormals = value;
}


// Instancing settings.
void NormalMapEffect::SetInstancingEnabled(bool value)
{
    if (value && (pImpl->weightsPerVertex > 0))
    {
        throw std::invalid_argument("Instancing is not supported for SkinnedNormalMapEffect");
    }

    pImpl->instancing = value;
}


//--------------------------------------------------------------------------------------
// SkinnedNormalMapEffect
//--------------------------------------------------------------------------------------

// Animation settings.
void SkinnedNormalMapEffect::SetWeightsPerVertex(int value)
{
    if ((value != 1) &&
        (value != 2) &&
        (value != 4))
    {
        throw std::invalid_argument("WeightsPerVertex must be 1, 2, or 4");
    }

    pImpl->weightsPerVertex = value;
}


void SkinnedNormalMapEffect::SetBoneTransforms(_In_reads_(count) XMMATRIX const* value, size_t count)
{
    if (count > MaxBones)
        throw std::invalid_argument("count parameter exceeds MaxBones");

    auto boneConstant = pImpl->boneConstants.Bones;

    for (size_t i = 0; i < count; i++)
    {
    #if DIRECTX_MATH_VERSION >= 313
        XMStoreFloat3x4A(reinterpret_cast<XMFLOAT3X4A*>(&boneConstant[i]), value[i]);
    #else
            // Xbox One XDK has an older version of DirectXMath
        XMMATRIX boneMatrix = XMMatrixTranspose(value[i]);

        boneConstant[i][0] = boneMatrix.r[0];
        boneConstant[i][1] = boneMatrix.r[1];
        boneConstant[i][2] = boneMatrix.r[2];
    #endif
    }

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBufferBones;
}


void SkinnedNormalMapEffect::ResetBoneTransforms()
{
    auto boneConstant = pImpl->boneConstants.Bones;

    for (size_t i = 0; i < MaxBones; ++i)
    {
        boneConstant[i][0] = g_XMIdentityR0;
        boneConstant[i][1] = g_XMIdentityR1;
        boneConstant[i][2] = g_XMIdentityR2;
    }

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBufferBones;
}
