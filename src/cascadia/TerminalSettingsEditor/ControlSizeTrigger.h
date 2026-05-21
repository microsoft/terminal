/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ControlSizeTrigger

Abstract:
- A conditional state trigger that activates based on the size (width and/or
  height) of a target FrameworkElement. Lets XAML visual states swap based on
  the live size of a templated part. Ported from the Windows Community Toolkit
  primitive `CommunityToolkit.WinUI.ControlSizeTrigger`.

  The trigger is "active" when:
    MinWidth  <= TargetElement.ActualWidth  < MaxWidth   AND
    MinHeight <= TargetElement.ActualHeight < MaxHeight

  Defaults: MinWidth = MinHeight = 0; MaxWidth = MaxHeight = +inf, which makes
  the trigger always active unless `CanTrigger` is false or `TargetElement` is
  null.

Author(s):
- Carlos Zamora - May 2026 (port from CommunityToolkit.WinUI.ControlSizeTrigger)

--*/

#pragma once

#include "ControlSizeTrigger.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ControlSizeTrigger : ControlSizeTriggerT<ControlSizeTrigger>
    {
    public:
        ControlSizeTrigger();

        bool IsActive() const { return _isActive; }

        DEPENDENCY_PROPERTY(bool, CanTrigger);
        DEPENDENCY_PROPERTY(double, MinWidth);
        DEPENDENCY_PROPERTY(double, MaxWidth);
        DEPENDENCY_PROPERTY(double, MinHeight);
        DEPENDENCY_PROPERTY(double, MaxHeight);
        DEPENDENCY_PROPERTY(Windows::UI::Xaml::FrameworkElement, TargetElement);

    private:
        static void _InitializeProperties();
        static void _OnTriggerInputChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);
        static void _OnTargetElementChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);

        void _UpdateTargetElement(const Windows::UI::Xaml::FrameworkElement& oldValue, const Windows::UI::Xaml::FrameworkElement& newValue);
        void _UpdateTrigger();

        Windows::UI::Xaml::FrameworkElement::SizeChanged_revoker _sizeChangedRevoker;
        bool _isActive{ false };
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(ControlSizeTrigger);
}
