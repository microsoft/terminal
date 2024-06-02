// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// Depends on the background texture
#define SHADING_TYPE_TEXT_BACKGROUND    0

// Depends on the glyphAtlas texture
#define SHADING_TYPE_TEXT_GRAYSCALE     1
#define SHADING_TYPE_TEXT_CLEARTYPE     2
#define SHADING_TYPE_TEXT_BUILTIN_GLYPH 3
#define SHADING_TYPE_TEXT_PASSTHROUGH   4

// Independent of any textures
#define SHADING_TYPE_DOTTED_LINE        5
#define SHADING_TYPE_DASHED_LINE        6
#define SHADING_TYPE_CURLY_LINE         7
#define SHADING_TYPE_SOLID_LINE         8
#define SHADING_TYPE_CURSOR             9
#define SHADING_TYPE_SELECTION          10

struct VSData
{
    float2 vertex : SV_Position;
    uint shadingType : shadingType;
    uint2 renditionScale : renditionScale;
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
    nointerpolation float2 renditionScale : renditionScale;
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
