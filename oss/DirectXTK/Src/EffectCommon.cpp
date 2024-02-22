//--------------------------------------------------------------------------------------
// File: EffectCommon.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "EffectCommon.h"
#include "DemandCreate.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;


// IEffectMatrices default method
void XM_CALLCONV IEffectMatrices::SetMatrices(FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection)
{
    SetWorld(world);
    SetView(view);
    SetProjection(projection);
}


// Constructor initializes default matrix values.
EffectMatrices::EffectMatrices() noexcept
{
    const XMMATRIX id = XMMatrixIdentity();
    world = id;
    view = id;
    projection = id;
    worldView = id;
}


// Lazily recomputes the combined world+view+projection matrix.
_Use_decl_annotations_
void EffectMatrices::SetConstants(int& dirtyFlags, XMMATRIX& worldViewProjConstant)
{
    if (dirtyFlags & EffectDirtyFlags::WorldViewProj)
    {
        worldView = XMMatrixMultiply(world, view);

        worldViewProjConstant = XMMatrixTranspose(XMMatrixMultiply(worldView, projection));

        dirtyFlags &= ~EffectDirtyFlags::WorldViewProj;
        dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
    }
}


// Constructor initializes default fog settings.
EffectFog::EffectFog() noexcept :
    enabled(false),
    start(0),
    end(1.f)
{
}


// Lazily recomputes the derived vector used by shader fog calculations.
_Use_decl_annotations_
void XM_CALLCONV EffectFog::SetConstants(int& dirtyFlags, FXMMATRIX worldView, XMVECTOR& fogVectorConstant)
{
    if (enabled)
    {
        if (dirtyFlags & (EffectDirtyFlags::FogVector | EffectDirtyFlags::FogEnable))
        {
            if (start == end)
            {
                // Degenerate case: force everything to 100% fogged if start and end are the same.
                static const XMVECTORF32 fullyFogged = { { { 0, 0, 0, 1 } } };

                fogVectorConstant = fullyFogged;
            }
            else
            {
                // We want to transform vertex positions into view space, take the resulting
                // Z value, then scale and offset according to the fog start/end distances.
                // Because we only care about the Z component, the shader can do all this
                // with a single dot product, using only the Z row of the world+view matrix.

                // _13, _23, _33, _43
                const XMVECTOR worldViewZ = XMVectorMergeXY(
                    XMVectorMergeZW(worldView.r[0], worldView.r[2]),
                    XMVectorMergeZW(worldView.r[1], worldView.r[3]));

                // 0, 0, 0, fogStart
                const XMVECTOR wOffset = XMVectorSwizzle<1, 2, 3, 0>(XMLoadFloat(&start));

                // (worldViewZ + wOffset) / (start - end);
                fogVectorConstant = XMVectorDivide(XMVectorAdd(worldViewZ, wOffset), XMVectorReplicate(start - end));
            }

            dirtyFlags &= ~(EffectDirtyFlags::FogVector | EffectDirtyFlags::FogEnable);
            dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
        }
    }
    else
    {
        // When fog is disabled, make sure the fog vector is reset to zero.
        if (dirtyFlags & EffectDirtyFlags::FogEnable)
        {
            fogVectorConstant = g_XMZero;

            dirtyFlags &= ~EffectDirtyFlags::FogEnable;
            dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
        }
    }
}


// Constructor initializes default material color settings.
EffectColor::EffectColor() noexcept :
    diffuseColor(g_XMOne),
    alpha(1.f)
{
}


// Lazily recomputes the material color parameter for shaders that do not support realtime lighting.
void EffectColor::SetConstants(_Inout_ int& dirtyFlags, _Inout_ XMVECTOR& diffuseColorConstant)
{
    if (dirtyFlags & EffectDirtyFlags::MaterialColor)
    {
        const XMVECTOR alphaVector = XMVectorReplicate(alpha);

        // xyz = diffuse * alpha, w = alpha.
        diffuseColorConstant = XMVectorSelect(alphaVector, XMVectorMultiply(diffuseColor, alphaVector), g_XMSelect1110);

        dirtyFlags &= ~EffectDirtyFlags::MaterialColor;
        dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
    }
}


// Constructor initializes default light settings.
EffectLights::EffectLights() noexcept :
    emissiveColor{},
    ambientLightColor{},
    lightEnabled{},
    lightDiffuseColor{},
    lightSpecularColor{}
{
    for (int i = 0; i < MaxDirectionalLights; i++)
    {
        lightEnabled[i] = (i == 0);
        lightDiffuseColor[i] = g_XMOne;
    }
}


#ifdef _PREFAST_
#pragma prefast(push)
#pragma prefast(disable:22103, "PREFAST doesn't understand buffer is bounded by a static const value even with SAL" )
#endif

// Initializes constant buffer fields to match the current lighting state.
_Use_decl_annotations_ void EffectLights::InitializeConstants(XMVECTOR& specularColorAndPowerConstant, XMVECTOR* lightDirectionConstant, XMVECTOR* lightDiffuseConstant, XMVECTOR* lightSpecularConstant) const
{
    static const XMVECTORF32 defaultSpecular = { { { 1, 1, 1, 16 } } };
    static const XMVECTORF32 defaultLightDirection = { { { 0, -1, 0, 0 } } };

    specularColorAndPowerConstant = defaultSpecular;

    for (int i = 0; i < MaxDirectionalLights; i++)
    {
        lightDirectionConstant[i] = defaultLightDirection;

        lightDiffuseConstant[i] = lightEnabled[i] ? lightDiffuseColor[i] : g_XMZero;
        lightSpecularConstant[i] = lightEnabled[i] ? lightSpecularColor[i] : g_XMZero;
    }
}

#ifdef _PREFAST_
#pragma prefast(pop)
#endif


// Lazily recomputes derived parameter values used by shader lighting calculations.
_Use_decl_annotations_ void EffectLights::SetConstants(int& dirtyFlags, EffectMatrices const& matrices, XMMATRIX& worldConstant, XMVECTOR worldInverseTransposeConstant[3], XMVECTOR& eyePositionConstant, XMVECTOR& diffuseColorConstant, XMVECTOR& emissiveColorConstant, bool lightingEnabled)
{
    if (lightingEnabled)
    {
        // World inverse transpose matrix.
        if (dirtyFlags & EffectDirtyFlags::WorldInverseTranspose)
        {
            worldConstant = XMMatrixTranspose(matrices.world);

            const XMMATRIX worldInverse = XMMatrixInverse(nullptr, matrices.world);

            worldInverseTransposeConstant[0] = worldInverse.r[0];
            worldInverseTransposeConstant[1] = worldInverse.r[1];
            worldInverseTransposeConstant[2] = worldInverse.r[2];

            dirtyFlags &= ~EffectDirtyFlags::WorldInverseTranspose;
            dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
        }

        // Eye position vector.
        if (dirtyFlags & EffectDirtyFlags::EyePosition)
        {
            XMMATRIX viewInverse = XMMatrixInverse(nullptr, matrices.view);

            eyePositionConstant = viewInverse.r[3];

            dirtyFlags &= ~EffectDirtyFlags::EyePosition;
            dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
        }
    }

    // Material color parameters. The desired lighting model is:
    //
    //     ((ambientLightColor + sum(diffuse directional light)) * diffuseColor) + emissiveColor
    //
    // When lighting is disabled, ambient and directional lights are ignored, leaving:
    //
    //     diffuseColor + emissiveColor
    //
    // For the lighting disabled case, we can save one shader instruction by precomputing
    // diffuse+emissive on the CPU, after which the shader can use diffuseColor directly,
    // ignoring its emissive parameter.
    //
    // When lighting is enabled, we can merge the ambient and emissive settings. If we
    // set our emissive parameter to emissive+(ambient*diffuse), the shader no longer
    // needs to bother adding the ambient contribution, simplifying its computation to:
    //
    //     (sum(diffuse directional light) * diffuseColor) + emissiveColor
    //
    // For futher optimization goodness, we merge material alpha with the diffuse
    // color parameter, and premultiply all color values by this alpha.

    if (dirtyFlags & EffectDirtyFlags::MaterialColor)
    {
        XMVECTOR diffuse = diffuseColor;
        const XMVECTOR alphaVector = XMVectorReplicate(alpha);

        if (lightingEnabled)
        {
            // Merge emissive and ambient light contributions.
            // (emissiveColor + ambientLightColor * diffuse) * alphaVector;
            emissiveColorConstant = XMVectorMultiply(XMVectorMultiplyAdd(ambientLightColor, diffuse, emissiveColor), alphaVector);
        }
        else
        {
            // Merge diffuse and emissive light contributions.
            diffuse = XMVectorAdd(diffuse, emissiveColor);
        }

        // xyz = diffuse * alpha, w = alpha.
        diffuseColorConstant = XMVectorSelect(alphaVector, XMVectorMultiply(diffuse, alphaVector), g_XMSelect1110);

        dirtyFlags &= ~EffectDirtyFlags::MaterialColor;
        dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
    }
}


#ifdef _PREFAST_
#pragma prefast(push)
#pragma prefast(disable:26015, "PREFAST doesn't understand that ValidateLightIndex bounds whichLight" )
#endif

// Helper for turning one of the directional lights on or off.
_Use_decl_annotations_ int EffectLights::SetLightEnabled(int whichLight, bool value, XMVECTOR* lightDiffuseConstant, XMVECTOR* lightSpecularConstant)
{
    ValidateLightIndex(whichLight);

    if (lightEnabled[whichLight] == value)
        return 0;

    lightEnabled[whichLight] = value;

    if (value)
    {
        // If this light is now on, store its color in the constant buffer.
        lightDiffuseConstant[whichLight] = lightDiffuseColor[whichLight];
        lightSpecularConstant[whichLight] = lightSpecularColor[whichLight];
    }
    else
    {
        // If the light is off, reset constant buffer colors to zero.
        lightDiffuseConstant[whichLight] = g_XMZero;
        lightSpecularConstant[whichLight] = g_XMZero;
    }

    return EffectDirtyFlags::ConstantBuffer;
}


// Helper for setting diffuse color of one of the directional lights.
_Use_decl_annotations_
int XM_CALLCONV EffectLights::SetLightDiffuseColor(int whichLight, FXMVECTOR value, XMVECTOR* lightDiffuseConstant)
{
    ValidateLightIndex(whichLight);

    // Locally store the new color.
    lightDiffuseColor[whichLight] = value;

    // If this light is currently on, also update the constant buffer.
    if (lightEnabled[whichLight])
    {
        lightDiffuseConstant[whichLight] = value;

        return EffectDirtyFlags::ConstantBuffer;
    }

    return 0;
}


// Helper for setting specular color of one of the directional lights.
_Use_decl_annotations_
int XM_CALLCONV EffectLights::SetLightSpecularColor(int whichLight, FXMVECTOR value, XMVECTOR* lightSpecularConstant)
{
    ValidateLightIndex(whichLight);

    // Locally store the new color.
    lightSpecularColor[whichLight] = value;

    // If this light is currently on, also update the constant buffer.
    if (lightEnabled[whichLight])
    {
        lightSpecularConstant[whichLight] = value;

        return EffectDirtyFlags::ConstantBuffer;
    }

    return 0;
}

#ifdef _PREFAST_
#pragma prefast(pop)
#endif


// Parameter validation helper.
void EffectLights::ValidateLightIndex(int whichLight)
{
    if (whichLight < 0 || whichLight >= MaxDirectionalLights)
    {
        throw std::invalid_argument("whichLight parameter invalid");
    }
}


// Activates the default lighting rig (key, fill, and back lights).
void EffectLights::EnableDefaultLighting(_In_ IEffectLights* effect)
{
    static const XMVECTORF32 defaultDirections[MaxDirectionalLights] =
    {
        { { { -0.5265408f, -0.5735765f, -0.6275069f, 0 } } },
        { { {  0.7198464f,  0.3420201f,  0.6040227f, 0 } } },
        { { {  0.4545195f, -0.7660444f,  0.4545195f, 0 } } },
    };

    static const XMVECTORF32 defaultDiffuse[MaxDirectionalLights] =
    {
        { { { 1.0000000f, 0.9607844f, 0.8078432f, 0 } }  },
        { { { 0.9647059f, 0.7607844f, 0.4078432f, 0 } }  },
        { { { 0.3231373f, 0.3607844f, 0.3937255f, 0 } }  },
    };

    static const XMVECTORF32 defaultSpecular[MaxDirectionalLights] =
    {
        { { { 1.0000000f, 0.9607844f, 0.8078432f, 0 } }  },
        { { { 0.0000000f, 0.0000000f, 0.0000000f, 0 } }  },
        { { { 0.3231373f, 0.3607844f, 0.3937255f, 0 } }  },
    };

    static const XMVECTORF32 defaultAmbient = { { { 0.05333332f, 0.09882354f, 0.1819608f, 0 } } };

    effect->SetLightingEnabled(true);
    effect->SetAmbientLightColor(defaultAmbient);

    for (int i = 0; i < MaxDirectionalLights; i++)
    {
        effect->SetLightEnabled(i, true);
        effect->SetLightDirection(i, defaultDirections[i]);
        effect->SetLightDiffuseColor(i, defaultDiffuse[i]);
        effect->SetLightSpecularColor(i, defaultSpecular[i]);
    }
}


// Gets or lazily creates the specified vertex shader permutation.
ID3D11VertexShader* EffectDeviceResources::DemandCreateVertexShader(_Inout_ ComPtr<ID3D11VertexShader>& vertexShader, ShaderBytecode const& bytecode)
{
    return DemandCreate(vertexShader, mMutex, [&](ID3D11VertexShader** pResult) -> HRESULT
        {
            HRESULT hr = mDevice->CreateVertexShader(bytecode.code, bytecode.length, nullptr, pResult);

            if (SUCCEEDED(hr))
                SetDebugObjectName(*pResult, "DirectXTK:Effect");

            return hr;
        });
}


// Gets or lazily creates the specified pixel shader permutation.
ID3D11PixelShader* EffectDeviceResources::DemandCreatePixelShader(_Inout_ ComPtr<ID3D11PixelShader>& pixelShader, ShaderBytecode const& bytecode)
{
    return DemandCreate(pixelShader, mMutex, [&](ID3D11PixelShader** pResult) -> HRESULT
        {
            HRESULT hr = mDevice->CreatePixelShader(bytecode.code, bytecode.length, nullptr, pResult);

            if (SUCCEEDED(hr))
                SetDebugObjectName(*pResult, "DirectXTK:Effect");

            return hr;
        });
}


// Gets or lazily creates the default texture
ID3D11ShaderResourceView* EffectDeviceResources::GetDefaultTexture()
{
    return DemandCreate(mDefaultTexture, mMutex, [&](ID3D11ShaderResourceView** pResult) -> HRESULT
        {
            static const uint32_t s_pixel = 0xffffffff;

            D3D11_SUBRESOURCE_DATA initData = { &s_pixel, sizeof(uint32_t), 0 };

            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = desc.Height = desc.MipLevels = desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_IMMUTABLE;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            ComPtr<ID3D11Texture2D> tex;
            HRESULT hr = mDevice->CreateTexture2D(&desc, &initData, tex.GetAddressOf());

            if (SUCCEEDED(hr))
            {
                SetDebugObjectName(tex.Get(), "DirectXTK:Effect");

                D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
                SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                SRVDesc.Texture2D.MipLevels = 1;

                hr = mDevice->CreateShaderResourceView(tex.Get(), &SRVDesc, pResult);
                if (SUCCEEDED(hr))
                    SetDebugObjectName(*pResult, "DirectXTK:Effect");
            }

            return hr;
        });
}

ID3D11ShaderResourceView* EffectDeviceResources::GetDefaultNormalTexture()
{
    return DemandCreate(mDefaultNormalTexture, mMutex, [&](ID3D11ShaderResourceView** pResult) -> HRESULT
        {
            static const uint16_t s_pixel = 0x7f7f;

            D3D11_SUBRESOURCE_DATA initData = { &s_pixel, sizeof(uint16_t), 0 };

            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = desc.Height = desc.MipLevels = desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8G8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_IMMUTABLE;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            ComPtr<ID3D11Texture2D> tex;
            HRESULT hr = mDevice->CreateTexture2D(&desc, &initData, tex.GetAddressOf());

            if (SUCCEEDED(hr))
            {
                SetDebugObjectName(tex.Get(), "DirectXTK:Effect");

                D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
                SRVDesc.Format = DXGI_FORMAT_R8G8_UNORM;
                SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                SRVDesc.Texture2D.MipLevels = 1;

                hr = mDevice->CreateShaderResourceView(tex.Get(), &SRVDesc, pResult);
                if (SUCCEEDED(hr))
                    SetDebugObjectName(*pResult, "DirectXTK:Effect");
            }

            return hr;
        });
}

// Gets device feature level
D3D_FEATURE_LEVEL EffectDeviceResources::GetDeviceFeatureLevel() const
{
    return mDevice->GetFeatureLevel();
}
