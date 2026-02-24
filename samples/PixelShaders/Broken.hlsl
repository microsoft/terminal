// Broken, can be used for explorative testing of pixel shader error handling
Texture2D shaderTexture;
SamplerState samplerState;

cbuffer PixelShaderSettings {
  float  Time;
  float  Scale;
  float2 Resolution;
  float4 Background;
};

float4 main(float4 pos : SV_POSITION, float2 tex : TEXCOORD) : SV_TARGET
{
    // OOPS; vec4 is not a hlsl but a glsl datatype!
    vec4 color = shaderTexture.Sample(samplerState, tex);

    return color;
}