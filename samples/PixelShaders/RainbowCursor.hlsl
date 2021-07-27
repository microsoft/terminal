// A minimal pixel shader that inverts the colors

// The terminal graphics as a texture
Texture2D shaderTexture;
SamplerState samplerState;

// Terminal settings such as the resolution of the texture
cbuffer PixelShaderSettings {

  float  Time; // The number of seconds since the pixel shader was enabled
  float  Scale; // UI Scale
  float2 Resolution; // Resolution of the shaderTexture
  float4 Background; // Background color as rgba

  float2 GlyphSize;
  float2 CursorPosition;
  float2 BufferSize;
};

// Helper for converting a hue [0, 1) to an RGB value.
// Credit to https://www.chilliant.com/rgb2hsv.html
float3 HUEtoRGB(float H)
{
  float R = abs(H * 6 - 3) - 1;
  float G = 2 - abs(H * 6 - 2);
  float B = 2 - abs(H * 6 - 4);
  return saturate(float3(R,G,B));
};

// A pixel shader is a program that given a texture coordinate (tex) produces a color.
// tex is an x,y tuple that ranges from 0,0 (top left) to 1,1 (bottom right).
// Just ignore the pos parameter.
float4 main(float4 pos : SV_POSITION, float2 tex : TEXCOORD) : SV_TARGET
{
    // Read the color value at the current texture coordinate (tex)
    //  float4 is tuple of 4 floats, rgba
    float4 color = shaderTexture.Sample(samplerState, tex);

    // Find the location of the cursor within the viewport, in pixels.
    float2 pxCursorTopLeft = CursorPosition * GlyphSize;
    float2 pxCursorBottomRight = (CursorPosition + float2(1, 1)) * GlyphSize;

    // Convert pixel position [{0}, {Resolution}) to texel space [{0}, {1})
    float2 texelRelativeTopLeft = pxCursorTopLeft / Resolution;
    float2 texelRelativeBottomRight = pxCursorBottomRight / Resolution;

    // If we're rendering the cells within the bounds of the cursor cell...
    if ((tex.x >= texelRelativeTopLeft.x && tex.x <= texelRelativeBottomRight.x) &&
        (tex.y >= texelRelativeTopLeft.y && tex.y <= texelRelativeBottomRight.y)) {

        float2 withinCursor = (tex - texelRelativeTopLeft) * (GlyphSize);

        // Duration of the animation, in seconds
        float duration = 2.0;

        // fmod(x, y) will cycle linearly between 0 and y. fmod(x, 1) will just
        // get the fractional component of x. Since HUEtoRGB(0) == HUEtoRBG(1),
        // the colors naturally cycle.
        // This lets us scroll through the entire color spectrum smoothly.
        // multiply by 1.25 to make the animation a little faster - this gets a
        // bit more hue variation in the cursor at a single time.
        float hue = lerp(0.00, 1, fmod(1.25*(Time - withinCursor.y) / duration, 1));
        color.xyz = HUEtoRGB(hue);
        color.a = 1.0;
    }
    
    // Return the final color
    return color;
}
