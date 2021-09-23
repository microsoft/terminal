// A minimal pixel shader that shows some raster bars

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

    // Read the color value at some offset, will be used as shadow
    float4 ocolor = shaderTexture.Sample(samplerState, tex+2.0*Scale*float2(-1.0, -1.0)/Resolution.y);

    // Thickness of raster
    const float thickness = 0.1;

    float ny = floor(tex.y/thickness);
    float my = tex.y%thickness;
    const float pi = 3.141592654;


    // ny is used to compute the rasterbar base color
    float cola = ny*2.0*pi;
    float3 col = 0.75+0.25*float3(sin(cola*0.111), sin(cola*0.222), sin(cola*0.333));

    // my is used to compute the rasterbar brightness
    //  smoothstep is a great little function: https://en.wikipedia.org/wiki/Smoothstep
    float brightness = 1.0-smoothstep(0.0, thickness*0.5, abs(my - 0.5*thickness));

    float3 rasterColor = col*brightness;

    // lerp(x, y, a) is another very useful function: https://en.wikipedia.org/wiki/Linear_interpolation
    float3 final = rasterColor;
    // Create the drop shadow of the terminal graphics
    //  .w is the alpha channel, 0 is fully transparent and 1 is fully opaque
    final = lerp(final, float(0.0), ocolor.w);
    // Draw the terminal graphics
    final = lerp(final, color.xyz, color.w);

    // Return the final color, set alpha to 1 (ie opaque)
    return float4(final, 1.0);
}