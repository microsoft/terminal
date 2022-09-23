// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// clang-format off
float4 main(
    float4 position : SV_Position,
    float2 texCoord : TEXCOORD,
    float4 color : COLOR
) : SV_Target
// clang-format on
{
    return float4(1, 1, 1, 1);
}
