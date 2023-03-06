// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// clang-format off
#define SHADING_TYPE_TEXT_BACKGROUND    0
#define SHADING_TYPE_TEXT_GRAYSCALE     1
#define SHADING_TYPE_TEXT_CLEARTYPE     2
#define SHADING_TYPE_PASSTHROUGH        3
#define SHADING_TYPE_DASHED_LINE        4
#define SHADING_TYPE_SOLID_FILL         5
// clang-format on

struct VSData
{
    int4 position : POSITION;
    int4 texcoord : TEXCOORD;
    uint color : COLOR;
    uint shadingType : ShadingType;
    // Structured Buffers are tightly packed. Nvidia recommends padding them to avoid crossing 128-bit
    // cache lines: https://developer.nvidia.com/content/understanding-structured-buffer-performance
    uint2 padding;
};

struct PSData
{
    nointerpolation uint shadingType : ShadingType;
    nointerpolation float4 color : COLOR;
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD;
};

float4 premultiplyColor(float4 color)
{
    color.rgb *= color.a;
    return color;
}

float4 alphaBlendPremultiplied(float4 bottom, float4 top)
{
    bottom *= 1 - top.a;
    return bottom + top;
}

float4 decodeRGBA(uint i)
{
    return (i >> uint4(0, 8, 16, 24) & 0xff) / 255.0f;
}
