// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ArgsTemplateSelectors.g.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ArgsTemplateSelectors : ArgsTemplateSelectorsT<ArgsTemplateSelectors>
    {
        ArgsTemplateSelectors() = default;

        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const winrt::Windows::Foundation::IInspectable&, const winrt::Windows::UI::Xaml::DependencyObject& = nullptr);

        til::property<winrt::Windows::UI::Xaml::DataTemplate> Int32Template;
        til::property<winrt::Windows::UI::Xaml::DataTemplate> Int32OptionalTemplate;
        til::property<winrt::Windows::UI::Xaml::DataTemplate> UInt32Template;
        til::property<winrt::Windows::UI::Xaml::DataTemplate> UInt32OptionalTemplate;
        til::property<winrt::Windows::UI::Xaml::DataTemplate> FloatTemplate;
        til::property<winrt::Windows::UI::Xaml::DataTemplate> SplitSizeTemplate;
        til::property<winrt::Windows::UI::Xaml::DataTemplate> StringTemplate;
        til::property<winrt::Windows::UI::Xaml::DataTemplate> ColorSchemeTemplate;
        til::property<winrt::Windows::UI::Xaml::DataTemplate> FilePickerTemplate;
        til::property<winrt::Windows::UI::Xaml::DataTemplate> FolderPickerTemplate;
        til::property<winrt::Windows::UI::Xaml::DataTemplate> BoolTemplate;
        til::property<winrt::Windows::UI::Xaml::DataTemplate> BoolOptionalTemplate;
        til::property<winrt::Windows::UI::Xaml::DataTemplate> EnumTemplate;
        til::property<winrt::Windows::UI::Xaml::DataTemplate> FlagTemplate;
        til::property<winrt::Windows::UI::Xaml::DataTemplate> TerminalCoreColorOptionalTemplate;
        til::property<winrt::Windows::UI::Xaml::DataTemplate> WindowsUIColorOptionalTemplate;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(ArgsTemplateSelectors);
}
