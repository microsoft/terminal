#pragma once

#include "ColorSchemes.g.h"

namespace winrt::Microsoft::Terminal::Settings::Control::implementation
{
    struct ColorSchemes : ColorSchemesT<ColorSchemes>
    {
        ColorSchemes();

        int32_t MyProperty();
        void MyProperty(int32_t value);

        void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Control::factory_implementation
{
    struct ColorSchemes : ColorSchemesT<ColorSchemes, implementation::ColorSchemes>
    {
    };
}
