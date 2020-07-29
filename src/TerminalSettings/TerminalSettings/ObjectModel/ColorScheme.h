#pragma once
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include "ObjectModel.ColorScheme.g.h"

namespace winrt::ObjectModel::implementation
{
    struct ColorScheme : ColorSchemeT<ColorScheme>
    {
        ColorScheme();

        Windows::UI::Color Background();
        Windows::UI::Xaml::Media::Brush BackgroundBrush();
        winrt::hstring BackgroundHexValue();
        void Background(Windows::UI::Xaml::Media::Brush const& brush);
        void Background(Windows::UI::Color const& color);

        winrt::event_token PropertyChanged(Windows::UI::Xaml::Data::PropertyChangedEventHandler const& value);
        void PropertyChanged(winrt::event_token const& token);

    private:
        winrt::hstring colorToHex(Windows::UI::Color);
        Windows::UI::Xaml::Media::Brush colorToBrush(Windows::UI::Color);
        Windows::UI::Color brushToColor(Windows::UI::Xaml::Media::Brush);
        Windows::UI::Color m_background;
        winrt::event<Windows::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };
}
