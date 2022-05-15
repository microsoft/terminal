#include "pch.h"
#include "ColorHelper.h"
#include <limits>

using namespace winrt::TerminalApp;

// Method Description:
// Determines whether or not a given color is light
// Arguments:
// - color: this color is going to be examined whether it
// is light or not
// Return Value:
// - true of light, false if dark
bool ColorHelper::IsBrightColor(const winrt::Windows::UI::Color& color)
{
    // https://www.w3.org/TR/AERT#color-contrast
    auto brightness = (color.R * 299 + color.G * 587 + color.B * 114) / 1000.f;
    return brightness > 128.f;
}

// Method Description:
// Converts a rgb color to an hsl one
// Arguments:
// - color: the rgb color, which is going to be converted
// Return Value:
// - a hsl color with the following ranges
//      - H: [0.f -360.f]
//      - L: [0.f - 1.f] (rounded to the third decimal place)
//      - S: [0.f - 1.f] (rounded to the third decimal place)
HSL ColorHelper::RgbToHsl(const winrt::Windows::UI::Color& color)
{
    // https://www.rapidtables.com/convert/color/rgb-to-hsl.html
    auto epsilon = std::numeric_limits<float>::epsilon();
    auto r = color.R / 255.f;
    auto g = color.G / 255.f;
    auto b = color.B / 255.f;

    auto max = std::max(r, std::max(g, b));
    auto min = std::min(r, std::min(g, b));

    auto delta = max - min;

    auto h = 0.f;
    auto s = 0.f;
    auto l = (max + min) / 2;

    if (delta < epsilon || max < epsilon) /* delta == 0 || max == 0*/
    {
        l = std::roundf(l * 1000) / 1000;
        return HSL{ h, s, l };
    }

    s = l > 0.5 ? delta / (2 - max - min) : delta / (max + min);

    if (max - r < epsilon) // max == r
    {
        h = (g - b) / delta + (g < b ? 6 : 0);
    }
    else if (max - g < epsilon) // max == g
    {
        h = (b - r) / delta + 2;
    }
    else if (max - b < epsilon) // max == b
    {
        h = (r - g) / delta + 4;
    }

    // three decimal places after the comma ought
    // to be enough for everybody - Bill Gates, 1981
    auto finalH = std::roundf(h * 60);
    auto finalS = std::roundf(s * 1000) / 1000;
    auto finalL = std::roundf(l * 1000) / 1000;

    return HSL{ finalH, finalS, finalL };
}

// Method Description:
// Converts a hsl color to rgb one
// Arguments:
// - color: the hsl color, which is going to be converted
// Return Value:
// - the rgb color (r,g,b - [0, 255] range)
winrt::Windows::UI::Color ColorHelper::HslToRgb(const HSL& color)
{
    auto epsilon = std::numeric_limits<float>::epsilon();

    auto h = (color.H - 1.f > epsilon) ? color.H / 360.f : color.H;
    auto s = (color.S - 1.f > epsilon) ? color.S / 100.f : color.S;
    auto l = (color.L - 1.f > epsilon) ? color.L / 100.f : color.L;

    auto r = l;
    auto g = l;
    auto b = l;

    if (s > epsilon)
    {
        auto q = l < 0.5 ? l * (1 + s) : l + s - l * s;
        auto p = 2 * l - q;
        r = HueToRgb(p, q, h + 1.f / 3.f);
        g = HueToRgb(p, q, h);
        b = HueToRgb(p, q, h - 1.f / 3.f);
    }

    auto finalR = static_cast<uint8_t>(std::roundf(r * 255));
    auto finalG = static_cast<uint8_t>(std::roundf(g * 255));
    auto finalB = static_cast<uint8_t>(std::roundf(b * 255));
    uint8_t finalA = 255; //opaque

    return winrt::Windows::UI::ColorHelper::FromArgb(finalA, finalR, finalG, finalB);
}

float ColorHelper::HueToRgb(float p, float q, float t)
{
    auto epsilon = std::numeric_limits<float>::epsilon();

    if (t < 0)
        t += 1;
    if (t > 1)
        t -= 1;
    if (t - (1.f / 6.f) < epsilon)
        return p + (q - p) * 6 * t;
    if (t - .5f < epsilon)
        return q;
    if (t - 2.f / 3.f < epsilon)
        return p + (q - p) * (2.f / 3.f - t) * 6;
    return p;
}

// Method Description:
// Lightens a color by a given amount
// Arguments:
// - color: the color which is going to be lightened
// - amount: the lighten amount (0-100)
// Return Value:
// - the lightened color in RGB format
winrt::Windows::UI::Color ColorHelper::Lighten(const winrt::Windows::UI::Color& color, float amount /* = 10.f*/)
{
    auto hsl = RgbToHsl(color);
    hsl.L += amount / 100;
    hsl.L = std::clamp(hsl.L, 0.f, 1.f);
    return HslToRgb(hsl);
}

// Method Description:
// Darkens a color by a given amount
// Arguments:
// - color: the color which is going to be darkened
// - amount: the darken amount (0-100)
// Return Value:
// - the darkened color in RGB format
winrt::Windows::UI::Color ColorHelper::Darken(const winrt::Windows::UI::Color& color, float amount /* = 10.f*/)
{
    auto hsl = RgbToHsl(color);
    hsl.L -= amount / 100;
    hsl.L = std::clamp(hsl.L, 0.f, 1.f);
    return HslToRgb(hsl);
}

// Method Description:
// Gets an accent color to a given color. Basically, generates
// 16 shades of the color and finds the first which has a good
// contrast according to https://www.w3.org/TR/2008/REC-WCAG20-20081211/#contrast-ratiodef (WCAG Version 2)
// Readability ratio of 3.5 seems to look quite nicely
// Arguments:
// - color: the color for which we need an accent
// Return Value:
// - the accent color in RGB format
winrt::Windows::UI::Color ColorHelper::GetAccentColor(const winrt::Windows::UI::Color& color)
{
    auto accentColor = RgbToHsl(color);

    if (accentColor.S < 0.15)
    {
        accentColor.S = 0.15f;
    }

    constexpr auto shadeCount = 16;
    constexpr auto shadeStep = 1.f / shadeCount;
    auto shades = std::map<float, HSL>();
    for (auto i = 0; i < 15; i++)
    {
        auto shade = HSL{ accentColor.H, accentColor.S, i * shadeStep };
        auto contrast = GetReadability(shade, accentColor);
        shades.insert(std::make_pair(contrast, shade));
    }

    // 3f is quite nice if the whole non-client area is painted
    constexpr auto readability = 1.75f;
    for (auto shade : shades)
    {
        if (shade.first >= readability)
        {
            return HslToRgb(shade.second);
        }
    }
    return HslToRgb(shades.end()->second);
}

// Method Description:
// Gets the readability of two colors according to
// https://www.w3.org/TR/2008/REC-WCAG20-20081211/#contrast-ratiodef (WCAG Version 2)
// Arguments:
// - firstColor: the first color for the readability check (hsl)
// - secondColor: the second color for the readability check (hsl)
// Return Value:
// - the readability of the colors according to (WCAG Version 2)
float ColorHelper::GetReadability(const HSL& first, const HSL& second)
{
    return GetReadability(HslToRgb(first), HslToRgb(second));
}

// Method Description:
// Gets the readability of two colors according to
// https://www.w3.org/TR/2008/REC-WCAG20-20081211/#contrast-ratiodef (WCAG Version 2)
// Arguments:
// - firstColor: the first color for the readability check (rgb)
// - secondColor: the second color for the readability check (rgb)
// Return Value:
// - the readability of the colors according to (WCAG Version 2)
float ColorHelper::GetReadability(const winrt::Windows::UI::Color& first, const winrt::Windows::UI::Color& second)
{
    auto l1 = GetLuminance(first);
    auto l2 = GetLuminance(second);

    return (std::max(l1, l2) + 0.05f) / std::min(l1, l2) + 0.05f;
}

// Method Description:
// Calculates the luminance of a given color according to
// https://www.w3.org/TR/2008/REC-WCAG20-20081211/#relativeluminancedef
// Arguments:
// - color: its luminance is going to be calculated
// Return Value:
// - the luminance of the color
float ColorHelper::GetLuminance(const winrt::Windows::UI::Color& color)
{
    auto epsilon = std::numeric_limits<float>::epsilon();
    float R, G, B;
    auto RsRGB = color.R / 255.f;
    auto GsRGB = color.G / 255.f;
    auto BsRGB = color.B / 255.f;

    if (RsRGB - 0.03928f <= epsilon)
    {
        R = RsRGB / 12.92f;
    }
    else
    {
        R = std::pow(((RsRGB + 0.055f) / 1.055f), 2.4f);
    }
    if (GsRGB - 0.03928f <= epsilon)
    {
        G = GsRGB / 12.92f;
    }
    else
    {
        G = std::pow(((GsRGB + 0.055f) / 1.055f), 2.4f);
    }
    if (BsRGB - 0.03928f <= epsilon)
    {
        B = BsRGB / 12.92f;
    }
    else
    {
        B = std::pow(((BsRGB + 0.055f) / 1.055f), 2.4f);
    }
    auto luminance = (0.2126f * R) + (0.7152f * G) + (0.0722f * B);
    return std::roundf(luminance * 10000) / 10000.f;
}
