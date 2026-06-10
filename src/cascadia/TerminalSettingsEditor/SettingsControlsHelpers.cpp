// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SettingsControlsHelpers.h"
#include "ControlSizeTrigger.g.cpp"
#include "TopCornerRadiusFilterConverter.g.cpp"
#include "BottomCornerRadiusFilterConverter.g.cpp"
#include "StringDefaultTemplateSelector.g.cpp"
#include "StyleExtensions.g.cpp"

#include <limits>

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
#pragma region ControlSizeTrigger

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

#pragma endregion
#pragma region TopBottomCornerRadiusFilters

    winrt::Windows::Foundation::IInspectable TopCornerRadiusFilterConverter::Convert(const winrt::Windows::Foundation::IInspectable& value, const Interop::TypeName& /*targetType*/, const winrt::Windows::Foundation::IInspectable& /*parameter*/, const hstring& /*language*/)
    {
        if (!value)
        {
            return value;
        }
        const auto cr = unbox_value_or<CornerRadius>(value, CornerRadius{ 0, 0, 0, 0 });
        return box_value(CornerRadius{ cr.TopLeft, cr.TopRight, 0, 0 });
    }

    winrt::Windows::Foundation::IInspectable TopCornerRadiusFilterConverter::ConvertBack(const winrt::Windows::Foundation::IInspectable& value, const Interop::TypeName& /*targetType*/, const winrt::Windows::Foundation::IInspectable& /*parameter*/, const hstring& /*language*/)
    {
        return value;
    }

    winrt::Windows::Foundation::IInspectable BottomCornerRadiusFilterConverter::Convert(const winrt::Windows::Foundation::IInspectable& value, const Interop::TypeName& /*targetType*/, const winrt::Windows::Foundation::IInspectable& /*parameter*/, const hstring& /*language*/)
    {
        if (!value)
        {
            return value;
        }
        const auto cr = unbox_value_or<CornerRadius>(value, CornerRadius{ 0, 0, 0, 0 });
        return box_value(CornerRadius{ 0, 0, cr.BottomRight, cr.BottomLeft });
    }

    winrt::Windows::Foundation::IInspectable BottomCornerRadiusFilterConverter::ConvertBack(const winrt::Windows::Foundation::IInspectable& value, const Interop::TypeName& /*targetType*/, const winrt::Windows::Foundation::IInspectable& /*parameter*/, const hstring& /*language*/)
    {
        return value;
    }

#pragma endregion
#pragma region StringDefaultTemplateSelector

    DataTemplate StringDefaultTemplateSelector::SelectTemplateCore(const IInspectable& item, const DependencyObject& /*container*/)
    {
        return SelectTemplateCore(item);
    }

    DataTemplate StringDefaultTemplateSelector::SelectTemplateCore(const IInspectable& item)
    {
        if (const auto pv{ item.try_as<IPropertyValue>() }; pv && pv.Type() == PropertyType::String)
        {
            return _StringTemplate;
        }
        return nullptr;
    }

#pragma endregion
#pragma region StyleExtensions

    ResourceDictionary StyleExtensions::_sharedImplicitStylesDictionary{ nullptr };

    // Lazy singleton: loads SettingsControlsImplicitStyles.xaml exactly once
    // for the process lifetime. We do NOT append this dictionary itself to any
    // element's MergedDictionaries — that triggers "Element is already the
    // child of another element" once a second element tries to merge it.
    // Instead, EnsureImplicitStylesMergedInto copies the dictionary's entries
    // (Style references) into the target's own Resources. Style instances are
    // reference types but not UIElements, so sharing them across multiple
    // elements' Resources collections is safe.
    ResourceDictionary StyleExtensions::_SharedImplicitStylesDictionary()
    {
        if (!_sharedImplicitStylesDictionary)
        {
            try
            {
                auto dict{ ResourceDictionary{} };
                dict.Source(winrt::Windows::Foundation::Uri{ L"ms-appx:///Microsoft.Terminal.Settings.Editor/SettingsControlsImplicitStyles.xaml" });
                _sharedImplicitStylesDictionary = dict;
            }
            CATCH_LOG();
        }
        return _sharedImplicitStylesDictionary;
    }

    void StyleExtensions::EnsureImplicitStylesMergedInto(const FrameworkElement& target)
    {
        if (!target)
        {
            return;
        }

        try
        {
            const auto resources{ target.Resources() };
            if (!resources)
            {
                return;
            }

            // Idempotency marker: if we've already populated this element's
            // Resources with the implicit styles, skip. Cheap to check
            // (one hash lookup), independent of MergedDictionaries.
            const auto markerKey{ box_value(hstring{ L"__SettingsControls_ImplicitStyles" }) };
            if (resources.HasKey(markerKey))
            {
                return;
            }

            const auto sharedDict{ _SharedImplicitStylesDictionary() };
            if (!sharedDict)
            {
                return;
            }

            // Copy each entry (Style or other resource) from the shared loaded
            // dictionary into the target's Resources. Style instances are
            // reference types that can safely be shared across multiple
            // element Resources collections (unlike UIElements, which have a
            // single-parent constraint). We deliberately do NOT
            // MergedDictionaries.Append(sharedDict) — that path throws
            // "Element is already the child of another element" once a second
            // element tries to merge the same shared dict.
            for (const auto& kv : sharedDict)
            {
                if (!resources.HasKey(kv.Key()))
                {
                    resources.Insert(kv.Key(), kv.Value());
                }
            }
            resources.Insert(markerKey, box_value(true));
        }
        CATCH_LOG();
    }

#pragma endregion
}
