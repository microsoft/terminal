/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SolidBrushCache.h

Abstract:
- Make a GDI solid brush with a color, cache it. The next time, if a brush
  with the same color is requested, return the brush from the cache.
--*/

#pragma once
#include "pch.h"

class SolidBrushCache
{
public:
    HBRUSH MakeOrGetHandle(COLORREF color);

private:
    wil::unique_hbrush _handle;
    COLORREF _currentColor = 0x00000000;
};
