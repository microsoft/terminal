// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

cbuffer ConstBuffer : register(b0)
{
    float4 positionScale;
    float4 gammaRatios;
    float cleartypeEnhancedContrast;
    float grayscaleEnhancedContrast;
}

void main(
    // clang-format off
    inout float4 position : SV_Position,
    inout float2 texCoord : TEXCOORD,
    inout float4 foregroundStraight : COLOR,
    inout uint shadingType : ShadingType
    // clang-format on
)
{
    // positionScale is expected to be float4(2.0f / sizeInPixel.x, -2.0f / sizeInPixel.y, 1, 1).
    // Together with the addition below this will transform our "position" from pixel into NCD space.
    position = position * positionScale + float4(-1.0f, 1.0f, 0, 0);
}
