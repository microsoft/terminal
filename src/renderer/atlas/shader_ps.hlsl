// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "dwrite.hlsl"

#define INVALID_COLOR 0xffffffff

// These flags are shared with AtlasEngine::CellFlags.
//
// clang-format off
#define CellFlags_None            0x00000000
#define CellFlags_Inlined         0x00000001

#define CellFlags_ColoredGlyph    0x00000002

#define CellFlags_Cursor          0x00000008
#define CellFlags_Selected        0x00000010

#define CellFlags_BorderLeft      0x00000020
#define CellFlags_BorderTop       0x00000040
#define CellFlags_BorderRight     0x00000080
#define CellFlags_BorderBottom    0x00000100
#define CellFlags_Underline       0x00000200
#define CellFlags_UnderlineDotted 0x00000400
#define CellFlags_UnderlineDouble 0x00000800
#define CellFlags_Strikethrough   0x00001000
// clang-format on

// According to Nvidia's "Understanding Structured Buffer Performance" guide
// one should aim for structures with sizes divisible by 128 bits (16 bytes).
// This prevents elements from spanning cache lines.
struct Cell
{
    uint glyphPos;
    uint flags;
    uint2 color; // x: foreground, y: background
};

cbuffer ConstBuffer : register(b0)
{
    float4 viewport;
    float4 gammaRatios;
    float enhancedContrast;
    uint cellCountX;
    uint2 cellSize;
    uint underlinePos;
    uint underlineWidth;
    uint strikethroughPos;
    uint strikethroughWidth;
    uint2 doubleUnderlinePos;
    uint thinLineWidth;
    uint backgroundColor;
    uint cursorColor;
    uint selectionColor;
    uint useClearType;
};
StructuredBuffer<Cell> cells : register(t0);
Texture2D<float4> glyphs : register(t1);

float4 decodeRGBA(uint i)
{
    float4 c = (i >> uint4(0, 8, 16, 24) & 0xff) / 255.0f;
    // Convert to premultiplied alpha for simpler alpha blending.
    c.rgb *= c.a;
    return c;
}

uint2 decodeU16x2(uint i)
{
    return uint2(i & 0xffff, i >> 16);
}

float4 alphaBlendPremultiplied(float4 bottom, float4 top)
{
    bottom *= 1 - top.a;
    return bottom + top;
}

// clang-format off
float4 main(float4 pos: SV_Position): SV_Target
// clang-format on
{
    // We need to fill the entire render target with pixels, but only our "viewport"
    // has cells we want to draw. The rest gets treated with the background color.
    [branch] if (any(pos.xy < viewport.xy || pos.xy >= viewport.zw))
    {
        return decodeRGBA(backgroundColor);
    }

    uint2 viewportPos = pos.xy - viewport.xy;
    uint2 cellIndex = viewportPos / cellSize;
    uint2 cellPos = viewportPos % cellSize;
    Cell cell = cells[cellIndex.y * cellCountX + cellIndex.x];

    // Layer 0:
    // The cell's background color
    float4 color = decodeRGBA(cell.color.y);
    float4 fg = decodeRGBA(cell.color.x);

    // Layer 1 (optional):
    // Colored cursors are drawn "in between" the background color and the text of a cell.
    [branch] if (cell.flags & CellFlags_Cursor)
    {
        [branch] if (cursorColor != INVALID_COLOR)
        {
            // The cursor texture is stored at the top-left-most glyph cell.
            // Cursor pixels are either entirely transparent or opaque.
            // --> We can just use .a as a mask to flip cursor pixels on or off.
            color = alphaBlendPremultiplied(color, decodeRGBA(cursorColor) * glyphs[cellPos].a);
        }
        else if (glyphs[cellPos].a != 0)
        {
            // Make sure the cursor is always readable (see gh-3647)
            // If we imagine the two colors to be in 0-255 instead of 0-1,
            // this effectively XORs them with 63. This avoids a situation
            // where a gray background color (0.5) gets inverted to the
            // same gray making the cursor invisible.
            float2x4 colors = { color, fg };
            float2x4 ip; // integral part
            float2x4 frac = modf(colors * (255.0f / 64.0f), ip);
            colors = (3.0f - ip + frac) * (64.0f / 255.0f);
            color = float4(colors[0].rgb, 1);
            fg = float4(colors[1].rgb, 1);
        }
    }

    // Layer 2:
    // Step 1: The cell's glyph, potentially drawn in the foreground color
    {
        float4 glyph = glyphs[decodeU16x2(cell.glyphPos) + cellPos];

        [branch] if (cell.flags & CellFlags_ColoredGlyph)
        {
            color = alphaBlendPremultiplied(color, glyph);
        }
        else
        {
            float3 foregroundStraight = DWrite_UnpremultiplyColor(fg);
            float blendEnhancedContrast = DWrite_ApplyLightOnDarkContrastAdjustment(enhancedContrast, foregroundStraight);

            [branch] if (useClearType)
            {
                // See DWrite_ClearTypeBlend
                float3 contrasted = DWrite_EnhanceContrast3(glyph.rgb, blendEnhancedContrast);
                float3 alphaCorrected = DWrite_ApplyAlphaCorrection3(contrasted, foregroundStraight, gammaRatios);
                color = float4(lerp(color.rgb, foregroundStraight, alphaCorrected * fg.a), 1.0f);
            }
            else
            {
                // See DWrite_GrayscaleBlend
                float intensity = DWrite_CalcColorIntensity(foregroundStraight);
                float contrasted = DWrite_EnhanceContrast(glyph.a, blendEnhancedContrast);
                float4 alphaCorrected = DWrite_ApplyAlphaCorrection(contrasted, intensity, gammaRatios);
                color = alphaBlendPremultiplied(color, alphaCorrected * fg);
            }
        }
    }
    // Step 2: Lines
    {
        // What a nice coincidence that we have exactly 8 flags to handle right now!
        // `mask` will mask away any positive results from checks we don't want.
        // (I.e. even if we're in an underline, it doesn't matter if we don't want an underline.)
        bool2x4 mask = {
            cell.flags & CellFlags_BorderLeft,
            cell.flags & CellFlags_BorderTop,
            cell.flags & CellFlags_BorderRight,
            cell.flags & CellFlags_BorderBottom,
            cell.flags & CellFlags_Underline,
            cell.flags & CellFlags_UnderlineDotted,
            cell.flags & CellFlags_UnderlineDouble,
            cell.flags & CellFlags_Strikethrough,
        };
        // The following <lineWidth checks rely on underflow turning the
        // uint into a way larger number than any reasonable lineWidth.
        // That way we don't need to write `y >= lo && y < hi`.
        bool2x4 checks = {
            // These 2 expand to 4 bools, because cellPos is a
            // uint2 vector which results in a bool2 result each.
            cellPos < thinLineWidth,
            (cellSize - cellPos) <= thinLineWidth,
            // These 4 are 4 regular bools.
            (cellPos.y - underlinePos) < underlineWidth,
            (cellPos.y - underlinePos) < underlineWidth && (viewportPos.x / underlineWidth & 3) == 0,
            any((cellPos.y - doubleUnderlinePos) < thinLineWidth),
            (cellPos.y - strikethroughPos) < strikethroughWidth,
        };
        [flatten] if (any(mask && checks))
        {
            color = alphaBlendPremultiplied(color, fg);
        }
    }

    // Layer 4:
    // The current selection is drawn semi-transparent on top.
    [branch] if (cell.flags & CellFlags_Selected)
    {
        color = alphaBlendPremultiplied(color, decodeRGBA(selectionColor));
    }

    return color;
}
