// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929

static const int MAX_SAMPLES = 16;


Texture2D<float4> Texture : register(t0);
sampler Sampler : register(s0);


cbuffer Parameters : register(b0)
{
    float4 sampleOffsets[MAX_SAMPLES];
    float4 sampleWeights[MAX_SAMPLES];
};


#include "Structures.fxh"


// Vertex shader: self-created quad.
VSInputTx VSQuad(uint vI : SV_VertexId)
{
    VSInputTx vout;

    // We use the 'big triangle' optimization so you only Draw 3 verticies instead of 4.
    float2 texcoord = float2((vI << 1) & 2, vI & 2);
    vout.TexCoord = texcoord;

    vout.Position = float4(texcoord.x * 2 - 1, -texcoord.y * 2 + 1, 0, 1);
    return vout;
}


//--------------------------------------------------------------------------------------
// Pixel shader: copy.
float4 PSCopy(VSInputTx pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord);
    return color;
}


// Pixel shader: monochrome.
float4 PSMonochrome(VSInputTx pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord);
    float3 grayscale = float3(0.2125f, 0.7154f, 0.0721f);
    float3 output = dot(color.rgb, grayscale);
    return float4(output, color.a);
}


// Pixel shader: sepia.
float4 PSSepia(VSInputTx pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord);

    float3 red = float3(0.393f, 0.769f, 0.189f);
    float3 green = float3(0.349f, 0.686f, 0.168f);
    float3 blue = float3(0.272f, 0.534f, 0.131f);

    float3 output;
    output.r = dot(color.rgb, red);
    output.g = dot(color.rgb, green);
    output.b = dot(color.rgb, blue);
    return float4(output, color.a);
}


// Pixel shader: down-sample 2x2.
float4 PSDownScale2x2(VSInputTx pin) : SV_Target0
{
    const int NUM_SAMPLES = 4;
    float4 vColor = 0.0f;

    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        vColor += Texture.Sample(Sampler, pin.TexCoord + sampleOffsets[i].xy);
    }

    return vColor / NUM_SAMPLES;
}


// Pixel shader: down-sample 4x4.
float4 PSDownScale4x4(VSInputTx pin) : SV_Target0
{
    const int NUM_SAMPLES = 16;
    float4 vColor = 0.0f;

    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        vColor += Texture.Sample(Sampler, pin.TexCoord + sampleOffsets[i].xy);
    }

    return vColor / NUM_SAMPLES;
}


// Pixel shader: gaussian blur 5x5.
float4 PSGaussianBlur5x5(VSInputTx pin) : SV_Target0
{
    float4 vColor = 0.0f;

    for (int i = 0; i < 13; i++)
    {
        vColor += sampleWeights[i] * Texture.Sample(Sampler, pin.TexCoord + sampleOffsets[i].xy);
    }

    return vColor;
}


// Pixel shader: bloom (extract)
float4 PSBloomExtract(VSInputTx pin) : SV_Target0
{
    // Uses sampleWeights[0] as 'bloom threshold'
    float4 c = Texture.Sample(Sampler, pin.TexCoord);
    return saturate((c - sampleWeights[0]) / (1 - sampleWeights[0]));
}


// Pixel shader: bloom (blur)
float4 PSBloomBlur(VSInputTx pin) : SV_Target0
{
    float4 vColor = 0.0f;

    // Perform a one-directional gaussian blur
    for (int i = 0; i < 15; i++)
    {
        vColor += sampleWeights[i] * Texture.Sample(Sampler, pin.TexCoord + sampleOffsets[i].xy);
    }

    return vColor;
}


//--------------------------------------------------------------------------------------
Texture2D<float4> Texture2 : register(t1);

// Pixel shader: merge
float4 PSMerge(VSInputTx pin) : SV_Target0
{
    float4 vColor = sampleWeights[0] * Texture.Sample(Sampler, pin.TexCoord);
    vColor += sampleWeights[1] * Texture2.Sample(Sampler, pin.TexCoord);
    return vColor;
}


// Pixel shader: bloom (combine)
float4 AdjustSaturation(float4 color, float saturation)
{
    float3 grayscale = float3(0.2125f, 0.7154f, 0.0721f);
    float gray = dot(color.rgb, grayscale);
    return lerp(gray, color, saturation);
}

float4 PSBloomCombine(VSInputTx pin) : SV_Target0
{
    // Uses sampleWeights[0].x as base saturation, sampleWeights[0].y as bloom saturation
    // Uses sampleWeights[1] as base intensity; sampleWeights[2] as bloom intensity
    float4 base = Texture.Sample(Sampler, pin.TexCoord);
    float4 bloom = Texture2.Sample(Sampler, pin.TexCoord);

    // Adjust color saturation and intensity.
    base = AdjustSaturation(base, sampleWeights[0].x) * sampleWeights[1];
    bloom = AdjustSaturation(bloom, sampleWeights[0].y) * sampleWeights[2];

    // Darken down the base image in areas where there is a lot of bloom,
    // to prevent things looking excessively burned-out.
    base *= (1 - saturate(bloom));

    // Combine the two images.
    return base + bloom;
}
