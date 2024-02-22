// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929

Texture2D<float4> HDRTexture : register(t0);
sampler Sampler : register(s0);


cbuffer Parameters : register(b0)
{
    float linearExposure    : packoffset(c0.x);
    float paperWhiteNits    : packoffset(c0.y);
    float4x3 colorRotation  : packoffset(c1);
};



#include "Structures.fxh"
#include "Utilities.fxh"


// Vertex shader: self-created quad.
VSInputTx VSQuad(uint vI : SV_VertexId)
{
    VSInputTx vout;

    // We use the 'big triangle' optimization so you only Draw 3 verticies instead of 4.
    float2 texcoord = float2((vI << 1) & 2, vI & 2);
    vout.TexCoord = texcoord;

    vout.Position = float4(texcoord.x * 2 - 1, -texcoord.y * 2 + 1, 0, 1);
    return vout;
}


//--------------------------------------------------------------------------------------
// Pixel shader: pass-through
float4 PSCopy(VSInputTx pin) : SV_Target0
{
    return HDRTexture.Sample(Sampler, pin.TexCoord);
}


// Pixel shader: saturate (clips above 1.0)
float4 PSSaturate(VSInputTx pin) : SV_Target0
{
    float4 hdr = HDRTexture.Sample(Sampler, pin.TexCoord);
    float3 sdr = saturate(hdr.xyz * linearExposure);
    return float4(sdr, hdr.a);
}


// Pixel shader: reinhard operator
float4 PSReinhard(VSInputTx pin) : SV_Target0
{
    float4 hdr = HDRTexture.Sample(Sampler, pin.TexCoord);
    float3 sdr = ToneMapReinhard(hdr.xyz * linearExposure);
    return float4(sdr, hdr.a);
}


// Pixel shader: ACES filmic operator
float4 PSACESFilmic(VSInputTx pin) : SV_Target0
{
    float4 hdr = HDRTexture.Sample(Sampler, pin.TexCoord);
    float3 sdr = ToneMapACESFilmic(hdr.xyz * linearExposure);
    return float4(sdr, hdr.a);
}


//--------------------------------------------------------------------------------------
// SRGB, using Rec.709 color primaries and a gamma 2.2 curve

// Pixel shader: sRGB
float4 PS_SRGB(VSInputTx pin) : SV_Target0
{
    float4 hdr = HDRTexture.Sample(Sampler, pin.TexCoord);
    float3 srgb = LinearToSRGBEst(hdr.xyz);
    return float4(srgb, hdr.a);
}


// Pixel shader: saturate (clips above 1.0)
float4 PSSaturate_SRGB(VSInputTx pin) : SV_Target0
{
    float4 hdr = HDRTexture.Sample(Sampler, pin.TexCoord);
    float3 sdr = saturate(hdr.xyz * linearExposure);
    float3 srgb = LinearToSRGBEst(sdr);
    return float4(srgb, hdr.a);
}


// Pixel shader: reinhard operator
float4 PSReinhard_SRGB(VSInputTx pin) : SV_Target0
{
    float4 hdr = HDRTexture.Sample(Sampler, pin.TexCoord);
    float3 sdr = ToneMapReinhard(hdr.xyz * linearExposure);
    float3 srgb = LinearToSRGBEst(sdr);
    return float4(srgb, hdr.a);
}


// Pixel shader: ACES filmic operator
float4 PSACESFilmic_SRGB(VSInputTx pin) : SV_Target0
{
    float4 hdr = HDRTexture.Sample(Sampler, pin.TexCoord);
    float3 sdr = ToneMapACESFilmic(hdr.xyz * linearExposure);
    float3 srgb = LinearToSRGBEst(sdr);
    return float4(srgb, hdr.a);
}


//--------------------------------------------------------------------------------------
// HDR10, using Rec.2020 color primaries and ST.2084 curve

float3 HDR10(float3 color)
{
    // Rotate from Rec.709 to Rec.2020 primaries
    float3 rgb = mul(color, (float3x3)colorRotation);

    // ST.2084 spec defines max nits as 10,000 nits
    float3 normalized = rgb * paperWhiteNits / 10000.f;

    // Apply ST.2084 curve
    return LinearToST2084(normalized);
}

float4 PSHDR10(VSInputTx pin) : SV_Target0
{
    float4 hdr = HDRTexture.Sample(Sampler, pin.TexCoord);
    float3 rgb = HDR10(hdr.xyz);
    return float4(rgb, hdr.a);
}


//--------------------------------------------------------------------------------------
struct MRTOut
{
    float4 hdr : SV_Target0;
    float4 sdr : SV_Target1;
};

MRTOut PSHDR10_Saturate(VSInputTx pin)
{
    MRTOut output;

    float4 hdr = HDRTexture.Sample(Sampler, pin.TexCoord);
    float3 rgb = HDR10(hdr.xyz);
    output.hdr = float4(rgb, hdr.a);

    float3 sdr = saturate(hdr.xyz * linearExposure);
    output.sdr = float4(sdr, hdr.a);

    return output;
}

MRTOut PSHDR10_Reinhard(VSInputTx pin)
{
    MRTOut output;

    float4 hdr = HDRTexture.Sample(Sampler, pin.TexCoord);
    float3 rgb = HDR10(hdr.xyz);
    output.hdr = float4(rgb, hdr.a);

    float3 sdr = ToneMapReinhard(hdr.xyz * linearExposure);
    output.sdr = float4(sdr, hdr.a);

    return output;
}

MRTOut PSHDR10_ACESFilmic(VSInputTx pin)
{
    MRTOut output;

    float4 hdr = HDRTexture.Sample(Sampler, pin.TexCoord);
    float3 rgb = HDR10(hdr.xyz);
    output.hdr = float4(rgb, hdr.a);

    float3 sdr = ToneMapACESFilmic(hdr.xyz * linearExposure);
    output.sdr = float4(sdr, hdr.a);

    return output;
}

MRTOut PSHDR10_Saturate_SRGB(VSInputTx pin)
{
    MRTOut output;

    float4 hdr = HDRTexture.Sample(Sampler, pin.TexCoord);
    float3 rgb = HDR10(hdr.xyz);
    output.hdr = float4(rgb, hdr.a);

    float3 sdr = saturate(hdr.xyz * linearExposure);
    float3 srgb = LinearToSRGBEst(sdr);
    output.sdr = float4(srgb, hdr.a);

    return output;
}

MRTOut PSHDR10_Reinhard_SRGB(VSInputTx pin)
{
    MRTOut output;

    float4 hdr = HDRTexture.Sample(Sampler, pin.TexCoord);
    float3 rgb = HDR10(hdr.xyz);
    output.hdr = float4(rgb, hdr.a);

    float3 sdr = ToneMapReinhard(hdr.xyz * linearExposure);
    float3 srgb = LinearToSRGBEst(sdr);
    output.sdr = float4(srgb, hdr.a);

    return output;
}

MRTOut PSHDR10_ACESFilmic_SRGB(VSInputTx pin)
{
    MRTOut output;

    float4 hdr = HDRTexture.Sample(Sampler, pin.TexCoord);
    float3 rgb = HDR10(hdr.xyz);
    output.hdr = float4(rgb, hdr.a);

    float3 sdr = ToneMapACESFilmic(hdr.xyz * linearExposure);
    float3 srgb = LinearToSRGBEst(sdr);
    output.sdr = float4(srgb, hdr.a);

    return output;
}
