/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- argb.h

Abstract:
- Replaces the RGB macro with one that fills the highest order byte with 0xff.
    We use this for the cascadia project, because it can have colors with an
    alpha component. For code that is alpha-aware, include this header to make
    RGB() fill the alpha byte. Otherwise, colors made with RGB will be transparent.
Author(s):
- Mike Griese (migrie) Feb 2019
--*/
#pragma once

constexpr COLORREF ARGB(const BYTE a, const BYTE r, const BYTE g, const BYTE b) noexcept
{
    return (a << 24) | (b << 16) | (g << 8) | (r);
}

#ifdef RGB
#undef RGB
#define RGB(r, g, b) (ARGB(255, (r), (g), (b)))
#endif
