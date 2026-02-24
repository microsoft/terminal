// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "VisualBellLight.g.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct VisualBellLight : VisualBellLightT<VisualBellLight>
    {
        VisualBellLight();

        winrt::hstring GetId();

        static Windows::UI::Xaml::DependencyProperty IsTargetProperty() { return _IsTargetProperty; }

        static bool GetIsTarget(const Windows::UI::Xaml::DependencyObject& target)
        {
            return winrt::unbox_value<bool>(target.GetValue(_IsTargetProperty));
        }

        static void SetIsTarget(const Windows::UI::Xaml::DependencyObject& target, bool value)
        {
            target.SetValue(_IsTargetProperty, winrt::box_value(value));
        }

        void OnConnected(const Windows::UI::Xaml::UIElement& newElement);
        void OnDisconnected(const Windows::UI::Xaml::UIElement& oldElement);

        static void OnIsTargetChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);

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
