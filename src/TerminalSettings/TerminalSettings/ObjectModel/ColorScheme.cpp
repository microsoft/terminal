#include "pch.h"
#include "ColorScheme.h"
#include "ObjectModel.ColorScheme.g.cpp"

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

    Color ColorScheme::Background()
    {
        return m_background;
    }

    Brush ColorScheme::BackgroundBrush()
    {
        return ColorScheme::colorToBrush(m_background);
    }

    winrt::hstring ColorScheme::BackgroundHexValue()
    {
        return colorToHex(m_background);
    }

    void ColorScheme::Background(Color const& color)
    {
        if (color != m_background)
        {
            m_background = color;
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"Background" });
        }
    }

    void ColorScheme::Background(Brush const& brush)
    {
        ColorScheme::Background(brushToColor(brush));
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

    winrt::hstring ColorScheme::colorToHex(Windows::UI::Color color)
    {
        char hex[9];
        sprintf_s(hex, "#%x%x%x%x", color.A, color.R, color.G, color.B);
        return winrt::to_hstring(hex);
    }

    // helpers
    Brush ColorScheme::colorToBrush(Color color)
    {
        return SolidColorBrush(color);
    }

    Color ColorScheme::brushToColor(Brush brush)
    {
        return brush.as<SolidColorBrush>().Color();
    }
}
