//--------------------------------------------------------------------------------------
// File: PBREffect.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
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
    struct PBREffectConstants
    {
        XMVECTOR eyePosition;
        XMMATRIX world;
        XMVECTOR worldInverseTranspose[3];
        XMMATRIX worldViewProj;
        XMMATRIX prevWorldViewProj; // for velocity generation

        XMVECTOR lightDirection[IEffectLights::MaxDirectionalLights];
        XMVECTOR lightDiffuseColor[IEffectLights::MaxDirectionalLights];

        // PBR Parameters
        XMVECTOR Albedo;
        float    Metallic;
        float    Roughness;
        int      numRadianceMipLevels;

        // Size of render target
        float   targetWidth;
        float   targetHeight;
    };

    static_assert((sizeof(PBREffectConstants) % 16) == 0, "CB size not padded correctly");

    XM_ALIGNED_STRUCT(16) BoneConstants
    {
        XMVECTOR Bones[SkinnedPBREffect::MaxBones][3];
    };

    static_assert((sizeof(BoneConstants) % 16) == 0, "CB size not padded correctly");

    // Traits type describes our characteristics to the EffectBase template.
    struct PBREffectTraits
    {
        using ConstantBufferType = PBREffectConstants;

        static constexpr int VertexShaderCount = 8;
        static constexpr int PixelShaderCount = 5;
        static constexpr int ShaderPermutationCount = 22;
        static constexpr int RootSignatureCount = 1;
    };
}

// Internal PBREffect implementation class.
class PBREffect::Impl : public EffectBase<PBREffectTraits>
{
public:
    explicit Impl(_In_ ID3D11Device* device);

    void Initialize(_In_ ID3D11Device* device, bool enableSkinning);

    ComPtr<ID3D11ShaderResourceView> albedoTexture;
    ComPtr<ID3D11ShaderResourceView> normalTexture;
    ComPtr<ID3D11ShaderResourceView> rmaTexture;
    ComPtr<ID3D11ShaderResourceView> emissiveTexture;

    ComPtr<ID3D11ShaderResourceView> radianceTexture;
    ComPtr<ID3D11ShaderResourceView> irradianceTexture;

    bool biasedVertexNormals;
    bool velocityEnabled;
    bool instancing;
    int weightsPerVertex;

    XMVECTOR lightColor[MaxDirectionalLights];

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
#include "XboxOnePBREffect_VSConstant.inc"
#include "XboxOnePBREffect_VSConstantBn.inc"

#include "XboxOnePBREffect_VSConstantInst.inc"
#include "XboxOnePBREffect_VSConstantBnInst.inc"

#include "XboxOnePBREffect_VSConstantVelocity.inc"
#include "XboxOnePBREffect_VSConstantVelocityBn.inc"

#include "XboxOnePBREffect_VSSkinned.inc"
#include "XboxOnePBREffect_VSSkinnedBn.inc"

#include "XboxOnePBREffect_PSConstant.inc"
#include "XboxOnePBREffect_PSTextured.inc"
#include "XboxOnePBREffect_PSTexturedEmissive.inc"
#include "XboxOnePBREffect_PSTexturedVelocity.inc"
#include "XboxOnePBREffect_PSTexturedEmissiveVelocity.inc"
#else
#include "PBREffect_VSConstant.inc"
#include "PBREffect_VSConstantBn.inc"

#include "PBREffect_VSConstantInst.inc"
#include "PBREffect_VSConstantBnInst.inc"

#include "PBREffect_VSConstantVelocity.inc"
#include "PBREffect_VSConstantVelocityBn.inc"

#include "PBREffect_VSSkinned.inc"
#include "PBREffect_VSSkinnedBn.inc"

#include "PBREffect_PSConstant.inc"
#include "PBREffect_PSTextured.inc"
#include "PBREffect_PSTexturedEmissive.inc"
#include "PBREffect_PSTexturedVelocity.inc"
#include "PBREffect_PSTexturedEmissiveVelocity.inc"
#endif
}


template<>
const ShaderBytecode EffectBase<PBREffectTraits>::VertexShaderBytecode[] =
{
    { PBREffect_VSConstant,           sizeof(PBREffect_VSConstant)           },
    { PBREffect_VSConstantVelocity,   sizeof(PBREffect_VSConstantVelocity)   },
    { PBREffect_VSConstantBn,         sizeof(PBREffect_VSConstantBn)         },
    { PBREffect_VSConstantVelocityBn, sizeof(PBREffect_VSConstantVelocityBn) },

    { PBREffect_VSConstantInst,       sizeof(PBREffect_VSConstantInst)       },
    { PBREffect_VSConstantBnInst,     sizeof(PBREffect_VSConstantBnInst)     },

    { PBREffect_VSSkinned,            sizeof(PBREffect_VSSkinned)            },
    { PBREffect_VSSkinnedBn,          sizeof(PBREffect_VSSkinnedBn)          },
};


template<>
const int EffectBase<PBREffectTraits>::VertexShaderIndices[] =
{
    0,      // constant
    0,      // textured
    0,      // textured + emissive

    4,      // instancing + constant
    4,      // instancing + textured
    4,      // instancing + textured + emissive

    6,      // skinning + constant
    6,      // skinning + textured
    6,      // skinning + textured + emissive

    1,      // textured + velocity
    1,      // textured + emissive + velocity

    2,      // constant (biased vertex normals)
    2,      // textured (biased vertex normals)
    2,      // textured + emissive (biased vertex normals)

    5,      // instancing + constant (biased vertex normals)
    5,      // instancing + textured (biased vertex normals)
    5,      // instancing + textured + emissive (biased vertex normals)

    7,      // skinning + constant (biased vertex normals)
    7,      // skinning + textured (biased vertex normals)
    7,      // skinning + textured + emissive (biased vertex normals)

    3,      // textured + velocity (biased vertex normals)
    3,      // textured + emissive + velocity (biased vertex normals)
};


template<>
const ShaderBytecode EffectBase<PBREffectTraits>::PixelShaderBytecode[] =
{
    { PBREffect_PSConstant,                 sizeof(PBREffect_PSConstant)                 },
    { PBREffect_PSTextured,                 sizeof(PBREffect_PSTextured)                 },
    { PBREffect_PSTexturedEmissive,         sizeof(PBREffect_PSTexturedEmissive)         },
    { PBREffect_PSTexturedVelocity,         sizeof(PBREffect_PSTexturedVelocity)         },
    { PBREffect_PSTexturedEmissiveVelocity, sizeof(PBREffect_PSTexturedEmissiveVelocity) },
};


template<>
const int EffectBase<PBREffectTraits>::PixelShaderIndices[] =
{
    0,      // constant
    1,      // textured
    2,      // textured + emissive

    0,      // instancing + constant
    1,      // instancing + textured
    2,      // instancing + textured + emissive

    0,      // skinning + constant
    1,      // skinning + textured
    2,      // skinning + textured + emissive

    3,      // textured + velocity
    4,      // textured + emissive + velocity

    0,      // constant (biased vertex normals)
    1,      // textured (biased vertex normals)
    2,      // textured + emissive (biased vertex normals)

    0,      // instancing + constant (biased vertex normals)
    1,      // instancing + textured (biased vertex normals)
    2,      // instancing + textured + emissive (biased vertex normals)

    0,      // skinning + constant (biased vertex normals)
    1,      // skinning + textured (biased vertex normals)
    2,      // skinning + textured + emissive (biased vertex normals)

    3,      // textured + velocity (biased vertex normals)
    4,      // textured + emissive + velocity (biased vertex normals)
};
#pragma endregion

// Global pool of per-device PBREffect resources. Required by EffectBase<>, but not used.
template<>
SharedResourcePool<ID3D11Device*, EffectBase<PBREffectTraits>::DeviceResources> EffectBase<PBREffectTraits>::deviceResourcesPool = {};

// Constructor.
PBREffect::Impl::Impl(_In_ ID3D11Device* device)
    : EffectBase(device),
    biasedVertexNormals(false),
    velocityEnabled(false),
    instancing(false),
    weightsPerVertex(0),
    lightColor{},
    boneConstants{}
{
    if (device->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0)
    {
        throw std::runtime_error("PBREffect requires Feature Level 10.0 or later");
    }

    static_assert(static_cast<int>(std::size(EffectBase<PBREffectTraits>::VertexShaderIndices)) == PBREffectTraits::ShaderPermutationCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<PBREffectTraits>::VertexShaderBytecode)) == PBREffectTraits::VertexShaderCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<PBREffectTraits>::PixelShaderBytecode)) == PBREffectTraits::PixelShaderCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<PBREffectTraits>::PixelShaderIndices)) == PBREffectTraits::ShaderPermutationCount, "array/max mismatch");
}

void PBREffect::Impl::Initialize(_In_ ID3D11Device* device, bool enableSkinning)
{
    // Lighting
    static const XMVECTORF32 defaultLightDirection = { { { 0, -1, 0, 0 } } };
    for (int i = 0; i < MaxDirectionalLights; i++)
    {
        lightColor[i] = g_XMOne;
        constants.lightDirection[i] = defaultLightDirection;
        constants.lightDiffuseColor[i] = g_XMZero;
    }

    // Default PBR values
    constants.Albedo = g_XMOne;
    constants.Metallic = 0.5f;
    constants.Roughness = 0.2f;
    constants.numRadianceMipLevels = 1;

    if (enableSkinning)
    {
        weightsPerVertex = 4;

        mBones.Create(device);

        for (size_t j = 0; j < SkinnedPBREffect::MaxBones; ++j)
        {
            boneConstants.Bones[j][0] = g_XMIdentityR0;
            boneConstants.Bones[j][1] = g_XMIdentityR1;
            boneConstants.Bones[j][2] = g_XMIdentityR2;
        }
    }
}

int PBREffect::Impl::GetCurrentShaderPermutation() const noexcept
{
    int permutation = 0;

    // Using an emissive texture?
    if (emissiveTexture)
    {
        permutation += 1;
    }

    if (biasedVertexNormals)
    {
        // Compressed normals need to be scaled and biased in the vertex shader.
        permutation += 11;
    }

    if (weightsPerVertex > 0)
    {
        // Vertex skinning.
        permutation += 6;
    }
    else if (instancing)
    {
        // Vertex shader needs to use vertex matrix transform.
        permutation += 3;
    }
    else if (velocityEnabled)
    {
        // Optional velocity buffer (implies textured).
        permutation += 9;
    }

    if (albedoTexture && !velocityEnabled)
    {
        // Textured RMA vs. constant albedo/roughness/metalness.
        permutation += 1;
    }

    return permutation;
}


// Sets our state onto the D3D device.
void PBREffect::Impl::Apply(_In_ ID3D11DeviceContext* deviceContext)
{
    assert(deviceContext != nullptr);

    // Store old wvp for velocity calculation in shader
    constants.prevWorldViewProj = constants.worldViewProj;

    // Compute derived parameter values.
    matrices.SetConstants(dirtyFlags, constants.worldViewProj);

    // World inverse transpose matrix.
    if (dirtyFlags & EffectDirtyFlags::WorldInverseTranspose)
    {
        constants.world = XMMatrixTranspose(matrices.world);

        const XMMATRIX worldInverse = XMMatrixInverse(nullptr, matrices.world);

        constants.worldInverseTranspose[0] = worldInverse.r[0];
        constants.worldInverseTranspose[1] = worldInverse.r[1];
        constants.worldInverseTranspose[2] = worldInverse.r[2];

        dirtyFlags &= ~EffectDirtyFlags::WorldInverseTranspose;
        dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
    }

    // Eye position vector.
    if (dirtyFlags & EffectDirtyFlags::EyePosition)
    {
        const XMMATRIX viewInverse = XMMatrixInverse(nullptr, matrices.view);

        constants.eyePosition = viewInverse.r[3];

        dirtyFlags &= ~EffectDirtyFlags::EyePosition;
        dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
    }

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
    if (albedoTexture)
    {
        ID3D11ShaderResourceView* textures[6] =
        {
            albedoTexture.Get(), normalTexture.Get(), rmaTexture.Get(),
            emissiveTexture.Get(),
            radianceTexture.Get(), irradianceTexture.Get()
        };
        deviceContext->PSSetShaderResources(0, 6, textures);
    }
    else
    {
        ID3D11ShaderResourceView* textures[6] =
        {
            nullptr, nullptr, nullptr,
            nullptr,
            radianceTexture.Get(), irradianceTexture.Get()
        };
        deviceContext->PSSetShaderResources(0, 6, textures);
    }

    // Set shaders and constant buffers.
    ApplyShaders(deviceContext, GetCurrentShaderPermutation());
}


//--------------------------------------------------------------------------------------
// PBREffect
//--------------------------------------------------------------------------------------

PBREffect::PBREffect(_In_ ID3D11Device* device, bool skinningEnabled)
    : pImpl(std::make_unique<Impl>(device))
{
    pImpl->Initialize(device, skinningEnabled);
}

PBREffect::PBREffect(PBREffect&&) noexcept = default;
PBREffect& PBREffect::operator= (PBREffect&&) noexcept = default;
PBREffect::~PBREffect() = default;


// IEffect methods.
void PBREffect::Apply(_In_ ID3D11DeviceContext* deviceContext)
{
    pImpl->Apply(deviceContext);
}


void PBREffect::GetVertexShaderBytecode(_Out_ void const** pShaderByteCode, _Out_ size_t* pByteCodeLength)
{
    pImpl->GetVertexShaderBytecode(pImpl->GetCurrentShaderPermutation(), pShaderByteCode, pByteCodeLength);
}


// Camera settings.
void XM_CALLCONV PBREffect::SetWorld(FXMMATRIX value)
{
    pImpl->matrices.world = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::WorldInverseTranspose;
}


void XM_CALLCONV PBREffect::SetView(FXMMATRIX value)
{
    pImpl->matrices.view = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::EyePosition;
}


void XM_CALLCONV PBREffect::SetProjection(FXMMATRIX value)
{
    pImpl->matrices.projection = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj;
}


void XM_CALLCONV PBREffect::SetMatrices(FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection)
{
    pImpl->matrices.world = world;
    pImpl->matrices.view = view;
    pImpl->matrices.projection = projection;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::WorldInverseTranspose | EffectDirtyFlags::EyePosition;
}


// Light settings
void PBREffect::SetLightingEnabled(bool value)
{
    if (!value)
    {
        throw std::invalid_argument("PBREffect does not support turning off lighting");
    }
}


void PBREffect::SetPerPixelLighting(bool)
{
    // Unsupported interface method.
}


void XM_CALLCONV PBREffect::SetAmbientLightColor(FXMVECTOR)
{
    // Unsupported interface.
}


void PBREffect::SetLightEnabled(int whichLight, bool value)
{
    EffectLights::ValidateLightIndex(whichLight);

    pImpl->constants.lightDiffuseColor[whichLight] = (value) ? pImpl->lightColor[whichLight] : g_XMZero;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


void XM_CALLCONV PBREffect::SetLightDirection(int whichLight, FXMVECTOR value)
{
    EffectLights::ValidateLightIndex(whichLight);

    pImpl->constants.lightDirection[whichLight] = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


void XM_CALLCONV PBREffect::SetLightDiffuseColor(int whichLight, FXMVECTOR value)
{
    EffectLights::ValidateLightIndex(whichLight);

    pImpl->lightColor[whichLight] = value;
    pImpl->constants.lightDiffuseColor[whichLight] = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


void XM_CALLCONV PBREffect::SetLightSpecularColor(int, FXMVECTOR)
{
    // Unsupported interface.
}


void PBREffect::EnableDefaultLighting()
{
    EffectLights::EnableDefaultLighting(this);
}


// PBR Settings
void PBREffect::SetAlpha(float value)
{
    // Set w to new value, but preserve existing xyz (constant albedo).
    pImpl->constants.Albedo = XMVectorSetW(pImpl->constants.Albedo, value);

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


void PBREffect::SetConstantAlbedo(FXMVECTOR value)
{
    // Set xyz to new value, but preserve existing w (alpha).
    pImpl->constants.Albedo = XMVectorSelect(pImpl->constants.Albedo, value, g_XMSelect1110);

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


void PBREffect::SetConstantMetallic(float value)
{
    pImpl->constants.Metallic = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


void PBREffect::SetConstantRoughness(float value)
{
    pImpl->constants.Roughness = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


// Texture settings.
void PBREffect::SetAlbedoTexture(_In_opt_ ID3D11ShaderResourceView* value)
{
    pImpl->albedoTexture = value;
}


void PBREffect::SetNormalTexture(_In_opt_ ID3D11ShaderResourceView* value)
{
    pImpl->normalTexture = value;
}


void PBREffect::SetRMATexture(_In_opt_ ID3D11ShaderResourceView* value)
{
    pImpl->rmaTexture = value;
}

void PBREffect::SetEmissiveTexture(_In_opt_ ID3D11ShaderResourceView* value)
{
    pImpl->emissiveTexture = value;
}


void PBREffect::SetSurfaceTextures(
    _In_opt_ ID3D11ShaderResourceView* albedo,
    _In_opt_ ID3D11ShaderResourceView* normal,
    _In_opt_ ID3D11ShaderResourceView* roughnessMetallicAmbientOcclusion)
{
    pImpl->albedoTexture = albedo;
    pImpl->normalTexture = normal;
    pImpl->rmaTexture = roughnessMetallicAmbientOcclusion;
}


void PBREffect::SetIBLTextures(
    _In_opt_ ID3D11ShaderResourceView* radiance,
    int numRadianceMips,
    _In_opt_ ID3D11ShaderResourceView* irradiance)
{
    pImpl->radianceTexture = radiance;
    pImpl->irradianceTexture = irradiance;

    pImpl->constants.numRadianceMipLevels = numRadianceMips;
    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


// Normal compression settings.
void PBREffect::SetBiasedVertexNormals(bool value)
{
    pImpl->biasedVertexNormals = value;
}


// Instancing settings.
void PBREffect::SetInstancingEnabled(bool value)
{
    if (value && (pImpl->weightsPerVertex > 0))
    {
        throw std::invalid_argument("Instancing is not supported for SkinnedPBREffect");
    }

    pImpl->instancing = value;
}


// Additional settings.
void PBREffect::SetVelocityGeneration(bool value)
{
    if (value && (pImpl->weightsPerVertex > 0))
    {
        throw std::invalid_argument("Velocity generation is not supported for SkinnedPBREffect");
    }

    pImpl->velocityEnabled = value;
}


void PBREffect::SetRenderTargetSizeInPixels(int width, int height)
{
    pImpl->constants.targetWidth = static_cast<float>(width);
    pImpl->constants.targetHeight = static_cast<float>(height);

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


//--------------------------------------------------------------------------------------
// SkinnedPBREffect
//--------------------------------------------------------------------------------------

// Animation settings.
void SkinnedPBREffect::SetWeightsPerVertex(int value)
{
    if ((value != 1) &&
        (value != 2) &&
        (value != 4))
    {
        throw std::invalid_argument("WeightsPerVertex must be 1, 2, or 4");
    }

    pImpl->weightsPerVertex = value;
}


void SkinnedPBREffect::SetBoneTransforms(_In_reads_(count) XMMATRIX const* value, size_t count)
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


void SkinnedPBREffect::ResetBoneTransforms()
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
