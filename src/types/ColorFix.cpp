// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/ColorFix.hpp"

// This table contains a direct mapping from 8-bit sRGB to linear RGB.
// It was generated using the following code:
//   #include <charconv>
//
//   int main() {
//       char buffer[32];
//
//       for (int i = 0; i < 256; ++i) {
//           auto v = i / 255.0f;
//           v = v <= 0.04045f ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f);
//
//           auto p = std::to_chars(&buffer[0], &buffer[32], v, std::chars_format::hex, -1).ptr;
//           strcpy_s(p, &buffer[32] - p, "f,");
//
//           if (i % 16 == 0) printf("\n    ");
//           printf("0x%-15s", &buffer[0]);
//       }
//
//       return 0;
//   }
//
// clang-format off
static constexpr float srgbToRgbLUT[256]{
    0x0.000000p+0f,  0x1.3e4568p-12f, 0x1.3e4568p-11f, 0x1.dd681cp-11f, 0x1.3e4568p-10f, 0x1.8dd6c2p-10f, 0x1.dd681cp-10f, 0x1.167cbap-9f,  0x1.3e4568p-9f,  0x1.660e16p-9f,  0x1.8dd6c2p-9f,  0x1.b6a31ap-9f,  0x1.e1e31ap-9f,  0x1.07c38cp-8f,  0x1.1fcc2cp-8f,  0x1.390ffap-8f,
    0x1.53936ep-8f,  0x1.6f5adep-8f,  0x1.8c6a92p-8f,  0x1.aac6c2p-8f,  0x1.ca7382p-8f,  0x1.eb74e0p-8f,  0x1.06e76ap-7f,  0x1.18c2a4p-7f,  0x1.2b4e06p-7f,  0x1.3e8b7cp-7f,  0x1.527cd6p-7f,  0x1.6723eep-7f,  0x1.7c8292p-7f,  0x1.929a86p-7f,  0x1.a96d8ep-7f,  0x1.c0fd62p-7f,
    0x1.d94bbep-7f,  0x1.f25a44p-7f,  0x1.061550p-6f,  0x1.135f3ep-6f,  0x1.210bb6p-6f,  0x1.2f1b8ap-6f,  0x1.3d8f84p-6f,  0x1.4c6866p-6f,  0x1.5ba6fap-6f,  0x1.6b4c02p-6f,  0x1.7b5840p-6f,  0x1.8bcc72p-6f,  0x1.9ca956p-6f,  0x1.adefaap-6f,  0x1.bfa020p-6f,  0x1.d1bb72p-6f,
    0x1.e44258p-6f,  0x1.f73582p-6f,  0x1.054ad2p-5f,  0x1.0f31b8p-5f,  0x1.194fccp-5f,  0x1.23a55ep-5f,  0x1.2e32c6p-5f,  0x1.38f85cp-5f,  0x1.43f678p-5f,  0x1.4f2d64p-5f,  0x1.5a9d76p-5f,  0x1.664700p-5f,  0x1.722a56p-5f,  0x1.7e47c6p-5f,  0x1.8a9fa2p-5f,  0x1.973238p-5f,
    0x1.a3ffdep-5f,  0x1.b108d4p-5f,  0x1.be4d6ep-5f,  0x1.cbcdfcp-5f,  0x1.d98ac8p-5f,  0x1.e7841ep-5f,  0x1.f5ba52p-5f,  0x1.0216cep-4f,  0x1.096f2ap-4f,  0x1.10e660p-4f,  0x1.187c94p-4f,  0x1.2031ecp-4f,  0x1.28068ap-4f,  0x1.2ffa94p-4f,  0x1.380e2cp-4f,  0x1.404176p-4f,
    0x1.489496p-4f,  0x1.5107aep-4f,  0x1.599ae0p-4f,  0x1.624e50p-4f,  0x1.6b2224p-4f,  0x1.741676p-4f,  0x1.7d2b6ap-4f,  0x1.866124p-4f,  0x1.8fb7c4p-4f,  0x1.992f6cp-4f,  0x1.a2c83ep-4f,  0x1.ac8258p-4f,  0x1.b65ddep-4f,  0x1.c05aeep-4f,  0x1.ca79aap-4f,  0x1.d4ba32p-4f,
    0x1.df1ca4p-4f,  0x1.e9a122p-4f,  0x1.f447d0p-4f,  0x1.ff10c2p-4f,  0x1.04fe0ep-3f,  0x1.0a8500p-3f,  0x1.101d46p-3f,  0x1.15c6f0p-3f,  0x1.1b820ap-3f,  0x1.214ea8p-3f,  0x1.272cd6p-3f,  0x1.2d1ca4p-3f,  0x1.331e20p-3f,  0x1.39315ap-3f,  0x1.3f5660p-3f,  0x1.458d46p-3f,
    0x1.4bd612p-3f,  0x1.5230d8p-3f,  0x1.589da0p-3f,  0x1.5f1c82p-3f,  0x1.65ad88p-3f,  0x1.6c50c2p-3f,  0x1.73063cp-3f,  0x1.79ce06p-3f,  0x1.80a82cp-3f,  0x1.8794bep-3f,  0x1.8e93c8p-3f,  0x1.95a55cp-3f,  0x1.9cc984p-3f,  0x1.a4004ep-3f,  0x1.ab49ccp-3f,  0x1.b2a606p-3f,
    0x1.ba1516p-3f,  0x1.c196f8p-3f,  0x1.c92bc0p-3f,  0x1.d0d37ep-3f,  0x1.d88e40p-3f,  0x1.e05c18p-3f,  0x1.e83d08p-3f,  0x1.f03120p-3f,  0x1.f83870p-3f,  0x1.002984p-2f,  0x1.044078p-2f,  0x1.08611ap-2f,  0x1.0c8b74p-2f,  0x1.10bf8ap-2f,  0x1.14fd64p-2f,  0x1.194506p-2f,
    0x1.1d967ap-2f,  0x1.21f1c2p-2f,  0x1.2656e8p-2f,  0x1.2ac5f0p-2f,  0x1.2f3ee2p-2f,  0x1.33c1c4p-2f,  0x1.384e9cp-2f,  0x1.3ce56ep-2f,  0x1.418644p-2f,  0x1.463124p-2f,  0x1.4ae610p-2f,  0x1.4fa512p-2f,  0x1.546e30p-2f,  0x1.59416ep-2f,  0x1.5e1ed2p-2f,  0x1.630666p-2f,
    0x1.67f830p-2f,  0x1.6cf430p-2f,  0x1.71fa6ep-2f,  0x1.770af2p-2f,  0x1.7c25c2p-2f,  0x1.814ae2p-2f,  0x1.867a5ap-2f,  0x1.8bb42ep-2f,  0x1.90f866p-2f,  0x1.964706p-2f,  0x1.9ba016p-2f,  0x1.a1039ap-2f,  0x1.a67198p-2f,  0x1.abea16p-2f,  0x1.b16d1ap-2f,  0x1.b6faa8p-2f,
    0x1.bc92cap-2f,  0x1.c23582p-2f,  0x1.c7e2d6p-2f,  0x1.cd9acep-2f,  0x1.d35d6cp-2f,  0x1.d92abap-2f,  0x1.df02bap-2f,  0x1.e4e574p-2f,  0x1.ead2ecp-2f,  0x1.f0cb28p-2f,  0x1.f6ce2ep-2f,  0x1.fcdc04p-2f,  0x1.017a5ap-1f,  0x1.048c1cp-1f,  0x1.07a34ep-1f,  0x1.0abff2p-1f,
    0x1.0de20cp-1f,  0x1.11099ep-1f,  0x1.1436acp-1f,  0x1.176936p-1f,  0x1.1aa140p-1f,  0x1.1ddecep-1f,  0x1.2121e2p-1f,  0x1.246a7cp-1f,  0x1.27b8a2p-1f,  0x1.2b0c56p-1f,  0x1.2e659ap-1f,  0x1.31c472p-1f,  0x1.3528dep-1f,  0x1.3892e2p-1f,  0x1.3c0282p-1f,  0x1.3f77bep-1f,
    0x1.42f29cp-1f,  0x1.46731cp-1f,  0x1.49f940p-1f,  0x1.4d850cp-1f,  0x1.511682p-1f,  0x1.54ada6p-1f,  0x1.584a7ap-1f,  0x1.5bed00p-1f,  0x1.5f953cp-1f,  0x1.63432ep-1f,  0x1.66f6d8p-1f,  0x1.6ab040p-1f,  0x1.6e6f66p-1f,  0x1.72344cp-1f,  0x1.75fef8p-1f,  0x1.79cf6ap-1f,
    0x1.7da5a4p-1f,  0x1.8181a8p-1f,  0x1.85637cp-1f,  0x1.894b20p-1f,  0x1.8d3896p-1f,  0x1.912be2p-1f,  0x1.952504p-1f,  0x1.992402p-1f,  0x1.9d28dcp-1f,  0x1.a13396p-1f,  0x1.a54430p-1f,  0x1.a95aaep-1f,  0x1.ad7714p-1f,  0x1.b19960p-1f,  0x1.b5c19ap-1f,  0x1.b9efc0p-1f,
    0x1.be23d8p-1f,  0x1.c25ddcp-1f,  0x1.c69ddep-1f,  0x1.cae3d2p-1f,  0x1.cf2fc6p-1f,  0x1.d381acp-1f,  0x1.d7d99ap-1f,  0x1.dc377ep-1f,  0x1.e09b70p-1f,  0x1.e5055ep-1f,  0x1.e9755cp-1f,  0x1.edeb5cp-1f,  0x1.f26772p-1f,  0x1.f6e98cp-1f,  0x1.fb71c0p-1f,  0x1.000000p+0f,
};
// clang-format on

TIL_FAST_MATH_BEGIN

constexpr float saturate(float f) noexcept
{
    return f < 0 ? 0 : (f > 1 ? 1 : f);
}

// FP32 is the very epitome of defined behavior.
#pragma warning(suppress : 26497) // You can attempt to make 'cbrtf_est' constexpr unless it contains any undefined behavior (f.4).)
__forceinline float cbrtf_est(float a) noexcept
{
    // We can't use std::bit_cast here, even though it exists exactly for this purpose.
    // MSVC is dumb and introduces function calls to std::bit_cast<uint32_t, float>
    // even at the highest optimization and inlining levels. What.
    union FP32
    {
        uint32_t u;
        float f;
    };

    // http://metamerist.com/cbrt/cbrt.htm showed a great estimator for the cube root:
    //   float_as_uint32_t / 3 + 709921077
    // It's similar to the well known "fast inverse square root" trick. Lots of numbers around 709921077 perform
    // at least equally well to 709921077, and it is unknown how and why 709921077 was chosen specifically.
    FP32 fp{ .f = a }; // evil floating point bit level hacking
    fp.u = fp.u / 3 + 709921077; // what the fuck?
    const auto x = fp.f;

    // One round of Newton's method. It follows the Wikipedia article at
    //   https://en.wikipedia.org/wiki/Cube_root#Numerical_methods
    // For `a`s in the range between 0 and 1, this results in a maximum error of
    // less than 6.7e-4f, which is not good, but good enough for us, because
    // we're not an image editor. The benefit is that it's really fast.
    return (1.0f / 3.0f) * (a / (x * x) + (x + x)); // 1st iteration
}

// This namespace contains functions and structures as defined by: https://bottosson.github.io/posts/oklab/
// It has been released into the public domain and is also available under an MIT license.
// The only change I made is to replace cbrtf() with cbrtf_est() to cut the CPU cost by a third.
namespace oklab
{
    struct Lab
    {
        float l;
        float a;
        float b;
    };

    struct RGB
    {
        float r;
        float g;
        float b;
    };

    __forceinline Lab linear_srgb_to_oklab(const RGB& c) noexcept
    {
        const auto l = 0.4122214708f * c.r + 0.5363325363f * c.g + 0.0514459929f * c.b;
        const auto m = 0.2119034982f * c.r + 0.6806995451f * c.g + 0.1073969566f * c.b;
        const auto s = 0.0883024619f * c.r + 0.2817188376f * c.g + 0.6299787005f * c.b;

        const auto l_ = cbrtf_est(l);
        const auto m_ = cbrtf_est(m);
        const auto s_ = cbrtf_est(s);

        return {
            0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_,
            1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_,
            0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_,
        };
    }

    __forceinline RGB oklab_to_linear_srgb(const Lab& c) noexcept
    {
        const auto l_ = c.l + 0.3963377774f * c.a + 0.2158037573f * c.b;
        const auto m_ = c.l - 0.1055613458f * c.a - 0.0638541728f * c.b;
        const auto s_ = c.l - 0.0894841775f * c.a - 1.2914855480f * c.b;

        const auto l = l_ * l_ * l_;
        const auto m = m_ * m_ * m_;
        const auto s = s_ * s_ * s_;

        return {
            +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
            -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
            -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s,
        };
    }
}

#pragma warning(push)
#pragma warning(disable : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
#pragma warning(disable : 26482) // Only index into arrays using constant expressions (bounds.2).

__forceinline oklab::RGB colorrefToLinear(COLORREF c) noexcept
{
    const auto r = srgbToRgbLUT[(c >> 0) & 0xff];
    const auto g = srgbToRgbLUT[(c >> 8) & 0xff];
    const auto b = srgbToRgbLUT[(c >> 16) & 0xff];
    return { r, g, b };
}

#pragma warning(pop)

__forceinline COLORREF linearToColorref(const oklab::RGB& c) noexcept
{
    auto r = saturate(c.r);
    auto g = saturate(c.g);
    auto b = saturate(c.b);
    r = (r > 0.0031308f ? (1.055f * powf(r, 1.0f / 2.4f) - 0.055f) : 12.92f * r) * 255.0f;
    g = (g > 0.0031308f ? (1.055f * powf(g, 1.0f / 2.4f) - 0.055f) : 12.92f * g) * 255.0f;
    b = (b > 0.0031308f ? (1.055f * powf(b, 1.0f / 2.4f) - 0.055f) : 12.92f * b) * 255.0f;
    return lrintf(r) | (lrintf(g) << 8) | (lrintf(b) << 16);
}

// This function changes `color` so that it is visually different
// enough from `reference` that it's (much more easily) readable.
// See /doc/color_nudging.html
COLORREF ColorFix::GetPerceivableColor(COLORREF color, COLORREF reference, float minSquaredDistance) noexcept
{
    const auto referenceOklab = oklab::linear_srgb_to_oklab(colorrefToLinear(reference));
    auto colorOklab = oklab::linear_srgb_to_oklab(colorrefToLinear(color));

    // To determine whether the two colors are too close to each other we use the ΔEOK metric
    // based on the Oklab color space. It's defined as the simple euclidean distance between.
    auto dl = referenceOklab.l - colorOklab.l;
    auto da = referenceOklab.a - colorOklab.a;
    auto db = referenceOklab.b - colorOklab.b;
    dl *= dl;
    da *= da;
    db *= db;

    const auto distance = dl + da + db;
    if (distance >= minSquaredDistance)
    {
        return color;
    }

    // Thanks to ΔEOK being the euclidean distance we can immediately compute the
    // minimum .l distance that's required for `distance` to be >= `minSquaredDistance`.
    auto deltaL = sqrtf(minSquaredDistance - da - db);

    // Try to retain the brightness relationship between `reference` and `color`.
    // If `color` is darker than `reference`, we should first try to make it even darker.
    if (colorOklab.l < referenceOklab.l)
    {
        deltaL = -deltaL;
    }

    // This primitive way of adjusting the lightness will result in gamut clipping. That's really not good
    // (it reduces the contrast significantly for some colors), but on the other hand, doing proper gamut
    // mapping is annoying and expensive. I was unable to find an easy and cheap (!) algorithm that would
    // change the chroma so that we're (mostly) back inside the gamut, but found none. I'm sure it's
    // possible to come up with something, but I left that as a future improvement.
    colorOklab.l = referenceOklab.l + deltaL;
    if (colorOklab.l < 0 || colorOklab.l > 1)
    {
        colorOklab.l = referenceOklab.l - deltaL;
    }

    return linearToColorref(oklab::oklab_to_linear_srgb(colorOklab)) | (color & 0xff000000);
}

TIL_FAST_MATH_END
