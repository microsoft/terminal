//--------------------------------------------------------------------------------------
// File: AlphaTestEffect.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "EffectCommon.h"

using namespace DirectX;

namespace
{
    // Constant buffer layout. Must match the shader!
    struct AlphaTestEffectConstants
    {
        XMVECTOR diffuseColor;
        XMVECTOR alphaTest;
        XMVECTOR fogColor;
        XMVECTOR fogVector;
        XMMATRIX worldViewProj;
    };

    static_assert((sizeof(AlphaTestEffectConstants) % 16) == 0, "CB size not padded correctly");

    // Traits type describes our characteristics to the EffectBase template.
    struct AlphaTestEffectTraits
    {
        using ConstantBufferType = AlphaTestEffectConstants;

        static constexpr int VertexShaderCount = 4;
        static constexpr int PixelShaderCount = 4;
        static constexpr int ShaderPermutationCount = 8;
    };
}

// Internal AlphaTestEffect implementation class.
class AlphaTestEffect::Impl : public EffectBase<AlphaTestEffectTraits>
{
public:
    explicit Impl(_In_ ID3D11Device* device);

    D3D11_COMPARISON_FUNC alphaFunction;
    int referenceAlpha;

    bool vertexColorEnabled;

    EffectColor color;

    int GetCurrentShaderPermutation() const noexcept;

    void Apply(_In_ ID3D11DeviceContext* deviceContext);
};


#pragma region Shaders
// Include the precompiled shader code.
namespace
{
#if defined(_XBOX_ONE) && defined(_TITLE)
#include "XboxOneAlphaTestEffect_VSAlphaTest.inc"
#include "XboxOneAlphaTestEffect_VSAlphaTestNoFog.inc"
#include "XboxOneAlphaTestEffect_VSAlphaTestVc.inc"
#include "XboxOneAlphaTestEffect_VSAlphaTestVcNoFog.inc"

#include "XboxOneAlphaTestEffect_PSAlphaTestLtGt.inc"
#include "XboxOneAlphaTestEffect_PSAlphaTestLtGtNoFog.inc"
#include "XboxOneAlphaTestEffect_PSAlphaTestEqNe.inc"
#include "XboxOneAlphaTestEffect_PSAlphaTestEqNeNoFog.inc"
#else
#include "AlphaTestEffect_VSAlphaTest.inc"
#include "AlphaTestEffect_VSAlphaTestNoFog.inc"
#include "AlphaTestEffect_VSAlphaTestVc.inc"
#include "AlphaTestEffect_VSAlphaTestVcNoFog.inc"

#include "AlphaTestEffect_PSAlphaTestLtGt.inc"
#include "AlphaTestEffect_PSAlphaTestLtGtNoFog.inc"
#include "AlphaTestEffect_PSAlphaTestEqNe.inc"
#include "AlphaTestEffect_PSAlphaTestEqNeNoFog.inc"
#endif
}


template<>
const ShaderBytecode EffectBase<AlphaTestEffectTraits>::VertexShaderBytecode[] =
{
    { AlphaTestEffect_VSAlphaTest,        sizeof(AlphaTestEffect_VSAlphaTest)        },
    { AlphaTestEffect_VSAlphaTestNoFog,   sizeof(AlphaTestEffect_VSAlphaTestNoFog)   },
    { AlphaTestEffect_VSAlphaTestVc,      sizeof(AlphaTestEffect_VSAlphaTestVc)      },
    { AlphaTestEffect_VSAlphaTestVcNoFog, sizeof(AlphaTestEffect_VSAlphaTestVcNoFog) },
};


template<>
const int EffectBase<AlphaTestEffectTraits>::VertexShaderIndices[] =
{
    0,      // lt/gt
    1,      // lt/gt, no fog
    2,      // lt/gt, vertex color
    3,      // lt/gt, vertex color, no fog

    0,      // eq/ne
    1,      // eq/ne, no fog
    2,      // eq/ne, vertex color
    3,      // eq/ne, vertex color, no fog
};


template<>
const ShaderBytecode EffectBase<AlphaTestEffectTraits>::PixelShaderBytecode[] =
{
    { AlphaTestEffect_PSAlphaTestLtGt,      sizeof(AlphaTestEffect_PSAlphaTestLtGt)      },
    { AlphaTestEffect_PSAlphaTestLtGtNoFog, sizeof(AlphaTestEffect_PSAlphaTestLtGtNoFog) },
    { AlphaTestEffect_PSAlphaTestEqNe,      sizeof(AlphaTestEffect_PSAlphaTestEqNe)      },
    { AlphaTestEffect_PSAlphaTestEqNeNoFog, sizeof(AlphaTestEffect_PSAlphaTestEqNeNoFog) },
};


template<>
const int EffectBase<AlphaTestEffectTraits>::PixelShaderIndices[] =
{
    0,      // lt/gt
    1,      // lt/gt, no fog
    0,      // lt/gt, vertex color
    1,      // lt/gt, vertex color, no fog

    2,      // eq/ne
    3,      // eq/ne, no fog
    2,      // eq/ne, vertex color
    3,      // eq/ne, vertex color, no fog
};
#pragma endregion

// Global pool of per-device AlphaTestEffect resources.
template<>
SharedResourcePool<ID3D11Device*, EffectBase<AlphaTestEffectTraits>::DeviceResources> EffectBase<AlphaTestEffectTraits>::deviceResourcesPool = {};


// Constructor.
AlphaTestEffect::Impl::Impl(_In_ ID3D11Device* device)
    : EffectBase(device),
    alphaFunction(D3D11_COMPARISON_GREATER),
    referenceAlpha(0),
    vertexColorEnabled(false)
{
    static_assert(static_cast<int>(std::size(EffectBase<AlphaTestEffectTraits>::VertexShaderIndices)) == AlphaTestEffectTraits::ShaderPermutationCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<AlphaTestEffectTraits>::VertexShaderBytecode)) == AlphaTestEffectTraits::VertexShaderCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<AlphaTestEffectTraits>::PixelShaderBytecode)) == AlphaTestEffectTraits::PixelShaderCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<AlphaTestEffectTraits>::PixelShaderIndices)) == AlphaTestEffectTraits::ShaderPermutationCount, "array/max mismatch");
}


int AlphaTestEffect::Impl::GetCurrentShaderPermutation() const noexcept
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

    // Which alpha compare mode?
    if (alphaFunction == D3D11_COMPARISON_EQUAL ||
        alphaFunction == D3D11_COMPARISON_NOT_EQUAL)
    {
        permutation += 4;
    }

    return permutation;
}


// Sets our state onto the D3D device.
void AlphaTestEffect::Impl::Apply(_In_ ID3D11DeviceContext* deviceContext)
{
    assert(deviceContext != nullptr);

    // Compute derived parameter values.
    matrices.SetConstants(dirtyFlags, constants.worldViewProj);

    fog.SetConstants(dirtyFlags, matrices.worldView, constants.fogVector);

    color.SetConstants(dirtyFlags, constants.diffuseColor);

    // Recompute the alpha test settings?
    if (dirtyFlags & EffectDirtyFlags::AlphaTest)
    {
        // Convert reference alpha from 8 bit integer to 0-1 float format.
        auto const reference = static_cast<float>(referenceAlpha) / 255.0f;

        // Comparison tolerance of half the 8 bit integer precision.
        constexpr float threshold = 0.5f / 255.0f;

        // What to do if the alpha comparison passes or fails. Positive accepts the pixel, negative clips it.
        static const XMVECTORF32 selectIfTrue = { { {  1, -1 } } };
        static const XMVECTORF32 selectIfFalse = { { { -1,  1 } } };
        static const XMVECTORF32 selectNever = { { { -1, -1 } } };
        static const XMVECTORF32 selectAlways = { { {  1,  1 } } };

        float compareTo;
        XMVECTOR resultSelector;

        switch (alphaFunction)
        {
        case D3D11_COMPARISON_LESS:
            // Shader will evaluate: clip((a < x) ? z : w)
            compareTo = reference - threshold;
            resultSelector = selectIfTrue;
            break;

        case D3D11_COMPARISON_LESS_EQUAL:
            // Shader will evaluate: clip((a < x) ? z : w)
            compareTo = reference + threshold;
            resultSelector = selectIfTrue;
            break;

        case D3D11_COMPARISON_GREATER_EQUAL:
            // Shader will evaluate: clip((a < x) ? z : w)
            compareTo = reference - threshold;
            resultSelector = selectIfFalse;
            break;

        case D3D11_COMPARISON_GREATER:
            // Shader will evaluate: clip((a < x) ? z : w)
            compareTo = reference + threshold;
            resultSelector = selectIfFalse;
            break;

        case D3D11_COMPARISON_EQUAL:
            // Shader will evaluate: clip((abs(a - x) < y) ? z : w)
            compareTo = reference;
            resultSelector = selectIfTrue;
            break;

        case D3D11_COMPARISON_NOT_EQUAL:
            // Shader will evaluate: clip((abs(a - x) < y) ? z : w)
            compareTo = reference;
            resultSelector = selectIfFalse;
            break;

        case D3D11_COMPARISON_NEVER:
            // Shader will evaluate: clip((a < x) ? z : w)
            compareTo = 0;
            resultSelector = selectNever;
            break;

        case D3D11_COMPARISON_ALWAYS:
            // Shader will evaluate: clip((a < x) ? z : w)
            compareTo = 0;
            resultSelector = selectAlways;
            break;

        default:
            throw std::runtime_error("Unknown alpha test function");
        }

        // x = compareTo, y = threshold, zw = resultSelector.
        constants.alphaTest = XMVectorPermute<0, 1, 4, 5>(XMVectorSet(compareTo, threshold, 0, 0), resultSelector);

        dirtyFlags &= ~EffectDirtyFlags::AlphaTest;
        dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
    }

    // Set the texture.
    deviceContext->PSSetShaderResources(0, 1, texture.GetAddressOf());

    // Set shaders and constant buffers.
    ApplyShaders(deviceContext, GetCurrentShaderPermutation());
}


// Public constructor.
AlphaTestEffect::AlphaTestEffect(_In_ ID3D11Device* device)
    : pImpl(std::make_unique<Impl>(device))
{
}


AlphaTestEffect::AlphaTestEffect(AlphaTestEffect&&) noexcept = default;
AlphaTestEffect& AlphaTestEffect::operator= (AlphaTestEffect&&) noexcept = default;
AlphaTestEffect::~AlphaTestEffect() = default;


// IEffect methods.
void AlphaTestEffect::Apply(_In_ ID3D11DeviceContext* deviceContext)
{
    pImpl->Apply(deviceContext);
}


void AlphaTestEffect::GetVertexShaderBytecode(_Out_ void const** pShaderByteCode, _Out_ size_t* pByteCodeLength)
{
    pImpl->GetVertexShaderBytecode(pImpl->GetCurrentShaderPermutation(), pShaderByteCode, pByteCodeLength);
}


// Camera settings.
void XM_CALLCONV AlphaTestEffect::SetWorld(FXMMATRIX value)
{
    pImpl->matrices.world = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::WorldInverseTranspose | EffectDirtyFlags::FogVector;
}


void XM_CALLCONV AlphaTestEffect::SetView(FXMMATRIX value)
{
    pImpl->matrices.view = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::EyePosition | EffectDirtyFlags::FogVector;
}


void XM_CALLCONV AlphaTestEffect::SetProjection(FXMMATRIX value)
{
    pImpl->matrices.projection = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj;
}


void XM_CALLCONV AlphaTestEffect::SetMatrices(FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection)
{
    pImpl->matrices.world = world;
    pImpl->matrices.view = view;
    pImpl->matrices.projection = projection;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::WorldInverseTranspose | EffectDirtyFlags::EyePosition | EffectDirtyFlags::FogVector;
}


// Material settings
void XM_CALLCONV AlphaTestEffect::SetDiffuseColor(FXMVECTOR value)
{
    pImpl->color.diffuseColor = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


void AlphaTestEffect::SetAlpha(float value)
{
    pImpl->color.alpha = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


void XM_CALLCONV AlphaTestEffect::SetColorAndAlpha(FXMVECTOR value)
{
    pImpl->color.diffuseColor = value;
    pImpl->color.alpha = XMVectorGetW(value);

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


// Fog settings.
void AlphaTestEffect::SetFogEnabled(bool value)
{
    pImpl->fog.enabled = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::FogEnable;
}


void AlphaTestEffect::SetFogStart(float value)
{
    pImpl->fog.start = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::FogVector;
}


void AlphaTestEffect::SetFogEnd(float value)
{
    pImpl->fog.end = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::FogVector;
}


void XM_CALLCONV AlphaTestEffect::SetFogColor(FXMVECTOR value)
{
    pImpl->constants.fogColor = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


// Vertex color setting.
void AlphaTestEffect::SetVertexColorEnabled(bool value)
{
    pImpl->vertexColorEnabled = value;
}


// Texture settings.
void AlphaTestEffect::SetTexture(_In_opt_ ID3D11ShaderResourceView* value)
{
    pImpl->texture = value;
}


void AlphaTestEffect::SetAlphaFunction(D3D11_COMPARISON_FUNC value)
{
    pImpl->alphaFunction = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::AlphaTest;
}


void AlphaTestEffect::SetReferenceAlpha(int value)
{
    pImpl->referenceAlpha = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::AlphaTest;
}
