#pragma once
#include "ColorPickupFlyout.g.h"

namespace winrt::Microsoft::Terminal::App::implementation
{
    struct ColorPickupFlyout : ColorPickupFlyoutT<ColorPickupFlyout>
    {
        ColorPickupFlyout();

        void ColorButton_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void ShowColorPickerButton_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void CustomColorButton_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void ClearColorButton_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void ColorPicker_ColorChanged(const Microsoft::UI::Xaml::Controls::ColorPicker&, const Microsoft::UI::Xaml::Controls::ColorChangedEventArgs& args);

        WINRT_CALLBACK(ColorCleared, Microsoft::Terminal::App::ColorClearedArgs);
        WINRT_CALLBACK(ColorSelected, Microsoft::Terminal::App::ColorSelectedArgs);
    };
}

namespace winrt::Microsoft::Terminal::App::factory_implementation
{
    BASIC_FACTORY(ColorPickupFlyout);
}
