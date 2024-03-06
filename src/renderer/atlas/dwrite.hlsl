// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

float3 DWrite_UnpremultiplyColor(float4 color)
{
    if (color.a != 0)
    {
        color.rgb /= color.a;
    }
    return color.rgb;
}

float DWrite_ApplyLightOnDarkContrastAdjustment(float grayscaleEnhancedContrast, float3 color)
{
    // The following 1 line is the same as this direct translation of the
    // original code, but simplified to reduce the number of instructions:
    //   float lightness = dot(color, float3(0.30f, 0.59f, 0.11f);
    //   float multiplier = saturate(4.0f * (0.75f - lightness));
    //   return grayscaleEnhancedContrast * multiplier;
    return grayscaleEnhancedContrast * saturate(dot(color, float3(0.30f, 0.59f, 0.11f) * -4.0f) + 3.0f);
}

float DWrite_CalcColorIntensity(float3 color)
{
    return dot(color, float3(0.25f, 0.5f, 0.25f));
}

float DWrite_EnhanceContrast(float alpha, float k)
{
    return alpha * (k + 1.0f) / (alpha * k + 1.0f);
}

float3 DWrite_EnhanceContrast3(float3 alpha, float k)
{
    return alpha * (k + 1.0f) / (alpha * k + 1.0f);
}

float DWrite_ApplyAlphaCorrection(float a, float f, float4 g)
{
    return a + a * (1.0f - a) * ((g.x * f + g.y) * a + (g.z * f + g.w));
}

float3 DWrite_ApplyAlphaCorrection3(float3 a, float3 f, float4 g)
{
    return a + a * (1.0f - a) * ((g.x * f + g.y) * a + (g.z * f + g.w));
}

// Call this function to get the same gamma corrected alpha blending effect
// as DirectWrite's native algorithm for gray-scale anti-aliased glyphs.
//
// The result is a premultiplied color value, resulting
// out of the blending of foregroundColor with glyphAlpha.
//
// gammaRatios:
//   Magic constants produced by DWrite_GetGammaRatios() in dwrite.cpp.
//   The default value for this are the 1.8 gamma ratios, which equates to:
//     0.148054421f, -0.894594550f, 1.47590804f, -0.324668258f
// grayscaleEnhancedContrast:
//   An additional contrast boost, making the font lighter/darker.
//   The default value for this is 1.0f.
//   This value should be set to the return value of DWrite_GetRenderParams() or (pseudo-code):
//     IDWriteRenderingParams1* defaultParams;
//     dwriteFactory->CreateRenderingParams(&defaultParams);
//     gamma = defaultParams->GetGrayscaleEnhancedContrast();
// isThinFont:
//   This constant is true for certain fonts that are simply too thin for AA.
//   Unlike the previous two values, this value isn't a constant and can change per font family.
//   If you only use modern fonts (like Roboto) you can safely assume that it's false.
//   If you draw your glyph atlas with any DirectWrite method except IDWriteGlyphRunAnalysis::CreateAlphaTexture
//   then you must set this to false as well, as not even tricks like setting the
//   gamma to 1.0 disables the internal thin-font contrast-boost inside DirectWrite.
//   Applying the contrast-boost twice would then look incorrectly.
// foregroundColor:
//   The text's foreground color in premultiplied alpha.
// glyphAlpha:
//   The alpha value of the current glyph pixel in your texture atlas.
float4 DWrite_GrayscaleBlend(float4 gammaRatios, float grayscaleEnhancedContrast, bool isThinFont, float4 foregroundColor, float glyphAlpha)
{
    float3 foregroundStraight = DWrite_UnpremultiplyColor(foregroundColor);
    float contrastBoost = isThinFont ? 0.5f : 0.0f;
    float blendEnhancedContrast = contrastBoost + DWrite_ApplyLightOnDarkContrastAdjustment(grayscaleEnhancedContrast, foregroundStraight);
    float intensity = DWrite_CalcColorIntensity(foregroundStraight);
    float contrasted = DWrite_EnhanceContrast(glyphAlpha, blendEnhancedContrast);
    float alphaCorrected = DWrite_ApplyAlphaCorrection(contrasted, intensity, gammaRatios);
    return alphaCorrected * foregroundColor;
}

// Call this function to get the same gamma corrected alpha blending effect
// as DirectWrite's native algorithm for ClearType anti-aliased glyphs.
//
// The result is a color value with alpha = 1, resulting out of the blending
// of foregroundColor and backgroundColor using glyphColor to do sub-pixel AA.
//
// gammaRatios:
//   Magic constants produced by DWrite_GetGammaRatios() in dwrite.cpp.
//   The default value for this are the 1.8 gamma ratios, which equates to:
//     0.148054421f, -0.894594550f, 1.47590804f, -0.324668258f
// enhancedContrast:
//   An additional contrast boost, making the font lighter/darker.
//   The default value for this is 0.5f.
//   This value should be set to the return value of DWrite_GetRenderParams() or (pseudo-code):
//     IDWriteRenderingParams* defaultParams;
//     dwriteFactory->CreateRenderingParams(&defaultParams);
//     gamma = defaultParams->GetEnhancedContrast();
// isThinFont:
//   This constant is true for certain fonts that are simply too thin for AA.
//   Unlike the previous two values, this value isn't a constant and can change per font family.
//   If you only use modern fonts (like Roboto) you can safely assume that it's false.
//   If you draw your glyph atlas with any DirectWrite method except IDWriteGlyphRunAnalysis::CreateAlphaTexture
//   then you must set this to false as well, as not even tricks like setting the
//   gamma to 1.0 disables the internal thin-font contrast-boost inside DirectWrite.
//   Applying the contrast-boost twice would then look incorrectly.
// backgroundColor:
//   The background color in premultiplied alpha (the color behind the text pixel).
// foregroundColor:
//   The text's foreground color in premultiplied alpha.
// glyphAlpha:
//   The RGB color of the current glyph pixel in your texture atlas.
//   The A value is ignored, because ClearType doesn't work with alpha blending.
//   RGB is required because ClearType performs sub-pixel AA. The most common ClearType drawing type is 6x1
//   overscale (meaning: the glyph is rasterized with 6x the required resolution in the X axis) and thus
//   only 7 different RGB combinations can exist in this texture (black/white and 5 states in between).
//   If you wanted to you could just store these in a A8 texture and restore the RGB values in this shader.
float4 DWrite_ClearTypeBlend(float4 gammaRatios, float enhancedContrast, bool isThinFont, float4 backgroundColor, float4 foregroundColor, float4 glyphColor)
{
    float3 foregroundStraight = DWrite_UnpremultiplyColor(foregroundColor);
    float contrastBoost = isThinFont ? 0.5f : 0.0f;
    float blendEnhancedContrast = contrastBoost + DWrite_ApplyLightOnDarkContrastAdjustment(enhancedContrast, foregroundStraight);
    float3 contrasted = DWrite_EnhanceContrast3(glyphColor.rgb, blendEnhancedContrast);
    float3 alphaCorrected = DWrite_ApplyAlphaCorrection3(contrasted, foregroundStraight, gammaRatios);
    return float4(lerp(backgroundColor.rgb, foregroundStraight, alphaCorrected * foregroundColor.a), 1.0f);
}
