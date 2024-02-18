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
    float curlyLinePeakHeight;
    float curlyLineWaveFreq;
    float curlyLineCellOffset;
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
        color = glyphAtlas[data.texcoord].a * data.color;
        weights = color.aaaa;
        break;
    }
    case SHADING_TYPE_TEXT_CLEARTYPE:
    {
        weights = float4(glyphAtlas[data.texcoord].rgb * data.color.a, 1);
        color = weights * data.color;
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
        const bool on = frac(data.position.x / (2.0f * underlineWidth * data.renditionScale.x)) < 0.5f;
        color = on * data.color;
        weights = color.aaaa;
        break;
    }
    case SHADING_TYPE_DASHED_LINE:
    {
        const bool on = frac(data.position.x / (backgroundCellSize.x * data.renditionScale.x)) < 0.5f;
        color = on * data.color;
        weights = color.aaaa;
        break;
    }
    case SHADING_TYPE_CURLY_LINE:
    {
        uint cellRow = floor(data.position.y / backgroundCellSize.y);
        // Use the previous cell when drawing 'Double Height' curly line.
        cellRow -= data.renditionScale.y - 1;
        const float cellTop = cellRow * backgroundCellSize.y;
        const float centerY = cellTop + curlyLineCellOffset * data.renditionScale.y;
        const float strokeWidthHalf = underlineWidth * data.renditionScale.y / 2.0f;
        const float amp = curlyLinePeakHeight * data.renditionScale.y;
        const float freq = curlyLineWaveFreq / data.renditionScale.x;

        const float s = sin(data.position.x * freq);
        const float d = abs(centerY - (s * amp) - data.position.y);
        const float a = 1 - saturate(d - strokeWidthHalf);
        color = a * data.color;
        weights = color.aaaa;
        break;
    }
    default:
    {
        color = data.color;
        weights = color.aaaa;
        break;
    }
    }

    Output output;
    output.color = color;
    output.weights = weights;
    return output;
}
