#include "pch.h"
#include "ColorPickupFlyout.h"
#include "ColorPickupFlyout.g.cpp"
#include "winrt/Windows.UI.Xaml.Media.h"
#include "winrt/Windows.UI.Xaml.Shapes.h"
#include "winrt/Windows.UI.Xaml.Interop.h"


namespace winrt::TerminalApp::implementation
{
	ColorPickupFlyout::ColorPickupFlyout()
	{
		InitializeComponent();
	}

	void ColorPickupFlyout::ColorButton_Click(IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const&)
	{
		auto btn{ sender.as<Windows::UI::Xaml::Controls::Button>() };
		auto rectangle{ btn.Content().as<Windows::UI::Xaml::Shapes::Rectangle>() };
		auto rectClr{ rectangle.Fill().as<Windows::UI::Xaml::Media::SolidColorBrush>() };
		SelectedColor(rectClr.Color());
		Hide();
	}

	void ColorPickupFlyout::CustomColorButton_Click(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&)
	{
		auto targetType = this->FlyoutPresenterStyle().TargetType();
		auto s = Windows::UI::Xaml::Style{ };
		s.TargetType(targetType);
		auto visibility = customColorPicker().Visibility();
		if (visibility == winrt::Windows::UI::Xaml::Visibility::Collapsed)
		{
			customColorPicker().Visibility(winrt::Windows::UI::Xaml::Visibility::Visible);
			auto setter = Windows::UI::Xaml::Setter(Windows::UI::Xaml::FrameworkElement::MinWidthProperty(), winrt::box_value(540));
			s.Setters().Append(setter);
		} 
		else
		{
			customColorPicker().Visibility(winrt::Windows::UI::Xaml::Visibility::Collapsed);
			auto setter = Windows::UI::Xaml::Setter(Windows::UI::Xaml::FrameworkElement::MinWidthProperty(), winrt::box_value(0));
			s.Setters().Append(setter);
		}
		this->FlyoutPresenterStyle(s);
	}

    Windows::UI::Xaml::DependencyProperty ColorPickupFlyout::m_SelectedColorProperty =
        Windows::UI::Xaml::DependencyProperty::Register(
            L"SelectedColor",
            winrt::xaml_typename<winrt::Windows::UI::Color>(),
            winrt::xaml_typename<TerminalApp::ColorPickupFlyout>(),
            Windows::UI::Xaml::PropertyMetadata{ winrt::box_value(winrt::Windows::UI::Colors::Transparent()),
            Windows::UI::Xaml::PropertyChangedCallback{ &ColorPickupFlyout::OnSelectedColorChanged } });

    void ColorPickupFlyout::OnSelectedColorChanged(Windows::UI::Xaml::DependencyObject const&, Windows::UI::Xaml::DependencyPropertyChangedEventArgs const& /* e */)
    {
    }
}
