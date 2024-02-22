// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929


Texture2D<float4> Texture : register(t0);
sampler Sampler : register(s0);


cbuffer Parameters : register(b0)
{
    float4   DiffuseColor   : packoffset(c0);
    float4   AlphaTest      : packoffset(c1);
    float3   FogColor       : packoffset(c2);
    float4   FogVector      : packoffset(c3);
    float4x4 WorldViewProj  : packoffset(c4);
};


#include "Structures.fxh"
#include "Common.fxh"


// Vertex shader: basic.
VSOutputTx VSAlphaTest(VSInputTx vin)
{
    VSOutputTx vout;

    CommonVSOutput cout = ComputeCommonVSOutput(vin.Position);
    SetCommonVSOutputParams;

    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Vertex shader: no fog.
VSOutputTxNoFog VSAlphaTestNoFog(VSInputTx vin)
{
    VSOutputTxNoFog vout;

    CommonVSOutput cout = ComputeCommonVSOutput(vin.Position);
    SetCommonVSOutputParamsNoFog;

    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Vertex shader: vertex color.
VSOutputTx VSAlphaTestVc(VSInputTxVc vin)
{
    VSOutputTx vout;

    CommonVSOutput cout = ComputeCommonVSOutput(vin.Position);
    SetCommonVSOutputParams;

    vout.TexCoord = vin.TexCoord;
    vout.Diffuse *= vin.Color;

    return vout;
}


// Vertex shader: vertex color, no fog.
VSOutputTxNoFog VSAlphaTestVcNoFog(VSInputTxVc vin)
{
    VSOutputTxNoFog vout;

    CommonVSOutput cout = ComputeCommonVSOutput(vin.Position);
    SetCommonVSOutputParamsNoFog;

    vout.TexCoord = vin.TexCoord;
    vout.Diffuse *= vin.Color;

    return vout;
}


// Pixel shader: less/greater compare function.
float4 PSAlphaTestLtGt(PSInputTx pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;

    clip((color.a < AlphaTest.x) ? AlphaTest.z : AlphaTest.w);

    ApplyFog(color, pin.Specular.w);

    return color;
}


// Pixel shader: less/greater compare function, no fog.
float4 PSAlphaTestLtGtNoFog(PSInputTxNoFog pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;

    clip((color.a < AlphaTest.x) ? AlphaTest.z : AlphaTest.w);

    return color;
}


// Pixel shader: equal/notequal compare function.
float4 PSAlphaTestEqNe(PSInputTx pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;

    clip((abs(color.a - AlphaTest.x) < AlphaTest.y) ? AlphaTest.z : AlphaTest.w);

    ApplyFog(color, pin.Specular.w);

    return color;
}


// Pixel shader: equal/notequal compare function, no fog.
float4 PSAlphaTestEqNeNoFog(PSInputTxNoFog pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;

    clip((abs(color.a - AlphaTest.x) < AlphaTest.y) ? AlphaTest.z : AlphaTest.w);

    return color;
}
