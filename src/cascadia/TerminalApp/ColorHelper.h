#pragma once
#include "pch.h"

#include <winrt/windows.ui.core.h>

namespace WUI = winrt::Windows::UI;

namespace winrt::TerminalApp
{
    class HSL
    {
    public:
        float H;
        float S;
        float L;
    };

    class ColorHelper
    {
    public:
        static bool IsBrightColor(const WUI::Color& color);
        static HSL RgbToHsl(const WUI::Color& color);
        static WUI::Color HslToRgb(const HSL& color);
        static WUI::Color Lighten(const WUI::Color& color, float amount = 10.f);
        static WUI::Color Darken(const WUI::Color& color, float amount = 10.f);
        static WUI::Color GetAccentColor(const WUI::Color& color);
        static float GetLuminance(const WUI::Color& color);
        static float GetReadability(const WUI::Color& first, const WUI::Color& second);
        static float GetReadability(const HSL& first, const HSL& second);

    private:
        static float HueToRgb(float p, float q, float t);
    };
}
