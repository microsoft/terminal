#include "pch.h"

#include "NativeFrameColor.h"

NativeFrameColor::NativeFrameColor()
{
    LOG_IF_WIN32_ERROR(::RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\DWM", 0, KEY_READ, _dwmKey.addressof()));
    Update();
}

COLORREF NativeFrameColor::GetActiveColor() const
{
    return _activeColor;
}

COLORREF NativeFrameColor::GetInactiveColor() const
{
    // TODO: This is not the real color
    //  The real color is transparent and is blended on top of other windows.
    return 0x404040;
}

namespace
{
    int AlphaBlendComponent(int bg, int fg, float alpha)
    {
        return static_cast<int>(bg * (1.0f - alpha) + fg * alpha);
    }

    COLORREF AlphaBlendColor(COLORREF bg, COLORREF fg, float alpha)
    {
        return AlphaBlendComponent(bg & 0x000000FF, fg & 0x000000FF, alpha) | // red
               (AlphaBlendComponent((bg & 0x0000FF00) >> 8, (fg & 0x0000FF00) >> 8, alpha) << 8) | // green
               (AlphaBlendComponent((bg & 0x00FF0000) >> 16, (fg & 0x00FF0000) >> 16, alpha) << 16); // blue
    }

    COLORREF XrgbToColorref(DWORD xrgb)
    {
        return ((xrgb & 0x00FF0000) >> 16) | // red
               (xrgb & 0x0000FF00) | // green
               ((xrgb & 0x000000FF) << 16); // blue
    }
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

        _activeColor = AlphaBlendColor(0xD9D9D9,
                                       XrgbToColorref(colorizationColor),
                                       colorizationColorBalance / 100.0f);
    }
    else
    {
        // TODO: This is not the real color
        //  The real color is transparent and is blended on top of other windows.
        _activeColor = 0x303030;
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
