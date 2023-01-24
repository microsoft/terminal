/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ColorFix

Abstract:
- Implementation of perceptual color nudging, which allows the Terminal
  to slightly shift the foreground color to make it more perceivable on
  the current background (for cases where the foreground is very close
  to being imperceivable on the background).

Author(s):
- Pankaj Bhojwani - Sep 2021

--*/

#pragma once

struct ColorFix
{
public:
    ColorFix(COLORREF color) noexcept;

    static COLORREF GetPerceivableColor(COLORREF fg, COLORREF bg);

#pragma warning(push)
    // CL will complain about the both nameless and anonymous struct.
#pragma warning(disable : 4201)
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
        float L, A, B;
    };
#pragma warning(pop)

private:
    static float _GetHPrimeFn(float x, float y) noexcept;
    static float _GetDeltaE(ColorFix x1, ColorFix x2) noexcept;
    void _ToLab() noexcept;
    void _ToRGB();
};
