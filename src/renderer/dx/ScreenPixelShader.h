#pragma once

const char screenPixelShaderString[] = R"(
Texture2D shaderTexture;
SamplerState samplerState;

#define SCANLINE_FACTOR 0.5
#define SCANLINE_PERIOD 1

static const float M_PI = 3.14159265f;

// https://en.wikipedia.org/wiki/Gaussian_blur
static float gaussianKernel[7][7] =
{
    { 0.00000067, 0.00002292, 0.00019117, 0.00038771, 0.00019117, 0.00002292, 0.00000067 },
    { 0.00002292, 0.00078633, 0.00655965, 0.01330373, 0.00655965, 0.00078633, 0.00002292 },
    { 0.00019117, 0.00655965, 0.05472157, 0.11098164, 0.05472157, 0.00655965, 0.00019117 },
    { 0.00038771, 0.01330373, 0.11098164, 0.22508352, 0.11098164, 0.01330373, 0.00038771 },
    { 0.00019117, 0.00655965, 0.05472157, 0.11098164, 0.05472157, 0.00655965, 0.00019117 },
    { 0.00002292, 0.00078633, 0.00655965, 0.01330373, 0.00655965, 0.00078633, 0.00002292 },
    { 0.00000067, 0.00002292, 0.00019117, 0.00038771, 0.00019117, 0.00002292, 0.00000067 }
};

float4 Blur(Texture2D input, float2 tex_coord)
{
    uint width, height;
    shaderTexture.GetDimensions(width, height);

    float textureWidth = 1.0f/width;
    float textureHeight = 1.0f/height;

    float4 color = { 0, 0, 0, 0 };
    float factor = 1;

    int start = 0, end = 7; // sizeof(gaussianKernel[0])
    for (int x = start; x < end; x++) 
    {
        float2 samplePos = { 0, 0 };

        samplePos.x = tex_coord.x + (x - 7/2) * textureWidth;
        for (int y = start; y < end; y++)
        {
            samplePos.y = tex_coord.y + (y - 7/2) * textureHeight;
            color += input.Sample(samplerState, samplePos) * gaussianKernel[x][y] * factor;
        }
    }

    return color;
}

float Gaussian2D(float x, float y, float sigma)
{
    return 1/(sigma*sqrt(2*M_PI)) * exp(-0.5*(x*x + y*y)/sigma/sigma);
}

float4 Blur2(Texture2D input, float2 tex_coord, float sigma)
{
    uint width, height;
    shaderTexture.GetDimensions(width, height);

    float texelWidth = 1.0f/width;
    float texelHeight = 1.0f/height;

    float4 color = { 0, 0, 0, 0 };
    float factor = 1;

    int sampleCount = 13;

    for (int x = 0; x < sampleCount; x++) 
    {
        float2 samplePos = { 0, 0 };

        samplePos.x = tex_coord.x + (x - sampleCount/2) * texelWidth;
        for (int y = 0; y < sampleCount; y++)
        {
            samplePos.y = tex_coord.y + (y - sampleCount/2) * texelHeight;
            if (samplePos.x <= 0 || samplePos.y <= 0 || samplePos.x >= width || samplePos.y >= height) continue;

            color += input.Sample(samplerState, samplePos) * Gaussian2D((x - sampleCount/2), (y - sampleCount/2), sigma);
        }
    }

    return color;

}

float SquareWave(float y)
{
    // Square wave gogogo.
    // return 1.0 - SCANLINE_FACTOR * (1 - fmod(pos.y, 2.0));
    // return sin(pos.y / SCANLINE_PERIOD * 2.0 * M_PI) >= 0.0 ? 1: SCANLINE_FACTOR;
    return 1 - (floor(y / SCANLINE_PERIOD) % 2) * SCANLINE_FACTOR;
}

float4 Scanline(float4 color, float4 pos)
{
    float wave = SquareWave(pos.y);

    // Remove the && false to draw scanlines everywhere.
    if (length(color.rgb) < 0.2 && false)
    {
        return color + wave*0.1;
    }
    else
    {
        return color * wave;
    }
}

float4 main(float4 pos : SV_POSITION, float2 tex : TEXCOORD) : SV_TARGET
{
    Texture2D input = shaderTexture;

    // TODO: Make these configurable in some way.
    float4 color = input.Sample(samplerState, tex);
    color += Blur2(input, tex, 2)*0.3;
    color = Scanline(color, pos);

    return color;
}
)";
