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
    float4 c = float4(r, g, b, a) / 255.0;
    // Convert to premultiplied alpha for simpler alpha blending.
    c.rgb *= c.a;
    // Convert to linear space for correct alpha blending
    // with color data in the glyphs texture.
    c.rgb = pow(c.rgb, gamma);
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

    uint2 cellIndex = pos.xy / cellSize;
    uint2 cellPos = pos.xy % cellSize;
    Cell cell = cells[cellIndex.y * cellCountX + cellIndex.x];

    // Layer 0:
    // The cell's background color
    float4 color = decodeRGBA(cell.color.y);

    // Layer 1 (optional):
    // Colored cursors are drawn "in between" the background color and the text of a cell.
    if ((cell.flags & 2) && cursorColor != 0xffffffff)
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
        // TODO: Can we convert to linear space right during DirectWrite rasterization?
        // Pros: Saves us cycles in this shader. Cons: Precision loss?
        glyph.rgb = pow(abs(glyph.rgb), gamma);

        if ((cell.flags & 1) == 0)
        {
            // Unlike colored glyphs, regular glyphs are mixed with the current foreground color.
            glyph = decodeRGBA(cell.color.x) * glyph;
        }

        color = alphaBlendPremultiplied(color, glyph);
    }

    // Layer 3 (optional):
    // Uncolored cursors invert the cells color.
    if ((cell.flags & 2) && cursorColor == 0xffffffff)
    {
        color.rgb = abs(glyphs[cellPos].rgb - color.rgb);
    }

    // Layer 4:
    // The current selection is drawn semi-transparent on top.
    if (cell.flags & 4)
    {
        color = alphaBlendPremultiplied(color, decodeRGBA(selectionColor));
    }

    color.rgb = pow(abs(color.rgb), 1.0 / gamma);
    return color;
}
