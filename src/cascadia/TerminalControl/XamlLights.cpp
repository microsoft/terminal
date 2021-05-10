// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "XamlLights.h"
#include "VisualBellLight.g.cpp"
#include "TermControlAutomationPeer.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    Windows::UI::Xaml::DependencyProperty VisualBellLight::m_isTargetProperty =
        Windows::UI::Xaml::DependencyProperty::RegisterAttached(
            L"IsTarget",
            winrt::xaml_typename<bool>(),
            winrt::xaml_typename<winrt::Microsoft::Terminal::Control::VisualBellLight>(),
            Windows::UI::Xaml::PropertyMetadata{ winrt::box_value(false), Windows::UI::Xaml::PropertyChangedCallback{ &VisualBellLight::OnIsTargetChanged } });

    void VisualBellLight::OnConnected(Windows::UI::Xaml::UIElement const& /* newElement */)
    {
        if (!CompositionLight())
        {
            // OnConnected is called when the first target UIElement is shown on the screen. This enables delaying composition object creation until it's actually necessary.
            auto spotLight2{ Windows::UI::Xaml::Window::Current().Compositor().CreateAmbientLight() };
            spotLight2.Color(Windows::UI::Colors::WhiteSmoke());
            spotLight2.Intensity(static_cast<float>(1.5));
            CompositionLight(spotLight2);
        }
    }

    void VisualBellLight::OnDisconnected(Windows::UI::Xaml::UIElement const& /* oldElement */)
    {
        // OnDisconnected is called when there are no more target UIElements on the screen.
        // Dispose of composition resources when no longer in use.
        if (CompositionLight())
        {
            CompositionLight(nullptr);
        }
    }

    winrt::hstring VisualBellLight::GetId()
    {
        return VisualBellLight::GetIdStatic();
    }

    void VisualBellLight::OnIsTargetChanged(Windows::UI::Xaml::DependencyObject const& d, Windows::UI::Xaml::DependencyPropertyChangedEventArgs const& e)
    {
        auto uielem{ d.try_as<Windows::UI::Xaml::UIElement>() };
        auto brush{ d.try_as<Windows::UI::Xaml::Media::Brush>() };

        auto isAdding = winrt::unbox_value<bool>(e.NewValue());
        if (isAdding)
        {
            if (uielem)
            {
                Windows::UI::Xaml::Media::XamlLight::AddTargetElement(VisualBellLight::GetIdStatic(), uielem);
            }
            else if (brush)
            {
                Windows::UI::Xaml::Media::XamlLight::AddTargetBrush(VisualBellLight::GetIdStatic(), brush);
            }
        }
        else
        {
            if (uielem)
            {
                Windows::UI::Xaml::Media::XamlLight::RemoveTargetElement(VisualBellLight::GetIdStatic(), uielem);
            }
            else if (brush)
            {
                Windows::UI::Xaml::Media::XamlLight::RemoveTargetBrush(VisualBellLight::GetIdStatic(), brush);
            }
        }
    }
}
