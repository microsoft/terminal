// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// clang-format off
#define SHADING_TYPE_TEXT_BACKGROUND    0
#define SHADING_TYPE_TEXT_GRAYSCALE     1
#define SHADING_TYPE_TEXT_CLEARTYPE     2
#define SHADING_TYPE_TEXT_PASSTHROUGH   3
#define SHADING_TYPE_DOTTED_LINE        4
#define SHADING_TYPE_DOTTED_LINE_WIDE   5
// clang-format on

struct VSData
{
    float2 vertex : SV_Position;
    uint shadingType : shadingType;
    int2 position : position;
    uint2 size : size;
    uint2 texcoord : texcoord;
    float4 color : color;
};

struct PSData
{
    float4 position : SV_Position;
    float2 texcoord : texcoord;
    nointerpolation uint shadingType : shadingType;
    nointerpolation float4 color : color;
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
