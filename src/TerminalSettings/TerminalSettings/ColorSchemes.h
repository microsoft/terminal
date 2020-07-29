#pragma once

#include "ColorSchemes.g.h"
#include "ObjectModel/ColorSchemeModel.h"

namespace winrt::SettingsControl::implementation
{
    struct ColorSchemes : ColorSchemesT<ColorSchemes>
    {
        ColorSchemes();
        ObjectModel::ColorSchemeModel ColorSchemeModel();

        void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void Background_ColorChanged(Windows::UI::Xaml::Controls::ColorPicker const& picker, Windows::UI::Xaml::Controls::ColorChangedEventArgs const& event);

        void BackgroundHexValue_Changed(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::Controls::TextChangedEventArgs const& event);
    private:
        ObjectModel::ColorSchemeModel m_colorSchemeModel{ nullptr };
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    struct ColorSchemes : ColorSchemesT<ColorSchemes, implementation::ColorSchemes>
    {
    };
}
