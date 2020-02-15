// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "SolidBrushCache.h"

HBRUSH SolidBrushCache::MakeOrGetHandle(COLORREF color)
{
    if (!_handle || _currentColor != color)
    {
        HBRUSH ret = ::CreateSolidBrush(color);
        if (ret == NULL)
        {
            LOG_LAST_ERROR();
            return reinterpret_cast<HBRUSH>(INVALID_HANDLE_VALUE);
        }

        _handle = wil::unique_hbrush(ret);
        _currentColor = color;
    }

    return _handle.get();
}
