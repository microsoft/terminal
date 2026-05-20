// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#pragma once

#include "StringDefaultTemplateSelector.g.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct StringDefaultTemplateSelector : StringDefaultTemplateSelectorT<StringDefaultTemplateSelector>
    {
        StringDefaultTemplateSelector() = default;

        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const winrt::Windows::Foundation::IInspectable& item, const winrt::Windows::UI::Xaml::DependencyObject& container);
        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const winrt::Windows::Foundation::IInspectable& item);

        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, StringTemplate, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(StringDefaultTemplateSelector);
}
