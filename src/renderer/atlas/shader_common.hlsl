// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

struct VSData {
    float2 position : SV_Position;
    float4 rect : Rect;
    float4 tex : Tex;
    float4 color : Color;
    uint shadingType : ShadingType;
};

struct PSData {
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD;
    nointerpolation float4 color : Color;
    nointerpolation uint shadingType : ShadingType;
};

float4 premultiplyColor(float4 color)
{
    color.rgb *= color.a;
    return color;
}
