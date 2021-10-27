// According to Nvidia's "Understanding Structured Buffer Performance" guide
// one should aim for structures with sizes divisible by 128 bits (16 bytes).
// This prevents elements from spanning cache lines.
struct Cell
{
    uint glyphPos;
    uint flags;
    uint2 color;
};

cbuffer ConstBuffer : register(b0)
{
    float4 viewport;
    uint2 cellSize;
    uint cellCountX;
    uint backgroundColor;
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

float4 main(float4 pos: SV_Position): SV_Target
{
    if (!insideRect(pos.xy, viewport))
    {
        return decodeRGB(backgroundColor);
    }

    uint2 cellIndex = pos.xy / cellSize;
    uint2 cellPos = pos.xy % cellSize;

    Cell cell = cells[cellIndex.y * cellCountX + cellIndex.x];

    uint2 glyphPos = decodeU16x2(cell.glyphPos);
    uint2 pixelPos = glyphPos + cellPos;
    float4 alpha = glyphs[pixelPos];

    float3 color = lerp(
        decodeRGB(cell.color.y).rgb,
        decodeRGB(cell.color.x).rgb,
        alpha.rgb
    );

    if (cell.flags & 1)
    {
        color = abs(glyphs[cellPos].rgb - color);
    }
    if (cell.flags & 2)
    {
        float4 sc = decodeRGB(selectionColor);
        color = lerp(color, sc.rgb, sc.a);
    }

    return float4(color, 1);
}
