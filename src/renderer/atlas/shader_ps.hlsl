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
    float2 viewportSize;
    uint2 cellSize;
    uint cellCountX;
    uint backgroundColor;
    uint cursorColor;
    uint selectionColor;
};
StructuredBuffer<Cell> cells : register(t0);
Texture2D<float4> glyphs : register(t1);

float4 decodeRGB(uint i)
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

float4 main(float4 pos : SV_Position) : SV_Target
{
    if (any(pos.xy > viewportSize))
    {
        return decodeRGB(backgroundColor);
    }

    uint2 cellIndex = pos.xy / cellSize;
    uint2 cellPos = pos.xy % cellSize;
    Cell cell = cells[cellIndex.y * cellCountX + cellIndex.x];
    float4 glyph = glyphs[decodeU16x2(cell.glyphPos) + cellPos];
    float3 bg = decodeRGB(cell.color.y).rgb;
    float3 color;
    
    if ((cell.flags & 2) && cursorColor != 0xffffffff)
    {
        // Colored cursors are drawn "in between" the background color and the text of a cell.
        float3 cursor = decodeRGB(cursorColor).rgb;
        bg = lerp(bg, cursor, glyphs[cellPos].rgb);
    }

    if (cell.flags & 1)
    {
        // Colored glyphs (rgb are color values, a is alpha).
        // --> Alpha blending with premultiplied colors in glyph.
        color = glyph.rgb + bg * (1 - glyph.a);
    }
    else
    {
        // Regular glyphs (rgb are alpha values).
        // --> Alpha blending with regular colors (aka interpolation/lerp).
        float3 fg = decodeRGB(cell.color.x).rgb;
        color = lerp(bg, fg, glyph.a);
    }

    if ((cell.flags & 2) && cursorColor == 0xffffffff)
    {
        // Uncolored cursors invert the color.
        color = abs(glyphs[cellPos].rgb - color);
    }

    if (cell.flags & 4)
    {
        // Selection coloring.
        float4 sc = decodeRGB(selectionColor);
        color = lerp(color, sc.rgb, sc.a);
    }

    return float4(color, 1);
}
