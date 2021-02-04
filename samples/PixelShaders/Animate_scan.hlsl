// A simple animated shader that draws an inverted line that scrolls down the screen

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

// A pixel shader is a program that given a texture coordinate (tex) produces a color.
// tex is an x,y tuple that ranges from 0,0 (top left) to 1,1 (bottom right).
// Just ignore the pos parameter.
float4 main(float4 pos : SV_POSITION, float2 tex : TEXCOORD) : SV_TARGET
{
    // Read the color value at the current texture coordinate (tex)
    float4 color = shaderTexture.Sample(samplerState, tex);
    
    // Here we spread the animation over 5 seconds. We use time modulo 5 because we want
    // the timer to count to five repeatedly. We then divide the result by five again
    // to get a value between 0.0 and 1.0, which maps to our texture coordinate.
    float linePosition = Time % 5 / 5;
    
    // Since TEXCOORD ranges from 0.0 to 1.0, we need to divide 1.0 by the height of the
    // texture to find out the size of a single pixel
    float lineWidth = 1.0 / Resolution.y;
	
    // If the current texture coordinate is in the range of our line on the Y axis: 
    if (tex.y > linePosition - lineWidth && tex.y < linePosition)
    {
        // Invert the sampled color
        color.rgb = 1.0 - color.rgb;
    }
    
    return color;
}
