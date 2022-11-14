// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// A lot of code was taken from
// https://github.com/Maximus5/ConEmu/blob/master/src/ConEmu/ColorFix.cpp
// and then adjusted to fit our style guidelines

#include "precomp.h"

#include "inc/ColorFix.hpp"

TIL_FAST_MATH_BEGIN

static constexpr float gMinThreshold = 12.0f;
static constexpr float gExpThreshold = 20.0f;
static constexpr float gLStep = 5.0f;

static constexpr float rad006 = 0.104719755119659774f;
static constexpr float rad025 = 0.436332312998582394f;
static constexpr float rad030 = 0.523598775598298873f;
static constexpr float rad060 = 1.047197551196597746f;
static constexpr float rad063 = 1.099557428756427633f;
static constexpr float rad180 = 3.141592653589793238f;
static constexpr float rad275 = 4.799655442984406336f;
static constexpr float rad360 = 6.283185307179586476f;

ColorFix::ColorFix(COLORREF color) noexcept
{
    rgb = color;
    _ToLab();
}

// Method Description:
// - Helper function to calculate HPrime
float ColorFix::_GetHPrimeFn(float x, float y) noexcept
{
    if (x == 0 && y == 0)
    {
        return 0;
    }

    const auto hueAngle = atan2(x, y);
    return hueAngle >= 0 ? hueAngle : hueAngle + rad360;
}

// Method Description:
// - Given 2 colors, computes the DeltaE value between them
// Arguments:
// - x1: the first color
// - x2: the second color
// Return Value:
// - The DeltaE value between x1 and x2
float ColorFix::_GetDeltaE(ColorFix x1, ColorFix x2) noexcept
{
    constexpr float kSubL = 1;
    constexpr float kSubC = 1;
    constexpr float kSubH = 1;

    // Delta L Prime
    const auto deltaLPrime = x2.L - x1.L;

    // L Bar
    const auto lBar = (x1.L + x2.L) / 2;

    // C1 & C2
    const auto c1 = sqrtf(powf(x1.A, 2) + powf(x1.B, 2));
    const auto c2 = sqrtf(powf(x2.A, 2) + powf(x2.B, 2));

    // C Bar
    const auto cBar = (c1 + c2) / 2;

    // A Prime 1
    const auto aPrime1 = x1.A + (x1.A / 2) * (1 - sqrtf(powf(cBar, 7) / (powf(cBar, 7) + powf(25.0f, 7))));

    // A Prime 2
    const auto aPrime2 = x2.A + (x2.A / 2) * (1 - sqrtf(powf(cBar, 7) / (powf(cBar, 7) + powf(25.0f, 7))));

    // C Prime 1
    const auto cPrime1 = sqrtf(powf(aPrime1, 2) + powf(x1.B, 2));

    // C Prime 2
    const auto cPrime2 = sqrtf(powf(aPrime2, 2) + powf(x2.B, 2));

    // C Bar Prime
    const auto cBarPrime = (cPrime1 + cPrime2) / 2;

    // Delta C Prime
    const auto deltaCPrime = cPrime2 - cPrime1;

    // S sub L
    const auto sSubL = 1 + ((0.015f * powf(lBar - 50, 2)) / sqrtf(20 + powf(lBar - 50, 2)));

    // S sub C
    const auto sSubC = 1 + 0.045f * cBarPrime;

    // h Prime 1
    const auto hPrime1 = _GetHPrimeFn(x1.B, aPrime1);

    // h Prime 2
    const auto hPrime2 = _GetHPrimeFn(x2.B, aPrime2);

    // Delta H Prime
    const auto deltaHPrime = 0 == c1 || 0 == c2 ? 0 : 2 * sqrtf(cPrime1 * cPrime2) * sin(abs(hPrime1 - hPrime2) <= rad180 ? hPrime2 - hPrime1 : (hPrime2 <= hPrime1 ? hPrime2 - hPrime1 + rad360 : hPrime2 - hPrime1 - rad360) / 2);

    // H Bar Prime
    const auto hBarPrime = (abs(hPrime1 - hPrime2) > rad180) ? (hPrime1 + hPrime2 + rad360) / 2 : (hPrime1 + hPrime2) / 2;

    // T
    const auto t = 1 - 0.17f * cosf(hBarPrime - rad030) + 0.24f * cosf(2 * hBarPrime) + 0.32f * cosf(3 * hBarPrime + rad006) - 0.20f * cosf(4 * hBarPrime - rad063);

    // S sub H
    const auto sSubH = 1 + 0.015f * cBarPrime * t;

    // R sub T
    const auto rSubT = -2 * sqrtf(powf(cBarPrime, 7) / (powf(cBarPrime, 7) + powf(25.0f, 7))) * sin(rad060 * exp(-powf((hBarPrime - rad275) / rad025, 2)));

    // Put it all together!
    const auto lightness = deltaLPrime / (kSubL * sSubL);
    const auto chroma = deltaCPrime / (kSubC * sSubC);
    const auto hue = deltaHPrime / (kSubH * sSubH);

    return sqrtf(powf(lightness, 2) + powf(chroma, 2) + powf(hue, 2) + rSubT * chroma * hue);
}

// Method Description:
// - Populates our L, A, B values, based on our r, g, b values
// - Converts a color in rgb format to a color in lab format
// - Reference: http://www.easyrgb.com/index.php?X=MATH&H=01#text1
void ColorFix::_ToLab() noexcept
{
    auto var_R = r / 255.0f;
    auto var_G = g / 255.0f;
    auto var_B = b / 255.0f;

    var_R = var_R > 0.04045f ? powf(((var_R + 0.055f) / 1.055f), 2.4f) : var_R / 12.92f;
    var_G = var_G > 0.04045f ? powf(((var_G + 0.055f) / 1.055f), 2.4f) : var_G / 12.92f;
    var_B = var_B > 0.04045f ? powf(((var_B + 0.055f) / 1.055f), 2.4f) : var_B / 12.92f;

    var_R = var_R * 100.0f;
    var_G = var_G * 100.0f;
    var_B = var_B * 100.0f;

    //Observer. = 2 degrees, Illuminant = D65
    const auto X = var_R * 0.4124f + var_G * 0.3576f + var_B * 0.1805f;
    const auto Y = var_R * 0.2126f + var_G * 0.7152f + var_B * 0.0722f;
    const auto Z = var_R * 0.0193f + var_G * 0.1192f + var_B * 0.9505f;

    auto var_X = X / 95.047f; //ref_X =  95.047   (Observer= 2 degrees, Illuminant= D65)
    auto var_Y = Y / 100.000f; //ref_Y = 100.000
    auto var_Z = Z / 108.883f; //ref_Z = 108.883

    var_X = var_X > 0.008856f ? powf(var_X, (1.0f / 3.0f)) : (7.787f * var_X) + (16.0f / 116.0f);
    var_Y = var_Y > 0.008856f ? powf(var_Y, (1.0f / 3.0f)) : (7.787f * var_Y) + (16.0f / 116.0f);
    var_Z = var_Z > 0.008856f ? powf(var_Z, (1.0f / 3.0f)) : (7.787f * var_Z) + (16.0f / 116.0f);

    L = (116.0f * var_Y) - 16.0f;
    A = 500.0f * (var_X - var_Y);
    B = 200.0f * (var_Y - var_Z);
}

// Method Description:
// - Populates our r, g, b values, based on our L, A, B values
// - Converts a color in lab format to a color in rgb format
// - Reference: http://www.easyrgb.com/index.php?X=MATH&H=01#text1
void ColorFix::_ToRGB()
{
    auto var_Y = (L + 16.0f) / 116.0f;
    auto var_X = A / 500.0f + var_Y;
    auto var_Z = var_Y - B / 200.0f;

    var_Y = (powf(var_Y, 3) > 0.008856f) ? powf(var_Y, 3) : (var_Y - 16.0f / 116.0f) / 7.787f;
    var_X = (powf(var_X, 3) > 0.008856f) ? powf(var_X, 3) : (var_X - 16.0f / 116.0f) / 7.787f;
    var_Z = (powf(var_Z, 3) > 0.008856f) ? powf(var_Z, 3) : (var_Z - 16.0f / 116.0f) / 7.787f;

    const auto X = 95.047f * var_X; //ref_X =  95.047     (Observer= 2 degrees, Illuminant= D65)
    const auto Y = 100.000f * var_Y; //ref_Y = 100.000
    const auto Z = 108.883f * var_Z; //ref_Z = 108.883

    var_X = X / 100.0f; //X from 0 to  95.047      (Observer = 2 degrees, Illuminant = D65)
    var_Y = Y / 100.0f; //Y from 0 to 100.000
    var_Z = Z / 100.0f; //Z from 0 to 108.883

    auto var_R = var_X * 3.2406f + var_Y * -1.5372f + var_Z * -0.4986f;
    auto var_G = var_X * -0.9689f + var_Y * 1.8758f + var_Z * 0.0415f;
    auto var_B = var_X * 0.0557f + var_Y * -0.2040f + var_Z * 1.0570f;

    var_R = var_R > 0.0031308f ? 1.055f * powf(var_R, (1 / 2.4f)) - 0.055f : var_R = 12.92f * var_R;
    var_G = var_G > 0.0031308f ? 1.055f * powf(var_G, (1 / 2.4f)) - 0.055f : var_G = 12.92f * var_G;
    var_B = var_B > 0.0031308f ? 1.055f * powf(var_B, (1 / 2.4f)) - 0.055f : var_B = 12.92f * var_B;

    r = (BYTE)std::clamp(var_R * 255.0f, 0.0f, 255.0f);
    g = (BYTE)std::clamp(var_G * 255.0f, 0.0f, 255.0f);
    b = (BYTE)std::clamp(var_B * 255.0f, 0.0f, 255.0f);
}

// Method Description:
// - Given foreground and background colors, change the foreground color to
//   make it more perceivable if necessary
// - Arguments:
// - fg: the foreground color
// - bg: the background color
// - Return Value:
// - The foreground color after performing any necessary changes to make it more perceivable
COLORREF ColorFix::GetPerceivableColor(COLORREF fg, COLORREF bg)
{
    const ColorFix backLab(bg);
    ColorFix frontLab(fg);
    const auto de1 = _GetDeltaE(frontLab, backLab);
    if (de1 < gMinThreshold)
    {
        for (auto i = 0; i <= 1; i++)
        {
            const auto step = (i == 0) ? gLStep : -gLStep;
            frontLab.L += step;

            while (((i == 0) && (frontLab.L <= 100)) || ((i == 1) && (frontLab.L >= 0)))
            {
                const auto de2 = _GetDeltaE(frontLab, backLab);
                if (de2 >= gExpThreshold)
                {
                    frontLab._ToRGB();
                    return frontLab.rgb;
                }
                frontLab.L += step;
            }
        }
    }
    return frontLab.rgb;
}

TIL_FAST_MATH_END
