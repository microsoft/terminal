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

float4 main(float4 pos: SV_Position): SV_Target
{
    if (!insideRect(pos.xy, viewport))
    {
        return decodeRGBA(backgroundColor);
    }

    // If you want to write test a before/after change simultaneously
    // you can turn the image into a checkerboard by writing:
    //   if ((uint(pos.x / 4) ^ uint(pos.y / 4)) & 1) { return float(1, 0, 0, 1); }
    // This will generate a checkerboard of 4*4px red squares.
    // Of course you wouldn't just return a red color there, but instead
    // for instance run your new code and compare it with the old.

    uint2 cellIndex = pos.xy / cellSize;
    uint2 cellPos = pos.xy % cellSize;
    Cell cell = cells[cellIndex.y * cellCountX + cellIndex.x];

    //return glyphs[decodeU16x2(cell.glyphPos) + cellPos];

    // Layer 0:
    // The cell's background color
    float4 color = decodeRGBA(cell.color.y);

    // Layer 1 (optional):
    // Colored cursors are drawn "in between" the background color and the text of a cell.
    if ((cell.flags & 8) && cursorColor != 0xffffffff)
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

        if ((cell.flags & 2) == 0)
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
    if ((cell.flags & 8) && cursorColor == 0xffffffff)
    {
        color.rgb = abs(glyphs[cellPos].rgb - color.rgb);
    }

    // Layer 4:
    // The current selection is drawn semi-transparent on top.
    if (cell.flags & 16)
    {
        color = alphaBlendPremultiplied(color, decodeRGBA(selectionColor));
    }

    return color;
}
