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

VSData main(VSData data)
{
    // positionScale is expected to be float4(2.0f / sizeInPixel.x, -2.0f / sizeInPixel.y, 1, 1).
    // Together with the addition below this will transform our "position" from pixel into NCD space.
    data.position = data.position * positionScale + float4(-1.0f, 1.0f, 0, 0);
    return data;
}
