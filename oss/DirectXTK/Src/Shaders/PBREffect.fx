// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929


Texture2D<float4> AlbedoTexture : register(t0);
Texture2D<float3> NormalTexture : register(t1);
Texture2D<float3> RMATexture    : register(t2);

Texture2D<float3> EmissiveTexture : register(t3);

TextureCube<float3> RadianceTexture   : register(t4);
TextureCube<float3> IrradianceTexture : register(t5);

sampler SurfaceSampler : register(s0);
sampler IBLSampler     : register(s1);

cbuffer Constants : register(b0)
{
    float3   EyePosition            : packoffset(c0);
    float4x4 World                  : packoffset(c1);
    float3x3 WorldInverseTranspose  : packoffset(c5);
    float4x4 WorldViewProj          : packoffset(c8);
    float4x4 PrevWorldViewProj      : packoffset(c12);

    float3 LightDirection[3]        : packoffset(c16);
    float3 LightColor[3]            : packoffset(c19);   // "Specular and diffuse light" in PBR

    float3 ConstantAlbedo           : packoffset(c22);   // Constant values if not a textured effect
    float  Alpha                    : packoffset(c22.w);
    float  ConstantMetallic         : packoffset(c23.x);
    float  ConstantRoughness        : packoffset(c23.y);

    int NumRadianceMipLevels        : packoffset(c23.z);

    // Size of render target
    float TargetWidth : packoffset(c23.w);
    float TargetHeight : packoffset(c24.x);
};

cbuffer SkinningParameters : register(b1)
{
    float4x3 Bones[72];
}


#include "Structures.fxh"
#include "PBRCommon.fxh"
#include "Utilities.fxh"
#include "Skinning.fxh"


// Vertex shader: pbr
VSOutputPixelLightingTx VSConstant(VSInputNmTx vin)
{
    VSOutputPixelLightingTx vout;

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, vin.Normal);

    vout.PositionPS = cout.Pos_ps;
    vout.PositionWS = float4(cout.Pos_ws, 1);
    vout.NormalWS = cout.Normal_ws;
    vout.Diffuse = float4(ConstantAlbedo, Alpha);
    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Vertex shader: pbr + instancing
VSOutputPixelLightingTx VSConstantInst(VSInputNmTxInst vin)
{
    VSOutputPixelLightingTx vout;

    CommonInstancing inst = ComputeCommonInstancing(vin.Position, vin.Normal, vin.Transform);

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(inst.Position, inst.Normal);

    vout.PositionPS = cout.Pos_ps;
    vout.PositionWS = float4(cout.Pos_ws, 1);
    vout.NormalWS = cout.Normal_ws;
    vout.Diffuse = float4(ConstantAlbedo, Alpha);
    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Vertex shader: pbr + velocity
VSOut_Velocity VSConstantVelocity(VSInputNmTx vin)
{
    VSOut_Velocity vout;

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, vin.Normal);

    vout.current.PositionPS = cout.Pos_ps;
    vout.current.PositionWS = float4(cout.Pos_ws, 1);
    vout.current.NormalWS = cout.Normal_ws;
    vout.current.Diffuse = float4(ConstantAlbedo, Alpha);
    vout.current.TexCoord = vin.TexCoord;
    vout.prevPosition = mul(vin.Position, PrevWorldViewProj);

    return vout;
}


// Vertex shader: pbr (biased normal)
VSOutputPixelLightingTx VSConstantBn(VSInputNmTx vin)
{
    VSOutputPixelLightingTx vout;

    float3 normal = BiasX2(vin.Normal);

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, normal);

    vout.PositionPS = cout.Pos_ps;
    vout.PositionWS = float4(cout.Pos_ws, 1);
    vout.NormalWS = cout.Normal_ws;
    vout.Diffuse = float4(ConstantAlbedo, Alpha);
    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Vertex shader: pbr + instancing (biased normal)
VSOutputPixelLightingTx VSConstantBnInst(VSInputNmTxInst vin)
{
    VSOutputPixelLightingTx vout;

    float3 normal = BiasX2(vin.Normal);

    CommonInstancing inst = ComputeCommonInstancing(vin.Position, normal, vin.Transform);

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(inst.Position, inst.Normal);

    vout.PositionPS = cout.Pos_ps;
    vout.PositionWS = float4(cout.Pos_ws, 1);
    vout.NormalWS = cout.Normal_ws;
    vout.Diffuse = float4(ConstantAlbedo, Alpha);
    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Vertex shader: pbr + velocity (biased normal)
VSOut_Velocity VSConstantVelocityBn(VSInputNmTx vin)
{
    VSOut_Velocity vout;

    float3 normal = BiasX2(vin.Normal);

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, normal);

    vout.current.PositionPS = cout.Pos_ps;
    vout.current.PositionWS = float4(cout.Pos_ws, 1);
    vout.current.NormalWS = cout.Normal_ws;
    vout.current.Diffuse = float4(ConstantAlbedo, Alpha);
    vout.current.TexCoord = vin.TexCoord;

    vout.prevPosition = mul(vin.Position, PrevWorldViewProj);

    return vout;
}


// Vertex shader: pbr + skinning (four bones)
VSOutputPixelLightingTx VSSkinned(VSInputNmTxWeights vin)
{
    VSOutputPixelLightingTx vout;

    float3 normal = Skin(vin, vin.Normal, 4);

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, normal);

    vout.PositionPS = cout.Pos_ps;
    vout.PositionWS = float4(cout.Pos_ws, 1);
    vout.NormalWS = cout.Normal_ws;
    vout.Diffuse = float4(ConstantAlbedo, Alpha);
    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Vertex shader: pbr + skinning (four bones) (biased normal)
VSOutputPixelLightingTx VSSkinnedBn(VSInputNmTxWeights vin)
{
    VSOutputPixelLightingTx vout;

    float3 normal = BiasX2(vin.Normal);

    normal = Skin(vin, normal, 4);

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, normal);

    vout.PositionPS = cout.Pos_ps;
    vout.PositionWS = float4(cout.Pos_ws, 1);
    vout.NormalWS = cout.Normal_ws;
    vout.Diffuse = float4(ConstantAlbedo, Alpha);
    vout.TexCoord = vin.TexCoord;

    return vout;
}


// Pixel shader: pbr (constants) + image-based lighting
float4 PSConstant(PSInputPixelLightingTx pin) : SV_Target0
{
    // vectors
    const float3 V = normalize(EyePosition - pin.PositionWS.xyz);   // view vector
    const float3 N = normalize(pin.NormalWS);                       // surface normal
    const float AO = 1;                                             // ambient term

    float3 color = LightSurface(V, N, 3,
        LightColor, LightDirection,
        ConstantAlbedo, ConstantRoughness, ConstantMetallic, AO);

    return float4(color, Alpha);
}


// Pixel shader: pbr (textures) + image-based lighting
float4 PSTextured(PSInputPixelLightingTx pin) : SV_Target0
{
    const float3 V = normalize(EyePosition - pin.PositionWS.xyz); // view vector
    const float3 L = normalize(-LightDirection[0]);               // light vector ("to light" opposite of light's direction)

    // Before lighting, peturb the surface's normal by the one given in normal map.
    float3 localNormal = TwoChannelNormalX2(NormalTexture.Sample(SurfaceSampler, pin.TexCoord).xy);
    float3 N = PeturbNormal(localNormal, pin.PositionWS.xyz, pin.NormalWS, pin.TexCoord);

    // Get albedo
    float4 albedo = AlbedoTexture.Sample(SurfaceSampler, pin.TexCoord);

    // Get roughness, metalness, and ambient occlusion
    float3 RMA = RMATexture.Sample(SurfaceSampler, pin.TexCoord);

    // glTF2 defines metalness as B channel, roughness as G channel, and occlusion as R channel

    // Shade surface
    float3 color = LightSurface(V, N, 3, LightColor, LightDirection, albedo.rgb, RMA.g, RMA.b, RMA.r);

    return float4(color, albedo.w * Alpha);
}


// Pixel shader: pbr (textures) + emissive + image-based lighting
float4 PSTexturedEmissive(PSInputPixelLightingTx pin) : SV_Target0
{
    const float3 V = normalize(EyePosition - pin.PositionWS.xyz); // view vector
    const float3 L = normalize(-LightDirection[0]);               // light vector ("to light" opposite of light's direction)

    // Before lighting, peturb the surface's normal by the one given in normal map.
    float3 localNormal = TwoChannelNormalX2(NormalTexture.Sample(SurfaceSampler, pin.TexCoord).xy);
    float3 N = PeturbNormal(localNormal, pin.PositionWS.xyz, pin.NormalWS, pin.TexCoord);

    // Get albedo
    float4 albedo = AlbedoTexture.Sample(SurfaceSampler, pin.TexCoord);

    // Get roughness, metalness, and ambient occlusion
    float3 RMA = RMATexture.Sample(SurfaceSampler, pin.TexCoord);

    // glTF2 defines metalness as B channel, roughness as G channel, and occlusion as R channel

    // Shade surface
    float3 color = LightSurface(V, N, 3, LightColor, LightDirection, albedo.rgb, RMA.g, RMA.b, RMA.r);

    color += EmissiveTexture.Sample(SurfaceSampler, pin.TexCoord).rgb;

    return float4(color, albedo.w * Alpha);
}


// Pixel shader: pbr (textures) + image-based lighting + velocity
#include "PixelPacking_Velocity.hlsli"

struct PSOut_Velocity
{
    float4 color : SV_Target0;
    packed_velocity_t velocity : SV_Target1;
};

PSOut_Velocity PSTexturedVelocity(VSOut_Velocity pin)
{
    PSOut_Velocity output;

    const float3 V = normalize(EyePosition - pin.current.PositionWS.xyz); // view vector
    const float3 L = normalize(-LightDirection[0]);                       // light vector ("to light" opposite of light's direction)

    // Before lighting, peturb the surface's normal by the one given in normal map.
    float3 localNormal = TwoChannelNormalX2(NormalTexture.Sample(SurfaceSampler, pin.current.TexCoord).xy);
    float3 N = PeturbNormal(localNormal, pin.current.PositionWS.xyz, pin.current.NormalWS, pin.current.TexCoord);

    // Get albedo
    float4 albedo = AlbedoTexture.Sample(SurfaceSampler, pin.current.TexCoord);

    // Get roughness, metalness, and ambient occlusion
    float3 RMA = RMATexture.Sample(SurfaceSampler, pin.current.TexCoord);

    // glTF2 defines metalness as B channel, roughness as G channel, and occlusion as R channel

    // Shade surface
    float3 color = LightSurface(V, N, 3, LightColor, LightDirection, albedo.rgb, RMA.g, RMA.b, RMA.r);

    output.color = float4(color, albedo.w * Alpha);

    // Calculate velocity of this point
    float4 prevPos = pin.prevPosition;
    prevPos.xyz /= prevPos.w;
    prevPos.xy *= float2(0.5f, -0.5f);
    prevPos.xy += 0.5f;
    prevPos.xy *= float2(TargetWidth, TargetHeight);

    output.velocity = PackVelocity(prevPos.xyz - pin.current.PositionPS.xyz);

    return output;
}

PSOut_Velocity PSTexturedEmissiveVelocity(VSOut_Velocity pin)
{
    PSOut_Velocity output;

    const float3 V = normalize(EyePosition - pin.current.PositionWS.xyz); // view vector
    const float3 L = normalize(-LightDirection[0]);                       // light vector ("to light" opposite of light's direction)

    // Before lighting, peturb the surface's normal by the one given in normal map.
    float3 localNormal = TwoChannelNormalX2(NormalTexture.Sample(SurfaceSampler, pin.current.TexCoord).xy);
    float3 N = PeturbNormal(localNormal, pin.current.PositionWS.xyz, pin.current.NormalWS, pin.current.TexCoord);

    // Get albedo
    float4 albedo = AlbedoTexture.Sample(SurfaceSampler, pin.current.TexCoord);

    // Get roughness, metalness, and ambient occlusion
    float3 RMA = RMATexture.Sample(SurfaceSampler, pin.current.TexCoord);

    // glTF2 defines metalness as B channel, roughness as G channel, and occlusion as R channel

    // Shade surface
    float3 color = LightSurface(V, N, 3, LightColor, LightDirection, albedo.rgb, RMA.g, RMA.b, RMA.r);

    color += EmissiveTexture.Sample(SurfaceSampler, pin.current.TexCoord).rgb;

    output.color = float4(color, albedo.w * Alpha);

    // Calculate velocity of this point
    float4 prevPos = pin.prevPosition;
    prevPos.xyz /= prevPos.w;
    prevPos.xy *= float2(0.5f, -0.5f);
    prevPos.xy += 0.5f;
    prevPos.xy *= float2(TargetWidth, TargetHeight);

    output.velocity = PackVelocity(prevPos.xyz - pin.current.PositionPS.xyz);

    return output;
}
