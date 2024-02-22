// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929


cbuffer Parameters : register(b0)
{
    float3 AmbientDown              : packoffset(c0);
    float  Alpha                    : packoffset(c0.w);
    float3 AmbientRange             : packoffset(c1);

    float4x4 World                  : packoffset(c2);
    float3x3 WorldInverseTranspose  : packoffset(c6);
    float4x4 WorldViewProj          : packoffset(c9);
};


#include "Structures.fxh"
#include "Utilities.fxh"


// Vertex shader: basic
VSOutputPixelLightingTx VSDebug(VSInputNmTx vin)
{
    VSOutputPixelLightingTx vout;

    vout.PositionPS = mul(vin.Position, WorldViewProj);
    vout.PositionWS = float4(mul(vin.Position, World).xyz, 1);
    vout.NormalWS = normalize(mul(vin.Normal, WorldInverseTranspose));
    vout.Diffuse = float4(1, 1, 1, Alpha);
    vout.TexCoord = vin.TexCoord;

    return vout;
}

VSOutputPixelLightingTx VSDebugBn(VSInputNmTx vin)
{
    VSOutputPixelLightingTx vout;

    float3 normal = BiasX2(vin.Normal);

    vout.PositionPS = mul(vin.Position, WorldViewProj);
    vout.PositionWS = float4(mul(vin.Position, World).xyz, 1);
    vout.NormalWS = normalize(mul(normal, WorldInverseTranspose));
    vout.Diffuse = float4(1, 1, 1, Alpha);
    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Vertex shader: vertex color.
VSOutputPixelLightingTx VSDebugVc(VSInputNmTxVc vin)
{
    VSOutputPixelLightingTx vout;

    vout.PositionPS = mul(vin.Position, WorldViewProj);
    vout.PositionWS = float4(mul(vin.Position, World).xyz, 1);
    vout.NormalWS = normalize(mul(vin.Normal, WorldInverseTranspose));
    vout.Diffuse.rgb = vin.Color.rgb;
    vout.Diffuse.a = vin.Color.a * Alpha;
    vout.TexCoord = vin.TexCoord;

    return vout;
}

VSOutputPixelLightingTx VSDebugVcBn(VSInputNmTxVc vin)
{
    VSOutputPixelLightingTx vout;

    float3 normal = BiasX2(vin.Normal);

    vout.PositionPS = mul(vin.Position, WorldViewProj);
    vout.PositionWS = float4(mul(vin.Position, World).xyz, 1);
    vout.NormalWS = normalize(mul(normal, WorldInverseTranspose));
    vout.Diffuse.rgb = vin.Color.rgb;
    vout.Diffuse.a = vin.Color.a * Alpha;
    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Vertex shader: instancing
VSOutputPixelLightingTx VSDebugInst(VSInputNmTxInst vin)
{
    VSOutputPixelLightingTx vout;

    CommonInstancing inst = ComputeCommonInstancing(vin.Position, vin.Normal, vin.Transform);

    vout.PositionPS = mul(inst.Position, WorldViewProj);
    vout.PositionWS = float4(mul(inst.Position, World).xyz, 1);
    vout.NormalWS = normalize(mul(inst.Normal, WorldInverseTranspose));
    vout.Diffuse = float4(1, 1, 1, Alpha);
    vout.TexCoord = vin.TexCoord;

    return vout;
}

VSOutputPixelLightingTx VSDebugBnInst(VSInputNmTxInst vin)
{
    VSOutputPixelLightingTx vout;

    float3 normal = BiasX2(vin.Normal);

    CommonInstancing inst = ComputeCommonInstancing(vin.Position, normal, vin.Transform);

    vout.PositionPS = mul(inst.Position, WorldViewProj);
    vout.PositionWS = float4(mul(inst.Position, World).xyz, 1);
    vout.NormalWS = normalize(mul(inst.Normal, WorldInverseTranspose));
    vout.Diffuse = float4(1, 1, 1, Alpha);
    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Vertex shader: vertex color + instancing
VSOutputPixelLightingTx VSDebugVcInst(VSInputNmTxVcInst vin)
{
    VSOutputPixelLightingTx vout;

    CommonInstancing inst = ComputeCommonInstancing(vin.Position, vin.Normal, vin.Transform);

    vout.PositionPS = mul(inst.Position, WorldViewProj);
    vout.PositionWS = float4(mul(inst.Position, World).xyz, 1);
    vout.NormalWS = normalize(mul(inst.Normal, WorldInverseTranspose));
    vout.Diffuse.rgb = vin.Color.rgb;
    vout.Diffuse.a = vin.Color.a * Alpha;
    vout.TexCoord = vin.TexCoord;

    return vout;
}

VSOutputPixelLightingTx VSDebugVcBnInst(VSInputNmTxVcInst vin)
{
    VSOutputPixelLightingTx vout;

    float3 normal = BiasX2(vin.Normal);

    CommonInstancing inst = ComputeCommonInstancing(vin.Position, normal, vin.Transform);

    vout.PositionPS = mul(inst.Position, WorldViewProj);
    vout.PositionWS = float4(mul(inst.Position, World).xyz, 1);
    vout.NormalWS = normalize(mul(inst.Normal, WorldInverseTranspose));
    vout.Diffuse.rgb = vin.Color.rgb;
    vout.Diffuse.a = vin.Color.a * Alpha;
    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Pixel shader: default
float3 CalcHemiAmbient(float3 normal, float3 color)
{
    float3 up = BiasD2(normal);
    float3 ambient = AmbientDown + up.y * AmbientRange;
    return ambient * color;
}

float4 PSHemiAmbient(PSInputPixelLightingTx pin) : SV_Target0
{
    float3 normal = normalize(pin.NormalWS);

    // Do lighting
    float3 color = CalcHemiAmbient(normal, pin.Diffuse.rgb);

    return float4(color, pin.Diffuse.a);
}


// Pixel shader: RGB normals
float4 PSRGBNormals(PSInputPixelLightingTx pin) : SV_Target0
{
    float3 normal = normalize(pin.NormalWS);

    float3 color = BiasD2(normal);

    return float4(color, pin.Diffuse.a);
}

// Pixel shader: RGB tangents
float4 PSRGBTangents(PSInputPixelLightingTx pin) : SV_Target0
{
    const float3x3 TBN = CalculateTBN(pin.PositionWS.xyz, pin.NormalWS, pin.TexCoord);
    float3 tangent = normalize(TBN[0]);

    float3 color = BiasD2(tangent);

    return float4(color, pin.Diffuse.a);
}

// Pixel shader: RGB bi-tangents
float4 PSRGBBiTangents(PSInputPixelLightingTx pin) : SV_Target0
{
    const float3x3 TBN = CalculateTBN(pin.PositionWS.xyz, pin.NormalWS, pin.TexCoord);
    float3 bitangent = normalize(TBN[1]);

    float3 color = BiasD2(bitangent);

    return float4(color, pin.Diffuse.a);
}
