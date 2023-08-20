// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "shader_common.hlsl"

cbuffer ConstBuffer : register(b0)
{
    float2 positionScale;
}

// clang-format off
PSData main(VSData data)
// clang-format on
{
    PSData output;
    output.color = data.color;
    output.shadingType = data.shadingType;
    // positionScale is expected to be float2(2.0f / sizeInPixel.x, -2.0f / sizeInPixel.y). Together with the
    // addition below this will transform our "position" from pixel into normalized device coordinate (NDC) space.
    output.position.xy = (data.position + data.vertex.xy * data.size) * positionScale + float2(-1.0f, 1.0f);
    output.position.zw = float2(0, 1);
    output.texcoord = data.texcoord + data.vertex.xy * data.size;
    return output;
}
