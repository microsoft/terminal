// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD;
};

// clang-format off
VS_OUTPUT main(uint id : SV_VERTEXID)
// clang-format on
{
    VS_OUTPUT output;
    // The following two lines are taken from https://gamedev.stackexchange.com/a/77670
    // written by János Turánszki, licensed under CC BY-SA 3.0.
    output.tex = float2(id % 2, id % 4 / 2);
    output.pos = float4((output.tex.x - 0.5f) * 2.0f, -(output.tex.y - 0.5f) * 2.0f, 0, 1);
    return output;
}
