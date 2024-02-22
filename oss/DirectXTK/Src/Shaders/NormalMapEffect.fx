// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929


Texture2D<float4> Texture : register(t0);
Texture2D<float3> NormalTexture : register(t1);
Texture2D<float3> SpecularTexture : register(t2);

sampler Sampler : register(s0);

cbuffer Parameters : register(b0)
{
    float4 DiffuseColor             : packoffset(c0);
    float3 EmissiveColor            : packoffset(c1);
    float3 SpecularColor            : packoffset(c2);
    float  SpecularPower            : packoffset(c2.w);

    float3 LightDirection[3]        : packoffset(c3);
    float3 LightDiffuseColor[3]     : packoffset(c6);
    float3 LightSpecularColor[3]    : packoffset(c9);

    float3 EyePosition              : packoffset(c12);

    float3 FogColor                 : packoffset(c13);
    float4 FogVector                : packoffset(c14);

    float4x4 World                  : packoffset(c15);
    float3x3 WorldInverseTranspose  : packoffset(c19);
    float4x4 WorldViewProj          : packoffset(c22);
};

cbuffer SkinningParameters : register(b1)
{
    float4x3 Bones[72];
}


#include "Structures.fxh"
#include "Common.fxh"
#include "Lighting.fxh"
#include "Utilities.fxh"
#include "Skinning.fxh"


// Vertex shader: pixel lighting + texture.
VSOutputPixelLightingTx VSNormalPixelLightingTx(VSInputNmTx vin)
{
    VSOutputPixelLightingTx vout;

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, vin.Normal);
    SetCommonVSOutputParamsPixelLighting;

    vout.Diffuse = float4(1, 1, 1, DiffuseColor.a);
    vout.TexCoord = vin.TexCoord;

    return vout;
}

VSOutputPixelLightingTx VSNormalPixelLightingTxBn(VSInputNmTx vin)
{
    VSOutputPixelLightingTx vout;

    float3 normal = BiasX2(vin.Normal);

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, normal);
    SetCommonVSOutputParamsPixelLighting;

    vout.Diffuse = float4(1, 1, 1, DiffuseColor.a);
    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Vertex shader: pixel lighting + texture + vertex color.
VSOutputPixelLightingTx VSNormalPixelLightingTxVc(VSInputNmTxVc vin)
{
    VSOutputPixelLightingTx vout;

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, vin.Normal);
    SetCommonVSOutputParamsPixelLighting;

    vout.Diffuse.rgb = vin.Color.rgb;
    vout.Diffuse.a = vin.Color.a * DiffuseColor.a;
    vout.TexCoord = vin.TexCoord;

    return vout;
}

VSOutputPixelLightingTx VSNormalPixelLightingTxVcBn(VSInputNmTxVc vin)
{
    VSOutputPixelLightingTx vout;

    float3 normal = BiasX2(vin.Normal);

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, normal);
    SetCommonVSOutputParamsPixelLighting;

    vout.Diffuse.rgb = vin.Color.rgb;
    vout.Diffuse.a = vin.Color.a * DiffuseColor.a;
    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Vertex shader: pixel lighting + texture + instancing.
VSOutputPixelLightingTx VSNormalPixelLightingTxInst(VSInputNmTxInst vin)
{
    VSOutputPixelLightingTx vout;

    CommonInstancing inst = ComputeCommonInstancing(vin.Position, vin.Normal, vin.Transform);

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(inst.Position, inst.Normal);
    SetCommonVSOutputParamsPixelLighting;

    vout.Diffuse = float4(1, 1, 1, DiffuseColor.a);
    vout.TexCoord = vin.TexCoord;

    return vout;
}

VSOutputPixelLightingTx VSNormalPixelLightingTxBnInst(VSInputNmTxInst vin)
{
    VSOutputPixelLightingTx vout;

    float3 normal = BiasX2(vin.Normal);

    CommonInstancing inst = ComputeCommonInstancing(vin.Position, normal, vin.Transform);

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(inst.Position, inst.Normal);
    SetCommonVSOutputParamsPixelLighting;

    vout.Diffuse = float4(1, 1, 1, DiffuseColor.a);
    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Vertex shader: pixel lighting + texture + vertex color + instancing.
VSOutputPixelLightingTx VSNormalPixelLightingTxVcInst(VSInputNmTxVcInst vin)
{
    VSOutputPixelLightingTx vout;

    CommonInstancing inst = ComputeCommonInstancing(vin.Position, vin.Normal, vin.Transform);

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(inst.Position, inst.Normal);
    SetCommonVSOutputParamsPixelLighting;

    vout.Diffuse.rgb = vin.Color.rgb;
    vout.Diffuse.a = vin.Color.a * DiffuseColor.a;
    vout.TexCoord = vin.TexCoord;

    return vout;
}

VSOutputPixelLightingTx VSNormalPixelLightingTxVcBnInst(VSInputNmTxVcInst vin)
{
    VSOutputPixelLightingTx vout;

    float3 normal = BiasX2(vin.Normal);

    CommonInstancing inst = ComputeCommonInstancing(vin.Position, normal, vin.Transform);

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(inst.Position, inst.Normal);
    SetCommonVSOutputParamsPixelLighting;

    vout.Diffuse.rgb = vin.Color.rgb;
    vout.Diffuse.a = vin.Color.a * DiffuseColor.a;
    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Vertex shader: skinning (four bones) + pixel lighting + texture
VSOutputPixelLightingTx VSSkinnedPixelLightingTx(VSInputNmTxWeights vin)
{
    VSOutputPixelLightingTx vout;

    float3 normal = Skin(vin, vin.Normal, 4);

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, normal);
    SetCommonVSOutputParamsPixelLighting;

    vout.Diffuse = float4(1, 1, 1, DiffuseColor.a);
    vout.TexCoord = vin.TexCoord;

    return vout;
}

VSOutputPixelLightingTx VSSkinnedPixelLightingTxBn(VSInputNmTxWeights vin)
{
    VSOutputPixelLightingTx vout;

    float3 normal = BiasX2(vin.Normal);

    normal = Skin(vin, normal, 4);

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, normal);
    SetCommonVSOutputParamsPixelLighting;

    vout.Diffuse = float4(1, 1, 1, DiffuseColor.a);
    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Pixel shader: pixel lighting + texture + no fog
float4 PSNormalPixelLightingTxNoFog(PSInputPixelLightingTx pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;

    float3 eyeVector = normalize(EyePosition - pin.PositionWS.xyz);
    float3 worldNormal = normalize(pin.NormalWS);

    // Before lighting, peturb the surface's normal by the one given in normal map.
    float3 localNormal = TwoChannelNormalX2(NormalTexture.Sample(Sampler, pin.TexCoord).xy);
    float3 normal = PeturbNormal(localNormal, pin.PositionWS.xyz, worldNormal, pin.TexCoord);

    // Do lighting
    ColorPair lightResult = ComputeLights(eyeVector, normal, 3);

    color.rgb *= lightResult.Diffuse;

    // Apply specular, modulated by the intensity given in the specular map
    float3 specIntensity = SpecularTexture.Sample(Sampler, pin.TexCoord);
    AddSpecular(color, lightResult.Specular * specIntensity);

    return color;
}


// Pixel shader: pixel lighting + texture
float4 PSNormalPixelLightingTx(PSInputPixelLightingTx pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;

    float3 eyeVector = normalize(EyePosition - pin.PositionWS.xyz);
    float3 worldNormal = normalize(pin.NormalWS);

    // Before lighting, peturb the surface's normal by the one given in normal map.
    float3 localNormal = TwoChannelNormalX2(NormalTexture.Sample(Sampler, pin.TexCoord).xy);
    float3 normal = PeturbNormal(localNormal, pin.PositionWS.xyz, worldNormal, pin.TexCoord);

    // Do lighting
    ColorPair lightResult = ComputeLights(eyeVector, normal, 3);

    color.rgb *= lightResult.Diffuse;

    // Apply specular, modulated by the intensity given in the specular map
    float3 specIntensity = SpecularTexture.Sample(Sampler, pin.TexCoord);
    AddSpecular(color, lightResult.Specular * specIntensity);

    ApplyFog(color, pin.PositionWS.w);

    return color;
}


// Pixel shader: pixel lighting + texture + no fog + no specular map
float4 PSNormalPixelLightingTxNoFogSpec(PSInputPixelLightingTx pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;

    float3 eyeVector = normalize(EyePosition - pin.PositionWS.xyz);
    float3 worldNormal = normalize(pin.NormalWS);

    // Before lighting, peturb the surface's normal by the one given in normal map.
    float3 localNormal = TwoChannelNormalX2(NormalTexture.Sample(Sampler, pin.TexCoord).xy);
    float3 normal = PeturbNormal(localNormal, pin.PositionWS.xyz, worldNormal, pin.TexCoord);

    // Do lighting
    ColorPair lightResult = ComputeLights(eyeVector, normal, 3);

    color.rgb *= lightResult.Diffuse;

    AddSpecular(color, lightResult.Specular);

    return color;
}


// Pixel shader: pixel lighting + texture + no specular map
float4 PSNormalPixelLightingTxNoSpec(PSInputPixelLightingTx pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;

    float3 eyeVector = normalize(EyePosition - pin.PositionWS.xyz);
    float3 worldNormal = normalize(pin.NormalWS);

    // Before lighting, peturb the surface's normal by the one given in normal map.
    float3 localNormal = TwoChannelNormalX2(NormalTexture.Sample(Sampler, pin.TexCoord).xy);
    float3 normal = PeturbNormal(localNormal, pin.PositionWS.xyz, worldNormal, pin.TexCoord);

    // Do lighting
    ColorPair lightResult = ComputeLights(eyeVector, normal, 3);

    color.rgb *= lightResult.Diffuse;

    AddSpecular(color, lightResult.Specular);
    ApplyFog(color, pin.PositionWS.w);

    return color;
}
