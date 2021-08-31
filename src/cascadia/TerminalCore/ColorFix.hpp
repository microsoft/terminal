// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// We probably want to include something else here, whatever gives us COLORREF and BYTE
#include "../../terminal/input/terminalInput.hpp"

struct ColorFix
{
    ColorFix();
    ColorFix(COLORREF a_rgb);
    ColorFix(double a_L, double a_a, double a_b);
    ColorFix(const ColorFix& clr);

    void ToLab();
    void ToRGB();

    double DeltaE(ColorFix color);

    bool PerceivableColor(COLORREF back /*, COLORREF alt*/, ColorFix& pColor, double* oldDE = NULL, double* newDE = NULL);

    // RGB
    union
    {
        struct
        {
            BYTE r, g, b, dummy;
        };
        COLORREF rgb;
    };

    // Lab
    struct
    {
        double L, A, B;
    };
};

struct dE00
{
public:
    dE00(ColorFix ax1, ColorFix ax2, double weight_lightness = 1, double weight_chroma = 1, double weight_hue = 1);
    double GetDeltaE();

private:
    double _GetRSubT();
    double _GetT();
    double _GetHBarPrime();
    double _GetDeltaHPrime();
    double _GetHPrime1();
    double _GetHPrime2();
    double _GetHPrimeFn(double x, double y);
    double _RadiansToDegrees(double radians);
    double _DegreesToRadians(double degrees);

    ColorFix _x1, _x2;
    double _kSubL, _kSubC, _kSubH;
    double _deltaLPrime, _lBar;
    double _c1, _c2, _cBar;
    double _aPrime1, _aPrime2;
    double _cPrime1, _cPrime2;
    double _cBarPrime, _deltaCPrime;
    double _sSubL, _sSubC;
    double _hPrime1, _hPrime2, _deltaHPrime;
    double _hBarPrime, _t, _sSubH, _rSubT;
};
