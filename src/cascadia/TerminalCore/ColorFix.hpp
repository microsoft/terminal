// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// We probably want to include something else here, whatever gives us COLORREF and BYTE
#include "../../terminal/input/terminalInput.hpp"

struct ColorFix
{
public:
    ColorFix(COLORREF color);

    static double GetDeltaE(ColorFix x1, ColorFix x2);
    static COLORREF GetPerceivableColor(COLORREF fg, COLORREF bg);

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

private:
    static double _GetHPrimeFn(double x, double y);
    void _ToLab();
    void _ToRGB();
    BYTE _Clamp(double v);
};
