//--------------------------------------------------------------------------------------
// File: EnvironmentMapEffect.cpp
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
    struct EnvironmentMapEffectConstants
    {
        XMVECTOR environmentMapSpecular;
        float environmentMapAmount;
        float fresnelFactor;
        float pad[2];

        XMVECTOR diffuseColor;
        XMVECTOR emissiveColor;

        XMVECTOR lightDirection[IEffectLights::MaxDirectionalLights];
        XMVECTOR lightDiffuseColor[IEffectLights::MaxDirectionalLights];

        XMVECTOR eyePosition;

        XMVECTOR fogColor;
        XMVECTOR fogVector;

        XMMATRIX world;
        XMVECTOR worldInverseTranspose[3];
        XMMATRIX worldViewProj;
    };

    static_assert((sizeof(EnvironmentMapEffectConstants) % 16) == 0, "CB size not padded correctly");

    // Traits type describes our characteristics to the EffectBase template.
    struct EnvironmentMapEffectTraits
    {
        using ConstantBufferType = EnvironmentMapEffectConstants;

        static constexpr int VertexShaderCount = 12;
        static constexpr int PixelShaderCount = 16;
        static constexpr int ShaderPermutationCount = 56;

        static constexpr int MappingCount = 3;
    };
}

// Internal EnvironmentMapEffect implementation class.
class EnvironmentMapEffect::Impl : public EffectBase<EnvironmentMapEffectTraits>
{
public:
    explicit Impl(_In_ ID3D11Device* device);

    bool preferPerPixelLighting;
    bool fresnelEnabled;
    bool specularEnabled;
    bool biasedVertexNormals;
    EnvironmentMapEffect::Mapping mapping;

    EffectLights lights;

    ComPtr<ID3D11ShaderResourceView> environmentMap;

    int GetCurrentShaderPermutation() const noexcept;

    void Apply(_In_ ID3D11DeviceContext* deviceContext);
};


#pragma region Shaders
// Include the precompiled shader code.
namespace
{
#if defined(_XBOX_ONE) && defined(_TITLE)
#include "XboxOneEnvironmentMapEffect_VSEnvMap.inc"
#include "XboxOneEnvironmentMapEffect_VSEnvMapFresnel.inc"
#include "XboxOneEnvironmentMapEffect_VSEnvMapOneLight.inc"
#include "XboxOneEnvironmentMapEffect_VSEnvMapOneLightFresnel.inc"
#include "XboxOneEnvironmentMapEffect_VSEnvMapPixelLighting.inc"

#include "XboxOneEnvironmentMapEffect_VSEnvMapBn.inc"
#include "XboxOneEnvironmentMapEffect_VSEnvMapFresnelBn.inc"
#include "XboxOneEnvironmentMapEffect_VSEnvMapOneLightBn.inc"
#include "XboxOneEnvironmentMapEffect_VSEnvMapOneLightFresnelBn.inc"
#include "XboxOneEnvironmentMapEffect_VSEnvMapPixelLightingBn.inc"

#include "XboxOneEnvironmentMapEffect_PSEnvMap.inc"
#include "XboxOneEnvironmentMapEffect_PSEnvMapNoFog.inc"
#include "XboxOneEnvironmentMapEffect_PSEnvMapSpecular.inc"
#include "XboxOneEnvironmentMapEffect_PSEnvMapSpecularNoFog.inc"
#include "XboxOneEnvironmentMapEffect_PSEnvMapPixelLighting.inc"
#include "XboxOneEnvironmentMapEffect_PSEnvMapPixelLightingNoFog.inc"
#include "XboxOneEnvironmentMapEffect_PSEnvMapPixelLightingFresnel.inc"
#include "XboxOneEnvironmentMapEffect_PSEnvMapPixelLightingFresnelNoFog.inc"

#include "XboxOneEnvironmentMapEffect_PSEnvMapSpherePixelLighting.inc"
#include "XboxOneEnvironmentMapEffect_PSEnvMapSpherePixelLightingNoFog.inc"
#include "XboxOneEnvironmentMapEffect_PSEnvMapSpherePixelLightingFresnel.inc"
#include "XboxOneEnvironmentMapEffect_PSEnvMapSpherePixelLightingFresnelNoFog.inc"

#include "XboxOneEnvironmentMapEffect_PSEnvMapDualParabolaPixelLighting.inc"
#include "XboxOneEnvironmentMapEffect_PSEnvMapDualParabolaPixelLightingNoFog.inc"
#include "XboxOneEnvironmentMapEffect_PSEnvMapDualParabolaPixelLightingFresnel.inc"
#include "XboxOneEnvironmentMapEffect_PSEnvMapDualParabolaPixelLightingFresnelNoFog.inc"
#else
#include "EnvironmentMapEffect_VSEnvMap.inc"
#include "EnvironmentMapEffect_VSEnvMapFresnel.inc"
#include "EnvironmentMapEffect_VSEnvMapOneLight.inc"
#include "EnvironmentMapEffect_VSEnvMapOneLightFresnel.inc"
#include "EnvironmentMapEffect_VSEnvMapPixelLighting.inc"
#include "EnvironmentMapEffect_VSEnvMapPixelLightingSM4.inc"

#include "EnvironmentMapEffect_VSEnvMapBn.inc"
#include "EnvironmentMapEffect_VSEnvMapFresnelBn.inc"
#include "EnvironmentMapEffect_VSEnvMapOneLightBn.inc"
#include "EnvironmentMapEffect_VSEnvMapOneLightFresnelBn.inc"
#include "EnvironmentMapEffect_VSEnvMapPixelLightingBn.inc"
#include "EnvironmentMapEffect_VSEnvMapPixelLightingBnSM4.inc"

#include "EnvironmentMapEffect_PSEnvMap.inc"
#include "EnvironmentMapEffect_PSEnvMapNoFog.inc"
#include "EnvironmentMapEffect_PSEnvMapSpecular.inc"
#include "EnvironmentMapEffect_PSEnvMapSpecularNoFog.inc"
#include "EnvironmentMapEffect_PSEnvMapPixelLighting.inc"
#include "EnvironmentMapEffect_PSEnvMapPixelLightingNoFog.inc"
#include "EnvironmentMapEffect_PSEnvMapPixelLightingFresnel.inc"
#include "EnvironmentMapEffect_PSEnvMapPixelLightingFresnelNoFog.inc"

#include "EnvironmentMapEffect_PSEnvMapSpherePixelLighting.inc"
#include "EnvironmentMapEffect_PSEnvMapSpherePixelLightingNoFog.inc"
#include "EnvironmentMapEffect_PSEnvMapSpherePixelLightingFresnel.inc"
#include "EnvironmentMapEffect_PSEnvMapSpherePixelLightingFresnelNoFog.inc"

#include "EnvironmentMapEffect_PSEnvMapDualParabolaPixelLighting.inc"
#include "EnvironmentMapEffect_PSEnvMapDualParabolaPixelLightingNoFog.inc"
#include "EnvironmentMapEffect_PSEnvMapDualParabolaPixelLightingFresnel.inc"
#include "EnvironmentMapEffect_PSEnvMapDualParabolaPixelLightingFresnelNoFog.inc"
#endif
}


template<>
const ShaderBytecode EffectBase<EnvironmentMapEffectTraits>::VertexShaderBytecode[] =
{
    { EnvironmentMapEffect_VSEnvMap,                   sizeof(EnvironmentMapEffect_VSEnvMap)                   },
    { EnvironmentMapEffect_VSEnvMapFresnel,            sizeof(EnvironmentMapEffect_VSEnvMapFresnel)            },
    { EnvironmentMapEffect_VSEnvMapOneLight,           sizeof(EnvironmentMapEffect_VSEnvMapOneLight)           },
    { EnvironmentMapEffect_VSEnvMapOneLightFresnel,    sizeof(EnvironmentMapEffect_VSEnvMapOneLightFresnel)    },
    { EnvironmentMapEffect_VSEnvMapPixelLighting,      sizeof(EnvironmentMapEffect_VSEnvMapPixelLighting)      },

    { EnvironmentMapEffect_VSEnvMapBn,                 sizeof(EnvironmentMapEffect_VSEnvMapBn)                 },
    { EnvironmentMapEffect_VSEnvMapFresnelBn,          sizeof(EnvironmentMapEffect_VSEnvMapFresnelBn)          },
    { EnvironmentMapEffect_VSEnvMapOneLightBn,         sizeof(EnvironmentMapEffect_VSEnvMapOneLightBn)         },
    { EnvironmentMapEffect_VSEnvMapOneLightFresnelBn,  sizeof(EnvironmentMapEffect_VSEnvMapOneLightFresnelBn)  },
    { EnvironmentMapEffect_VSEnvMapPixelLightingBn,    sizeof(EnvironmentMapEffect_VSEnvMapPixelLightingBn)    },

#if defined(_XBOX_ONE) && defined(_TITLE)
    { EnvironmentMapEffect_VSEnvMapPixelLighting,      sizeof(EnvironmentMapEffect_VSEnvMapPixelLighting)      },
    { EnvironmentMapEffect_VSEnvMapPixelLightingBn,    sizeof(EnvironmentMapEffect_VSEnvMapPixelLightingBn)    },
#else
    { EnvironmentMapEffect_VSEnvMapPixelLightingSM4,   sizeof(EnvironmentMapEffect_VSEnvMapPixelLightingSM4)   },
    { EnvironmentMapEffect_VSEnvMapPixelLightingBnSM4, sizeof(EnvironmentMapEffect_VSEnvMapPixelLightingBnSM4) },
#endif
};


template<>
const int EffectBase<EnvironmentMapEffectTraits>::VertexShaderIndices[] =
{
    0,      // basic
    0,      // basic, no fog
    1,      // fresnel
    1,      // fresnel, no fog
    0,      // specular
    0,      // specular, no fog
    1,      // fresnel + specular
    1,      // fresnel + specular, no fog

    2,      // one light
    2,      // one light, no fog
    3,      // one light, fresnel
    3,      // one light, fresnel, no fog
    2,      // one light, specular
    2,      // one light, specular, no fog
    3,      // one light, fresnel + specular
    3,      // one light, fresnel + specular, no fog

    4,      // pixel lighting
    4,      // pixel lighting, no fog
    4,      // pixel lighting, fresnel
    4,      // pixel lighting, fresnel, no fog

    5,      // basic (biased vertex normals)
    5,      // basic (biased vertex normals), no fog
    6,      // fresnel (biased vertex normals)
    6,      // fresnel (biased vertex normals), no fog
    5,      // specular (biased vertex normals)
    5,      // specular (biased vertex normals), no fog
    6,      // fresnel + specular (biased vertex normals)
    6,      // fresnel + specular (biased vertex normals), no fog

    7,      // one light (biased vertex normals)
    7,      // one light (biased vertex normals), no fog
    8,      // one light (biased vertex normals), fresnel
    8,      // one light (biased vertex normals), fresnel, no fog
    7,      // one light (biased vertex normals), specular
    7,      // one light (biased vertex normals), specular, no fog
    8,      // one light (biased vertex normals), fresnel + specular
    8,      // one light (biased vertex normals), fresnel + specular, no fog

    9,      // pixel lighting (biased vertex normals)
    9,      // pixel lighting (biased vertex normals), no fog
    9,      // pixel lighting (biased vertex normals), fresnel
    9,      // pixel lighting (biased vertex normals), fresnel, no fog

    4,      // spheremap pixel lighting
    4,      // spheremap pixel lighting, no fog
    4,      // spheremap pixel lighting, fresnel
    4,      // spheremap pixel lighting, fresnel, no fog

    9,      // spheremap pixel lighting (biased vertex normals)
    9,      // spheremap pixel lighting (biased vertex normals), no fog
    9,      // spheremap pixel lighting (biased vertex normals), fresnel
    9,      // spheremap pixel lighting (biased vertex normals), fresnel, no fog

    10,     // dual-parabola pixel lighting
    10,     // dual-parabola pixel lighting, no fog
    10,     // dual-parabola pixel lighting, fresnel
    10,     // dual-parabola pixel lighting, fresnel, no fog

    11,     // dual-parabola pixel lighting (biased vertex normals)
    11,     // dual-parabola pixel lighting (biased vertex normals), no fog
    11,     // dual-parabola pixel lighting (biased vertex normals), fresnel
    11,     // dual-parabola pixel lighting (biased vertex normals), fresnel, no fog
};


template<>
const ShaderBytecode EffectBase<EnvironmentMapEffectTraits>::PixelShaderBytecode[] =
{
    { EnvironmentMapEffect_PSEnvMap,                          sizeof(EnvironmentMapEffect_PSEnvMap)                          },
    { EnvironmentMapEffect_PSEnvMapNoFog,                     sizeof(EnvironmentMapEffect_PSEnvMapNoFog)                     },
    { EnvironmentMapEffect_PSEnvMapSpecular,                  sizeof(EnvironmentMapEffect_PSEnvMapSpecular)                  },
    { EnvironmentMapEffect_PSEnvMapSpecularNoFog,             sizeof(EnvironmentMapEffect_PSEnvMapSpecularNoFog)             },
    { EnvironmentMapEffect_PSEnvMapPixelLighting,             sizeof(EnvironmentMapEffect_PSEnvMapPixelLighting)             },
    { EnvironmentMapEffect_PSEnvMapPixelLightingNoFog,        sizeof(EnvironmentMapEffect_PSEnvMapPixelLightingNoFog)        },
    { EnvironmentMapEffect_PSEnvMapPixelLightingFresnel,      sizeof(EnvironmentMapEffect_PSEnvMapPixelLightingFresnel)      },
    { EnvironmentMapEffect_PSEnvMapPixelLightingFresnelNoFog, sizeof(EnvironmentMapEffect_PSEnvMapPixelLightingFresnelNoFog) },

    { EnvironmentMapEffect_PSEnvMapSpherePixelLighting,             sizeof(EnvironmentMapEffect_PSEnvMapSpherePixelLighting) },
    { EnvironmentMapEffect_PSEnvMapSpherePixelLightingNoFog,        sizeof(EnvironmentMapEffect_PSEnvMapSpherePixelLightingNoFog) },
    { EnvironmentMapEffect_PSEnvMapSpherePixelLightingFresnel,      sizeof(EnvironmentMapEffect_PSEnvMapSpherePixelLightingFresnel) },
    { EnvironmentMapEffect_PSEnvMapSpherePixelLightingFresnelNoFog, sizeof(EnvironmentMapEffect_PSEnvMapSpherePixelLightingFresnelNoFog) },

    { EnvironmentMapEffect_PSEnvMapDualParabolaPixelLighting,             sizeof(EnvironmentMapEffect_PSEnvMapDualParabolaPixelLighting) },
    { EnvironmentMapEffect_PSEnvMapDualParabolaPixelLightingNoFog,        sizeof(EnvironmentMapEffect_PSEnvMapDualParabolaPixelLightingNoFog) },
    { EnvironmentMapEffect_PSEnvMapDualParabolaPixelLightingFresnel,      sizeof(EnvironmentMapEffect_PSEnvMapDualParabolaPixelLightingFresnel) },
    { EnvironmentMapEffect_PSEnvMapDualParabolaPixelLightingFresnelNoFog, sizeof(EnvironmentMapEffect_PSEnvMapDualParabolaPixelLightingFresnelNoFog) },
};


template<>
const int EffectBase<EnvironmentMapEffectTraits>::PixelShaderIndices[] =
{
    0,      // basic
    1,      // basic, no fog
    0,      // fresnel
    1,      // fresnel, no fog
    2,      // specular
    3,      // specular, no fog
    2,      // fresnel + specular
    3,      // fresnel + specular, no fog

    0,      // one light
    1,      // one light, no fog
    0,      // one light, fresnel
    1,      // one light, fresnel, no fog
    2,      // one light, specular
    3,      // one light, specular, no fog
    2,      // one light, fresnel + specular
    3,      // one light, fresnel + specular, no fog

    4,      // per pixel lighting
    5,      // per pixel lighting, no fog
    6,      // per pixel lighting, fresnel
    7,      // per pixel lighting, fresnel, no fog

    0,      // basic (biased vertex normals)
    1,      // basic (biased vertex normals), no fog
    0,      // fresnel (biased vertex normals)
    1,      // fresnel (biased vertex normals), no fog
    2,      // specular (biased vertex normals)
    3,      // specular (biased vertex normals), no fog
    2,      // fresnel + specular (biased vertex normals)
    3,      // fresnel + specular (biased vertex normals), no fog

    0,      // one light (biased vertex normals)
    1,      // one light (biased vertex normals), no fog
    0,      // one light (biased vertex normals), fresnel
    1,      // one light (biased vertex normals), fresnel, no fog
    2,      // one light (biased vertex normals), specular
    3,      // one light (biased vertex normals), specular, no fog
    2,      // one light (biased vertex normals), fresnel + specular
    3,      // one light (biased vertex normals), fresnel + specular, no fog

    4,      // per pixel lighting (biased vertex normals)
    5,      // per pixel lighting (biased vertex normals), no fog
    6,      // per pixel lighting (biased vertex normals), fresnel
    7,      // per pixel lighting (biased vertex normals), fresnel, no fog

    8,      // spheremap pixel lighting
    9,      // spheremap pixel lighting, no fog
    10,     // spheremap pixel lighting, fresnel
    11,     // spheremap pixel lighting, fresnel, no fog

    8,      // spheremap pixel lighting (biased vertex normals)
    9,      // spheremap pixel lighting (biased vertex normals), no fog
    10,     // spheremap pixel lighting (biased vertex normals), fresnel
    11,     // spheremap pixel lighting (biased vertex normals), fresnel, no fog

    12,     // dual-parabola pixel lighting
    13,     // dual-parabola pixel lighting, no fog
    14,     // dual-parabola pixel lighting, fresnel
    15,     // dual-parabola pixel lighting, fresnel, no fog

    12,     // dual-parabola pixel lighting (biased vertex normals)
    13,     // dual-parabola pixel lighting (biased vertex normals), no fog
    14,     // dual-parabola pixel lighting (biased vertex normals), fresnel
    15,     // dual-parabola pixel lighting (biased vertex normals), fresnel, no fog
};
#pragma endregion

// Global pool of per-device EnvironmentMapEffect resources.
template<>
SharedResourcePool<ID3D11Device*, EffectBase<EnvironmentMapEffectTraits>::DeviceResources> EffectBase<EnvironmentMapEffectTraits>::deviceResourcesPool = {};


// Constructor.
EnvironmentMapEffect::Impl::Impl(_In_ ID3D11Device* device)
    : EffectBase(device),
    preferPerPixelLighting(false),
    fresnelEnabled(true),
    specularEnabled(false),
    biasedVertexNormals(false),
    mapping(Mapping_Cube)
{
    static_assert(static_cast<int>(std::size(EffectBase<EnvironmentMapEffectTraits>::VertexShaderIndices)) == EnvironmentMapEffectTraits::ShaderPermutationCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<EnvironmentMapEffectTraits>::VertexShaderBytecode)) == EnvironmentMapEffectTraits::VertexShaderCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<EnvironmentMapEffectTraits>::PixelShaderBytecode)) == EnvironmentMapEffectTraits::PixelShaderCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<EnvironmentMapEffectTraits>::PixelShaderIndices)) == EnvironmentMapEffectTraits::ShaderPermutationCount, "array/max mismatch");

    constants.environmentMapAmount = 1;
    constants.fresnelFactor = 1;

    XMVECTOR unwantedOutput[MaxDirectionalLights];

    lights.InitializeConstants(unwantedOutput[0], constants.lightDirection, constants.lightDiffuseColor, unwantedOutput);
}


int EnvironmentMapEffect::Impl::GetCurrentShaderPermutation() const noexcept
{
    int permutation = 0;

    // Use optimized shaders if fog is disabled.
    if (!fog.enabled)
    {
        permutation += 1;
    }

    // Support fresnel?
    if (fresnelEnabled)
    {
        permutation += 2;
    }

    if (mapping == Mapping_Sphere)
    {
        permutation += 40;

        if (biasedVertexNormals)
        {
            permutation += 4;
        }
    }
    else if (mapping == Mapping_DualParabola)
    {
        permutation += 48;

        if (biasedVertexNormals)
        {
            permutation += 4;
        }
    }
    else // Mapping_Cube
    {
        if (preferPerPixelLighting)
        {
            permutation += 16;
        }
        else
        {
            // Support specular?
            if (specularEnabled)
            {
                permutation += 4;
            }

            // Use the only-bother-with-the-first-light shader optimization?
            if (!lights.lightEnabled[1] && !lights.lightEnabled[2])
            {
                permutation += 8;
            }
        }

        if (biasedVertexNormals)
        {
            // Compressed normals need to be scaled and biased in the vertex shader.
            permutation += 20;
        }
    }

    return permutation;
}


// Sets our state onto the D3D device.
void EnvironmentMapEffect::Impl::Apply(_In_ ID3D11DeviceContext* deviceContext)
{
    assert(deviceContext != nullptr);

    // Compute derived parameter values.
    matrices.SetConstants(dirtyFlags, constants.worldViewProj);

    fog.SetConstants(dirtyFlags, matrices.worldView, constants.fogVector);

    lights.SetConstants(dirtyFlags, matrices, constants.world, constants.worldInverseTranspose, constants.eyePosition, constants.diffuseColor, constants.emissiveColor, true);

    // Set the textures.
    ID3D11ShaderResourceView* textures[2] =
    {
        (texture) ? texture.Get() : GetDefaultTexture(),
        environmentMap.Get(),
    };

    deviceContext->PSSetShaderResources(0, 2, textures);

    // Set shaders and constant buffers.
    ApplyShaders(deviceContext, GetCurrentShaderPermutation());
}


// Public constructor.
EnvironmentMapEffect::EnvironmentMapEffect(_In_ ID3D11Device* device)
    : pImpl(std::make_unique<Impl>(device))
{
}


EnvironmentMapEffect::EnvironmentMapEffect(EnvironmentMapEffect&&) noexcept = default;
EnvironmentMapEffect& EnvironmentMapEffect::operator= (EnvironmentMapEffect&&) noexcept = default;
EnvironmentMapEffect::~EnvironmentMapEffect() = default;


// IEffect methods.
void EnvironmentMapEffect::Apply(_In_ ID3D11DeviceContext* deviceContext)
{
    pImpl->Apply(deviceContext);
}


void EnvironmentMapEffect::GetVertexShaderBytecode(_Out_ void const** pShaderByteCode, _Out_ size_t* pByteCodeLength)
{
    pImpl->GetVertexShaderBytecode(pImpl->GetCurrentShaderPermutation(), pShaderByteCode, pByteCodeLength);
}


// Camera settings.
void XM_CALLCONV EnvironmentMapEffect::SetWorld(FXMMATRIX value)
{
    pImpl->matrices.world = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::WorldInverseTranspose | EffectDirtyFlags::FogVector;
}


void XM_CALLCONV EnvironmentMapEffect::SetView(FXMMATRIX value)
{
    pImpl->matrices.view = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::EyePosition | EffectDirtyFlags::FogVector;
}


void XM_CALLCONV EnvironmentMapEffect::SetProjection(FXMMATRIX value)
{
    pImpl->matrices.projection = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj;
}


void XM_CALLCONV EnvironmentMapEffect::SetMatrices(FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection)
{
    pImpl->matrices.world = world;
    pImpl->matrices.view = view;
    pImpl->matrices.projection = projection;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::WorldInverseTranspose | EffectDirtyFlags::EyePosition | EffectDirtyFlags::FogVector;
}


// Material settings.
void XM_CALLCONV EnvironmentMapEffect::SetDiffuseColor(FXMVECTOR value)
{
    pImpl->lights.diffuseColor = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


void XM_CALLCONV EnvironmentMapEffect::SetEmissiveColor(FXMVECTOR value)
{
    pImpl->lights.emissiveColor = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


void EnvironmentMapEffect::SetAlpha(float value)
{
    pImpl->lights.alpha = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


void XM_CALLCONV EnvironmentMapEffect::SetColorAndAlpha(FXMVECTOR value)
{
    pImpl->lights.diffuseColor = value;
    pImpl->lights.alpha = XMVectorGetW(value);

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


// Light settings.
void EnvironmentMapEffect::SetLightingEnabled(bool value)
{
    if (!value)
    {
        throw std::invalid_argument("EnvironmentMapEffect does not support turning off lighting");
    }
}


void EnvironmentMapEffect::SetPerPixelLighting(bool value)
{
    pImpl->preferPerPixelLighting = value;
}


void XM_CALLCONV EnvironmentMapEffect::SetAmbientLightColor(FXMVECTOR value)
{
    pImpl->lights.ambientLightColor = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::MaterialColor;
}


void EnvironmentMapEffect::SetLightEnabled(int whichLight, bool value)
{
    XMVECTOR unwantedOutput[MaxDirectionalLights] = {};

    pImpl->dirtyFlags |= pImpl->lights.SetLightEnabled(whichLight, value, pImpl->constants.lightDiffuseColor, unwantedOutput);
}


void XM_CALLCONV EnvironmentMapEffect::SetLightDirection(int whichLight, FXMVECTOR value)
{
    EffectLights::ValidateLightIndex(whichLight);

    pImpl->constants.lightDirection[whichLight] = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


void XM_CALLCONV EnvironmentMapEffect::SetLightDiffuseColor(int whichLight, FXMVECTOR value)
{
    pImpl->dirtyFlags |= pImpl->lights.SetLightDiffuseColor(whichLight, value, pImpl->constants.lightDiffuseColor);
}


void XM_CALLCONV EnvironmentMapEffect::SetLightSpecularColor(int, FXMVECTOR)
{
    // Unsupported interface method.
}


void EnvironmentMapEffect::EnableDefaultLighting()
{
    EffectLights::EnableDefaultLighting(this);
}


// Fog settings.
void EnvironmentMapEffect::SetFogEnabled(bool value)
{
    pImpl->fog.enabled = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::FogEnable;
}


void EnvironmentMapEffect::SetFogStart(float value)
{
    pImpl->fog.start = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::FogVector;
}


void EnvironmentMapEffect::SetFogEnd(float value)
{
    pImpl->fog.end = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::FogVector;
}


void XM_CALLCONV EnvironmentMapEffect::SetFogColor(FXMVECTOR value)
{
    pImpl->constants.fogColor = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


// Texture settings.
void EnvironmentMapEffect::SetTexture(_In_opt_ ID3D11ShaderResourceView* value)
{
    pImpl->texture = value;
}


void EnvironmentMapEffect::SetEnvironmentMap(_In_opt_ ID3D11ShaderResourceView* value)
{
    pImpl->environmentMap = value;
}


// Additional settings.
void EnvironmentMapEffect::SetMode(EnvironmentMapEffect::Mapping mapping)
{
    if (static_cast<int>(mapping) < 0 || static_cast<int>(mapping) >= EnvironmentMapEffectTraits::MappingCount)
    {
        throw std::invalid_argument("Unsupported mapping");
    }

    if (mapping == Mapping_DualParabola)
    {
        if (pImpl->GetDeviceFeatureLevel() < D3D_FEATURE_LEVEL_10_0)
        {
            throw std::runtime_error("Dual Parabola requires Feature Level 10.0 or later");
        }
    }

    pImpl->mapping = mapping;
}


void EnvironmentMapEffect::SetEnvironmentMapAmount(float value)
{
    pImpl->constants.environmentMapAmount = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


void XM_CALLCONV EnvironmentMapEffect::SetEnvironmentMapSpecular(FXMVECTOR value)
{
    pImpl->constants.environmentMapSpecular = value;

    pImpl->specularEnabled = !XMVector3Equal(value, XMVectorZero());

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


void EnvironmentMapEffect::SetFresnelFactor(float value)
{
    pImpl->constants.fresnelFactor = value;

    pImpl->fresnelEnabled = (value != 0);

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


// Normal compression settings.
void EnvironmentMapEffect::SetBiasedVertexNormals(bool value)
{
    pImpl->biasedVertexNormals = value;
}
