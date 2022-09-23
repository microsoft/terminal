// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "dwrite.hlsl"

cbuffer ConstBuffer : register(b0)
{
    float4 positionScale;
    float4 gammaRatios;
    float cleartypeEnhancedContrast;
    float grayscaleEnhancedContrast;
}

Texture2D<float4> glyphAtlas : register(t0);

// clang-format off
float4 main(
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
        return glyphAtlas[texCoord];
    }
    default:
    {
        float4 foregroundColor = float4(foregroundStraight.rgb * foregroundStraight.a, foregroundStraight.a);
        float4 glyphColor = glyphAtlas[texCoord];
        float blendEnhancedContrast = DWrite_ApplyLightOnDarkContrastAdjustment(grayscaleEnhancedContrast, foregroundStraight.rgb);
        float intensity = DWrite_CalcColorIntensity(foregroundStraight.rgb);
        float contrasted = DWrite_EnhanceContrast(glyphColor.a, blendEnhancedContrast);
        float alphaCorrected = DWrite_ApplyAlphaCorrection(contrasted, intensity, gammaRatios);
        return alphaCorrected * foregroundColor;
    }
    }
}
