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

        static WUX::DependencyProperty IsTargetProperty() { return _IsTargetProperty; }

        static bool GetIsTarget(const WUX::DependencyObject& target)
        {
            return winrt::unbox_value<bool>(target.GetValue(_IsTargetProperty));
        }

        static void SetIsTarget(const WUX::DependencyObject& target, bool value)
        {
            target.SetValue(_IsTargetProperty, winrt::box_value(value));
        }

        void OnConnected(const WUX::UIElement& newElement);
        void OnDisconnected(const WUX::UIElement& oldElement);

        static void OnIsTargetChanged(const WUX::DependencyObject& d, const WUX::DependencyPropertyChangedEventArgs& e);

        inline static winrt::hstring GetIdStatic()
        {
            // This specifies the unique name of the light. In most cases you should use the type's full name.
            return winrt::xaml_typename<MTControl::VisualBellLight>().Name;
        }

    private:
        static void _InitializeProperties();
        static WUX::DependencyProperty _IsTargetProperty;
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(VisualBellLight);
}
