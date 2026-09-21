/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SettingsControlsHelpers

Abstract:
- Helper types backing the Windows Community Toolkit port of SettingsCard/SettingsExpander.
  Grouped into a single file so the port boundary is obvious.

Author(s):
- Carlos Zamora - June 2026

--*/

#pragma once

#include "ControlSizeTrigger.g.h"
#include "TopCornerRadiusFilterConverter.g.h"
#include "BottomCornerRadiusFilterConverter.g.h"
#include "StringDefaultTemplateSelector.g.h"
#include "StyleExtensions.g.h"
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

    struct TopCornerRadiusFilterConverter : TopCornerRadiusFilterConverterT<TopCornerRadiusFilterConverter>
    {
        TopCornerRadiusFilterConverter() = default;

        Windows::Foundation::IInspectable Convert(const Windows::Foundation::IInspectable& value, const Windows::UI::Xaml::Interop::TypeName& targetType, const Windows::Foundation::IInspectable& parameter, const hstring& language);
        Windows::Foundation::IInspectable ConvertBack(const Windows::Foundation::IInspectable& value, const Windows::UI::Xaml::Interop::TypeName& targetType, const Windows::Foundation::IInspectable& parameter, const hstring& language);
    };

    struct BottomCornerRadiusFilterConverter : BottomCornerRadiusFilterConverterT<BottomCornerRadiusFilterConverter>
    {
        BottomCornerRadiusFilterConverter() = default;

        Windows::Foundation::IInspectable Convert(const Windows::Foundation::IInspectable& value, const Windows::UI::Xaml::Interop::TypeName& targetType, const Windows::Foundation::IInspectable& parameter, const hstring& language);
        Windows::Foundation::IInspectable ConvertBack(const Windows::Foundation::IInspectable& value, const Windows::UI::Xaml::Interop::TypeName& targetType, const Windows::Foundation::IInspectable& parameter, const hstring& language);
    };

    struct StringDefaultTemplateSelector : StringDefaultTemplateSelectorT<StringDefaultTemplateSelector>
    {
        StringDefaultTemplateSelector() = default;

        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const winrt::Windows::Foundation::IInspectable& item, const winrt::Windows::UI::Xaml::DependencyObject& container);
        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const winrt::Windows::Foundation::IInspectable& item);

        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, StringTemplate, nullptr);
    };

    struct StyleExtensions
    {
        StyleExtensions() = default;

        static void EnsureImplicitStylesMergedInto(const Windows::UI::Xaml::FrameworkElement& target);

    private:
        static Windows::UI::Xaml::ResourceDictionary _SharedImplicitStylesDictionary();

        static Windows::UI::Xaml::ResourceDictionary _sharedImplicitStylesDictionary;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(ControlSizeTrigger);
    BASIC_FACTORY(TopCornerRadiusFilterConverter);
    BASIC_FACTORY(BottomCornerRadiusFilterConverter);
    BASIC_FACTORY(StringDefaultTemplateSelector);
    BASIC_FACTORY(StyleExtensions);
}
