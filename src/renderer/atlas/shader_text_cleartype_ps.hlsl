// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "dwrite.hlsl"

struct Output
{
    float4 color0;
    float4 color1;
};

cbuffer ConstBuffer : register(b0)
{
    float4 positionScale;
    float4 gammaRatios;
    float cleartypeEnhancedContrast;
    float grayscaleEnhancedContrast;
}

Texture2D<float4> glyphAtlas : register(t0);

// clang-format off
Output main(
    float4 position : SV_Position,
    float2 texCoord : TEXCOORD,
    float4 foregroundStraight : COLOR,
    uint shadingType : ShadingType
) : SV_Target
// clang-format on
{
    switch (shadingType)
    {
    case 0:
    {
        float4 glyph = glyphAtlas[texCoord];

        Output output;
        output.color0 = glyph;
        output.color1 = glyph.aaaa;
        return output;
    }
    default:
    {
        float4 glyph = glyphAtlas[texCoord];
        float blendEnhancedContrast = DWrite_ApplyLightOnDarkContrastAdjustment(cleartypeEnhancedContrast, foregroundStraight.rgb);
        float3 contrasted = DWrite_EnhanceContrast3(glyph.rgb, blendEnhancedContrast);
        float3 alphaCorrected = DWrite_ApplyAlphaCorrection3(contrasted, foregroundStraight.rgb, gammaRatios);
        float4 color1 = float4(alphaCorrected * foregroundStraight.a, 1);

        Output output;
        output.color0 = color1 * foregroundStraight;
        output.color1 = color1;
        return output;
    }
    }
}
