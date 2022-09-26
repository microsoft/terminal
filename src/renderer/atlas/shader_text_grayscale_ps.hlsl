// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "dwrite.hlsl"
#include "shader_common.hlsl"

cbuffer ConstBuffer : register(b0)
{
    float4 positionScale;
    float4 gammaRatios;
    float cleartypeEnhancedContrast;
    float grayscaleEnhancedContrast;
}

Texture2D<float4> glyphAtlas : register(t0);

// clang-format off
float4 main(PSData data) : SV_Target
// clang-format on
{
    switch (data.shadingType)
    {
    case 0:
    {
        float4 foreground = premultiplyColor(data.color);
        float4 glyphColor = glyphAtlas[data.texcoord];
        float blendEnhancedContrast = DWrite_ApplyLightOnDarkContrastAdjustment(grayscaleEnhancedContrast, data.color.rgb);
        float intensity = DWrite_CalcColorIntensity(data.color.rgb);
        float contrasted = DWrite_EnhanceContrast(glyphColor.a, blendEnhancedContrast);
        float alphaCorrected = DWrite_ApplyAlphaCorrection(contrasted, intensity, gammaRatios);
        return alphaCorrected * foreground;
    }
    case 1:
        return glyphAtlas[data.texcoord];
    default:
        return premultiplyColor(data.color);
    }
}
