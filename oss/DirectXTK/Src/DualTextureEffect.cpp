//--------------------------------------------------------------------------------------
// File: DualTextureEffect.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "EffectCommon.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace
{
    // Constant buffer layout. Must match the shader!
    struct DualTextureEffectConstants
    {
        XMVECTOR diffuseColor;
        XMVECTOR fogColor;
        XMVECTOR fogVector;
        XMMATRIX worldViewProj;
    };

    static_assert((sizeof(DualTextureEffectConstants) % 16) == 0, "CB size not padded correctly");

    // Traits type describes our characteristics to the EffectBase template.
    struct DualTextureEffectTraits
    {
        using ConstantBufferType = DualTextureEffectConstants;

        static constexpr int VertexShaderCount = 4;
        static constexpr int PixelShaderCount = 2;
        static constexpr int ShaderPermutationCount = 4;
    };
}

// Internal DualTextureEffect implementation class.
class DualTextureEffect::Impl : public EffectBase<DualTextureEffectTraits>
{
public:
    explicit Impl(_In_ ID3D11Device* device);

    bool vertexColorEnabled;

    EffectColor color;

    ComPtr<ID3D11ShaderResourceView> texture2;

    int GetCurrentShaderPermutation() const noexcept;

    void Apply(_In_ ID3D11DeviceContext* deviceContext);
};


#pragma region Shaders
// Include the precompiled shader code.
namespace
{
#if defined(_XBOX_ONE) && defined(_TITLE)
#include "XboxOneDualTextureEffect_VSDualTexture.inc"
#include "XboxOneDualTextureEffect_VSDualTextureNoFog.inc"
#include "XboxOneDualTextureEffect_VSDualTextureVc.inc"
#include "XboxOneDualTextureEffect_VSDualTextureVcNoFog.inc"

#include "XboxOneDualTextureEffect_PSDualTexture.inc"
#include "XboxOneDualTextureEffect_PSDualTextureNoFog.inc"
#else
#include "DualTextureEffect_VSDualTexture.inc"
#include "DualTextureEffect_VSDualTextureNoFog.inc"
#include "DualTextureEffect_VSDualTextureVc.inc"
#include "DualTextureEffect_VSDualTextureVcNoFog.inc"

#include "DualTextureEffect_PSDualTexture.inc"
#include "DualTextureEffect_PSDualTextureNoFog.inc"
#endif
}


template<>
const ShaderBytecode EffectBase<DualTextureEffectTraits>::VertexShaderBytecode[] =
{
    { DualTextureEffect_VSDualTexture,        sizeof(DualTextureEffect_VSDualTexture)        },
    { DualTextureEffect_VSDualTextureNoFog,   sizeof(DualTextureEffect_VSDualTextureNoFog)   },
    { DualTextureEffect_VSDualTextureVc,      sizeof(DualTextureEffect_VSDualTextureVc)      },
    { DualTextureEffect_VSDualTextureVcNoFog, sizeof(DualTextureEffect_VSDualTextureVcNoFog) },

};


template<>
const int EffectBase<DualTextureEffectTraits>::VertexShaderIndices[] =
{
    0,      // basic
    1,      // no fog
    2,      // vertex color
    3,      // vertex color, no fog
};


template<>
const ShaderBytecode EffectBase<DualTextureEffectTraits>::PixelShaderBytecode[] =
{
    { DualTextureEffect_PSDualTexture,        sizeof(DualTextureEffect_PSDualTexture)        },
    { DualTextureEffect_PSDualTextureNoFog,   sizeof(DualTextureEffect_PSDualTextureNoFog)   },

};


template<>
const int EffectBase<DualTextureEffectTraits>::PixelShaderIndices[] =
{
    0,      // basic
    1,      // no fog
    0,      // vertex color
    1,      // vertex color, no fog
};
#pragma endregion

// Global pool of per-device DualTextureEffect resources.
template<>
SharedResourcePool<ID3D11Device*, EffectBase<DualTextureEffectTraits>::DeviceResources> EffectBase<DualTextureEffectTraits>::deviceResourcesPool = {};


// Constructor.
DualTextureEffect::Impl::Impl(_In_ ID3D11Device* device)
    : EffectBase(device),
    vertexColorEnabled(false)
{
    static_assert(static_cast<int>(std::size(EffectBase<DualTextureEffectTraits>::VertexShaderIndices)) == DualTextureEffectTraits::ShaderPermutationCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<DualTextureEffectTraits>::VertexShaderBytecode)) == DualTextureEffectTraits::VertexShaderCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<DualTextureEffectTraits>::PixelShaderBytecode)) == DualTextureEffectTraits::PixelShaderCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<DualTextureEffectTraits>::PixelShaderIndices)) == DualTextureEffectTraits::ShaderPermutationCount, "array/max mismatch");
}


int DualTextureEffect::Impl::GetCurrentShaderPermutation() const noexcept
{
    int permutation = 0;

    // Use optimized shaders if fog is disabled.
    if (!fog.enabled)
    {
        permutation += 1;
    }

    // Support vertex coloring?
    if (vertexColorEnabled)
    {
        permutation += 2;
    }

    return permutation;
}


// Sets our state onto the D3D device.
void DualTextureEffect::Impl::Apply(_In_ ID3D11DeviceContext* deviceContext)
{
    assert(deviceContext != nullptr);

    // Compute derived parameter values.
    matrices.SetConstants(dirtyFlags, constants.worldViewProj);

    fog.SetConstants(dirtyFlags, matrices.worldView, constants.fogVector);

    color.SetConstants(dirtyFlags, constants.diffuseColor);

    // Set the textures.
    ID3D11ShaderResourceView* textures[2] =
    {
        texture.Get(),
        texture2.Get(),
    };

    deviceContext->PSSetShaderResources(0, 2, textures);

    // Set shaders and constant buffers.
    ApplyShaders(deviceContext, GetCurrentShaderPermutation());
}


// Public constructor.
DualTextureEffect::DualTextureEffect(_In_ ID3D11Device* device)
    : pImpl(std::make_unique<Impl>(device))
{
}


DualTextureEffect::DualTextureEffect(DualTextureEffect&&) noexcept = default;
DualTextureEffect& DualTextureEffect::operator= (DualTextureEffect&&) noexcept = default;
DualTextureEffect::~DualTextureEffect() = default;


// IEffect methods.
void DualTextureEffect::Apply(_In_ ID3D11DeviceContext* deviceContext)
{
    pImpl->Apply(deviceContext);
}


void DualTextureEffect::GetVertexShaderBytecode(_Out_ void const** pShaderByteCode, _Out_ size_t* pByteCodeLength)
{
    pImpl->GetVertexShaderBytecode(pImpl->GetCurrentShaderPermutation(), pShaderByteCode, pByteCodeLength);
}


// Camera settings.
void XM_CALLCONV DualTextureEffect::SetWorld(FXMMATRIX value)
{
    pImpl->matrices.world = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::WorldInverseTranspose | EffectDirtyFlags::FogVector;
}


void XM_CALLCONV DualTextureEffect::SetView(FXMMATRIX value)
{
    pImpl->matrices.view = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::EyePosition | EffectDirtyFlags::FogVector;
}


void XM_CALLCONV DualTextureEffect::SetProjection(FXMMATRIX value)
{
    pImpl->matrices.projection = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj;
}


void XM_CALLCONV DualTextureEffect::SetMatrices(FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection)
{
    pImpl->matrices.world = world;
    pImpl->matrices.view = view;
    pImpl->matrices.projection = projection;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::WorldInverseTranspose | EffectDirtyFlags::EyePosition | EffectDirtyFlags::FogVector;
}


// Material settings.
void XM_CALLCONV DualTextureEffect::SetDiffuseColor(FXMVECTOR value)
{
    pImpl->color.diffuseColor = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


void DualTextureEffect::SetAlpha(float value)
{
    pImpl->color.alpha = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


void XM_CALLCONV DualTextureEffect::SetColorAndAlpha(FXMVECTOR value)
{
    pImpl->color.diffuseColor = value;
    pImpl->color.alpha = XMVectorGetW(value);

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


// Fog settings.
void DualTextureEffect::SetFogEnabled(bool value)
{
    pImpl->fog.enabled = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::FogEnable;
}


void DualTextureEffect::SetFogStart(float value)
{
    pImpl->fog.start = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::FogVector;
}


void DualTextureEffect::SetFogEnd(float value)
{
    pImpl->fog.end = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::FogVector;
}


void XM_CALLCONV DualTextureEffect::SetFogColor(FXMVECTOR value)
{
    pImpl->constants.fogColor = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


// Vertex color setting.
void DualTextureEffect::SetVertexColorEnabled(bool value)
{
    pImpl->vertexColorEnabled = value;
}


// Texture settings.
void DualTextureEffect::SetTexture(_In_opt_ ID3D11ShaderResourceView* value)
{
    pImpl->texture = value;
}


void DualTextureEffect::SetTexture2(_In_opt_ ID3D11ShaderResourceView* value)
{
    pImpl->texture2 = value;
}
