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
    uint2 underlinePos;
    uint2 strikethroughPos;
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
    if (any(pos.xy < viewport.xy) || any(pos.xy >= viewport.zw))
    {
        return decodeRGBA(backgroundColor);
    }

    // If you want to write test a before/after change simultaneously
    // you can turn the image into a checkerboard by writing:
    //   if ((uint(pos.x) ^ uint(pos.y)) / 4 & 1) { return float4(1, 0, 0, 1); }
    // This will generate a checkerboard of 4*4px red squares.
    // Of course you wouldn't just return a red color there, but instead
    // for instance run your new code and compare it with the old.

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
    if ((cell.flags & CellFlags_Cursor) && cursorColor != INVALID_COLOR)
    {
        // The cursor texture is stored at the top-left-most glyph cell.
        // Cursor pixels are either entirely transparent or opaque.
        // --> We can just use .a as a mask to flip cursor pixels on or off.
        color = alphaBlendPremultiplied(color, decodeRGBA(cursorColor) * glyphs[cellPos].a);
    }

    // Layer 2:
    // Step 1: Underlines
    if ((cell.flags & CellFlags_Underline) && cellPos.y >= underlinePos.x && cellPos.y < underlinePos.y)
    {
        color = alphaBlendPremultiplied(color, fg);
    }
    if ((cell.flags & CellFlags_UnderlineDotted) && cellPos.y >= underlinePos.x && cellPos.y < underlinePos.y && (viewportPos.x / (underlinePos.y - underlinePos.x) & 3) == 0)
    {
        color = alphaBlendPremultiplied(color, fg);
    }
    // Step 2: The cell's glyph, potentially drawn in the foreground color
    {
        float4 glyph = glyphs[decodeU16x2(cell.glyphPos) + cellPos];

        if (cell.flags & CellFlags_ColoredGlyph)
        {
            color = alphaBlendPremultiplied(color, glyph);
        }
        else if (useClearType)
        {
            color = DWrite_CleartypeBlend(gammaRatios, enhancedContrast, false, color, fg, glyph);
        }
        else
        {
            color = alphaBlendPremultiplied(color, DWrite_GrayscaleBlend(gammaRatios, enhancedContrast, false, fg, glyph.a));
        }
    }
    // Step 3: Lines, but not "under"lines
    if ((cell.flags & CellFlags_Strikethrough) && cellPos.y >= strikethroughPos.x && cellPos.y < strikethroughPos.y)
    {
        color = alphaBlendPremultiplied(color, fg);
    }

    // Layer 3 (optional):
    // Uncolored cursors invert the cells color.
    if ((cell.flags & CellFlags_Cursor) && cursorColor == INVALID_COLOR)
    {
        color.rgb = abs(glyphs[cellPos].rgb - color.rgb);
    }

    // Layer 4:
    // The current selection is drawn semi-transparent on top.
    if (cell.flags & CellFlags_Selected)
    {
        color = alphaBlendPremultiplied(color, decodeRGBA(selectionColor));
    }

    return color;
}
