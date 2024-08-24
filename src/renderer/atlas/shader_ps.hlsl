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
    float shadedGlyphDotSize;
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
        float2 cell = data.position.xy / backgroundCellSize;
        color = all(cell < backgroundCellCount) ? background[cell] : backgroundColor;
        weights = float4(1, 1, 1, 1);
        break;
    }
    case SHADING_TYPE_TEXT_GRAYSCALE:
    {
        // These are independent of the glyph texture and could be moved to the vertex shader or CPU side of things.
        float4 foreground = premultiplyColor(data.color);
        float blendEnhancedContrast = DWrite_ApplyLightOnDarkContrastAdjustment(enhancedContrast, data.color.rgb);
        float intensity = DWrite_CalcColorIntensity(data.color.rgb);
        // These aren't.
        float4 glyph = glyphAtlas[data.texcoord];
        float contrasted = DWrite_EnhanceContrast(glyph.a, blendEnhancedContrast);
        float alphaCorrected = DWrite_ApplyAlphaCorrection(contrasted, intensity, gammaRatios);
        color = alphaCorrected * foreground;
        weights = color.aaaa;
        break;
    }
    case SHADING_TYPE_TEXT_CLEARTYPE:
    {
        // These are independent of the glyph texture and could be moved to the vertex shader or CPU side of things.
        float blendEnhancedContrast = DWrite_ApplyLightOnDarkContrastAdjustment(enhancedContrast, data.color.rgb);
        // These aren't.
        float4 glyph = glyphAtlas[data.texcoord];
        float3 contrasted = DWrite_EnhanceContrast3(glyph.rgb, blendEnhancedContrast);
        float3 alphaCorrected = DWrite_ApplyAlphaCorrection3(contrasted, data.color.rgb, gammaRatios);
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
        float2 pos = floor(data.position.xy / (shadedGlyphDotSize * data.renditionScale));

        // A series of on/off/on/off/on/off pixels can be generated with:
        //   step(frac(x * 0.5f), 0)
        // The inner frac(x * 0.5f) will generate a series of
        //   0, 0.5, 0, 0.5, 0, 0.5
        // and the step() will transform that to
        //   1,   0, 1,   0, 1,   0
        //
        // We can now turn that into a checkerboard pattern quite easily,
        // if we imagine the fields of the checkerboard like this:
        //   +---+---+---+
        //   | 0 | 1 | 2 |
        //   +---+---+---+
        //   | 1 | 2 | 3 |
        //   +---+---+---+
        //   | 2 | 3 | 4 |
        //   +---+---+---+
        //
        // Because this means we just need to set
        //   x = pos.x + pos.y
        // and so we end up with
        //   step(frac(dot(pos, 0.5f)), 0)
        //
        // Finally, we need to implement the "stretch" explained above, which can
        // be easily achieved by simply replacing the factor 0.5 with 0.25 like so
        //   step(frac(x * 0.25f), 0)
        // as this gets us
        //   0, 0.25, 0.5, 0.75, 0, 0.25, 0.5, 0.75
        // = 1,    0,   0,    0, 1,    0,   0,    0
        //
        // Of course we only want to apply that to the X axis, which means
        // below we end up having 2 different multipliers for the dot().
        float stretched = step(frac(dot(pos, float2(glyph.r * -0.25f + 0.5f, 0.5f))), 0) * glyph.a;
        // Thankfully the remaining 2 operations are a lot simpler.
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
        bool on = frac(data.position.x / (3.0f * underlineWidth * data.renditionScale.x)) < (1.0f / 3.0f);
        color = on * premultiplyColor(data.color);
        weights = color.aaaa;
        break;
    }
    case SHADING_TYPE_DASHED_LINE:
    {
        bool on = frac(data.position.x / (6.0f * underlineWidth * data.renditionScale.x)) < (4.0f / 6.0f);
        color = on * premultiplyColor(data.color);
        weights = color.aaaa;
        break;
    }
    case SHADING_TYPE_CURLY_LINE:
    {
        // The curly line has the same thickness as a double underline.
        // We halve it to make the math a bit easier.
        float strokeWidthHalf = doubleUnderlineWidth * data.renditionScale.y * 0.5f;
        float center = curlyLineHalfHeight * data.renditionScale.y;
        float amplitude = center - strokeWidthHalf;
        // We multiply the frequency by pi/2 to get a sine wave which has an integer period.
        // This makes every period of the wave look exactly the same.
        float frequency = 1.57079632679489661923f / (curlyLineHalfHeight * data.renditionScale.x);
        // At very small sizes, like when the wave is just 3px tall and 1px wide, it'll look too fat and/or blurry.
        // Because we multiplied our frequency with pi, the extrema of the curve and its intersections with the
        // centerline always occur right between two pixels. This causes both to be lit with the same color.
        // By adding a small phase shift, we can break this symmetry up. It'll make the wave look a lot more crispy.
        float phase = 1.57079632679489661923f;
        float sine = sin(data.position.x * frequency + phase);
        // We use the distance to the sine curve as its alpha value - the closer the more opaque.
        // To give it a smooth appearance we don't want to simply calculate the vertical distance to the curve:
        //   abs(pixel.y - sin(pixel.x))
        //
        // ...because while a pixel may be vertically far away it may be horizontally close to the sine curve.
        // We need a proper distance calculation. This makes a large difference at especially small font sizes.
        //
        // While calculating the distance to a sine curve is complex, calculating the distance to its tangent is easy,
        // because tangents are straight lines and line-point distance are trivial. The tangent of sin(x) is cos(x).
        // The line-point distance is the vertical distance multiplied by the cos(angle) of the line.
        // To turn out tangent cos(x) into an angle we need to calculate atan(cos(x)). This nets us:
        //   abs(pixel.y - sin(pixel.x)) * cos(atan(cos(pixel.x))
        //
        // The expanded sine form of cos(atan(cos(x))) is 1 / sqrt(2 - sin(x)^2), which results in:
        //   abs(pixel.y - sin(pixel.x)) * rsqrt(2 - sin(pixel.x)^2)
        float distance = abs(center - data.texcoord.y - sine * amplitude) * rsqrt(2 - sine * sine);
        // Since pixel coordinates are always offset by half a pixel (i.e. data.texcoord is 1.5f, 2.5f, 3.5f, ...)
        // the distance is also off by half a pixel. We undo that by adding half a pixel to the distance.
        // This gives the line its proper thickness appearance.
        float a = 1 - saturate(distance - strokeWidthHalf + 0.5f);
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
