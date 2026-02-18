// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.
//
// Implementation of perceptual color nudging, which allows the Terminal
// to slightly shift the foreground color to make it more perceivable on
// the current background (for cases where the foreground is very close
// to being imperceivable on the background).
#pragma once

namespace ColorFix
{
    union RGB
    {
        struct
        {
            float r;
            float g;
            float b;
            float _; // Padding, to align it to 16 bytes for vectorization
        };
        float data[4];
    };

    union Lab
    {
        struct
        {
            float l;
            float a;
            float b;
            float _;
        };
        float data[4];
    };

    Lab ColorrefToOklab(COLORREF color) noexcept;
    COLORREF OklabToColorref(const Lab& color) noexcept;
    COLORREF GetPerceivableColor(COLORREF color, COLORREF reference, float minSquaredDistance) noexcept;
    COLORREF AdjustLightness(COLORREF color, float delta) noexcept;
    float GetLightness(COLORREF color) noexcept;
}
