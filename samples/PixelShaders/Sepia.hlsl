// A minimal pixel shader that applies a sepia tone effect

// The terminal graphics as a texture
Texture2D shaderTexture;
SamplerState samplerState;

// Terminal settings such as the resolution of the texture
cbuffer PixelShaderSettings {
  // The number of seconds since the pixel shader was enabled
  float  Time;
  // UI Scale
  float  Scale;
  // Resolution of the shaderTexture
  float2 Resolution;
  // Background color as rgba
  float4 Background;
};

// A pixel shader is a program that given a texture coordinate (tex) produces a color.
// tex is an x,y tuple that ranges from 0,0 (top left) to 1,1 (bottom right).
// Just ignore the pos parameter.
float4 main(float4 pos : SV_POSITION, float2 tex : TEXCOORD) : SV_TARGET
{
    // Read the color value at the current texture coordinate (tex)
    //  float4 is tuple of 4 floats, rgba
    float4 color = shaderTexture.Sample(samplerState, tex);

    // Convert to grayscale first by taking a weighted average of the rgb channels.
    // These weights reflect human eye sensitivity: we see green most strongly,
    // red moderately, and blue the least.
    float gray = dot(color.rgb, float3(0.299, 0.587, 0.114));

    // Apply the sepia tone by tinting the grayscale value with warm brown offsets.
    // The output rgb values come from the classic sepia matrix used in image processing.
    color.r = saturate(gray * 1.351 + 0.0);
    color.g = saturate(gray * 1.203 + 0.0);
    color.b = saturate(gray * 0.937 + 0.0);

    // Return the final color, preserving the original alpha (w)
    return color;
}