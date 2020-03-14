#pragma once

#ifdef __INSIDE_WINDOWS
const char screenVertexShaderString[] = "";
#else
const char screenVertexShaderString[] = R"(
struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD;
};
VS_OUTPUT main(float4 pos : POSITION, float2 tex : TEXCOORD)
{
    VS_OUTPUT output;
    output.pos = pos;
    output.tex = tex;
    return output;
}
)";
#endif
