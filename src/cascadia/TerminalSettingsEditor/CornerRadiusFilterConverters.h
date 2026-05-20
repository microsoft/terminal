// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#pragma once

#include "CornerRadiusConverter.g.h"
#include "TopCornerRadiusFilterConverter.g.h"
#include "BottomCornerRadiusFilterConverter.g.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct CornerRadiusConverter : CornerRadiusConverterT<CornerRadiusConverter>
    {
        CornerRadiusConverter() = default;

        Windows::Foundation::IInspectable Convert(const Windows::Foundation::IInspectable& value, const Windows::UI::Xaml::Interop::TypeName& targetType, const Windows::Foundation::IInspectable& parameter, const hstring& language);
        Windows::Foundation::IInspectable ConvertBack(const Windows::Foundation::IInspectable& value, const Windows::UI::Xaml::Interop::TypeName& targetType, const Windows::Foundation::IInspectable& parameter, const hstring& language);
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
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(CornerRadiusConverter);
    BASIC_FACTORY(TopCornerRadiusFilterConverter);
    BASIC_FACTORY(BottomCornerRadiusFilterConverter);
}
