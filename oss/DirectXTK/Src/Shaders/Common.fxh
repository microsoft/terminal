// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkId=248929
// http://go.microsoft.com/fwlink/?LinkID=615561


float ComputeFogFactor(float4 position)
{
    return saturate(dot(position, FogVector));
}


void ApplyFog(inout float4 color, float fogFactor)
{
    color.rgb = lerp(color.rgb, FogColor * color.a, fogFactor);
}


void AddSpecular(inout float4 color, float3 specular)
{
    color.rgb += specular * color.a;
}


struct CommonVSOutput
{
    float4 Pos_ps;
    float4 Diffuse;
    float3 Specular;
    float  FogFactor;
};


CommonVSOutput ComputeCommonVSOutput(float4 position)
{
    CommonVSOutput vout;

    vout.Pos_ps = mul(position, WorldViewProj);
    vout.Diffuse = DiffuseColor;
    vout.Specular = 0;
    vout.FogFactor = ComputeFogFactor(position);

    return vout;
}


#define SetCommonVSOutputParams \
    vout.PositionPS = cout.Pos_ps; \
    vout.Diffuse = cout.Diffuse; \
    vout.Specular = float4(cout.Specular, cout.FogFactor);


#define SetCommonVSOutputParamsNoFog \
    vout.PositionPS = cout.Pos_ps; \
    vout.Diffuse = cout.Diffuse;
