// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "shader_common.hlsl"

cbuffer ConstBuffer : register(b0)
{
    float2 positionScale;
}

StructuredBuffer<VSData> instances : register(t0);

// clang-format off
PSData main(uint id: SV_VertexID)
// clang-format on
{
    VSData data = instances[id / 4];

    PSData output;
    output.shadingType = data.shadingType;
    output.color = decodeRGBA(data.color);
    output.position.x = (id & 1) ? data.position.z : data.position.x;
    output.position.y = (id & 2) ? data.position.w : data.position.y;
    // positionScale is expected to be float2(2.0f / sizeInPixel.x, -2.0f / sizeInPixel.y). Together with the
    // addition below this will transform our "position" from pixel into normalized device coordinate (NDC) space.
    output.position.xy = output.position.xy * positionScale + float2(-1.0f, 1.0f);
    output.position.zw = float2(0, 1);
    output.texcoord.x = (id & 1) ? data.texcoord.z : data.texcoord.x;
    output.texcoord.y = (id & 2) ? data.texcoord.w : data.texcoord.y;
    return output;
}
