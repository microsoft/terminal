// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ExtensionPaletteMessageTemplateSelector.g.h"
#include "ExtensionPaletteMessageTemplateSelector2.g.h"
#include "ExtensionPaletteGroupedMessagesHeaderTemplateSelector.g.h"

namespace winrt::Microsoft::Terminal::Query::Extension::implementation
{
    struct ExtensionPaletteMessageTemplateSelector : ExtensionPaletteMessageTemplateSelectorT<ExtensionPaletteMessageTemplateSelector>
    {
        ExtensionPaletteMessageTemplateSelector() = default;

        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const winrt::Windows::Foundation::IInspectable&, const winrt::Windows::UI::Xaml::DependencyObject&);
        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const winrt::Windows::Foundation::IInspectable&);

        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, QueryMessageTemplate);
        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, TextResponseMessageTemplate);
        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, CodeResponseMessageTemplate);
    };

    struct ExtensionPaletteMessageTemplateSelector2 : ExtensionPaletteMessageTemplateSelector2T<ExtensionPaletteMessageTemplateSelector2>
    {
        ExtensionPaletteMessageTemplateSelector2() = default;

        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const winrt::Windows::Foundation::IInspectable&, const winrt::Windows::UI::Xaml::DependencyObject&);
        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const winrt::Windows::Foundation::IInspectable&);

        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, RichQueryMessageTemplate);
        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, RichResponseMessageTemplate);
    };

    struct ExtensionPaletteGroupedMessagesHeaderTemplateSelector : ExtensionPaletteGroupedMessagesHeaderTemplateSelectorT<ExtensionPaletteGroupedMessagesHeaderTemplateSelector>
    {
        ExtensionPaletteGroupedMessagesHeaderTemplateSelector() = default;

        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const winrt::Windows::Foundation::IInspectable&, const winrt::Windows::UI::Xaml::DependencyObject&);
        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const winrt::Windows::Foundation::IInspectable&);

        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, QueryGroupedMessageTemplate);
        WINRT_PROPERTY(winrt::Windows::UI::Xaml::DataTemplate, ResponseGroupedMessageTemplate);
    };
}

namespace winrt::Microsoft::Terminal::Query::Extension::factory_implementation
{
    BASIC_FACTORY(ExtensionPaletteMessageTemplateSelector);
    BASIC_FACTORY(ExtensionPaletteMessageTemplateSelector2);
    BASIC_FACTORY(ExtensionPaletteGroupedMessagesHeaderTemplateSelector);
}
