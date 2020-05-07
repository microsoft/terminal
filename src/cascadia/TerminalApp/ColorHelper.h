#pragma once
#include "pch.h"

#include <winrt/windows.ui.core.h>

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
        static bool IsBrightColor(const Windows::UI::Color& color);
        static HSL RgbToHsl(const Windows::UI::Color& color);
        static Windows::UI::Color HslToRgb(const HSL& color);
        static Windows::UI::Color Lighten(const Windows::UI::Color& color, float amount = 10.f);
        static Windows::UI::Color Darken(const Windows::UI::Color& color, float amount = 10.f);
        static Windows::UI::Color GetAccentColor(const Windows::UI::Color& color);
        static float GetLuminance(const Windows::UI::Color& color);
        static float GetReadability(const Windows::UI::Color& first, const Windows::UI::Color& second);
        static float GetReadability(const HSL& first, const HSL& second);

    private:
        static float HueToRgb(float p, float q, float t);
    };
}
