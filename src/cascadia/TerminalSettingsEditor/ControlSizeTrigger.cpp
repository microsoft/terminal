// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ControlSizeTrigger.h"
#include "ControlSizeTrigger.g.cpp"

#include <limits>

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    DependencyProperty ControlSizeTrigger::_CanTriggerProperty{ nullptr };
    DependencyProperty ControlSizeTrigger::_MinWidthProperty{ nullptr };
    DependencyProperty ControlSizeTrigger::_MaxWidthProperty{ nullptr };
    DependencyProperty ControlSizeTrigger::_MinHeightProperty{ nullptr };
    DependencyProperty ControlSizeTrigger::_MaxHeightProperty{ nullptr };
    DependencyProperty ControlSizeTrigger::_TargetElementProperty{ nullptr };

    ControlSizeTrigger::ControlSizeTrigger()
    {
        _InitializeProperties();
    }

    void ControlSizeTrigger::_InitializeProperties()
    {
        // Defaults mirror the toolkit: trigger is always evaluatable, bounds
        // are wide open, no target element until one is bound.
        if (!_CanTriggerProperty)
        {
            _CanTriggerProperty = DependencyProperty::Register(
                L"CanTrigger",
                xaml_typename<bool>(),
                xaml_typename<Editor::ControlSizeTrigger>(),
                PropertyMetadata{ box_value(true), PropertyChangedCallback{ &ControlSizeTrigger::_OnTriggerInputChanged } });
        }
        if (!_MinWidthProperty)
        {
            _MinWidthProperty = DependencyProperty::Register(
                L"MinWidth",
                xaml_typename<double>(),
                xaml_typename<Editor::ControlSizeTrigger>(),
                PropertyMetadata{ box_value(0.0), PropertyChangedCallback{ &ControlSizeTrigger::_OnTriggerInputChanged } });
        }
        if (!_MaxWidthProperty)
        {
            _MaxWidthProperty = DependencyProperty::Register(
                L"MaxWidth",
                xaml_typename<double>(),
                xaml_typename<Editor::ControlSizeTrigger>(),
                PropertyMetadata{ box_value(std::numeric_limits<double>::infinity()), PropertyChangedCallback{ &ControlSizeTrigger::_OnTriggerInputChanged } });
        }
        if (!_MinHeightProperty)
        {
            _MinHeightProperty = DependencyProperty::Register(
                L"MinHeight",
                xaml_typename<double>(),
                xaml_typename<Editor::ControlSizeTrigger>(),
                PropertyMetadata{ box_value(0.0), PropertyChangedCallback{ &ControlSizeTrigger::_OnTriggerInputChanged } });
        }
        if (!_MaxHeightProperty)
        {
            _MaxHeightProperty = DependencyProperty::Register(
                L"MaxHeight",
                xaml_typename<double>(),
                xaml_typename<Editor::ControlSizeTrigger>(),
                PropertyMetadata{ box_value(std::numeric_limits<double>::infinity()), PropertyChangedCallback{ &ControlSizeTrigger::_OnTriggerInputChanged } });
        }
        if (!_TargetElementProperty)
        {
            _TargetElementProperty = DependencyProperty::Register(
                L"TargetElement",
                xaml_typename<FrameworkElement>(),
                xaml_typename<Editor::ControlSizeTrigger>(),
                PropertyMetadata{ nullptr, PropertyChangedCallback{ &ControlSizeTrigger::_OnTargetElementChanged } });
        }
    }

    void ControlSizeTrigger::_OnTriggerInputChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& /*e*/)
    {
        if (const auto obj{ d.try_as<Editor::ControlSizeTrigger>() })
        {
            get_self<ControlSizeTrigger>(obj)->_UpdateTrigger();
        }
    }

    void ControlSizeTrigger::_OnTargetElementChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& e)
    {
        const auto obj{ d.try_as<Editor::ControlSizeTrigger>() };
        if (!obj)
        {
            return;
        }
        const auto oldElement = e.OldValue().try_as<FrameworkElement>();
        const auto newElement = e.NewValue().try_as<FrameworkElement>();
        get_self<ControlSizeTrigger>(obj)->_UpdateTargetElement(oldElement, newElement);
    }

    void ControlSizeTrigger::_UpdateTargetElement(const FrameworkElement& /*oldValue*/, const FrameworkElement& newValue)
    {
        // Revoking handles both unhooking the previous element and a null `newValue`.
        _sizeChangedRevoker.revoke();
        if (newValue)
        {
            _sizeChangedRevoker = newValue.SizeChanged(winrt::auto_revoke, [weakThis = get_weak()](auto&&, auto&&) {
                if (const auto strongThis = weakThis.get())
                {
                    strongThis->_UpdateTrigger();
                }
            });
        }
        _UpdateTrigger();
    }

    void ControlSizeTrigger::_UpdateTrigger()
    {
        const auto target = TargetElement();
        if (!target || !CanTrigger())
        {
            _isActive = false;
            SetActive(false);
            return;
        }

        const auto width = target.ActualWidth();
        const auto height = target.ActualHeight();

        const bool activate =
            MinWidth() <= width &&
            width < MaxWidth() &&
            MinHeight() <= height &&
            height < MaxHeight();

        _isActive = activate;
        SetActive(activate);
    }
}
