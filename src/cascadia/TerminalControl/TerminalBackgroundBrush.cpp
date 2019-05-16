#include "pch.h"
#include "TerminalBackgroundBrush.h"

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
	//winrt::Windows::UI::Xaml::DependencyProperty TerminalBackgroundBrush::m_blurAmountProperty =
	//	winrt::Windows::UI::Xaml::DependencyProperty::Register(
	//		L"BlurAmount",
	//		winrt::xaml_typename<double>(),
	//		winrt::xaml_typename<Microsoft::Terminal::TerminalControl::TerminalBackgroundBrush>(),
	//		Windows::UI::Xaml::PropertyMetadata{ winrt::box_value(0.), Windows::UI::Xaml::PropertyChangedCallback{ &TerminalBackgroundBrush::OnBlurAmountChanged } }
	//);

	void TerminalBackgroundBrush::OnBlurAmountChanged(Windows::UI::Xaml::DependencyObject const& d, Windows::UI::Xaml::DependencyPropertyChangedEventArgs const& e)
	{
		//auto brush{ d.as<MyApp::TerminalBackgroundBrush>() };
		//// Unbox and set a new blur amount if the CompositionBrush exists.
		//if (brush.CompositionBrush() != nullptr)
		//{
		//	brush.CompositionBrush().Properties().InsertScalar(L"Blur.BlurAmount", (float)winrt::unbox_value<double>(e.NewValue()));
		//}
	}

	void TerminalBackgroundBrush::OnConnected()
	{
		// Delay creating composition resources until they're required.
		if (!CompositionBrush())
		{
			auto backdrop{ Windows::UI::Xaml::Window::Current().Compositor().CreateBackdropBrush() };

			// Use a Win2D blur affect applied to a CompositionBackdropBrush.
			Microsoft::Graphics::Canvas::Effects::GaussianBlurEffect graphicsEffect{};
			graphicsEffect.Name(L"Blur");
			//graphicsEffect.BlurAmount(this->BlurAmount());
			graphicsEffect.BlurAmount(0.5);
			graphicsEffect.Source(Windows::UI::Composition::CompositionEffectSourceParameter(L"backdrop"));

			auto effectFactory{ Windows::UI::Xaml::Window::Current().Compositor().CreateEffectFactory(graphicsEffect, { L"Blur.BlurAmount" }) };
			auto effectBrush{ effectFactory.CreateBrush() };

			effectBrush.SetSourceParameter(L"backdrop", backdrop);

			CompositionBrush(effectBrush);
		}
	}

	void TerminalBackgroundBrush::OnDisconnected()
	{
		// Dispose of composition resources when no longer in use.
		if (CompositionBrush())
		{
			CompositionBrush(nullptr);
		}
	}

}