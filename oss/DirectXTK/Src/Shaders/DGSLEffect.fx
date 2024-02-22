// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

//
// Based on the Visual Studio 3D Starter Kit
//
// http://aka.ms/vs3dkit
//

cbuffer MaterialVars : register (b0)
{
    float4 MaterialAmbient;
    float4 MaterialDiffuse;
    float4 MaterialSpecular;
    float4 MaterialEmissive;
    float MaterialSpecularPower;
};

cbuffer ObjectVars : register(b2)
{
    float4x4 LocalToWorld4x4;
    float4x4 LocalToProjected4x4;
    float4x4 WorldToLocal4x4;
    float4x4 WorldToView4x4;
    float4x4 UVTransform4x4;
    float3 EyePosition;
};

cbuffer BoneVars : register(b4)
{
    float4x3 Bones[72];
};

struct A2V
{
    float4 pos : SV_Position;
    float3 normal : NORMAL0;
    float4 tangent : TANGENT0;
    float2 uv : TEXCOORD0;
};

struct A2V_Vc
{
    float4 pos : SV_Position;
    float3 normal : NORMAL0;
    float4 tangent : TANGENT0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

struct A2V_Weights
{
    float4 pos : SV_Position;
    float3 normal : NORMAL0;
    float4 tangent : TANGENT0;
    float2 uv : TEXCOORD0;
    uint4 boneIndices : BLENDINDICES0;
    float4 blendWeights : BLENDWEIGHT0;
};

struct A2V_WeightsVc
{
    float4 pos : SV_Position;
    float3 normal : NORMAL0;
    float4 tangent : TANGENT0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    uint4 boneIndices : BLENDINDICES0;
    float4 blendWeights : BLENDWEIGHT0;
};

struct V2P
{
    float4 pos : SV_POSITION;
    float4 diffuse : COLOR;
    float2 uv : TEXCOORD0;
    float3 worldNorm : TEXCOORD1;
    float3 worldPos : TEXCOORD2;
    float3 toEye : TEXCOORD3;
    float4 tangent : TEXCOORD4;
    float3 normal : TEXCOORD5;
};


// Skinning helper functions
void Skin(inout A2V_Weights vertex, uniform int boneCount)
{
    float4x3 skinning = 0;

    [unroll]
    for (int i = 0; i < boneCount; i++)
    {
        skinning += Bones[vertex.boneIndices[i]] * vertex.blendWeights[i];
    }

    vertex.pos.xyz = mul(vertex.pos, skinning);
    vertex.normal = mul(vertex.normal, (float3x3)skinning);
    vertex.tangent.xyz = mul((float3)vertex.tangent, (float3x3)skinning);
}

void SkinVc(inout A2V_WeightsVc vertex, uniform int boneCount)
{
    float4x3 skinning = 0;

    [unroll]
    for (int i = 0; i < boneCount; i++)
    {
        skinning += Bones[vertex.boneIndices[i]] * vertex.blendWeights[i];
    }

    vertex.pos.xyz = mul(vertex.pos, skinning);
    vertex.normal = mul(vertex.normal, (float3x3)skinning);
    vertex.tangent.xyz = mul((float3)vertex.tangent, (float3x3)skinning);
}


// Vertex shader: no per-vertex-color, no skinning
V2P main(A2V vertex)
{
    V2P result;

    float3 wp = mul(vertex.pos, LocalToWorld4x4).xyz;

    // set output data
    result.pos = mul(vertex.pos, LocalToProjected4x4);
    result.diffuse = MaterialDiffuse;
    result.uv = mul(float4(vertex.uv.x, vertex.uv.y, 0, 1), UVTransform4x4).xy;
    result.worldNorm = mul(vertex.normal, (float3x3)LocalToWorld4x4);
    result.worldPos = wp;
    result.toEye = EyePosition - wp;
    result.tangent = vertex.tangent;
    result.normal = vertex.normal;

    return result;
}


// Vertex shader: per-vertex-color, no skinning
V2P mainVc(A2V_Vc vertex)
{
    V2P result;

    float3 wp = mul(vertex.pos, LocalToWorld4x4).xyz;

    // set output data
    result.pos = mul(vertex.pos, LocalToProjected4x4);
    result.diffuse = vertex.color * MaterialDiffuse;
    result.uv = mul(float4(vertex.uv.x, vertex.uv.y, 0, 1), UVTransform4x4).xy;
    result.worldNorm = mul(vertex.normal, (float3x3)LocalToWorld4x4);
    result.worldPos = wp;
    result.toEye = EyePosition - wp;
    result.tangent = vertex.tangent;
    result.normal = vertex.normal;

    return result;
}


// Vertex shader: no per-vertex-color, 1-bone skinning
V2P main1Bones(A2V_Weights vertex)
{
    V2P result;

    Skin(vertex, 1);

    float3 wp = mul(vertex.pos, LocalToWorld4x4).xyz;

    // set output data
    result.pos = mul(vertex.pos, LocalToProjected4x4);
    result.diffuse = MaterialDiffuse;
    result.uv = mul(float4(vertex.uv.x, vertex.uv.y, 0, 1), UVTransform4x4).xy;
    result.worldNorm = mul(vertex.normal, (float3x3)LocalToWorld4x4);
    result.worldPos = wp;
    result.toEye = EyePosition - wp;
    result.tangent = vertex.tangent;
    result.normal = vertex.normal;

    return result;
}

// Vertex shader: no per-vertex-color, 2-bone skinning
V2P main2Bones(A2V_Weights vertex)
{
    V2P result;

    Skin(vertex, 2);

    float3 wp = mul(vertex.pos, LocalToWorld4x4).xyz;

    // set output data
    result.pos = mul(vertex.pos, LocalToProjected4x4);
    result.diffuse = MaterialDiffuse;
    result.uv = mul(float4(vertex.uv.x, vertex.uv.y, 0, 1), UVTransform4x4).xy;
    result.worldNorm = mul(vertex.normal, (float3x3)LocalToWorld4x4);
    result.worldPos = wp;
    result.toEye = EyePosition - wp;
    result.tangent = vertex.tangent;
    result.normal = vertex.normal;

    return result;
}

// Vertex shader: no per-vertex-color, 4-bone skinning
V2P main4Bones(A2V_Weights vertex)
{
    V2P result;

    Skin(vertex, 4);

    float3 wp = mul(vertex.pos, LocalToWorld4x4).xyz;

    // set output data
    result.pos = mul(vertex.pos, LocalToProjected4x4);
    result.diffuse = MaterialDiffuse;
    result.uv = mul(float4(vertex.uv.x, vertex.uv.y, 0, 1), UVTransform4x4).xy;
    result.worldNorm = mul(vertex.normal, (float3x3)LocalToWorld4x4);
    result.worldPos = wp;
    result.toEye = EyePosition - wp;
    result.tangent = vertex.tangent;
    result.normal = vertex.normal;

    return result;
}


// Vertex shader: per-vertex-color, 1-bone skinning
V2P main1BonesVc(A2V_WeightsVc vertex)
{
    V2P result;

    SkinVc(vertex, 1);

    float3 wp = mul(vertex.pos, LocalToWorld4x4).xyz;

    // set output data
    result.pos = mul(vertex.pos, LocalToProjected4x4);
    result.diffuse = vertex.color * MaterialDiffuse;
    result.uv = mul(float4(vertex.uv.x, vertex.uv.y, 0, 1), UVTransform4x4).xy;
    result.worldNorm = mul(vertex.normal, (float3x3)LocalToWorld4x4);
    result.worldPos = wp;
    result.toEye = EyePosition - wp;
    result.tangent = vertex.tangent;
    result.normal = vertex.normal;

    return result;
}

// Vertex shader: per-vertex-color, 2-bone skinning
V2P main2BonesVc(A2V_WeightsVc vertex)
{
    V2P result;

    SkinVc(vertex, 2);

    float3 wp = mul(vertex.pos, LocalToWorld4x4).xyz;

    // set output data
    result.pos = mul(vertex.pos, LocalToProjected4x4);
    result.diffuse = vertex.color * MaterialDiffuse;
    result.uv = mul(float4(vertex.uv.x, vertex.uv.y, 0, 1), UVTransform4x4).xy;
    result.worldNorm = mul(vertex.normal, (float3x3)LocalToWorld4x4);
    result.worldPos = wp;
    result.toEye = EyePosition - wp;
    result.tangent = vertex.tangent;
    result.normal = vertex.normal;

    return result;
}

// Vertex shader: per-vertex-color, 4-bone skinning
V2P main4BonesVc(A2V_WeightsVc vertex)
{
    V2P result;

    SkinVc(vertex, 4);

    float3 wp = mul(vertex.pos, LocalToWorld4x4).xyz;

    // set output data
    result.pos = mul(vertex.pos, LocalToProjected4x4);
    result.diffuse = vertex.color * MaterialDiffuse;
    result.uv = mul(float4(vertex.uv.x, vertex.uv.y, 0, 1), UVTransform4x4).xy;
    result.worldNorm = mul(vertex.normal, (float3x3)LocalToWorld4x4);
    result.worldPos = wp;
    result.toEye = EyePosition - wp;
    result.tangent = vertex.tangent;
    result.normal = vertex.normal;

    return result;
}
