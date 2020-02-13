#pragma once

#include "pch.h"

// Class Description:
// - A safe GDI solid brush wrapper that allows the brush's color to be
//   modified by recreating the brush.
class GdiSolidBrush
{
public:
    GdiSolidBrush();

    void SetColor(COLORREF color);

    HBRUSH GetHandle() const;

private:
    wil::unique_hbrush _handle;
    COLORREF _currentColor = 0xFFFFFFFF;
};
