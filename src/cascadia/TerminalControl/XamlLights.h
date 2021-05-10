// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Module Name:
// - ControlCore.h
//
// Abstract:
// - This encapsulates a `Terminal` instance, a `DxEngine` and `Renderer`, and
//   an `ITerminalConnection`. This is intended to be everything that someone
//   might need to stand up a terminal instance in a control, but without any
//   regard for how the UX works.
//
// Author:
// - Mike Griese (zadjii-msft) 01-Apr-2021

#pragma once

#include "cppwinrt_utils.h"
#include "VisualBellLight.g.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct VisualBellLight : VisualBellLightT<VisualBellLight>
    {
        VisualBellLight() = default;

        winrt::hstring GetId();

        static Windows::UI::Xaml::DependencyProperty IsTargetProperty() { return m_isTargetProperty; }

        static bool GetIsTarget(Windows::UI::Xaml::DependencyObject const& target)
        {
            return winrt::unbox_value<bool>(target.GetValue(m_isTargetProperty));
        }

        static void SetIsTarget(Windows::UI::Xaml::DependencyObject const& target, bool value)
        {
            target.SetValue(m_isTargetProperty, winrt::box_value(value));
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
        static Windows::UI::Xaml::DependencyProperty m_isTargetProperty;
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(VisualBellLight);
}
