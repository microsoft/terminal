// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TermControl.h"
#include "XamlLights.h"
#include "VisualBellLight.g.cpp"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Media;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    DependencyProperty VisualBellLight::_IsTargetProperty{ nullptr };

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
                DependencyProperty::RegisterAttached(
                    L"IsTarget",
                    winrt::xaml_typename<bool>(),
                    winrt::xaml_typename<Control::VisualBellLight>(),
                    PropertyMetadata{ winrt::box_value(false), PropertyChangedCallback{ &VisualBellLight::OnIsTargetChanged } });
        }
    }

    // Method Description:
    // - This function is called when the first target UIElement is shown on the screen,
    //   this enables delaying composition object creation until it's actually necessary.
    // Arguments:
    // - newElement: unused
    void VisualBellLight::OnConnected(const UIElement& /* newElement */)
    {
        if (!CompositionLight())
        {
            auto spotLight{ Window::Current().Compositor().CreateAmbientLight() };
            spotLight.Color(Windows::UI::Colors::Gray());
            CompositionLight(spotLight);
        }
    }

    // Method Description:
    // - This function is called when there are no more target UIElements on the screen
    // - Disposes of composition resources when no longer in use
    // Arguments:
    // - oldElement: unused
    void VisualBellLight::OnDisconnected(const UIElement& /* oldElement */)
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

    void VisualBellLight::OnIsTargetChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& e)
    {
        const auto uielem{ d.try_as<UIElement>() };
        const auto brush{ d.try_as<Brush>() };

        if (!uielem && !brush)
        {
            // terminate early
            return;
        }

        const auto isAdding = winrt::unbox_value<bool>(e.NewValue());
        const auto id = GetIdStatic();

        if (isAdding)
        {
            if (uielem)
            {
                XamlLight::AddTargetElement(id, uielem);
            }
            else
            {
                XamlLight::AddTargetBrush(id, brush);
            }
        }
        else
        {
            if (uielem)
            {
                XamlLight::RemoveTargetElement(id, uielem);
            }
            else
            {
                XamlLight::RemoveTargetBrush(id, brush);
            }
        }
    }
}
