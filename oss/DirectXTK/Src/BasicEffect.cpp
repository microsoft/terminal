//--------------------------------------------------------------------------------------
// File: BasicEffect.cpp
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
    struct BasicEffectConstants
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

    static_assert((sizeof(BasicEffectConstants) % 16) == 0, "CB size not padded correctly");

    // Traits type describes our characteristics to the EffectBase template.
    struct BasicEffectTraits
    {
        using ConstantBufferType = BasicEffectConstants;

        static constexpr int VertexShaderCount = 32;
        static constexpr int PixelShaderCount = 10;
        static constexpr int ShaderPermutationCount = 56;
    };
}

// Internal BasicEffect implementation class.
class BasicEffect::Impl : public EffectBase<BasicEffectTraits>
{
public:
    explicit Impl(_In_ ID3D11Device* device);

    bool lightingEnabled;
    bool preferPerPixelLighting;
    bool vertexColorEnabled;
    bool textureEnabled;
    bool biasedVertexNormals;

    EffectLights lights;

    int GetCurrentShaderPermutation() const noexcept;

    void Apply(_In_ ID3D11DeviceContext* deviceContext);
};


#pragma region Shaders
// Include the precompiled shader code.
namespace
{
#if defined(_XBOX_ONE) && defined(_TITLE)
#include "XboxOneBasicEffect_VSBasic.inc"
#include "XboxOneBasicEffect_VSBasicNoFog.inc"
#include "XboxOneBasicEffect_VSBasicVc.inc"
#include "XboxOneBasicEffect_VSBasicVcNoFog.inc"
#include "XboxOneBasicEffect_VSBasicTx.inc"
#include "XboxOneBasicEffect_VSBasicTxNoFog.inc"
#include "XboxOneBasicEffect_VSBasicTxVc.inc"
#include "XboxOneBasicEffect_VSBasicTxVcNoFog.inc"

#include "XboxOneBasicEffect_VSBasicVertexLighting.inc"
#include "XboxOneBasicEffect_VSBasicVertexLightingVc.inc"
#include "XboxOneBasicEffect_VSBasicVertexLightingTx.inc"
#include "XboxOneBasicEffect_VSBasicVertexLightingTxVc.inc"

#include "XboxOneBasicEffect_VSBasicOneLight.inc"
#include "XboxOneBasicEffect_VSBasicOneLightVc.inc"
#include "XboxOneBasicEffect_VSBasicOneLightTx.inc"
#include "XboxOneBasicEffect_VSBasicOneLightTxVc.inc"

#include "XboxOneBasicEffect_VSBasicPixelLighting.inc"
#include "XboxOneBasicEffect_VSBasicPixelLightingVc.inc"
#include "XboxOneBasicEffect_VSBasicPixelLightingTx.inc"
#include "XboxOneBasicEffect_VSBasicPixelLightingTxVc.inc"

#include "XboxOneBasicEffect_VSBasicVertexLightingBn.inc"
#include "XboxOneBasicEffect_VSBasicVertexLightingVcBn.inc"
#include "XboxOneBasicEffect_VSBasicVertexLightingTxBn.inc"
#include "XboxOneBasicEffect_VSBasicVertexLightingTxVcBn.inc"

#include "XboxOneBasicEffect_VSBasicOneLightBn.inc"
#include "XboxOneBasicEffect_VSBasicOneLightVcBn.inc"
#include "XboxOneBasicEffect_VSBasicOneLightTxBn.inc"
#include "XboxOneBasicEffect_VSBasicOneLightTxVcBn.inc"

#include "XboxOneBasicEffect_VSBasicPixelLightingBn.inc"
#include "XboxOneBasicEffect_VSBasicPixelLightingVcBn.inc"
#include "XboxOneBasicEffect_VSBasicPixelLightingTxBn.inc"
#include "XboxOneBasicEffect_VSBasicPixelLightingTxVcBn.inc"

#include "XboxOneBasicEffect_PSBasic.inc"
#include "XboxOneBasicEffect_PSBasicNoFog.inc"
#include "XboxOneBasicEffect_PSBasicTx.inc"
#include "XboxOneBasicEffect_PSBasicTxNoFog.inc"

#include "XboxOneBasicEffect_PSBasicVertexLighting.inc"
#include "XboxOneBasicEffect_PSBasicVertexLightingNoFog.inc"
#include "XboxOneBasicEffect_PSBasicVertexLightingTx.inc"
#include "XboxOneBasicEffect_PSBasicVertexLightingTxNoFog.inc"

#include "XboxOneBasicEffect_PSBasicPixelLighting.inc"
#include "XboxOneBasicEffect_PSBasicPixelLightingTx.inc"
#else
#include "BasicEffect_VSBasic.inc"
#include "BasicEffect_VSBasicNoFog.inc"
#include "BasicEffect_VSBasicVc.inc"
#include "BasicEffect_VSBasicVcNoFog.inc"
#include "BasicEffect_VSBasicTx.inc"
#include "BasicEffect_VSBasicTxNoFog.inc"
#include "BasicEffect_VSBasicTxVc.inc"
#include "BasicEffect_VSBasicTxVcNoFog.inc"

#include "BasicEffect_VSBasicVertexLighting.inc"
#include "BasicEffect_VSBasicVertexLightingVc.inc"
#include "BasicEffect_VSBasicVertexLightingTx.inc"
#include "BasicEffect_VSBasicVertexLightingTxVc.inc"

#include "BasicEffect_VSBasicOneLight.inc"
#include "BasicEffect_VSBasicOneLightVc.inc"
#include "BasicEffect_VSBasicOneLightTx.inc"
#include "BasicEffect_VSBasicOneLightTxVc.inc"

#include "BasicEffect_VSBasicPixelLighting.inc"
#include "BasicEffect_VSBasicPixelLightingVc.inc"
#include "BasicEffect_VSBasicPixelLightingTx.inc"
#include "BasicEffect_VSBasicPixelLightingTxVc.inc"

#include "BasicEffect_VSBasicVertexLightingBn.inc"
#include "BasicEffect_VSBasicVertexLightingVcBn.inc"
#include "BasicEffect_VSBasicVertexLightingTxBn.inc"
#include "BasicEffect_VSBasicVertexLightingTxVcBn.inc"

#include "BasicEffect_VSBasicOneLightBn.inc"
#include "BasicEffect_VSBasicOneLightVcBn.inc"
#include "BasicEffect_VSBasicOneLightTxBn.inc"
#include "BasicEffect_VSBasicOneLightTxVcBn.inc"

#include "BasicEffect_VSBasicPixelLightingBn.inc"
#include "BasicEffect_VSBasicPixelLightingVcBn.inc"
#include "BasicEffect_VSBasicPixelLightingTxBn.inc"
#include "BasicEffect_VSBasicPixelLightingTxVcBn.inc"

#include "BasicEffect_PSBasic.inc"
#include "BasicEffect_PSBasicNoFog.inc"
#include "BasicEffect_PSBasicTx.inc"
#include "BasicEffect_PSBasicTxNoFog.inc"

#include "BasicEffect_PSBasicVertexLighting.inc"
#include "BasicEffect_PSBasicVertexLightingNoFog.inc"
#include "BasicEffect_PSBasicVertexLightingTx.inc"
#include "BasicEffect_PSBasicVertexLightingTxNoFog.inc"

#include "BasicEffect_PSBasicPixelLighting.inc"
#include "BasicEffect_PSBasicPixelLightingTx.inc"
#endif
}


template<>
const ShaderBytecode EffectBase<BasicEffectTraits>::VertexShaderBytecode[] =
{
    { BasicEffect_VSBasic,                     sizeof(BasicEffect_VSBasic)                     },
    { BasicEffect_VSBasicNoFog,                sizeof(BasicEffect_VSBasicNoFog)                },
    { BasicEffect_VSBasicVc,                   sizeof(BasicEffect_VSBasicVc)                   },
    { BasicEffect_VSBasicVcNoFog,              sizeof(BasicEffect_VSBasicVcNoFog)              },
    { BasicEffect_VSBasicTx,                   sizeof(BasicEffect_VSBasicTx)                   },
    { BasicEffect_VSBasicTxNoFog,              sizeof(BasicEffect_VSBasicTxNoFog)              },
    { BasicEffect_VSBasicTxVc,                 sizeof(BasicEffect_VSBasicTxVc)                 },
    { BasicEffect_VSBasicTxVcNoFog,            sizeof(BasicEffect_VSBasicTxVcNoFog)            },

    { BasicEffect_VSBasicVertexLighting,       sizeof(BasicEffect_VSBasicVertexLighting)       },
    { BasicEffect_VSBasicVertexLightingVc,     sizeof(BasicEffect_VSBasicVertexLightingVc)     },
    { BasicEffect_VSBasicVertexLightingTx,     sizeof(BasicEffect_VSBasicVertexLightingTx)     },
    { BasicEffect_VSBasicVertexLightingTxVc,   sizeof(BasicEffect_VSBasicVertexLightingTxVc)   },

    { BasicEffect_VSBasicOneLight,             sizeof(BasicEffect_VSBasicOneLight)             },
    { BasicEffect_VSBasicOneLightVc,           sizeof(BasicEffect_VSBasicOneLightVc)           },
    { BasicEffect_VSBasicOneLightTx,           sizeof(BasicEffect_VSBasicOneLightTx)           },
    { BasicEffect_VSBasicOneLightTxVc,         sizeof(BasicEffect_VSBasicOneLightTxVc)         },

    { BasicEffect_VSBasicPixelLighting,        sizeof(BasicEffect_VSBasicPixelLighting)        },
    { BasicEffect_VSBasicPixelLightingVc,      sizeof(BasicEffect_VSBasicPixelLightingVc)      },
    { BasicEffect_VSBasicPixelLightingTx,      sizeof(BasicEffect_VSBasicPixelLightingTx)      },
    { BasicEffect_VSBasicPixelLightingTxVc,    sizeof(BasicEffect_VSBasicPixelLightingTxVc)    },

    { BasicEffect_VSBasicVertexLightingBn,     sizeof(BasicEffect_VSBasicVertexLightingBn)     },
    { BasicEffect_VSBasicVertexLightingVcBn,   sizeof(BasicEffect_VSBasicVertexLightingVcBn)   },
    { BasicEffect_VSBasicVertexLightingTxBn,   sizeof(BasicEffect_VSBasicVertexLightingTxBn)   },
    { BasicEffect_VSBasicVertexLightingTxVcBn, sizeof(BasicEffect_VSBasicVertexLightingTxVcBn) },

    { BasicEffect_VSBasicOneLightBn,           sizeof(BasicEffect_VSBasicOneLightBn)           },
    { BasicEffect_VSBasicOneLightVcBn,         sizeof(BasicEffect_VSBasicOneLightVcBn)         },
    { BasicEffect_VSBasicOneLightTxBn,         sizeof(BasicEffect_VSBasicOneLightTxBn)         },
    { BasicEffect_VSBasicOneLightTxVcBn,       sizeof(BasicEffect_VSBasicOneLightTxVcBn)       },

    { BasicEffect_VSBasicPixelLightingBn,      sizeof(BasicEffect_VSBasicPixelLightingBn)      },
    { BasicEffect_VSBasicPixelLightingVcBn,    sizeof(BasicEffect_VSBasicPixelLightingVcBn)    },
    { BasicEffect_VSBasicPixelLightingTxBn,    sizeof(BasicEffect_VSBasicPixelLightingTxBn)    },
    { BasicEffect_VSBasicPixelLightingTxVcBn,  sizeof(BasicEffect_VSBasicPixelLightingTxVcBn)  },
};


template<>
const int EffectBase<BasicEffectTraits>::VertexShaderIndices[] =
{
    0,      // basic
    1,      // no fog
    2,      // vertex color
    3,      // vertex color, no fog
    4,      // texture
    5,      // texture, no fog
    6,      // texture + vertex color
    7,      // texture + vertex color, no fog

    8,      // vertex lighting
    8,      // vertex lighting, no fog
    9,      // vertex lighting + vertex color
    9,      // vertex lighting + vertex color, no fog
    10,     // vertex lighting + texture
    10,     // vertex lighting + texture, no fog
    11,     // vertex lighting + texture + vertex color
    11,     // vertex lighting + texture + vertex color, no fog

    12,     // one light
    12,     // one light, no fog
    13,     // one light + vertex color
    13,     // one light + vertex color, no fog
    14,     // one light + texture
    14,     // one light + texture, no fog
    15,     // one light + texture + vertex color
    15,     // one light + texture + vertex color, no fog

    16,     // pixel lighting
    16,     // pixel lighting, no fog
    17,     // pixel lighting + vertex color
    17,     // pixel lighting + vertex color, no fog
    18,     // pixel lighting + texture
    18,     // pixel lighting + texture, no fog
    19,     // pixel lighting + texture + vertex color
    19,     // pixel lighting + texture + vertex color, no fog

    20,     // vertex lighting (biased vertex normals)
    20,     // vertex lighting (biased vertex normals), no fog
    21,     // vertex lighting (biased vertex normals) + vertex color
    21,     // vertex lighting (biased vertex normals) + vertex color, no fog
    22,     // vertex lighting (biased vertex normals) + texture
    22,     // vertex lighting (biased vertex normals) + texture, no fog
    23,     // vertex lighting (biased vertex normals) + texture + vertex color
    23,     // vertex lighting (biased vertex normals) + texture + vertex color, no fog

    24,     // one light (biased vertex normals)
    24,     // one light (biased vertex normals), no fog
    25,     // one light (biased vertex normals) + vertex color
    25,     // one light (biased vertex normals) + vertex color, no fog
    26,     // one light (biased vertex normals) + texture
    26,     // one light (biased vertex normals) + texture, no fog
    27,     // one light (biased vertex normals) + texture + vertex color
    27,     // one light (biased vertex normals) + texture + vertex color, no fog

    28,     // pixel lighting (biased vertex normals)
    28,     // pixel lighting (biased vertex normals), no fog
    29,     // pixel lighting (biased vertex normals) + vertex color
    29,     // pixel lighting (biased vertex normals) + vertex color, no fog
    30,     // pixel lighting (biased vertex normals) + texture
    30,     // pixel lighting (biased vertex normals) + texture, no fog
    31,     // pixel lighting (biased vertex normals) + texture + vertex color
    31,     // pixel lighting (biased vertex normals) + texture + vertex color, no fog
};


template<>
const ShaderBytecode EffectBase<BasicEffectTraits>::PixelShaderBytecode[] =
{
    { BasicEffect_PSBasic,                      sizeof(BasicEffect_PSBasic)                      },
    { BasicEffect_PSBasicNoFog,                 sizeof(BasicEffect_PSBasicNoFog)                 },
    { BasicEffect_PSBasicTx,                    sizeof(BasicEffect_PSBasicTx)                    },
    { BasicEffect_PSBasicTxNoFog,               sizeof(BasicEffect_PSBasicTxNoFog)               },

    { BasicEffect_PSBasicVertexLighting,        sizeof(BasicEffect_PSBasicVertexLighting)        },
    { BasicEffect_PSBasicVertexLightingNoFog,   sizeof(BasicEffect_PSBasicVertexLightingNoFog)   },
    { BasicEffect_PSBasicVertexLightingTx,      sizeof(BasicEffect_PSBasicVertexLightingTx)      },
    { BasicEffect_PSBasicVertexLightingTxNoFog, sizeof(BasicEffect_PSBasicVertexLightingTxNoFog) },

    { BasicEffect_PSBasicPixelLighting,         sizeof(BasicEffect_PSBasicPixelLighting)         },
    { BasicEffect_PSBasicPixelLightingTx,       sizeof(BasicEffect_PSBasicPixelLightingTx)       },
};


template<>
const int EffectBase<BasicEffectTraits>::PixelShaderIndices[] =
{
    0,      // basic
    1,      // no fog
    0,      // vertex color
    1,      // vertex color, no fog
    2,      // texture
    3,      // texture, no fog
    2,      // texture + vertex color
    3,      // texture + vertex color, no fog

    4,      // vertex lighting
    5,      // vertex lighting, no fog
    4,      // vertex lighting + vertex color
    5,      // vertex lighting + vertex color, no fog
    6,      // vertex lighting + texture
    7,      // vertex lighting + texture, no fog
    6,      // vertex lighting + texture + vertex color
    7,      // vertex lighting + texture + vertex color, no fog

    4,      // one light
    5,      // one light, no fog
    4,      // one light + vertex color
    5,      // one light + vertex color, no fog
    6,      // one light + texture
    7,      // one light + texture, no fog
    6,      // one light + texture + vertex color
    7,      // one light + texture + vertex color, no fog

    8,      // pixel lighting
    8,      // pixel lighting, no fog
    8,      // pixel lighting + vertex color
    8,      // pixel lighting + vertex color, no fog
    9,      // pixel lighting + texture
    9,      // pixel lighting + texture, no fog
    9,      // pixel lighting + texture + vertex color
    9,      // pixel lighting + texture + vertex color, no fog

    4,      // vertex lighting (biased vertex normals)
    5,      // vertex lighting (biased vertex normals), no fog
    4,      // vertex lighting (biased vertex normals) + vertex color
    5,      // vertex lighting (biased vertex normals) + vertex color, no fog
    6,      // vertex lighting (biased vertex normals) + texture
    7,      // vertex lighting (biased vertex normals) + texture, no fog
    6,      // vertex lighting (biased vertex normals) + texture + vertex color
    7,      // vertex lighting (biased vertex normals) + texture + vertex color, no fog

    4,      // one light (biased vertex normals)
    5,      // one light (biased vertex normals), no fog
    4,      // one light (biased vertex normals) + vertex color
    5,      // one light (biased vertex normals) + vertex color, no fog
    6,      // one light (biased vertex normals) + texture
    7,      // one light (biased vertex normals) + texture, no fog
    6,      // one light (biased vertex normals) + texture + vertex color
    7,      // one light (biased vertex normals) + texture + vertex color, no fog

    8,      // pixel lighting (biased vertex normals)
    8,      // pixel lighting (biased vertex normals), no fog
    8,      // pixel lighting (biased vertex normals) + vertex color
    8,      // pixel lighting (biased vertex normals) + vertex color, no fog
    9,      // pixel lighting (biased vertex normals) + texture
    9,      // pixel lighting (biased vertex normals) + texture, no fog
    9,      // pixel lighting (biased vertex normals) + texture + vertex color
    9,      // pixel lighting (biased vertex normals) + texture + vertex color, no fog
};
#pragma endregion

// Global pool of per-device BasicEffect resources.
template<>
SharedResourcePool<ID3D11Device*, EffectBase<BasicEffectTraits>::DeviceResources> EffectBase<BasicEffectTraits>::deviceResourcesPool = {};


// Constructor.
BasicEffect::Impl::Impl(_In_ ID3D11Device* device)
    : EffectBase(device),
    lightingEnabled(false),
    preferPerPixelLighting(false),
    vertexColorEnabled(false),
    textureEnabled(false),
    biasedVertexNormals(false)
{
    static_assert(static_cast<int>(std::size(EffectBase<BasicEffectTraits>::VertexShaderIndices)) == BasicEffectTraits::ShaderPermutationCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<BasicEffectTraits>::VertexShaderBytecode)) == BasicEffectTraits::VertexShaderCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<BasicEffectTraits>::PixelShaderBytecode)) == BasicEffectTraits::PixelShaderCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<BasicEffectTraits>::PixelShaderIndices)) == BasicEffectTraits::ShaderPermutationCount, "array/max mismatch");

    lights.InitializeConstants(constants.specularColorAndPower, constants.lightDirection, constants.lightDiffuseColor, constants.lightSpecularColor);
}


int BasicEffect::Impl::GetCurrentShaderPermutation() const noexcept
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

    // Support texturing?
    if (textureEnabled)
    {
        permutation += 4;
    }

    if (lightingEnabled)
    {
        if (preferPerPixelLighting)
        {
            // Do lighting in the pixel shader.
            permutation += 24;
        }
        else if (!lights.lightEnabled[1] && !lights.lightEnabled[2])
        {
            // Use the only-bother-with-the-first-light shader optimization.
            permutation += 16;
        }
        else
        {
            // Compute all three lights in the vertex shader.
            permutation += 8;
        }

        if (biasedVertexNormals)
        {
            // Compressed normals need to be scaled and biased in the vertex shader.
            permutation += 24;
        }
    }

    return permutation;
}


// Sets our state onto the D3D device.
void BasicEffect::Impl::Apply(_In_ ID3D11DeviceContext* deviceContext)
{
    assert(deviceContext != nullptr);

    // Compute derived parameter values.
    matrices.SetConstants(dirtyFlags, constants.worldViewProj);

    fog.SetConstants(dirtyFlags, matrices.worldView, constants.fogVector);

    lights.SetConstants(dirtyFlags, matrices, constants.world, constants.worldInverseTranspose, constants.eyePosition, constants.diffuseColor, constants.emissiveColor, lightingEnabled);

    // Set the texture.
    if (textureEnabled)
    {
        deviceContext->PSSetShaderResources(0, 1, texture.GetAddressOf());
    }

    // Set shaders and constant buffers.
    ApplyShaders(deviceContext, GetCurrentShaderPermutation());
}


// Public constructor.
BasicEffect::BasicEffect(_In_ ID3D11Device* device)
    : pImpl(std::make_unique<Impl>(device))
{
}


BasicEffect::BasicEffect(BasicEffect&&) noexcept = default;
BasicEffect& BasicEffect::operator= (BasicEffect&&) noexcept = default;
BasicEffect::~BasicEffect() = default;


// IEffect methods.
void BasicEffect::Apply(_In_ ID3D11DeviceContext* deviceContext)
{
    pImpl->Apply(deviceContext);
}


void BasicEffect::GetVertexShaderBytecode(_Out_ void const** pShaderByteCode, _Out_ size_t* pByteCodeLength)
{
    pImpl->GetVertexShaderBytecode(pImpl->GetCurrentShaderPermutation(), pShaderByteCode, pByteCodeLength);
}


// Camera settings.
void XM_CALLCONV BasicEffect::SetWorld(FXMMATRIX value)
{
    pImpl->matrices.world = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::WorldInverseTranspose | EffectDirtyFlags::FogVector;
}


void XM_CALLCONV BasicEffect::SetView(FXMMATRIX value)
{
    pImpl->matrices.view = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::EyePosition | EffectDirtyFlags::FogVector;
}


void XM_CALLCONV BasicEffect::SetProjection(FXMMATRIX value)
{
    pImpl->matrices.projection = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj;
}


void XM_CALLCONV BasicEffect::SetMatrices(FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection)
{
    pImpl->matrices.world = world;
    pImpl->matrices.view = view;
    pImpl->matrices.projection = projection;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::WorldInverseTranspose | EffectDirtyFlags::EyePosition | EffectDirtyFlags::FogVector;
}


// Material settings.
void XM_CALLCONV BasicEffect::SetDiffuseColor(FXMVECTOR value)
{
    pImpl->lights.diffuseColor = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


void XM_CALLCONV BasicEffect::SetEmissiveColor(FXMVECTOR value)
{
    pImpl->lights.emissiveColor = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


void XM_CALLCONV BasicEffect::SetSpecularColor(FXMVECTOR value)
{
    // Set xyz to new value, but preserve existing w (specular power).
    pImpl->constants.specularColorAndPower = XMVectorSelect(pImpl->constants.specularColorAndPower, value, g_XMSelect1110);

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


void BasicEffect::SetSpecularPower(float value)
{
    // Set w to new value, but preserve existing xyz (specular color).
    pImpl->constants.specularColorAndPower = XMVectorSetW(pImpl->constants.specularColorAndPower, value);

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


void BasicEffect::DisableSpecular()
{
    // Set specular color to black, power to 1
    // Note: Don't use a power of 0 or the shader will generate strange highlights on non-specular materials

    pImpl->constants.specularColorAndPower = g_XMIdentityR3;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


void BasicEffect::SetAlpha(float value)
{
    pImpl->lights.alpha = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


void XM_CALLCONV BasicEffect::SetColorAndAlpha(FXMVECTOR value)
{
    pImpl->lights.diffuseColor = value;
    pImpl->lights.alpha = XMVectorGetW(value);

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


// Light settings.
void BasicEffect::SetLightingEnabled(bool value)
{
    pImpl->lightingEnabled = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


void BasicEffect::SetPerPixelLighting(bool value)
{
    pImpl->preferPerPixelLighting = value;
}


void XM_CALLCONV BasicEffect::SetAmbientLightColor(FXMVECTOR value)
{
    pImpl->lights.ambientLightColor = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


void BasicEffect::SetLightEnabled(int whichLight, bool value)
{
    pImpl->dirtyFlags |= pImpl->lights.SetLightEnabled(whichLight, value, pImpl->constants.lightDiffuseColor, pImpl->constants.lightSpecularColor);
}


void XM_CALLCONV BasicEffect::SetLightDirection(int whichLight, FXMVECTOR value)
{
    EffectLights::ValidateLightIndex(whichLight);

    pImpl->constants.lightDirection[whichLight] = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


void XM_CALLCONV BasicEffect::SetLightDiffuseColor(int whichLight, FXMVECTOR value)
{
    pImpl->dirtyFlags |= pImpl->lights.SetLightDiffuseColor(whichLight, value, pImpl->constants.lightDiffuseColor);
}


void XM_CALLCONV BasicEffect::SetLightSpecularColor(int whichLight, FXMVECTOR value)
{
    pImpl->dirtyFlags |= pImpl->lights.SetLightSpecularColor(whichLight, value, pImpl->constants.lightSpecularColor);
}


void BasicEffect::EnableDefaultLighting()
{
    EffectLights::EnableDefaultLighting(this);
}


// Fog settings.
void BasicEffect::SetFogEnabled(bool value)
{
    pImpl->fog.enabled = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::FogEnable;
}


void BasicEffect::SetFogStart(float value)
{
    pImpl->fog.start = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::FogVector;
}


void BasicEffect::SetFogEnd(float value)
{
    pImpl->fog.end = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::FogVector;
}


void XM_CALLCONV BasicEffect::SetFogColor(FXMVECTOR value)
{
    pImpl->constants.fogColor = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


// Vertex color setting.
void BasicEffect::SetVertexColorEnabled(bool value)
{
    pImpl->vertexColorEnabled = value;
}


// Texture settings.
void BasicEffect::SetTextureEnabled(bool value)
{
    pImpl->textureEnabled = value;
}


void BasicEffect::SetTexture(_In_opt_ ID3D11ShaderResourceView* value)
{
    pImpl->texture = value;
}


// Normal compression settings.
void BasicEffect::SetBiasedVertexNormals(bool value)
{
    pImpl->biasedVertexNormals = value;
}
