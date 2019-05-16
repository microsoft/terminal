#pragma once

#include "TerminalBackgroundBrush.g.h"

#include <winrt/Microsoft.Graphics.Canvas.Effects.h>
#include <winrt/Windows.Graphics.Effects.h>


namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
	struct TerminalBackgroundBrush : TerminalBackgroundBrushT<TerminalBackgroundBrush>
	{
		TerminalBackgroundBrush() = default;

		static Windows::UI::Xaml::DependencyProperty BlurAmountProperty() { return m_blurAmountProperty; }

		double BlurAmount()
		{
			return winrt::unbox_value<double>(GetValue(m_blurAmountProperty));
		}

		void BlurAmount(double value)
		{
			SetValue(m_blurAmountProperty, winrt::box_value(value));
		}

		void OnConnected();
		void OnDisconnected();

		static void OnBlurAmountChanged(Windows::UI::Xaml::DependencyObject const& d, Windows::UI::Xaml::DependencyPropertyChangedEventArgs const& e);

	private:
		static Windows::UI::Xaml::DependencyProperty m_blurAmountProperty;
	};

}

namespace winrt::Microsoft::Terminal::TerminalControl::factory_implementation
{
	struct TerminalBackgroundBrush : TerminalBackgroundBrushT<TerminalBackgroundBrush, implementation::TerminalBackgroundBrush>
	{
	};
}