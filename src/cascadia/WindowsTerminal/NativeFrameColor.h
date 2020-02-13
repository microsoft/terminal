/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- NativeFrameColor.h

Abstract:
- This class determines the color used by the system to paint the window frame.
- It is useful if we need to draw our own frame but want to match the system's one.
--*/

#pragma once
#include "pch.h"

class NativeFrameColor
{
public:
    NativeFrameColor();

    COLORREF GetActiveColor() const;
    COLORREF GetInactiveColor() const;

    void Update();

private:
    wil::unique_hkey _dwmKey;

    std::optional<DWORD> _ReadDwmSetting(LPCWSTR key) const;

    COLORREF _activeColor;
};
