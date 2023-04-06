// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "dwrite.hlsl"
#include "shader_common.hlsl"

cbuffer ConstBuffer : register(b0)
{
    float4 backgroundColor;
    float2 cellSize;
    float2 cellCount;
    float4 gammaRatios;
    float enhancedContrast;
    float dashedLineLength;
}

Texture2D<float4> background : register(t0);
Texture2D<float4> glyphAtlas : register(t1);

struct Output
{
    float4 color;
    float4 weights;
};

// clang-format off
Output main(PSData data) : SV_Target
// clang-format on
{
    float4 color;
    float4 weights;

    switch (data.shadingType & ~SHADING_TYPE_LIGATURE_MARKER)
    {
    case SHADING_TYPE_TEXT_BACKGROUND:
    {
        const float2 cell = data.position.xy / cellSize;
        color = all(cell < cellCount) ? background[cell] : backgroundColor;
        weights = float4(1, 1, 1, 1);
        break;
    }
    case SHADING_TYPE_TEXT_GRAYSCALE:
    {
        // These are independent of the glyph texture and could be moved to the vertex shader or CPU side of things. Part 1...
        float4 foreground = premultiplyColor(data.color);

        if (data.shadingType & SHADING_TYPE_LIGATURE_MARKER)
        {
            float2 cell = data.position.xy / cellSize;
            cell.y += cellCount.y;
            foreground = background[cell];

            if (foreground.a == 0)
            {
                discard;
            }

            data.color = float4(foreground.rgb / foreground.a, foreground.a);
        }

        // ...Part 2:
        const float blendEnhancedContrast = DWrite_ApplyLightOnDarkContrastAdjustment(enhancedContrast, data.color.rgb);
        const float intensity = DWrite_CalcColorIntensity(data.color.rgb);
        // These aren't.
        float4 glyph = glyphAtlas[data.texcoord];
        const float contrasted = DWrite_EnhanceContrast(glyph.a, blendEnhancedContrast);
        const float alphaCorrected = DWrite_ApplyAlphaCorrection(contrasted, intensity, gammaRatios);
        color = alphaCorrected * foreground;
        weights = color.aaaa;
        break;
    }
    case SHADING_TYPE_TEXT_CLEARTYPE:
    {
        if (data.shadingType & SHADING_TYPE_LIGATURE_MARKER)
        {
            float2 cell = data.position.xy / cellSize;
            cell.y += cellCount.y;
            data.color = background[cell];

            if (data.color.a == 0)
            {
                discard;
            }

            data.color.rgb /= data.color.a;
        }

        // These are independent of the glyph texture and could be moved to the vertex shader or CPU side of things.
        const float blendEnhancedContrast = DWrite_ApplyLightOnDarkContrastAdjustment(enhancedContrast, data.color.rgb);
        // These aren't.
        float4 glyph = glyphAtlas[data.texcoord];
        const float3 contrasted = DWrite_EnhanceContrast3(glyph.rgb, blendEnhancedContrast);
        const float3 alphaCorrected = DWrite_ApplyAlphaCorrection3(contrasted, data.color.rgb, gammaRatios);
        weights = float4(alphaCorrected * data.color.a, 1);
        color = weights * data.color;
        break;
    }
    case SHADING_TYPE_PASSTHROUGH:
    {
        color = glyphAtlas[data.texcoord];
        weights = color.aaaa;
        break;
    }
    case SHADING_TYPE_DASHED_LINE:
    {
        const bool on = frac(data.position.x / dashedLineLength) < 0.333333333f;
        color = on * premultiplyColor(data.color);
        weights = color.aaaa;
        break;
    }
    case SHADING_TYPE_SOLID_FILL:
    default:
    {
        color = premultiplyColor(data.color);
        weights = color.aaaa;
        break;
    }
    }

    Output output;
    output.color = color;
    output.weights = weights;
    return output;
}
