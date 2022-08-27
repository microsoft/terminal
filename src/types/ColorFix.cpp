// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// A lot of code was taken from
// https://github.com/Maximus5/ConEmu/blob/master/src/ConEmu/ColorFix.cpp
// and then adjusted to fit our style guidelines

#include "precomp.h"

#include "inc/ColorFix.hpp"

static constexpr double gMinThreshold = 12.0;
static constexpr double gExpThreshold = 20.0;
static constexpr double gLStep = 5.0;

static constexpr double rad006 = 0.104719755119659774;
static constexpr double rad025 = 0.436332312998582394;
static constexpr double rad030 = 0.523598775598298873;
static constexpr double rad060 = 1.047197551196597746;
static constexpr double rad063 = 1.099557428756427633;
static constexpr double rad180 = 3.141592653589793238;
static constexpr double rad275 = 4.799655442984406336;
static constexpr double rad360 = 6.283185307179586476;

ColorFix::ColorFix(COLORREF color) noexcept
{
    rgb = color;
    _ToLab();
}

// Method Description:
// - Helper function to calculate HPrime
double ColorFix::_GetHPrimeFn(double x, double y) noexcept
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
double ColorFix::_GetDeltaE(ColorFix x1, ColorFix x2) noexcept
{
    constexpr double kSubL = 1;
    constexpr double kSubC = 1;
    constexpr double kSubH = 1;

    // Delta L Prime
    const auto deltaLPrime = x2.L - x1.L;

    // L Bar
    const auto lBar = (x1.L + x2.L) / 2;

    // C1 & C2
    const auto c1 = sqrt(pow(x1.A, 2) + pow(x1.B, 2));
    const auto c2 = sqrt(pow(x2.A, 2) + pow(x2.B, 2));

    // C Bar
    const auto cBar = (c1 + c2) / 2;

    // A Prime 1
    const auto aPrime1 = x1.A + (x1.A / 2) * (1 - sqrt(pow(cBar, 7) / (pow(cBar, 7) + pow(25.0, 7))));

    // A Prime 2
    const auto aPrime2 = x2.A + (x2.A / 2) * (1 - sqrt(pow(cBar, 7) / (pow(cBar, 7) + pow(25.0, 7))));

    // C Prime 1
    const auto cPrime1 = sqrt(pow(aPrime1, 2) + pow(x1.B, 2));

    // C Prime 2
    const auto cPrime2 = sqrt(pow(aPrime2, 2) + pow(x2.B, 2));

    // C Bar Prime
    const auto cBarPrime = (cPrime1 + cPrime2) / 2;

    // Delta C Prime
    const auto deltaCPrime = cPrime2 - cPrime1;

    // S sub L
    const auto sSubL = 1 + ((0.015 * pow(lBar - 50, 2)) / sqrt(20 + pow(lBar - 50, 2)));

    // S sub C
    const auto sSubC = 1 + 0.045 * cBarPrime;

    // h Prime 1
    const auto hPrime1 = _GetHPrimeFn(x1.B, aPrime1);

    // h Prime 2
    const auto hPrime2 = _GetHPrimeFn(x2.B, aPrime2);

    // Delta H Prime
    const auto deltaHPrime = 0 == c1 || 0 == c2 ? 0 : 2 * sqrt(cPrime1 * cPrime2) * sin(abs(hPrime1 - hPrime2) <= rad180 ? hPrime2 - hPrime1 : (hPrime2 <= hPrime1 ? hPrime2 - hPrime1 + rad360 : hPrime2 - hPrime1 - rad360) / 2);

    // H Bar Prime
    const auto hBarPrime = (abs(hPrime1 - hPrime2) > rad180) ? (hPrime1 + hPrime2 + rad360) / 2 : (hPrime1 + hPrime2) / 2;

    // T
    const auto t = 1 - 0.17 * cos(hBarPrime - rad030) + 0.24 * cos(2 * hBarPrime) + 0.32 * cos(3 * hBarPrime + rad006) - 0.20 * cos(4 * hBarPrime - rad063);

    // S sub H
    const auto sSubH = 1 + 0.015 * cBarPrime * t;

    // R sub T
    const auto rSubT = -2 * sqrt(pow(cBarPrime, 7) / (pow(cBarPrime, 7) + pow(25.0, 7))) * sin(rad060 * exp(-pow((hBarPrime - rad275) / rad025, 2)));

    // Put it all together!
    const auto lightness = deltaLPrime / (kSubL * sSubL);
    const auto chroma = deltaCPrime / (kSubC * sSubC);
    const auto hue = deltaHPrime / (kSubH * sSubH);

    return sqrt(pow(lightness, 2) + pow(chroma, 2) + pow(hue, 2) + rSubT * chroma * hue);
}

// Method Description:
// - Populates our L, A, B values, based on our r, g, b values
// - Converts a color in rgb format to a color in lab format
// - Reference: http://www.easyrgb.com/index.php?X=MATH&H=01#text1
void ColorFix::_ToLab() noexcept
{
    auto var_R = r / 255.0;
    auto var_G = g / 255.0;
    auto var_B = b / 255.0;

    var_R = var_R > 0.04045 ? pow(((var_R + 0.055) / 1.055), 2.4) : var_R / 12.92;
    var_G = var_G > 0.04045 ? pow(((var_G + 0.055) / 1.055), 2.4) : var_G / 12.92;
    var_B = var_B > 0.04045 ? pow(((var_B + 0.055) / 1.055), 2.4) : var_B / 12.92;

    var_R = var_R * 100.;
    var_G = var_G * 100.;
    var_B = var_B * 100.;

    //Observer. = 2 degrees, Illuminant = D65
    const auto X = var_R * 0.4124 + var_G * 0.3576 + var_B * 0.1805;
    const auto Y = var_R * 0.2126 + var_G * 0.7152 + var_B * 0.0722;
    const auto Z = var_R * 0.0193 + var_G * 0.1192 + var_B * 0.9505;

    auto var_X = X / 95.047; //ref_X =  95.047   (Observer= 2 degrees, Illuminant= D65)
    auto var_Y = Y / 100.000; //ref_Y = 100.000
    auto var_Z = Z / 108.883; //ref_Z = 108.883

    var_X = var_X > 0.008856 ? pow(var_X, (1. / 3.)) : (7.787 * var_X) + (16. / 116.);
    var_Y = var_Y > 0.008856 ? pow(var_Y, (1. / 3.)) : (7.787 * var_Y) + (16. / 116.);
    var_Z = var_Z > 0.008856 ? pow(var_Z, (1. / 3.)) : (7.787 * var_Z) + (16. / 116.);

    L = (116. * var_Y) - 16.;
    A = 500. * (var_X - var_Y);
    B = 200. * (var_Y - var_Z);
}

// Method Description:
// - Populates our r, g, b values, based on our L, A, B values
// - Converts a color in lab format to a color in rgb format
// - Reference: http://www.easyrgb.com/index.php?X=MATH&H=01#text1
void ColorFix::_ToRGB()
{
    auto var_Y = (L + 16.) / 116.;
    auto var_X = A / 500. + var_Y;
    auto var_Z = var_Y - B / 200.;

    var_Y = (pow(var_Y, 3) > 0.008856) ? pow(var_Y, 3) : (var_Y - 16. / 116.) / 7.787;
    var_X = (pow(var_X, 3) > 0.008856) ? pow(var_X, 3) : (var_X - 16. / 116.) / 7.787;
    var_Z = (pow(var_Z, 3) > 0.008856) ? pow(var_Z, 3) : (var_Z - 16. / 116.) / 7.787;

    const auto X = 95.047 * var_X; //ref_X =  95.047     (Observer= 2 degrees, Illuminant= D65)
    const auto Y = 100.000 * var_Y; //ref_Y = 100.000
    const auto Z = 108.883 * var_Z; //ref_Z = 108.883

    var_X = X / 100.; //X from 0 to  95.047      (Observer = 2 degrees, Illuminant = D65)
    var_Y = Y / 100.; //Y from 0 to 100.000
    var_Z = Z / 100.; //Z from 0 to 108.883

    auto var_R = var_X * 3.2406 + var_Y * -1.5372 + var_Z * -0.4986;
    auto var_G = var_X * -0.9689 + var_Y * 1.8758 + var_Z * 0.0415;
    auto var_B = var_X * 0.0557 + var_Y * -0.2040 + var_Z * 1.0570;

    var_R = var_R > 0.0031308 ? 1.055 * pow(var_R, (1 / 2.4)) - 0.055 : var_R = 12.92 * var_R;
    var_G = var_G > 0.0031308 ? 1.055 * pow(var_G, (1 / 2.4)) - 0.055 : var_G = 12.92 * var_G;
    var_B = var_B > 0.0031308 ? 1.055 * pow(var_B, (1 / 2.4)) - 0.055 : var_B = 12.92 * var_B;

    r = (BYTE)std::clamp(var_R * 255., 0., 255.);
    g = (BYTE)std::clamp(var_G * 255., 0., 255.);
    b = (BYTE)std::clamp(var_B * 255., 0., 255.);
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
