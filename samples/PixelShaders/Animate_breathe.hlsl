// A simple animated shader that fades the terminal background back and forth between two colours

// The terminal graphics as a texture
Texture2D shaderTexture;
SamplerState samplerState;

// Terminal settings such as the resolution of the texture
cbuffer PixelShaderSettings
{
    // The number of seconds since the pixel shader was enabled
    float Time;
    // UI Scale
    float Scale;
    // Resolution of the shaderTexture
    float2 Resolution;
    // Background color as rgba
    float4 Background;
};

// pi and tau (2 * pi) are useful constants when using trigonometric functions
#define TAU 6.28318530718

// A pixel shader is a program that given a texture coordinate (tex) produces a color.
// tex is an x,y tuple that ranges from 0,0 (top left) to 1,1 (bottom right).
// Just ignore the pos parameter.
float4 main(float4 pos : SV_POSITION, float2 tex : TEXCOORD) : SV_TARGET
{
	// Read the color value at the current texture coordinate (tex)
    float4 sample = shaderTexture.Sample(samplerState, tex);
    
    // The number of seconds the breathing effect should span
    float duration = 5.0;
    
    float3 color1 = float3(0.3, 0.0, 0.5);  // indigo
    float3 color2 = float3(0.1, 0.1, 0.44); // midnight blue
    
    // Set background colour based on the time
    float4 backgroundColor = float4(lerp(color1, color2, 0.5 * cos(TAU / duration * Time) + 0.5), 1.0);
    
    // Draw the terminal graphics over the background
    return lerp(backgroundColor, sample, sample.w);
}