#define INVALID_COLOR 0xffffffff

// These flags are shared with AtlasEngine::CellFlags.
//
// clang-format off
#define CellFlags_None            0x00000000
#define CellFlags_Inlined         0x00000001

#define CellFlags_ColoredGlyph    0x00000002
#define CellFlags_ThinFont        0x00000004

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
    uint2 cellSize;
    uint cellCountX;
    float gamma;
    float grayscaleEnhancedContrast;
    uint backgroundColor;
    uint cursorColor;
    uint selectionColor;
};
StructuredBuffer<Cell> cells : register(t0);
Texture2D<float4> glyphs : register(t1);

float4 decodeRGBA(uint i)
{
    uint r = i & 0xff;
    uint g = (i >> 8) & 0xff;
    uint b = (i >> 16) & 0xff;
    uint a = i >> 24;
    float4 c = float4(r, g, b, a) / 255.0f;
    // Convert to premultiplied alpha for simpler alpha blending.
    c.rgb *= c.a;
    return c;
}

uint2 decodeU16x2(uint i)
{
    return uint2(i & 0xffff, i >> 16);
}

float insideRect(float2 pos, float4 boundaries)
{
    float2 v = step(boundaries.xy, pos) - step(boundaries.zw, pos);
    return v.x * v.y;
}

float4 alphaBlendPremultiplied(float4 bottom, float4 top)
{
    float ia = 1 - top.a;
    return float4(bottom.rgb * ia + top.rgb, bottom.a * ia + top.a);
}

// clang-format off
float4 main(float4 pos: SV_Position): SV_Target
// clang-format on
{
    if (!insideRect(pos.xy, viewport))
    {
        return decodeRGBA(backgroundColor);
    }

    // If you want to write test a before/after change simultaneously
    // you can turn the image into a checkerboard by writing:
    //   if ((uint(pos.x) ^ uint(pos.y)) / 4 & 1) { return float4(1, 0, 0, 1); }
    // This will generate a checkerboard of 4*4px red squares.
    // Of course you wouldn't just return a red color there, but instead
    // for instance run your new code and compare it with the old.

    uint2 cellIndex = pos.xy / cellSize;
    uint2 cellPos = pos.xy % cellSize;
    Cell cell = cells[cellIndex.y * cellCountX + cellIndex.x];

    // Layer 0:
    // The cell's background color
    float4 color = decodeRGBA(cell.color.y);

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
    // The cell's glyph potentially drawn in the foreground color
    {
        float4 glyph = glyphs[decodeU16x2(cell.glyphPos) + cellPos];

        if (!(cell.flags & CellFlags_ColoredGlyph))
        {
            float4 fg = decodeRGBA(cell.color.x);

            // Put DirectWrite's secret magic sauce here.
            float correctedAlpha = glyph.a;

            glyph = fg * correctedAlpha;
        }

        color = alphaBlendPremultiplied(color, glyph);
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
