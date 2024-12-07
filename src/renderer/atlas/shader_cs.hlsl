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

struct Sprite
{
    uint2 position;
    uint2 size;
    uint2 texcoord;
    uint color;
    uint padding;
};

Texture2D<float4> glyphs : register(t0);
Texture2D<uint2> tiles : register(t1);
StructuredBuffer<Sprite> sprites : register(t2);
RWTexture2D<float4> output : register(u0);

[numthreads(8, 8, 1)]
void main(uint2 groupId : SV_GroupID, uint2 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 tile = tiles[groupId];
    float4 color = backgroundColor;
    float4 alphas = float4(0,0,0,0);

    for (uint i = tile.x; i < tile.y; i++)
    {
        Sprite sprite = sprites[i];
        // Figure out where the current dispatchThreadId is relative to the sprite.dst.
        uint2 offset = dispatchThreadId - sprite.position;
        // If the current dispatchThreadId is within the sprite, then sample the glyph texture.
        if (all(offset < sprite.size))
        {
            uint2 src = sprite.texcoord + offset;
            float4 glyph = glyphs[src];
            color = alphaBlendPremultiplied(color, decodeRGBA(sprite.color));
            alphas = max(alphas, glyph);
        }
    }

    color *= alphas;
    output[dispatchThreadId] = color;
}
