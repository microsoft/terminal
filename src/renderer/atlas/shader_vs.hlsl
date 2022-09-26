// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "shader_common.hlsl"

cbuffer ConstBuffer : register(b0)
{
    float4 positionScale;
    float4 gammaRatios;
    float cleartypeEnhancedContrast;
    float grayscaleEnhancedContrast;
}

PSData main(VSData data)
{
    PSData output;
    output.position = float4(data.position.xy * data.rect.zw + data.rect.xy, 0, 1);
    // positionScale is expected to be float4(2.0f / sizeInPixel.x, -2.0f / sizeInPixel.y, 1, 1).
    // Together with the addition below this will transform our "position" from pixel into NCD space.
    output.position = output.position * positionScale + float4(-1.0f, 1.0f, 0, 0);
    output.texcoord = data.position.xy * data.tex.zw + data.tex.xy;
    output.color = data.color;
    output.shadingType = data.shadingType;
    return output;
}
