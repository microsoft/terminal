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
    uint backgroundColor;
    uint cursorColor;
    uint selectionColor;
};
StructuredBuffer<Cell> cells : register(t0);
Texture2D<float4> glyphs : register(t1);

float3 decodeRGB(uint i)
{
    uint r = i & 0xff;
    uint g = (i >> 8) & 0xff;
    uint b = (i >> 16) & 0xff;
    return float3(r, g, b) / 255.0;
}

float4 decodeRGBA(uint i)
{
    uint r = i & 0xff;
    uint g = (i >> 8) & 0xff;
    uint b = (i >> 16) & 0xff;
    uint a = i >> 24;
    return float4(r, g, b, a) / 255.0;
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

float4 main(float4 pos: SV_Position): SV_Target
{
    if (!insideRect(pos.xy, viewport))
    {
        return float4(decodeRGB(backgroundColor), 1);
    }

    uint2 cellIndex = pos.xy / cellSize;
    uint2 cellPos = pos.xy % cellSize;
    Cell cell = cells[cellIndex.y * cellCountX + cellIndex.x];

    // Layer 0:
    // The cell's background color
    float3 color = decodeRGB(cell.color.y);

    // Layer 1 (optional):
    // Colored cursors are drawn "in between" the background color and the text of a cell.
    if ((cell.flags & 2) && cursorColor != 0xffffffff)
    {
        color = lerp(color, decodeRGB(cursorColor), glyphs[cellPos].a);
    }

    // Layer 2:
    // The cell's glyph potentially drawn in the foreground color
    {
        float4 glyph = glyphs[decodeU16x2(cell.glyphPos) + cellPos];

        if (cell.flags & 1)
        {
            // Colored glyphs like Emojis
            // --> Alpha blending with premultiplied colors in glyph.
            color = glyph.rgb + color * (1 - glyph.a);
        }
        else
        {
            // Regular glyphs
            // --> Alpha blending with regular colors (aka interpolation/lerp).
            color = lerp(color, decodeRGB(cell.color.x), glyph.a);
        }
    }

    // Layer 3 (optional):
    // Uncolored cursors invert the cells color.
    if ((cell.flags & 2) && cursorColor == 0xffffffff)
    {
        color = abs(glyphs[cellPos].rgb - color);
    }

    // Layer 4:
    // The selection is drawn as a semi-transparent, non-premultiplied color on top.
    if (cell.flags & 4)
    {
        // Selection coloring.
        float4 sc = decodeRGBA(selectionColor);
        color = lerp(color, sc.rgb, sc.a);
    }

    return float4(color, 1);
}
