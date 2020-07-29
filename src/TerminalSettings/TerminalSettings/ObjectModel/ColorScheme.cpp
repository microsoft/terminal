#include "pch.h"
#include "ColorScheme.h"
#include "ObjectModel.ColorScheme.g.cpp"
#include <sstream>
#include <string>

using namespace winrt;
using namespace Windows::UI;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Media;

namespace winrt::ObjectModel::implementation
{
    ColorScheme::ColorScheme()
    {
        m_background = Colors::Black();
    }

    // Background Handlers
    Color ColorScheme::Background()
    {
        return m_background;
    }

    void ColorScheme::Background(Brush const& brush)
    {
        ColorScheme::Background(brushToColor(brush));
    }

    void ColorScheme::Background(Color const& color)
    {
        if (cmpColor(color, m_background))
        {
            m_background = color;
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BackgroundBrush" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BackgroundHexValue" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"Background" });
        }
    }

    Brush ColorScheme::BackgroundBrush()
    {
        return ColorScheme::colorToBrush(m_background);
    }

    winrt::hstring ColorScheme::BackgroundHexValue()
    {
        return colorToHex(m_background);
    }

    void ColorScheme::BackgroundHexValue(winrt::hstring hex)
    {
        // TODO: Dirty dirty hack - We should try to parse this string a little harder than checking that its 9 long
        if (hex.size() == 9)
        {
            ColorScheme::Background(ColorScheme::hexToColor(hex));
        }
    }

    // event handlers
    event_token ColorScheme::PropertyChanged(PropertyChangedEventHandler const& handler)
    {
        return m_propertyChanged.add(handler);
    }

    void ColorScheme::PropertyChanged(event_token const& token)
    {
        m_propertyChanged.remove(token);
    }

    // helpers

    Color ColorScheme::hexToColor(winrt::hstring hex)
    { 
        std::string string = winrt::to_string(hex);
        Color newColor = Color();
        newColor.A = std::stoi(string.substr(1, 2), nullptr, 16);
        newColor.R = std::stoi(string.substr(3, 2), nullptr, 16);
        newColor.G = std::stoi(string.substr(5, 2), nullptr, 16);
        newColor.B = std::stoi(string.substr(7, 2), nullptr, 16);
        return newColor;
    }

    winrt::hstring ColorScheme::colorToHex(Windows::UI::Color color)
    {
        char hex[10];
        snprintf(hex, sizeof hex, "#%02x%02x%02x%02x", color.A, color.R, color.G, color.B);

        return winrt::to_hstring(hex);
    }

    bool ColorScheme::cmpColor(Color a, Color b)
    {
        if (a.A == b.A && a.R == b.R && a.G == b.G && a.B == b.B)
        {
            return false;
        }
        return true;
    }

    Brush ColorScheme::colorToBrush(Color color)
    {
        return SolidColorBrush(color);
    }

    Color ColorScheme::brushToColor(Brush brush)
    {
        return brush.as<SolidColorBrush>().Color();
    }
}
