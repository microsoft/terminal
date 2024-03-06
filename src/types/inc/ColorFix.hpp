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
    COLORREF GetPerceivableColor(COLORREF color, COLORREF reference, float minSquaredDistance) noexcept;
}
