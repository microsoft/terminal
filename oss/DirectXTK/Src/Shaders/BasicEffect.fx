// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929


Texture2D<float4> Texture : register(t0);
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


#include "Structures.fxh"
#include "Common.fxh"
#include "Lighting.fxh"
#include "Utilities.fxh"


// Vertex shader: basic.
VSOutput VSBasic(VSInput vin)
{
    VSOutput vout;

    CommonVSOutput cout = ComputeCommonVSOutput(vin.Position);
    SetCommonVSOutputParams;

    return vout;
}


// Vertex shader: no fog.
VSOutputNoFog VSBasicNoFog(VSInput vin)
{
    VSOutputNoFog vout;

    CommonVSOutput cout = ComputeCommonVSOutput(vin.Position);
    SetCommonVSOutputParamsNoFog;

    return vout;
}


// Vertex shader: vertex color.
VSOutput VSBasicVc(VSInputVc vin)
{
    VSOutput vout;

    CommonVSOutput cout = ComputeCommonVSOutput(vin.Position);
    SetCommonVSOutputParams;

    vout.Diffuse *= vin.Color;

    return vout;
}


// Vertex shader: vertex color, no fog.
VSOutputNoFog VSBasicVcNoFog(VSInputVc vin)
{
    VSOutputNoFog vout;

    CommonVSOutput cout = ComputeCommonVSOutput(vin.Position);
    SetCommonVSOutputParamsNoFog;

    vout.Diffuse *= vin.Color;

    return vout;
}


// Vertex shader: texture.
VSOutputTx VSBasicTx(VSInputTx vin)
{
    VSOutputTx vout;

    CommonVSOutput cout = ComputeCommonVSOutput(vin.Position);
    SetCommonVSOutputParams;

    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Vertex shader: texture, no fog.
VSOutputTxNoFog VSBasicTxNoFog(VSInputTx vin)
{
    VSOutputTxNoFog vout;

    CommonVSOutput cout = ComputeCommonVSOutput(vin.Position);
    SetCommonVSOutputParamsNoFog;

    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Vertex shader: texture + vertex color.
VSOutputTx VSBasicTxVc(VSInputTxVc vin)
{
    VSOutputTx vout;

    CommonVSOutput cout = ComputeCommonVSOutput(vin.Position);
    SetCommonVSOutputParams;

    vout.TexCoord = vin.TexCoord;
    vout.Diffuse *= vin.Color;

    return vout;
}


// Vertex shader: texture + vertex color, no fog.
VSOutputTxNoFog VSBasicTxVcNoFog(VSInputTxVc vin)
{
    VSOutputTxNoFog vout;

    CommonVSOutput cout = ComputeCommonVSOutput(vin.Position);
    SetCommonVSOutputParamsNoFog;

    vout.TexCoord = vin.TexCoord;
    vout.Diffuse *= vin.Color;

    return vout;
}


// Vertex shader: vertex lighting.
VSOutput VSBasicVertexLighting(VSInputNm vin)
{
    VSOutput vout;

    CommonVSOutput cout = ComputeCommonVSOutputWithLighting(vin.Position, vin.Normal, 3);
    SetCommonVSOutputParams;

    return vout;
}

VSOutput VSBasicVertexLightingBn(VSInputNm vin)
{
    VSOutput vout;

    float3 normal = BiasX2(vin.Normal);

    CommonVSOutput cout = ComputeCommonVSOutputWithLighting(vin.Position, normal, 3);
    SetCommonVSOutputParams;

    return vout;
}


// Vertex shader: vertex lighting + vertex color.
VSOutput VSBasicVertexLightingVc(VSInputNmVc vin)
{
    VSOutput vout;

    CommonVSOutput cout = ComputeCommonVSOutputWithLighting(vin.Position, vin.Normal, 3);
    SetCommonVSOutputParams;

    vout.Diffuse *= vin.Color;

    return vout;
}

VSOutput VSBasicVertexLightingVcBn(VSInputNmVc vin)
{
    VSOutput vout;

    float3 normal = BiasX2(vin.Normal);

    CommonVSOutput cout = ComputeCommonVSOutputWithLighting(vin.Position, normal, 3);
    SetCommonVSOutputParams;

    vout.Diffuse *= vin.Color;

    return vout;
}


// Vertex shader: vertex lighting + texture.
VSOutputTx VSBasicVertexLightingTx(VSInputNmTx vin)
{
    VSOutputTx vout;

    CommonVSOutput cout = ComputeCommonVSOutputWithLighting(vin.Position, vin.Normal, 3);
    SetCommonVSOutputParams;

    vout.TexCoord = vin.TexCoord;

    return vout;
}

VSOutputTx VSBasicVertexLightingTxBn(VSInputNmTx vin)
{
    VSOutputTx vout;

    float3 normal = BiasX2(vin.Normal);

    CommonVSOutput cout = ComputeCommonVSOutputWithLighting(vin.Position, normal, 3);
    SetCommonVSOutputParams;

    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Vertex shader: vertex lighting + texture + vertex color.
VSOutputTx VSBasicVertexLightingTxVc(VSInputNmTxVc vin)
{
    VSOutputTx vout;

    CommonVSOutput cout = ComputeCommonVSOutputWithLighting(vin.Position, vin.Normal, 3);
    SetCommonVSOutputParams;

    vout.TexCoord = vin.TexCoord;
    vout.Diffuse *= vin.Color;

    return vout;
}

VSOutputTx VSBasicVertexLightingTxVcBn(VSInputNmTxVc vin)
{
    VSOutputTx vout;

    float3 normal = BiasX2(vin.Normal);

    CommonVSOutput cout = ComputeCommonVSOutputWithLighting(vin.Position, normal, 3);
    SetCommonVSOutputParams;

    vout.TexCoord = vin.TexCoord;
    vout.Diffuse *= vin.Color;

    return vout;
}


// Vertex shader: one light.
VSOutput VSBasicOneLight(VSInputNm vin)
{
    VSOutput vout;

    CommonVSOutput cout = ComputeCommonVSOutputWithLighting(vin.Position, vin.Normal, 1);
    SetCommonVSOutputParams;

    return vout;
}

VSOutput VSBasicOneLightBn(VSInputNm vin)
{
    VSOutput vout;

    float3 normal = BiasX2(vin.Normal);

    CommonVSOutput cout = ComputeCommonVSOutputWithLighting(vin.Position, normal, 1);
    SetCommonVSOutputParams;

    return vout;
}


// Vertex shader: one light + vertex color.
VSOutput VSBasicOneLightVc(VSInputNmVc vin)
{
    VSOutput vout;

    CommonVSOutput cout = ComputeCommonVSOutputWithLighting(vin.Position, vin.Normal, 1);
    SetCommonVSOutputParams;

    vout.Diffuse *= vin.Color;

    return vout;
}

VSOutput VSBasicOneLightVcBn(VSInputNmVc vin)
{
    VSOutput vout;

    float3 normal = BiasX2(vin.Normal);

    CommonVSOutput cout = ComputeCommonVSOutputWithLighting(vin.Position, normal, 1);
    SetCommonVSOutputParams;

    vout.Diffuse *= vin.Color;

    return vout;
}


// Vertex shader: one light + texture.
VSOutputTx VSBasicOneLightTx(VSInputNmTx vin)
{
    VSOutputTx vout;

    CommonVSOutput cout = ComputeCommonVSOutputWithLighting(vin.Position, vin.Normal, 1);
    SetCommonVSOutputParams;

    vout.TexCoord = vin.TexCoord;

    return vout;
}

VSOutputTx VSBasicOneLightTxBn(VSInputNmTx vin)
{
    VSOutputTx vout;

    float3 normal = BiasX2(vin.Normal);

    CommonVSOutput cout = ComputeCommonVSOutputWithLighting(vin.Position, normal, 1);
    SetCommonVSOutputParams;

    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Vertex shader: one light + texture + vertex color.
VSOutputTx VSBasicOneLightTxVc(VSInputNmTxVc vin)
{
    VSOutputTx vout;

    CommonVSOutput cout = ComputeCommonVSOutputWithLighting(vin.Position, vin.Normal, 1);
    SetCommonVSOutputParams;

    vout.TexCoord = vin.TexCoord;
    vout.Diffuse *= vin.Color;

    return vout;
}

VSOutputTx VSBasicOneLightTxVcBn(VSInputNmTxVc vin)
{
    VSOutputTx vout;

    float3 normal = BiasX2(vin.Normal);

    CommonVSOutput cout = ComputeCommonVSOutputWithLighting(vin.Position, normal, 1);
    SetCommonVSOutputParams;

    vout.TexCoord = vin.TexCoord;
    vout.Diffuse *= vin.Color;

    return vout;
}


// Vertex shader: pixel lighting.
VSOutputPixelLighting VSBasicPixelLighting(VSInputNm vin)
{
    VSOutputPixelLighting vout;

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, vin.Normal);
    SetCommonVSOutputParamsPixelLighting;

    vout.Diffuse = float4(1, 1, 1, DiffuseColor.a);

    return vout;
}

VSOutputPixelLighting VSBasicPixelLightingBn(VSInputNm vin)
{
    VSOutputPixelLighting vout;

    float3 normal = BiasX2(vin.Normal);

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, normal);
    SetCommonVSOutputParamsPixelLighting;

    vout.Diffuse = float4(1, 1, 1, DiffuseColor.a);

    return vout;
}


// Vertex shader: pixel lighting + vertex color.
VSOutputPixelLighting VSBasicPixelLightingVc(VSInputNmVc vin)
{
    VSOutputPixelLighting vout;

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, vin.Normal);
    SetCommonVSOutputParamsPixelLighting;

    vout.Diffuse.rgb = vin.Color.rgb;
    vout.Diffuse.a = vin.Color.a * DiffuseColor.a;

    return vout;
}

VSOutputPixelLighting VSBasicPixelLightingVcBn(VSInputNmVc vin)
{
    VSOutputPixelLighting vout;

    float3 normal = BiasX2(vin.Normal);

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, normal);
    SetCommonVSOutputParamsPixelLighting;

    vout.Diffuse.rgb = vin.Color.rgb;
    vout.Diffuse.a = vin.Color.a * DiffuseColor.a;

    return vout;
}


// Vertex shader: pixel lighting + texture.
VSOutputPixelLightingTx VSBasicPixelLightingTx(VSInputNmTx vin)
{
    VSOutputPixelLightingTx vout;

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, vin.Normal);
    SetCommonVSOutputParamsPixelLighting;

    vout.Diffuse = float4(1, 1, 1, DiffuseColor.a);
    vout.TexCoord = vin.TexCoord;

    return vout;
}

VSOutputPixelLightingTx VSBasicPixelLightingTxBn(VSInputNmTx vin)
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
VSOutputPixelLightingTx VSBasicPixelLightingTxVc(VSInputNmTxVc vin)
{
    VSOutputPixelLightingTx vout;

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, vin.Normal);
    SetCommonVSOutputParamsPixelLighting;

    vout.Diffuse.rgb = vin.Color.rgb;
    vout.Diffuse.a = vin.Color.a * DiffuseColor.a;
    vout.TexCoord = vin.TexCoord;

    return vout;
}

VSOutputPixelLightingTx VSBasicPixelLightingTxVcBn(VSInputNmTxVc vin)
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


// Pixel shader: basic.
float4 PSBasic(PSInput pin) : SV_Target0
{
    float4 color = pin.Diffuse;

    ApplyFog(color, pin.Specular.w);

    return color;
}


// Pixel shader: no fog.
float4 PSBasicNoFog(PSInputNoFog pin) : SV_Target0
{
    return pin.Diffuse;
}


// Pixel shader: texture.
float4 PSBasicTx(PSInputTx pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;

    ApplyFog(color, pin.Specular.w);

    return color;
}


// Pixel shader: texture, no fog.
float4 PSBasicTxNoFog(PSInputTxNoFog pin) : SV_Target0
{
    return Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;
}


// Pixel shader: vertex lighting.
float4 PSBasicVertexLighting(PSInput pin) : SV_Target0
{
    float4 color = pin.Diffuse;

    AddSpecular(color, pin.Specular.rgb);
    ApplyFog(color, pin.Specular.w);

    return color;
}


// Pixel shader: vertex lighting, no fog.
float4 PSBasicVertexLightingNoFog(PSInput pin) : SV_Target0
{
    float4 color = pin.Diffuse;

    AddSpecular(color, pin.Specular.rgb);

    return color;
}


// Pixel shader: vertex lighting + texture.
float4 PSBasicVertexLightingTx(PSInputTx pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;

    AddSpecular(color, pin.Specular.rgb);
    ApplyFog(color, pin.Specular.w);

    return color;
}


// Pixel shader: vertex lighting + texture, no fog.
float4 PSBasicVertexLightingTxNoFog(PSInputTx pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;

    AddSpecular(color, pin.Specular.rgb);

    return color;
}


// Pixel shader: pixel lighting.
float4 PSBasicPixelLighting(PSInputPixelLighting pin) : SV_Target0
{
    float4 color = pin.Diffuse;

    float3 eyeVector = normalize(EyePosition - pin.PositionWS.xyz);
    float3 worldNormal = normalize(pin.NormalWS);

    ColorPair lightResult = ComputeLights(eyeVector, worldNormal, 3);

    color.rgb *= lightResult.Diffuse;

    AddSpecular(color, lightResult.Specular);
    ApplyFog(color, pin.PositionWS.w);

    return color;
}


// Pixel shader: pixel lighting + texture.
float4 PSBasicPixelLightingTx(PSInputPixelLightingTx pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;

    float3 eyeVector = normalize(EyePosition - pin.PositionWS.xyz);
    float3 worldNormal = normalize(pin.NormalWS);

    ColorPair lightResult = ComputeLights(eyeVector, worldNormal, 3);

    color.rgb *= lightResult.Diffuse;

    AddSpecular(color, lightResult.Specular);
    ApplyFog(color, pin.PositionWS.w);

    return color;
}
