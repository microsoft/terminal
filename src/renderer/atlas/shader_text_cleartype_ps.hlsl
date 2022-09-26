// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "dwrite.hlsl"
#include "shader_common.hlsl"

cbuffer ConstBuffer : register(b0)
{
    float4 positionScale;
    float4 gammaRatios;
    float cleartypeEnhancedContrast;
    float grayscaleEnhancedContrast;
}

Texture2D<float4> glyphAtlas : register(t0);

struct Output
{
    float4 color;
    float4 weights;
};

// clang-format off
Output main(PSData data) : SV_Target
// clang-format on
{
    switch (data.shadingType)
    {
    case 0:
    {
        float4 glyph = glyphAtlas[data.texcoord];

        float blendEnhancedContrast = DWrite_ApplyLightOnDarkContrastAdjustment(cleartypeEnhancedContrast, data.color.rgb);
        float3 contrasted = DWrite_EnhanceContrast3(glyph.rgb, blendEnhancedContrast);
        float3 alphaCorrected = DWrite_ApplyAlphaCorrection3(contrasted, data.color.rgb, gammaRatios);
        float4 weights = float4(alphaCorrected * data.color.a, 1);

        // The final step of the ClearType blending algorithm is a lerp() between the premultiplied alpha
        // background color and straight alpha foreground color given the 3 RGB weights in alphaCorrected:
        //   lerp(background, foreground, weights)
        // Which is equivalent to:
        //   background * (1 - weights) + foreground * weights
        // This can be implemented using dual source color blending like so:
        //   .SrcBlend = D3D11_BLEND_SRC1_COLOR,
        //   .DestBlend = D3D11_BLEND_INV_SRC1_COLOR,
        //   .BlendOp = D3D11_BLEND_OP_ADD,
        //
        // But we need simultaneous support for regular "source over" alpha blending
        // (shadingType = 1, for grayscale AA Emoji support) which looks like this:
        //   background * (1 - alpha) + foreground
        //
        // Clearly the lerp() and source over blending are fairly close with the latter
        // being the common denomiator and so the blend state has been set up as:
        //   .SrcBlend = D3D11_BLEND_ONE,
        //   .DestBlend = D3D11_BLEND_INV_SRC1_COLOR,
        //   .BlendOp = D3D11_BLEND_OP_ADD,
        //
        // ---> We need to multiply the foreground with the weights ourselves.
        Output output;
        output.color = weights * data.color;
        output.weights = weights;
        return output;
    }
    case 1:
    {
        float4 glyph = glyphAtlas[data.texcoord];

        Output output;
        output.color = glyph;
        output.weights = glyph.aaaa;
        return output;
    }
    default:
    {
        Output output;
        output.color = premultiplyColor(data.color);
        output.weights = data.color.aaaa;
        return output;
    }
    }
}
