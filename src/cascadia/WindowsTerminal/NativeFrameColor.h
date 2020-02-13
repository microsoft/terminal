#pragma once

#include "pch.h"

// Class Description:
// - This class determines the color used by the system to paint the window frame.
// - It is useful if we need to draw our own frame but want to match the system's one.
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
