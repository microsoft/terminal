// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkId=248929
// http://go.microsoft.com/fwlink/?LinkID=615561


// Vertex shader input structures.

struct VSInput
{
    float4 Position : SV_Position;
};

struct VSInputVc
{
    float4 Position : SV_Position;
    float4 Color    : COLOR;
};

struct VSInputTx
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

struct VSInputTxVc
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
    float4 Color    : COLOR;
};

struct VSInputNm
{
    float4 Position : SV_Position;
    float3 Normal   : NORMAL;
};

struct VSInputNmVc
{
    float4 Position : SV_Position;
    float3 Normal   : NORMAL;
    float4 Color    : COLOR;
};

struct VSInputNmTx
{
    float4 Position : SV_Position;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD0;
};

struct VSInputNmTxVc
{
    float4 Position : SV_Position;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float4 Color    : COLOR;
};

struct VSInputNmTxInst
{
    float4 Position    : SV_Position;
    float3 Normal      : NORMAL;
    float2 TexCoord    : TEXCOORD0;
    float4x3 Transform : InstMatrix;
};

struct VSInputNmTxVcInst
{
    float4 Position    : SV_Position;
    float3 Normal      : NORMAL;
    float2 TexCoord    : TEXCOORD0;
    float4 Color       : COLOR;
    float4x3 Transform : InstMatrix;
};

struct VSInputTx2
{
    float4 Position  : SV_Position;
    float2 TexCoord  : TEXCOORD0;
    float2 TexCoord2 : TEXCOORD1;
};

struct VSInputTx2Vc
{
    float4 Position  : SV_Position;
    float2 TexCoord  : TEXCOORD0;
    float2 TexCoord2 : TEXCOORD1;
    float4 Color     : COLOR;
};

struct VSInputNmTxWeights
{
    float4 Position : SV_Position;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD0;
    uint4  Indices  : BLENDINDICES0;
    float4 Weights  : BLENDWEIGHT0;
};



// Vertex shader output structures.

struct VSOutput
{
    float4 Diffuse    : COLOR0;
    float4 Specular   : COLOR1;
    float4 PositionPS : SV_Position;
};

struct VSOutputNoFog
{
    float4 Diffuse    : COLOR0;
    float4 PositionPS : SV_Position;
};

struct VSOutputTx
{
    float4 Diffuse    : COLOR0;
    float4 Specular   : COLOR1;
    float2 TexCoord   : TEXCOORD0;
    float4 PositionPS : SV_Position;
};

struct VSOutputTxNoFog
{
    float4 Diffuse    : COLOR0;
    float2 TexCoord   : TEXCOORD0;
    float4 PositionPS : SV_Position;
};

struct VSOutputPixelLighting
{
    float4 PositionWS : TEXCOORD0;
    float3 NormalWS   : TEXCOORD1;
    float4 Diffuse    : COLOR0;
    float4 PositionPS : SV_Position;
};

struct VSOutputPixelLightingTx
{
    float2 TexCoord   : TEXCOORD0;
    float4 PositionWS : TEXCOORD1;
    float3 NormalWS   : TEXCOORD2;
    float4 Diffuse    : COLOR0;
    float4 PositionPS : SV_Position;
};

struct VSOutputTx2
{
    float4 Diffuse    : COLOR0;
    float4 Specular   : COLOR1;
    float2 TexCoord   : TEXCOORD0;
    float2 TexCoord2  : TEXCOORD1;
    float4 PositionPS : SV_Position;
};

struct VSOutputTx2NoFog
{
    float4 Diffuse    : COLOR0;
    float2 TexCoord   : TEXCOORD0;
    float2 TexCoord2  : TEXCOORD1;
    float4 PositionPS : SV_Position;
};

struct VSOutputTxEnvMap
{
    float4 Diffuse    : COLOR0;
    float4 Specular   : COLOR1;
    float2 TexCoord   : TEXCOORD0;
    float3 EnvCoord   : TEXCOORD1;
    float4 PositionPS : SV_Position;
};



// Pixel shader input structures.

struct PSInput
{
    float4 Diffuse  : COLOR0;
    float4 Specular : COLOR1;
};

struct PSInputNoFog
{
    float4 Diffuse : COLOR0;
};

struct PSInputTx
{
    float4 Diffuse  : COLOR0;
    float4 Specular : COLOR1;
    float2 TexCoord : TEXCOORD0;
};

struct PSInputTxNoFog
{
    float4 Diffuse  : COLOR0;
    float2 TexCoord : TEXCOORD0;
};

struct PSInputPixelLighting
{
    float4 PositionWS : TEXCOORD0;
    float3 NormalWS   : TEXCOORD1;
    float4 Diffuse    : COLOR0;
};

struct PSInputPixelLightingTx
{
    float2 TexCoord   : TEXCOORD0;
    float4 PositionWS : TEXCOORD1;
    float3 NormalWS   : TEXCOORD2;
    float4 Diffuse    : COLOR0;
};

struct PSInputTx2
{
    float4 Diffuse   : COLOR0;
    float4 Specular  : COLOR1;
    float2 TexCoord  : TEXCOORD0;
    float2 TexCoord2 : TEXCOORD1;
};

struct PSInputTx2NoFog
{
    float4 Diffuse   : COLOR0;
    float2 TexCoord  : TEXCOORD0;
    float2 TexCoord2 : TEXCOORD1;
};

struct PSInputTxEnvMap
{
    float4 Diffuse  : COLOR0;
    float4 Specular : COLOR1;
    float2 TexCoord : TEXCOORD0;
    float3 EnvCoord : TEXCOORD1;
};
