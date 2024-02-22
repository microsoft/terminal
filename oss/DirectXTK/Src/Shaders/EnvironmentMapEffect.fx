// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929


Texture2D<float4>   Texture             : register(t0);
TextureCube<float4> EnvironmentMap      : register(t1);
Texture2D<float4>   SphereMap           : register(t1);
Texture2DArray<float4> DualParabolaMap  : register(t1);

sampler Sampler       : register(s0);
sampler EnvMapSampler : register(s1);


cbuffer Parameters : register(b0)
{
    float3 EnvironmentMapSpecular   : packoffset(c0);
    float  EnvironmentMapAmount     : packoffset(c1.x);
    float  FresnelFactor            : packoffset(c1.y);

    float4 DiffuseColor             : packoffset(c2);
    float3 EmissiveColor            : packoffset(c3);

    float3 LightDirection[3]        : packoffset(c4);
    float3 LightDiffuseColor[3]     : packoffset(c7);

    float3 EyePosition              : packoffset(c10);

    float3 FogColor                 : packoffset(c11);
    float4 FogVector                : packoffset(c12);

    float4x4 World                  : packoffset(c13);
    float3x3 WorldInverseTranspose  : packoffset(c17);
    float4x4 WorldViewProj          : packoffset(c20);
};


// We don't use these parameters, but Lighting.fxh won't compile without them.
#define SpecularPower       0
#define SpecularColor       0
#define LightSpecularColor  float3(0, 0, 0)


#include "Structures.fxh"
#include "Common.fxh"
#include "Lighting.fxh"
#include "Utilities.fxh"


float ComputeFresnelFactor(float3 eyeVector, float3 worldNormal)
{
    float viewAngle = dot(eyeVector, worldNormal);

    return pow(max(1 - abs(viewAngle), 0), FresnelFactor) * EnvironmentMapAmount;
}


VSOutputTxEnvMap ComputeEnvMapVSOutput(VSInputNmTx vin, float3 normal, uniform bool useFresnel, uniform int numLights)
{
    VSOutputTxEnvMap vout;

    float4 pos_ws = mul(vin.Position, World);
    float3 eyeVector = normalize(EyePosition - pos_ws.xyz);
    float3 worldNormal = normalize(mul(normal, WorldInverseTranspose));

    ColorPair lightResult = ComputeLights(eyeVector, worldNormal, numLights);

    vout.PositionPS = mul(vin.Position, WorldViewProj);
    vout.Diffuse = float4(lightResult.Diffuse, DiffuseColor.a);

    if (useFresnel)
        vout.Specular.rgb = ComputeFresnelFactor(eyeVector, worldNormal);
    else
        vout.Specular.rgb = EnvironmentMapAmount;

    vout.Specular.a = ComputeFogFactor(vin.Position);
    vout.TexCoord = vin.TexCoord;
    vout.EnvCoord = reflect(-eyeVector, worldNormal);

    return vout;
}


// Cubic environment mapping
// Greene, "Environment Mapping and Other Applications of World Projections", IEEE Computer Graphics and Applications. 1986.
float4 ComputeEnvMapPSOutput(PSInputPixelLightingTx pin, uniform bool useFresnel)
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;

    float3 eyeVector = normalize(EyePosition - pin.PositionWS.xyz);
    float3 worldNormal = normalize(pin.NormalWS);

    ColorPair lightResult = ComputeLights(eyeVector, worldNormal, 3);

    color.rgb *= lightResult.Diffuse;

    float3 envcoord = reflect(-eyeVector, worldNormal);

    float4 envmap = EnvironmentMap.Sample(EnvMapSampler, envcoord) * color.a;

    float3 amount;
    if (useFresnel)
        amount = ComputeFresnelFactor(eyeVector, worldNormal);
    else
        amount = EnvironmentMapAmount;

    color.rgb = lerp(color.rgb, envmap.rgb, amount.rgb);
    color.rgb += EnvironmentMapSpecular * envmap.a;

    return color;
}


// Spherical environment mapping
// Blinn & Newell, "Texture and Reflection in Computer Generated Images", Communications of the ACM. 1976.
float4 ComputeEnvMapSpherePSOutput(PSInputPixelLightingTx pin, uniform bool useFresnel)
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;

    float3 eyeVector = normalize(EyePosition - pin.PositionWS.xyz);
    float3 worldNormal = normalize(pin.NormalWS);

    ColorPair lightResult = ComputeLights(eyeVector, worldNormal, 3);

    color.rgb *= lightResult.Diffuse;

    float3 r = reflect(-eyeVector, worldNormal);
    float m = 2.0 * sqrt(r.x*r.x + r.y*r.y + (r.z + 1.0)*(r.z + 1.0));
    float2 envcoord = float2(r.x / m + 0.5, r.y / m + 0.5);

    float4 envmap = SphereMap.Sample(EnvMapSampler, envcoord) * color.a;

    float3 amount;
    if (useFresnel)
        amount = ComputeFresnelFactor(eyeVector, worldNormal);
    else
        amount = EnvironmentMapAmount;

    color.rgb = lerp(color.rgb, envmap.rgb, amount.rgb);
    color.rgb += EnvironmentMapSpecular * envmap.a;

    return color;
}


// Dual-parabola environment mapping
// Heidrich & Seidel, "View-independent Environment Maps", Eurographics Workshop on Graphics Hardware, 1998.
float4 ComputeEnvMapDualParabolaPSOutput(PSInputPixelLightingTx pin, uniform bool useFresnel)
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;

    float3 eyeVector = normalize(EyePosition - pin.PositionWS.xyz);
    float3 worldNormal = normalize(pin.NormalWS);

    ColorPair lightResult = ComputeLights(eyeVector, worldNormal, 3);

    color.rgb *= lightResult.Diffuse;

    float3 r = reflect(-eyeVector, worldNormal);
    float m = 2.0 * (1.0 + abs(r.z));
    float3 envcoord = float3(r.x / m + 0.5, r.y / m + 0.5, (r.z > 0) ? 0 : 1);

    float4 envmap = DualParabolaMap.Sample(EnvMapSampler, envcoord) * color.a;

    float3 amount;
    if (useFresnel)
        amount = ComputeFresnelFactor(eyeVector, worldNormal);
    else
        amount = EnvironmentMapAmount;

    color.rgb = lerp(color.rgb, envmap.rgb, amount.rgb);
    color.rgb += EnvironmentMapSpecular * envmap.a;

    return color;
}


// Vertex shader: basic.
VSOutputTxEnvMap VSEnvMap(VSInputNmTx vin)
{
    return ComputeEnvMapVSOutput(vin, vin.Normal, false, 3);
}

VSOutputTxEnvMap VSEnvMapBn(VSInputNmTx vin)
{
    float3 normal = BiasX2(vin.Normal);

    return ComputeEnvMapVSOutput(vin, normal, false, 3);
}


// Vertex shader: fresnel.
VSOutputTxEnvMap VSEnvMapFresnel(VSInputNmTx vin)
{
    return ComputeEnvMapVSOutput(vin, vin.Normal, true, 3);
}

VSOutputTxEnvMap VSEnvMapFresnelBn(VSInputNmTx vin)
{
    float3 normal = BiasX2(vin.Normal);

    return ComputeEnvMapVSOutput(vin, normal, true, 3);
}


// Vertex shader: one light.
VSOutputTxEnvMap VSEnvMapOneLight(VSInputNmTx vin)
{
    return ComputeEnvMapVSOutput(vin, vin.Normal, false, 1);
}

VSOutputTxEnvMap VSEnvMapOneLightBn(VSInputNmTx vin)
{
    float3 normal = BiasX2(vin.Normal);

    return ComputeEnvMapVSOutput(vin, normal, false, 1);
}


// Vertex shader: one light, fresnel.
VSOutputTxEnvMap VSEnvMapOneLightFresnel(VSInputNmTx vin)
{
    return ComputeEnvMapVSOutput(vin, vin.Normal, true, 1);
}

VSOutputTxEnvMap VSEnvMapOneLightFresnelBn(VSInputNmTx vin)
{
    float3 normal = BiasX2(vin.Normal);

    return ComputeEnvMapVSOutput(vin, normal, true, 1);
}


// Vertex shader: pixel lighting.
VSOutputPixelLightingTx VSEnvMapPixelLighting(VSInputNmTx vin)
{
    VSOutputPixelLightingTx vout;

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, vin.Normal);
    SetCommonVSOutputParamsPixelLighting;

    vout.Diffuse = float4(1, 1, 1, DiffuseColor.a);
    vout.TexCoord = vin.TexCoord;

    return vout;
}

VSOutputPixelLightingTx VSEnvMapPixelLightingSM4(VSInputNmTx vin)
{
    return VSEnvMapPixelLighting(vin);
}

VSOutputPixelLightingTx VSEnvMapPixelLightingBn(VSInputNmTx vin)
{
    VSOutputPixelLightingTx vout;

    float3 normal = BiasX2(vin.Normal);

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, normal);
    SetCommonVSOutputParamsPixelLighting;

    vout.Diffuse = float4(1, 1, 1, DiffuseColor.a);
    vout.TexCoord = vin.TexCoord;

    return vout;
}

VSOutputPixelLightingTx VSEnvMapPixelLightingBnSM4(VSInputNmTx vin)
{
    return VSEnvMapPixelLightingBn(vin);
}


// Pixel shader (cube mapping): basic.
float4 PSEnvMap(PSInputTxEnvMap pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;
    float4 envmap = EnvironmentMap.Sample(EnvMapSampler, pin.EnvCoord) * color.a;

    color.rgb = lerp(color.rgb, envmap.rgb, pin.Specular.rgb);

    ApplyFog(color, pin.Specular.w);

    return color;
}


// Pixel shader (cube mapping): no fog.
float4 PSEnvMapNoFog(PSInputTxEnvMap pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;
    float4 envmap = EnvironmentMap.Sample(EnvMapSampler, pin.EnvCoord) * color.a;

    color.rgb = lerp(color.rgb, envmap.rgb, pin.Specular.rgb);

    return color;
}


// Pixel shader (cube mapping): specular.
float4 PSEnvMapSpecular(PSInputTxEnvMap pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;
    float4 envmap = EnvironmentMap.Sample(EnvMapSampler, pin.EnvCoord) * color.a;

    color.rgb = lerp(color.rgb, envmap.rgb, pin.Specular.rgb);
    color.rgb += EnvironmentMapSpecular * envmap.a;

    ApplyFog(color, pin.Specular.w);

    return color;
}


// Pixel shader (cube mapping): specular, no fog.
float4 PSEnvMapSpecularNoFog(PSInputTxEnvMap pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;
    float4 envmap = EnvironmentMap.Sample(EnvMapSampler, pin.EnvCoord) * color.a;

    color.rgb = lerp(color.rgb, envmap.rgb, pin.Specular.rgb);
    color.rgb += EnvironmentMapSpecular * envmap.a;

    return color;
}


// Pixel shader (cube mapping): pixel lighting.
float4 PSEnvMapPixelLighting(PSInputPixelLightingTx pin) : SV_Target0
{
    float4 color = ComputeEnvMapPSOutput(pin, false);

    ApplyFog(color, pin.PositionWS.w);

    return color;
}


// Pixel shader (cube mapping): pixel lighting + no fog.
float4 PSEnvMapPixelLightingNoFog(PSInputPixelLightingTx pin) : SV_Target0
{
    float4 color = ComputeEnvMapPSOutput(pin, false);

    return color;
}


// Pixel shader (cube mapping): pixel lighting + fresnel
float4 PSEnvMapPixelLightingFresnel(PSInputPixelLightingTx pin) : SV_Target0
{
    float4 color = ComputeEnvMapPSOutput(pin, true);

    ApplyFog(color, pin.PositionWS.w);

    return color;
}


// Pixel shader (cube mapping): pixel lighting + fresnel + no fog.
float4 PSEnvMapPixelLightingFresnelNoFog(PSInputPixelLightingTx pin) : SV_Target0
{
    float4 color = ComputeEnvMapPSOutput(pin, true);

    return color;
}


// Pixel shader (sphere mapping): pixel lighting.
float4 PSEnvMapSpherePixelLighting(PSInputPixelLightingTx pin) : SV_Target0
{
    float4 color = ComputeEnvMapSpherePSOutput(pin, false);

    ApplyFog(color, pin.PositionWS.w);

    return color;
}


// Pixel shader (sphere mapping): pixel lighting + no fog.
float4 PSEnvMapSpherePixelLightingNoFog(PSInputPixelLightingTx pin) : SV_Target0
{
    float4 color = ComputeEnvMapSpherePSOutput(pin, false);

    return color;
}


// Pixel shader (sphere mapping): pixel lighting + fresnel
float4 PSEnvMapSpherePixelLightingFresnel(PSInputPixelLightingTx pin) : SV_Target0
{
    float4 color = ComputeEnvMapSpherePSOutput(pin, true);

    ApplyFog(color, pin.PositionWS.w);

    return color;
}


// Pixel shader (sphere mapping): pixel lighting + fresnel + no fog.
float4 PSEnvMapSpherePixelLightingFresnelNoFog(PSInputPixelLightingTx pin) : SV_Target0
{
    float4 color = ComputeEnvMapSpherePSOutput(pin, true);

    return color;
}


// Pixel shader (dual parabola mapping): pixel lighting.
float4 PSEnvMapDualParabolaPixelLighting(PSInputPixelLightingTx pin) : SV_Target0
{
    float4 color = ComputeEnvMapDualParabolaPSOutput(pin, false);

    ApplyFog(color, pin.PositionWS.w);

    return color;
}


// Pixel shader (dual parabola mapping): pixel lighting + no fog.
float4 PSEnvMapDualParabolaPixelLightingNoFog(PSInputPixelLightingTx pin) : SV_Target0
{
    float4 color = ComputeEnvMapDualParabolaPSOutput(pin, false);

    return color;
}


// Pixel shader (dual parabola mapping): pixel lighting + fresnel
float4 PSEnvMapDualParabolaPixelLightingFresnel(PSInputPixelLightingTx pin) : SV_Target0
{
    float4 color = ComputeEnvMapDualParabolaPSOutput(pin, true);

    ApplyFog(color, pin.PositionWS.w);

    return color;
}


// Pixel shader (dual parabola mapping): pixel lighting + fresnel + no fog.
float4 PSEnvMapDualParabolaPixelLightingFresnelNoFog(PSInputPixelLightingTx pin) : SV_Target0
{
    float4 color = ComputeEnvMapDualParabolaPSOutput(pin, true);

    return color;
}
