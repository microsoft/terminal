// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "NativeFrameColor.h"

namespace
{
    DWORD GetByte(DWORD value, int i)
    {
        const auto bitOffset = i * 8;
        return (value & (0xFF << bitOffset)) >> bitOffset;
    }

    int AlphaBlendComponent(int bg, int fg, float alpha)
    {
        return static_cast<int>(bg * (1.0f - alpha) + fg * alpha);
    }

    COLORREF AlphaBlendColor(COLORREF bg, COLORREF fg, float alpha)
    {
        return AlphaBlendComponent(GetByte(bg, 0), GetByte(fg, 0), alpha) | // red
               (AlphaBlendComponent(GetByte(bg, 1), GetByte(fg, 1), alpha) << 8) | // green
               (AlphaBlendComponent(GetByte(bg, 2), GetByte(fg, 2), alpha) << 16); // blue
    }

    COLORREF RgbToColorref(DWORD rgb)
    {
        return GetByte(rgb, 2) | // red
               (GetByte(rgb, 1) << 8) | // green
               (GetByte(rgb, 0) << 16); // blue
    }
}

NativeFrameColor::NativeFrameColor()
{
    LOG_IF_WIN32_ERROR(::RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\DWM", 0, KEY_READ, _dwmKey.addressof()));
    Update();
}

std::optional<COLORREF> NativeFrameColor::GetActiveColor() const
{
    return _activeColor;
}

std::optional<COLORREF> NativeFrameColor::GetInactiveColor() const
{
    // The real color is supposed to be transparent and is blended on top of
    // other windows. Because we can't render transparent colors easily, we
    // cheat and instead tell the render code to use a color that doesn't
    // look _too wrong_ by returning an empty value.
    // TODO (GH #4576): render the actual transparent color
    return std::nullopt;
}

void NativeFrameColor::Update()
{
    auto colorPrevalence = _ReadDwmSetting(L"ColorPrevalence").value_or(0);
    if (colorPrevalence)
    {
        // ---
        // The following algorithm was copied and adapted from Chromium's code.
        // ---

        auto colorizationColor = _ReadDwmSetting(L"ColorizationColor").value_or(0xC40078D7);
        auto colorizationColorBalance = _ReadDwmSetting(L"ColorizationColorBalance").value_or(89);

        // The accent border color is a linear blend between the colorization
        // color and the neutral #d9d9d9. colorization_color_balance is the
        // percentage of the colorization color in that blend.
        //
        // On Windows version 1611 colorizationColorBalance can be 0xfffffff3 if
        // the accent color is taken from the background and either the background
        // is a solid color or was just changed to a slideshow. It's unclear what
        // that value's supposed to mean, so change it to 80 to match Edge's
        // behavior.
        if (colorizationColorBalance > 100)
            colorizationColorBalance = 80;

        _activeColor = { AlphaBlendColor(0xD9D9D9,
                                         RgbToColorref(colorizationColor),
                                         colorizationColorBalance / 100.0f) };
    }
    else
    {
        // The real color is supposed to be transparent and is blended on top of
        // other windows. Because we can't render transparent colors easily, we
        // cheat and instead tell the render code to use a color that doesn't
        // look _too wrong_ by returning an empty value.
        // TODO (GH #4576): render the actual transparent color
        _activeColor = std::nullopt;
    }
}

std::optional<DWORD> NativeFrameColor::_ReadDwmSetting(LPCWSTR key) const
{
    if (!_dwmKey)
    {
        return std::nullopt;
    }

    DWORD value;
    DWORD valueSize = sizeof(value);

    auto ret = ::RegQueryValueExW(_dwmKey.get(), key, NULL, NULL, reinterpret_cast<LPBYTE>(&value), &valueSize);
    if (ret != ERROR_SUCCESS)
    {
        LOG_WIN32(ret);
        return std::nullopt;
    }

    return value;
}
