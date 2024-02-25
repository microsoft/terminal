// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "dwrite.hlsl"
#include "shader_common.hlsl"

cbuffer ConstBuffer : register(b0)
{
    float4 backgroundColor;
    float2 backgroundCellSize;
    float2 backgroundCellCount;
    float4 gammaRatios;
    float enhancedContrast;
    float underlineWidth;
    float doubleUnderlineWidth;
    float curlyLineHalfHeight;
    float thinLineWidth;
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
    
    switch (data.shadingType)
    {
    case SHADING_TYPE_TEXT_BACKGROUND:
    {
        const float2 cell = data.position.xy / backgroundCellSize;
        color = all(cell < backgroundCellCount) ? background[cell] : backgroundColor;
        weights = float4(1, 1, 1, 1);
        break;
    }
    case SHADING_TYPE_TEXT_GRAYSCALE:
    {
        // These are independent of the glyph texture and could be moved to the vertex shader or CPU side of things.
        const float4 foreground = premultiplyColor(data.color);
        const float blendEnhancedContrast = DWrite_ApplyLightOnDarkContrastAdjustment(enhancedContrast, data.color.rgb);
        const float intensity = DWrite_CalcColorIntensity(data.color.rgb);
        // These aren't.
        const float4 glyph = glyphAtlas[data.texcoord];
        const float contrasted = DWrite_EnhanceContrast(glyph.a, blendEnhancedContrast);
        const float alphaCorrected = DWrite_ApplyAlphaCorrection(contrasted, intensity, gammaRatios);
        color = alphaCorrected * foreground;
        weights = color.aaaa;
        break;
    }
    case SHADING_TYPE_TEXT_CLEARTYPE:
    {
        // These are independent of the glyph texture and could be moved to the vertex shader or CPU side of things.
        const float blendEnhancedContrast = DWrite_ApplyLightOnDarkContrastAdjustment(enhancedContrast, data.color.rgb);
        // These aren't.
        const float4 glyph = glyphAtlas[data.texcoord];
        const float3 contrasted = DWrite_EnhanceContrast3(glyph.rgb, blendEnhancedContrast);
        const float3 alphaCorrected = DWrite_ApplyAlphaCorrection3(contrasted, data.color.rgb, gammaRatios);
        weights = float4(alphaCorrected * data.color.a, 1);
        color = weights * data.color;
        break;
    }
    case SHADING_TYPE_TEXT_BUILTIN_GLYPH:
    {
        // The RGB components of builtin glyphs are used to control the generation of pixel patterns in this shader.
        // Below you can see their intended effects where # indicates lit pixels.
        //
        // .r = stretch
        //      0: #_#_#_#_
        //         _#_#_#_#
        //         #_#_#_#_
        //         _#_#_#_#
        //
        //      1: #___#___
        //         __#___#_
        //         #___#___
        //         __#___#_
        //
        // .g = invert
        //      0: #_#_#_#_
        //         _#_#_#_#
        //         #_#_#_#_
        //         _#_#_#_#
        //
        //      1: _#_#_#_#
        //         #_#_#_#_
        //         _#_#_#_#
        //         #_#_#_#_
        //
        // .r = fill
        //      0: #_#_#_#_
        //         _#_#_#_#
        //         #_#_#_#_
        //         _#_#_#_#
        //
        //      1: ########
        //         ########
        //         ########
        //         ########
        //      
        float4 glyph = glyphAtlas[data.texcoord];

        float2 pos = floor(data.position.xy / (thinLineWidth * data.renditionScale));
        float stretched = step(frac(dot(pos, float2(glyph.r * -0.25f + 0.5f, 0.5f))), 0) * glyph.a;
        float inverted = abs(glyph.g - stretched);
        float filled = max(glyph.b, inverted);

        color = premultiplyColor(data.color) * filled;
        weights = color.aaaa;
        break;
    }
    case SHADING_TYPE_TEXT_PASSTHROUGH:
    {
        color = glyphAtlas[data.texcoord];
        weights = color.aaaa;
        break;
    }
    case SHADING_TYPE_DOTTED_LINE:
    {
        const bool on = frac(data.position.x / (3.0f * underlineWidth * data.renditionScale.x)) < (1.0f / 3.0f);
        color = on * premultiplyColor(data.color);
        weights = color.aaaa;
        break;
    }
    case SHADING_TYPE_DASHED_LINE:
    {
        const bool on = frac(data.position.x / (6.0f * underlineWidth * data.renditionScale.x)) < (4.0f / 6.0f);
        color = on * premultiplyColor(data.color);
        weights = color.aaaa;
        break;
    }
    case SHADING_TYPE_CURLY_LINE:
    {
        const float strokeWidthHalf = doubleUnderlineWidth * data.renditionScale.y * 0.5f;
        const float amp = (curlyLineHalfHeight - strokeWidthHalf) * data.renditionScale.y;
        const float freq = data.renditionScale.x / curlyLineHalfHeight * 1.57079632679489661923f;
        const float s = sin(data.position.x * freq) * amp;
        const float d = abs(curlyLineHalfHeight - data.texcoord.y - s);
        const float a = 1 - saturate(d - strokeWidthHalf);
        color = a * premultiplyColor(data.color);
        weights = color.aaaa;
        break;
    }
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
