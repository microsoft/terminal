// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TermControl.h"
#include "XamlLights.h"
#include "VisualBellLight.g.cpp"
#include "CursorLight.g.cpp"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Media;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    DependencyProperty VisualBellLight::_IsTargetProperty{ nullptr };
    DependencyProperty CursorLight::_IsTargetProperty{ nullptr };

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
    void VisualBellLight::OnConnected(UIElement const& /* newElement */)
    {
        if (!CompositionLight())
        {
            auto spotLight{ Window::Current().Compositor().CreateAmbientLight() };
            spotLight.Color(Windows::UI::Colors::White());
            CompositionLight(spotLight);
        }
    }

    void CursorLight::_InitializeProperties()
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
                    winrt::xaml_typename<Control::CursorLight>(),
                    PropertyMetadata{ winrt::box_value(false), PropertyChangedCallback{ &CursorLight::OnIsTargetChanged } });
        }
    }

    void CursorLight::ChangeLocation(float xCoord, float yCoord)
    {
        if (const auto light = CompositionLight())
        {
            if (const auto cursorLight = light.try_as<Windows::UI::Composition::SpotLight>())
            {
                cursorLight.Offset({ xCoord, yCoord, 100 });
            }
        }
        else
        {
            _InitializeHelper(xCoord, yCoord);
        }
    }

    // Method Description:
    // - This function is called when the first target UIElement is shown on the screen,
    //   this enables delaying composition object creation until it's actually necessary.
    // Arguments:
    // - newElement: unused
    void CursorLight::OnConnected(UIElement const& /* newElement */)
    {
        if (!CompositionLight())
        {
            _InitializeHelper(0, 0);
        }
    }

    // Method Description:
    // - Helper to initialize the propoerties of the spotlight such as the location and
    //   the angles of the inner and outer cones
    // Arguments:
    // - xCoord: the x-coordinate of where to put the light
    // - yCoord: the y-coordinate of where to put the light
    void CursorLight::_InitializeHelper(float xCoord, float yCoord)
    {
        if (!CompositionLight())
        {
            auto spotLight{ Window::Current().Compositor().CreateSpotLight() };
            spotLight.InnerConeColor(Windows::UI::Colors::White());
            spotLight.InnerConeAngleInDegrees(10);
            spotLight.OuterConeAngleInDegrees(25);
            spotLight.Offset({ xCoord, yCoord, 100 });
            CompositionLight(spotLight);
        }
    }
}
