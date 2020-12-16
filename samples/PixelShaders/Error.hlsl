// Shader used to indicate something went wrong during shader loading
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
    float4 color = shaderTexture.Sample(samplerState, tex);
    float bars = 0.5+0.5*sin(tex.y*100);
    color.x += pow(bars, 20.0);
    return color;
}