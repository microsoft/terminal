// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkId=248929
// http://go.microsoft.com/fwlink/?LinkID=615561


float3 BiasX2(float3 x)
{
    return 2.0f * x - 1.0f;
}

float3 BiasD2(float3 x)
{
    return 0.5f * x + 0.5f;
}


// Christian Schuler, "Normal Mapping without Precomputed Tangents", ShaderX 5, Chapter 2.6, pp. 131-140
// See also follow-up blog post: http://www.thetenthplanet.de/archives/1180
float3x3 CalculateTBN(float3 p, float3 n, float2 tex)
{
    float3 dp1 = ddx(p);
    float3 dp2 = ddy(p);
    float2 duv1 = ddx(tex);
    float2 duv2 = ddy(tex);

    float3x3 M = float3x3(dp1, dp2, cross(dp1, dp2));
    float2x3 inverseM = float2x3(cross(M[1], M[2]), cross(M[2], M[0]));
    float3 t = normalize(mul(float2(duv1.x, duv2.x), inverseM));
    float3 b = normalize(mul(float2(duv1.y, duv2.y), inverseM));
    return float3x3(t, b, n);
}

float3 PeturbNormal(float3 localNormal, float3 position, float3 normal, float2 texCoord)
{
    const float3x3 TBN = CalculateTBN(position, normal, texCoord);
    return normalize(mul(localNormal, TBN));
}

float3 TwoChannelNormalX2(float2 normal)
{
    float2 xy = 2.0f * normal - 1.0f;
    float z = sqrt(1 - dot(xy, xy));
    return float3(xy.x, xy.y, z);
}


// sRGB 
// https://en.wikipedia.org/wiki/SRGB

// Apply the (approximate) sRGB curve to linear values
float3 LinearToSRGBEst(float3 color)
{
    return pow(abs(color), 1/2.2f);
}


// (Approximate) sRGB to linear
float3 SRGBToLinearEst(float3 srgb)
{
    return pow(abs(srgb), 2.2f);
}


// HDR10 Media Profile
// https://en.wikipedia.org/wiki/High-dynamic-range_video#HDR10


// Apply the ST.2084 curve to normalized linear values and outputs normalized non-linear values
float3 LinearToST2084(float3 normalizedLinearValue)
{
    return pow((0.8359375f + 18.8515625f * pow(abs(normalizedLinearValue), 0.1593017578f)) / (1.0f + 18.6875f * pow(abs(normalizedLinearValue), 0.1593017578f)), 78.84375f);
}


// ST.2084 to linear, resulting in a linear normalized value
float3 ST2084ToLinear(float3 ST2084)
{
    return pow(max(pow(abs(ST2084), 1.0f / 78.84375f) - 0.8359375f, 0.0f) / (18.8515625f - 18.6875f * pow(abs(ST2084), 1.0f / 78.84375f)), 1.0f / 0.1593017578f);
}


// Reinhard tonemap operator
// Reinhard et al. "Photographic tone reproduction for digital images." ACM Transactions on Graphics. 21. 2002.
// http://www.cs.utah.edu/~reinhard/cdrom/tonemap.pdf
float3 ToneMapReinhard(float3 color)
{
    return color / (1.0f + color);
}


// ACES Filmic tonemap operator
// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 ToneMapACESFilmic(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x*(a*x+b))/(x*(c*x+d)+e));
}


// Instancing
struct CommonInstancing
{
    float4 Position;
    float3 Normal;
};


CommonInstancing ComputeCommonInstancing(float4 position, float3 normal, float4x3 itransform)
{
    CommonInstancing vout;

    vout.Position = float4(mul(position, itransform), position.w);
    vout.Normal = mul(normal, (float3x3)itransform);

    return vout;
}
