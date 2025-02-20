// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ArgsTemplateSelectors.g.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ArgsTemplateSelectors : ArgsTemplateSelectorsT<ArgsTemplateSelectors>
    {
        ArgsTemplateSelectors() = default;

        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const winrt::Windows::Foundation::IInspectable&, const winrt::Windows::UI::Xaml::DependencyObject&);
        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const winrt::Windows::Foundation::IInspectable&);

        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, NoArgTemplate);
        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, Int32Template);
        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, UInt32Template);
        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, UInt32OptionalTemplate);
        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, FloatTemplate);
        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, StringTemplate);
        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, BoolTemplate);
        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, BoolOptionalTemplate);
        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, EnumTemplate);
        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, FlagTemplate);
        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, TerminalCoreColorOptionalTemplate);
        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, WindowsUIColorOptionalTemplate);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(ArgsTemplateSelectors);
}
