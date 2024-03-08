// Demo shader to show passing in an image using
// experimental.pixelShaderImagePath. This shader simply displays the Terminal
// contents on top of the given image.
//
// The image loaded by the terminal will be placed into the `image` texture.

SamplerState samplerState;
Texture2D shaderTexture : register(t0);
Texture2D image : register(t1);

cbuffer PixelShaderSettings {
    float Time;
    float Scale;
    float2 Resolution;
    float4 Background;
};

float4 main(float4 pos : SV_POSITION, float2 tex : TEXCOORD) : SV_TARGET
{
    float4 terminalColor = shaderTexture.Sample(samplerState, tex);
    float4 imageColor = image.Sample(samplerState, tex);
    return lerp(imageColor, terminalColor, terminalColor.a);
}
