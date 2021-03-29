// A minimal pixel shader that outlines text

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

    // Read the color value at some offset, will be used as shadow. For the best
    // effect, read the colors offset on the left, right, top, bottom of this
    // fragment, as well as on the corners of this fragment.
    //
    // You could get away with fewer samples, but the resulting outlines will be
    // blurrier.

    //left, right, top, bottom:
    float4 leftColor   = shaderTexture.Sample(samplerState, tex+1.0*Scale*float2( 1.0,  0.0)/Resolution.y);
    float4 rightColor  = shaderTexture.Sample(samplerState, tex+1.0*Scale*float2(-1.0,  0.0)/Resolution.y);
    float4 topColor    = shaderTexture.Sample(samplerState, tex+1.0*Scale*float2( 0.0,  1.0)/Resolution.y);
    float4 bottomColor = shaderTexture.Sample(samplerState, tex+1.0*Scale*float2( 0.0, -1.0)/Resolution.y);

    // Corners
    float4 topLeftColor     = shaderTexture.Sample(samplerState, tex+1.0*Scale*float2( 1.0,  1.0)/Resolution.y);
    float4 topRightColor    = shaderTexture.Sample(samplerState, tex+1.0*Scale*float2(-1.0,  1.0)/Resolution.y);
    float4 bottomLeftColor  = shaderTexture.Sample(samplerState, tex+1.0*Scale*float2( 1.0, -1.0)/Resolution.y);
    float4 bottomRightColor = shaderTexture.Sample(samplerState, tex+1.0*Scale*float2(-1.0, -1.0)/Resolution.y);


    // Now, if any of those adjacent cells has text in it, then the *color vec4
    // will have a non-zero .w (which is used for alpha). Use that alpha value
    // to add some black to the current fragment.
    //
    // This will result in only coloring fragments adjacent to text, but leaving
    // background images (for example) untouched.
    float3 outlineColor = float3(0, 0, 0);
    float4 result = color;
    result = result + float4(outlineColor, leftColor.w);
    result = result + float4(outlineColor, rightColor.w);
    result = result + float4(outlineColor, topColor.w);
    result = result + float4(outlineColor, bottomColor.w);

    result = result + float4(outlineColor, topLeftColor.w);
    result = result + float4(outlineColor, topRightColor.w);
    result = result + float4(outlineColor, bottomLeftColor.w);
    result = result + float4(outlineColor, bottomRightColor.w);
    return result;
}
