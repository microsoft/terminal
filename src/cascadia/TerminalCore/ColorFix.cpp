#include "pch.h"

#define _USE_MATH_DEFINES
#include <Windows.h>
#include <math.h>
#include "ColorFix.hpp"

const double gMinThreshold = 12.0;
const double gExpThreshold = 20.0;
const double gLStep = 5.0;

// DeltaE 2000
// Source: https://github.com/zschuessler/DeltaE
dE00::dE00(ColorFix x1, ColorFix x2, double weightLightness, double weightChroma, double weightHue)
{
    _x1 = x1;
    _x2 = x2;

    _kSubL = weightLightness;
    _kSubC = weightChroma;
    _kSubH = weightHue;

    // Delta L Prime
    _deltaLPrime = _x2.L - _x1.L;

    // L Bar
    _lBar = (_x1.L + _x2.L) / 2;

    // C1 & C2
    _c1 = sqrt(pow(_x1.A, 2) + pow(_x1.B, 2));
    _c2 = sqrt(pow(_x2.A, 2) + pow(_x2.B, 2));

    // C Bar
    _cBar = (_c1 + _c2) / 2;

    // A Prime 1
    _aPrime1 = _x1.A +
               (_x1.A / 2) *
                   (1 - sqrt(
                            pow(_cBar, 7) /
                            (pow(_cBar, 7) + pow((double)25, 7))));

    // A Prime 2
    _aPrime2 = _x2.A +
               (_x2.A / 2) *
                   (1 - sqrt(
                            pow(_cBar, 7) /
                            (pow(_cBar, 7) + pow((double)25, 7))));

    // C Prime 1
    _cPrime1 = sqrt(
        pow(_aPrime1, 2) +
        pow(_x1.B, 2));

    // C Prime 2
    _cPrime2 = sqrt(
        pow(_aPrime2, 2) +
        pow(_x2.B, 2));

    // C Bar Prime
    _cBarPrime = (_cPrime1 + _cPrime2) / 2;

    // Delta C Prime
    _deltaCPrime = _cPrime2 - _cPrime1;

    // S sub L
    _sSubL = 1 + ((0.015 * pow(_lBar - 50, 2)) /
                  sqrt(20 + pow(_lBar - 50, 2)));

    // S sub C
    _sSubC = 1 + 0.045 * _cBarPrime;
}

// Method Description:
// - Calculates and returns the deltaE value.
// Return Value:
// - The deltaE value
double dE00::GetDeltaE()
{
    // h Prime 1
    _hPrime1 = _GetHPrime1();

    // h Prime 2
    _hPrime2 = _GetHPrime2();

    // Delta H Prime
    _deltaHPrime = 2 * sqrt(_cPrime1 * _cPrime2) * sin(_DegreesToRadians(_GetDeltaHPrime()) / 2);

    // H Bar Prime
    _hBarPrime = _GetHBarPrime();

    // T
    _t = _GetT();

    // S sub H
    _sSubH = 1 + 0.015 * _cBarPrime * _t;

    // R sub T
    _rSubT = _GetRSubT();

    // Put it all together!
    double lightness = _deltaLPrime / (_kSubL * _sSubL);
    double chroma = _deltaCPrime / (_kSubC * _sSubC);
    double hue = _deltaHPrime / (_kSubH * _sSubH);

    return sqrt(
        pow(lightness, 2) +
        pow(chroma, 2) +
        pow(hue, 2) +
        _rSubT * chroma * hue);
};

// Method Description:
// - Calculates and returns the RT variable
// Return Value:
// - The RT variable
double dE00::_GetRSubT()
{
    return -2 *
           sqrt(
               pow(_cBarPrime, 7) /
               (pow(_cBarPrime, 7) + pow((double)25, 7))) *
           sin(_DegreesToRadians(
               60 *
               exp(
                   -(
                       pow(
                           (_hBarPrime - 275) / 25, 2)))));
};

// Method Description:
// - Calculates and returns the T variable
// Return Value:
// - The T variable
double dE00::_GetT()
{
    return 1 -
           0.17 * cos(_DegreesToRadians(_hBarPrime - 30)) +
           0.24 * cos(_DegreesToRadians(2 * _hBarPrime)) +
           0.32 * cos(_DegreesToRadians(3 * _hBarPrime + 6)) -
           0.20 * cos(_DegreesToRadians(4 * _hBarPrime - 63));
};

// Method Description:
// - Calculates and returns the HBarPrime variable
// Return Value:
// - The HBarPrime variable
double dE00::_GetHBarPrime()
{
    if (abs(_hPrime1 - _hPrime2) > 180)
    {
        return (_hPrime1 + _hPrime2 + 360) / 2;
    }

    return (_hPrime1 + _hPrime2) / 2;
};

// Method Description:
// - Calculates and returns the Delta h prime variable
// Return Value:
// - The Delta h prime variable
double dE00::_GetDeltaHPrime()
{
    // When either _c1 prime or _c2 prime is zero, then deltaH prime is irrelevant and may be set to
    // zero.
    if (0 == _c1 || 0 == _c2)
    {
        return 0;
    }

    if (abs(_hPrime1 - _hPrime2) <= 180)
    {
        return _hPrime2 - _hPrime1;
    }

    if (_hPrime2 <= _hPrime1)
    {
        return _hPrime2 - _hPrime1 + 360;
    }
    else
    {
        return _hPrime2 - _hPrime1 - 360;
    }
};

// Method Description:
// - Calculates and returns the h Prime 1 variable
// Return Value:
// - The h Prime 1 variable
double dE00::_GetHPrime1()
{
    return _GetHPrimeFn(_x1.B, _aPrime1);
}

// Method Description:
// - Calculates and returns the h Prime 2 variable
// Return Value:
// - The h Prime 2 variable
double dE00::_GetHPrime2()
{
    return _GetHPrimeFn(_x2.B, _aPrime2);
};

// Method Description:
// - Helper function to calculate the h Prime values
// Return Value:
// - The h Prime value
double dE00::_GetHPrimeFn(double x, double y)
{
    double hueAngle;

    if (x == 0 && y == 0)
    {
        return 0;
    }

    hueAngle = _RadiansToDegrees(atan2(x, y));

    if (hueAngle >= 0)
    {
        return hueAngle;
    }
    else
    {
        return hueAngle + 360;
    }
};

// Method Description:
// - Converts radians to degrees
// Arguments:
// - radians: the angle in radians
// Return Value:
// - the given angle, converted to degrees
double dE00::_RadiansToDegrees(double radians)
{
    return radians * (180 / M_PI);
};

// Method Description:
// - Converts degrees to radians
// Arguments:
// - degrees: the angle in degrees
// Return Value:
// - the given angle, converted to radians
double dE00::_DegreesToRadians(double degrees)
{
    return degrees * (M_PI / 180);
};

ColorFix::ColorFix()
{
    rgb = 0;
    L = 0;
    A = 0;
    B = 0;
}

ColorFix::ColorFix(COLORREF color)
{
    rgb = color;
    ToLab();
}

ColorFix::ColorFix(double l, double a, double b)
{
    L = l;
    A = a;
    B = b;
    ToRGB();
}

ColorFix::ColorFix(const ColorFix& color)
{
    L = color.L;
    A = color.A;
    B = color.B;
    rgb = color.rgb;
}

// Method Description:
// - Populates our L, A, B values, based on our r, g, b values
// - Converts a color in rgb format to a color in lab format
// - Reference: http://www.easyrgb.com/index.php?X=MATH&H=01#text1
void ColorFix::ToLab()
{
    //dE00::RGBToLAB(r, g, b, L, A, B);
    double var_R = r / 255.0;
    double var_G = g / 255.0;
    double var_B = b / 255.0;

    if (var_R > 0.04045)
        var_R = pow(((var_R + 0.055) / 1.055), 2.4);
    else
        var_R = var_R / 12.92;
    if (var_G > 0.04045)
        var_G = pow(((var_G + 0.055) / 1.055), 2.4);
    else
        var_G = var_G / 12.92;
    if (var_B > 0.04045)
        var_B = pow(((var_B + 0.055) / 1.055), 2.4);
    else
        var_B = var_B / 12.92;

    var_R = var_R * 100.;
    var_G = var_G * 100.;
    var_B = var_B * 100.;

    //Observer. = 2 degrees, Illuminant = D65
    double X = var_R * 0.4124 + var_G * 0.3576 + var_B * 0.1805;
    double Y = var_R * 0.2126 + var_G * 0.7152 + var_B * 0.0722;
    double Z = var_R * 0.0193 + var_G * 0.1192 + var_B * 0.9505;

    double var_X = X / 95.047; //ref_X =  95.047   (Observer= 2 degrees, Illuminant= D65)
    double var_Y = Y / 100.000; //ref_Y = 100.000
    double var_Z = Z / 108.883; //ref_Z = 108.883

    if (var_X > 0.008856)
        var_X = pow(var_X, (1. / 3.));
    else
        var_X = (7.787 * var_X) + (16. / 116.);
    if (var_Y > 0.008856)
        var_Y = pow(var_Y, (1. / 3.));
    else
        var_Y = (7.787 * var_Y) + (16. / 116.);
    if (var_Z > 0.008856)
        var_Z = pow(var_Z, (1. / 3.));
    else
        var_Z = (7.787 * var_Z) + (16. / 116.);

    L = (116. * var_Y) - 16.;
    A = 500. * (var_X - var_Y);
    B = 200. * (var_Y - var_Z);
}

// Method Description:
// - Populates our r, g, b values, based on our L, A, B values
// - Converts a color in lab format to a color in rgb format
// - Reference: http://www.easyrgb.com/index.php?X=MATH&H=01#text1
void ColorFix::ToRGB()
{
    double var_Y = (L + 16.) / 116.;
    double var_X = A / 500. + var_Y;
    double var_Z = var_Y - B / 200.;

    if (pow(var_Y, 3) > 0.008856)
        var_Y = pow(var_Y, 3);
    else
        var_Y = (var_Y - 16. / 116.) / 7.787;
    if (pow(var_X, 3) > 0.008856)
        var_X = pow(var_X, 3);
    else
        var_X = (var_X - 16. / 116.) / 7.787;
    if (pow(var_Z, 3) > 0.008856)
        var_Z = pow(var_Z, 3);
    else
        var_Z = (var_Z - 16. / 116.) / 7.787;

    double X = 95.047 * var_X; //ref_X =  95.047     (Observer= 2 degrees, Illuminant= D65)
    double Y = 100.000 * var_Y; //ref_Y = 100.000
    double Z = 108.883 * var_Z; //ref_Z = 108.883

    var_X = X / 100.; //X from 0 to  95.047      (Observer = 2 degrees, Illuminant = D65)
    var_Y = Y / 100.; //Y from 0 to 100.000
    var_Z = Z / 100.; //Z from 0 to 108.883

    double var_R = var_X * 3.2406 + var_Y * -1.5372 + var_Z * -0.4986;
    double var_G = var_X * -0.9689 + var_Y * 1.8758 + var_Z * 0.0415;
    double var_B = var_X * 0.0557 + var_Y * -0.2040 + var_Z * 1.0570;

    if (var_R > 0.0031308)
        var_R = 1.055 * pow(var_R, (1 / 2.4)) - 0.055;
    else
        var_R = 12.92 * var_R;
    if (var_G > 0.0031308)
        var_G = 1.055 * pow(var_G, (1 / 2.4)) - 0.055;
    else
        var_G = 12.92 * var_G;
    if (var_B > 0.0031308)
        var_B = 1.055 * pow(var_B, (1 / 2.4)) - 0.055;
    else
        var_B = 12.92 * var_B;

    r = _Clamp(var_R * 255.);
    g = _Clamp(var_G * 255.);
    b = _Clamp(var_B * 255.);
}

// Method Description:
// - Given a color, computes the DeltaE between us and that color
// - Arguments:
// - color: the color to compare against
// - Return Value:
// - The DeltaE value between us and that color
double ColorFix::DeltaE(ColorFix color)
{
    dE00 delta{ *this, color };
    return delta.GetDeltaE();
}

// Method Description:
// - Given a background color, change our own color to make it more perceivable if necessary
// - Arguments:
// - back: the color to compare against
// - pColor: where to store the resulting color
// - Return Value:
// - True if we changed our color, false otherwise
bool ColorFix::PerceivableColor(COLORREF back, ColorFix& pColor, double* oldDE, double* newDE)
{
    bool bChanged = false;
    ColorFix backLab(back);
    double de1 = DeltaE(backLab);
    if (oldDE)
        *oldDE = de1;
    if (newDE)
        *newDE = de1;
    if (de1 < gMinThreshold)
    {
        for (int i = 0; i <= 1; i++)
        {
            double step = (i == 0) ? gLStep : -gLStep;
            pColor.L = L + step;
            pColor.A = A;
            pColor.B = B;

            while (((i == 0) && (pColor.L <= 100)) || ((i == 1) && (pColor.L >= 0)))
            {
                double de2 = pColor.DeltaE(backLab);
                if (de2 >= gExpThreshold)
                {
                    if (newDE)
                        *newDE = de2;
                    bChanged = true;
                    goto wrap;
                }
                pColor.L += step;
            }
        }
    }
wrap:
    if (bChanged)
        pColor.ToRGB();
    else
        pColor = *this;
    return bChanged;
}

// Method Description:
// - Clamps the given value to be between 0-255 inclusive
// - Converts the result to BYTE
// Arguments:
// - v: the value to clamp
// Return Value:
// - The clamped value
BYTE ColorFix::_Clamp(double v)
{
    if (v <= 0)
        return 0;
    else if (v >= 255)
        return 255;
    else
        return (BYTE)v;
}
