#pragma once
#include "ColorPickupFlyout.g.h"
#include "../cascadia/inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct ColorPickupFlyout : ColorPickupFlyoutT<ColorPickupFlyout>
    {
        ColorPickupFlyout();

        void ColorButton_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void ShowColorPickerButton_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void CustomColorButton_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void ClearColorButton_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void ColorPicker_ColorChanged(const Windows::UI::Xaml::Controls::ColorPicker&, const Windows::UI::Xaml::Controls::ColorChangedEventArgs& args);

        WINRT_CALLBACK(ColorCleared, TerminalApp::ColorClearedArgs);
        WINRT_CALLBACK(ColorSelected, TerminalApp::ColorSelectedArgs);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct ColorPickupFlyout : ColorPickupFlyoutT<ColorPickupFlyout, implementation::ColorPickupFlyout>
    {
    };
}
