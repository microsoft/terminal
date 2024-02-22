// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929


Texture2D<float4> Texture  : register(t0);
Texture2D<float4> Texture2 : register(t1);

sampler Sampler  : register(s0);
sampler Sampler2 : register(s1);


cbuffer Parameters : register(b0)
{
    float4   DiffuseColor   : packoffset(c0);
    float3   FogColor       : packoffset(c1);
    float4   FogVector      : packoffset(c2);
    float4x4 WorldViewProj  : packoffset(c3);
};


#include "Structures.fxh"
#include "Common.fxh"


// Vertex shader: basic.
VSOutputTx2 VSDualTexture(VSInputTx2 vin)
{
    VSOutputTx2 vout;

    CommonVSOutput cout = ComputeCommonVSOutput(vin.Position);
    SetCommonVSOutputParams;

    vout.TexCoord = vin.TexCoord;
    vout.TexCoord2 = vin.TexCoord2;

    return vout;
}


// Vertex shader: no fog.
VSOutputTx2NoFog VSDualTextureNoFog(VSInputTx2 vin)
{
    VSOutputTx2NoFog vout;

    CommonVSOutput cout = ComputeCommonVSOutput(vin.Position);
    SetCommonVSOutputParamsNoFog;

    vout.TexCoord = vin.TexCoord;
    vout.TexCoord2 = vin.TexCoord2;

    return vout;
}


// Vertex shader: vertex color.
VSOutputTx2 VSDualTextureVc(VSInputTx2Vc vin)
{
    VSOutputTx2 vout;

    CommonVSOutput cout = ComputeCommonVSOutput(vin.Position);
    SetCommonVSOutputParams;

    vout.TexCoord = vin.TexCoord;
    vout.TexCoord2 = vin.TexCoord2;
    vout.Diffuse *= vin.Color;

    return vout;
}


// Vertex shader: vertex color, no fog.
VSOutputTx2NoFog VSDualTextureVcNoFog(VSInputTx2Vc vin)
{
    VSOutputTx2NoFog vout;

    CommonVSOutput cout = ComputeCommonVSOutput(vin.Position);
    SetCommonVSOutputParamsNoFog;

    vout.TexCoord = vin.TexCoord;
    vout.TexCoord2 = vin.TexCoord2;
    vout.Diffuse *= vin.Color;

    return vout;
}


// Pixel shader: basic.
float4 PSDualTexture(PSInputTx2 pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord);
    float4 overlay = Texture2.Sample(Sampler2, pin.TexCoord2);

    color.rgb *= 2;
    color *= overlay * pin.Diffuse;

    ApplyFog(color, pin.Specular.w);

    return color;
}


// Pixel shader: no fog.
float4 PSDualTextureNoFog(PSInputTx2NoFog pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord);
    float4 overlay = Texture2.Sample(Sampler2, pin.TexCoord2);

    color.rgb *= 2;
    color *= overlay * pin.Diffuse;

    return color;
}
