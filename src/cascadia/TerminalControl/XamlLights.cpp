// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TermControl.h"
#include "XamlLights.h"
#include "VisualBellLight.g.cpp"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    Windows::UI::Xaml::DependencyProperty VisualBellLight::_IsTargetProperty{ nullptr };

    VisualBellLight::VisualBellLight()
    {
        _InitializeProperties();
    }

    void VisualBellLight::_InitializeProperties()
    {
        // Initialize any dependency properties here.
        // This performs a lazy load on these properties, instead of
        // initializing them when the DLL loads.
        if (!_IsTargetProperty)
        {
            _IsTargetProperty =
                Windows::UI::Xaml::DependencyProperty::RegisterAttached(
                    L"IsTarget",
                    winrt::xaml_typename<bool>(),
                    winrt::xaml_typename<winrt::Microsoft::Terminal::Control::VisualBellLight>(),
                    Windows::UI::Xaml::PropertyMetadata{ winrt::box_value(false), Windows::UI::Xaml::PropertyChangedCallback{ &VisualBellLight::OnIsTargetChanged } });
        }
    }

    // Method Description:
    // - This function is called when the first target UIElement is shown on the screen,
    //   this enables delaying composition object creation until it's actually necessary.
    // Arguments:
    // - newElement: unused
    void VisualBellLight::OnConnected(Windows::UI::Xaml::UIElement const& /* newElement */)
    {
        if (!CompositionLight())
        {
            auto spotLight{ Windows::UI::Xaml::Window::Current().Compositor().CreateAmbientLight() };
            spotLight.Color(Windows::UI::Colors::White());
            spotLight.Intensity(static_cast<float>(1.5));
            CompositionLight(spotLight);
        }
    }

    // Method Description:
    // - This function is called when there are no more target UIElements on the screen
    // - Disposes of composition resources when no longer in use
    // Arguments:
    // - oldElement: unused
    void VisualBellLight::OnDisconnected(Windows::UI::Xaml::UIElement const& /* oldElement */)
    {
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
        const auto& uielem{ d.try_as<Windows::UI::Xaml::UIElement>() };
        const auto& brush{ d.try_as<Windows::UI::Xaml::Media::Brush>() };

        const auto isAdding = winrt::unbox_value<bool>(e.NewValue());
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
