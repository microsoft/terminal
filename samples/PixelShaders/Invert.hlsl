// A minimal pixel shader that inverts the colors

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

  float2 GlyphSize;
  float2 CursorPosition;
  float2 BufferSize;
};

float3 HUEtoRGB(float H)
{
  float R = abs(H * 6 - 3) - 1;
  float G = 2 - abs(H * 6 - 2);
  float B = 2 - abs(H * 6 - 4);
  return saturate(float3(R,G,B));
};

#define TAU 6.28318530718
#define PI 3.1415926


// A pixel shader is a program that given a texture coordinate (tex) produces a color.
// tex is an x,y tuple that ranges from 0,0 (top left) to 1,1 (bottom right).
// Just ignore the pos parameter.
float4 main(float4 pos : SV_POSITION, float2 tex : TEXCOORD) : SV_TARGET
{
    // Read the color value at the current texture coordinate (tex)
    //  float4 is tuple of 4 floats, rgba
    float4 color = shaderTexture.Sample(samplerState, tex);

    // Inverts the rgb values (xyz) but don't touch the alpha (w)
    // color.xyz = 1.0 - color.xyz;

    float2 pxCursorTopLeft = CursorPosition * GlyphSize;
    float2 pxCursorBottomRight = (CursorPosition + float2(1, 1)) * GlyphSize;
    // Convert pixel position [{0}, {Resolution}) to texel space [{0}, {1})
    float2 texelRelativeTopLeft = pxCursorTopLeft / Resolution;
    float2 texelRelativeBottomRight = pxCursorBottomRight / Resolution;

    // float2 relativeCursorPos = CursorPosition / BufferSize;
    if ((tex.x >= texelRelativeTopLeft.x && tex.x <= texelRelativeBottomRight.x) &&
        (tex.y >= texelRelativeTopLeft.y && tex.y <= texelRelativeBottomRight.y)) {

        // float yWithinCursor = (tex.y-texelRelativeTopLeft.y) / texelRelativeBottomRight.y;
        // float yWithinCursor = (tex.y) / texelRelativeBottomRight.y;
        // float yWithinCursor = (texelRelativeTopLeft.y) / texelRelativeBottomRight.y;
        // float2 withinCursor = (texelRelativeTopLeft) / texelRelativeBottomRight;
        float2 withinCursor = (tex - texelRelativeTopLeft) * (GlyphSize);

        float duration = 2.0;
        // float hue = lerp(0, 1, 0.5 * cos(((TAU*Time - withinCursor.y)) / duration) + 0.5);
        // float hue = lerp(0, 1, 0.5 * cos(((PI*Time - withinCursor.y)) / duration) + 0.5);
        // float hue = lerp(0, 1, cos(((PI*Time - withinCursor.y)) / duration)+1);
        // float hue = lerp(0.05, .95, .5*cos(((2*TAU*(Time - withinCursor.y))) / duration)+.5);
        // float hue = lerp(0.00, .95, fmod(.5*cos(((TAU*(Time - withinCursor.y))) / duration)+.5, 1));
        float hue = lerp(0.00, 1, fmod(1.25*(Time - withinCursor.y) / duration, 1));
        // float hue = lerp(0, 1, 0.5 * cos((TAU) / duration * Time) + 0.5);
        // float hue = withinCursor.y;
        float3 hsv = float3(hue, 1, 1);
        // float3 result = hsv2rgb(hsv);
        float3 result = HUEtoRGB(hsv.x);
        // color.xy = relativeCursorPos;
        // color.z = 0.0;

        // color.xyz = yWithinCursor;
        // color.xy = withinCursor;
        color.xyz = result;
        color.a = 1.0;
    }
    

    // Return the final color
    return color;
}
