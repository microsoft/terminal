#pragma once
#include "ColorPickupFlyout.g.h"

namespace winrt::TerminalApp::implementation
{
    struct ColorPickupFlyout : ColorPickupFlyoutT<ColorPickupFlyout>
    {
		ColorPickupFlyout();
		void ColorButton_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
		void CustomColorButton_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);

        winrt::Windows::UI::Color SelectedColor()
        {
            return winrt::unbox_value<winrt::Windows::UI::Color>(GetValue(m_SelectedColorProperty));
        }

        void SelectedColor(winrt::Windows::UI::Color const& color)
        {
            SetValue(m_SelectedColorProperty, winrt::box_value(color));
        }

		static Windows::UI::Xaml::DependencyProperty SelectedColorProperty() { return m_SelectedColorProperty; }
        static void OnSelectedColorChanged(Windows::UI::Xaml::DependencyObject const&, Windows::UI::Xaml::DependencyPropertyChangedEventArgs const&);

	private:
		static Windows::UI::Xaml::DependencyProperty m_SelectedColorProperty;
    };
}
namespace winrt::TerminalApp::factory_implementation
{
    struct ColorPickupFlyout : ColorPickupFlyoutT<ColorPickupFlyout, implementation::ColorPickupFlyout>
    {
		
    };
}
