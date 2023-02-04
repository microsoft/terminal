#pragma once
#include "ColorPickupFlyout.g.h"

namespace winrt::TerminalApp::implementation
{
    struct ColorPickupFlyout : ColorPickupFlyoutT<ColorPickupFlyout>
    {
        ColorPickupFlyout();

        void ColorButton_Click(const WF::IInspectable& sender, const WUX::RoutedEventArgs& args);
        void ShowColorPickerButton_Click(const WF::IInspectable& sender, const WUX::RoutedEventArgs& args);
        void CustomColorButton_Click(const WF::IInspectable& sender, const WUX::RoutedEventArgs& args);
        void ClearColorButton_Click(const WF::IInspectable& sender, const WUX::RoutedEventArgs& args);
        void ColorPicker_ColorChanged(const MUXC::ColorPicker&, const MUXC::ColorChangedEventArgs& args);

        WINRT_CALLBACK(ColorCleared, TerminalApp::ColorClearedArgs);
        WINRT_CALLBACK(ColorSelected, TerminalApp::ColorSelectedArgs);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(ColorPickupFlyout);
}
