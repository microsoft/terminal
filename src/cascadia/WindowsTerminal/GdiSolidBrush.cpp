// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "GdiSolidBrush.h"

GdiSolidBrush::GdiSolidBrush()
{
    SetColor(RGB(0, 0, 0));
}

void GdiSolidBrush::SetColor(COLORREF color)
{
    if (_currentColor == color)
    {
        return;
    }

    HBRUSH ret = ::CreateSolidBrush(color);
    if (ret == NULL)
    {
        LOG_LAST_ERROR();
        return;
    }

    _handle = wil::unique_hbrush(ret);
    _currentColor = color;
}

HBRUSH GdiSolidBrush::GetHandle() const
{
    return _handle.get();
}
