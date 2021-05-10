// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "cppwinrt_utils.h"
#include "VisualBellLight.g.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct VisualBellLight : VisualBellLightT<VisualBellLight>
    {
        VisualBellLight();

        winrt::hstring GetId();

        static Windows::UI::Xaml::DependencyProperty IsTargetProperty() { return _IsTargetProperty; }

        static bool GetIsTarget(Windows::UI::Xaml::DependencyObject const& target)
        {
            return winrt::unbox_value<bool>(target.GetValue(_IsTargetProperty));
        }

        static void SetIsTarget(Windows::UI::Xaml::DependencyObject const& target, bool value)
        {
            target.SetValue(_IsTargetProperty, winrt::box_value(value));
        }

        void OnConnected(Windows::UI::Xaml::UIElement const& newElement);
        void OnDisconnected(Windows::UI::Xaml::UIElement const& oldElement);

        static void OnIsTargetChanged(Windows::UI::Xaml::DependencyObject const& d, Windows::UI::Xaml::DependencyPropertyChangedEventArgs const& e);

        inline static winrt::hstring GetIdStatic()
        {
            // This specifies the unique name of the light. In most cases you should use the type's full name.
            return winrt::xaml_typename<winrt::Microsoft::Terminal::Control::VisualBellLight>().Name;
        }

    private:
        static void _InitializeProperties();
        static Windows::UI::Xaml::DependencyProperty _IsTargetProperty;
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(VisualBellLight);
}
