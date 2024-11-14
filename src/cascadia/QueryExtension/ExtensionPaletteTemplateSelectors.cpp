// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ExtensionPaletteTemplateSelectors.h"
#include "ExtensionPaletteMessageTemplateSelector.g.cpp"
#include "ExtensionPaletteMessageTemplateSelector2.g.cpp"
#include "ExtensionPaletteGroupedMessagesHeaderTemplateSelector.g.cpp"

namespace winrt::Microsoft::Terminal::Query::Extension::implementation
{
    Windows::UI::Xaml::DataTemplate ExtensionPaletteMessageTemplateSelector::SelectTemplateCore(const winrt::Windows::Foundation::IInspectable& item, const winrt::Windows::UI::Xaml::DependencyObject& /*container*/)
    {
        return SelectTemplateCore(item);
    }

    // Method Description:
    // - This method is called once command palette decides how to render a filtered command.
    //   Currently we support two ways to render command, that depend on its palette item type:
    //   - For TabPalette item we render an icon, a title, and some tab-related indicators like progress bar (as defined by TabItemTemplate)
    //   - All other items are currently rendered with icon, title and optional key-chord (as defined by GeneralItemTemplate)
    // Arguments:
    // - item - an instance of filtered command to render
    // Return Value:
    // - data template to use for rendering
    Windows::UI::Xaml::DataTemplate ExtensionPaletteMessageTemplateSelector::SelectTemplateCore(const winrt::Windows::Foundation::IInspectable& item)
    {
        if (const auto message{ item.try_as<winrt::Microsoft::Terminal::Query::Extension::ChatMessage>() })
        {
            if (!message.IsQuery())
            {
                if (message.IsCode())
                {
                    return CodeResponseMessageTemplate();
                }
                else
                {
                    return TextResponseMessageTemplate();
                }
            }
        }
        return QueryMessageTemplate();
    }

    Windows::UI::Xaml::DataTemplate ExtensionPaletteMessageTemplateSelector2::SelectTemplateCore(const winrt::Windows::Foundation::IInspectable& item, const winrt::Windows::UI::Xaml::DependencyObject& /*container*/)
    {
        return SelectTemplateCore(item);
    }

    // Method Description:
    // - This method is called once command palette decides how to render a filtered command.
    //   Currently we support two ways to render command, that depend on its palette item type:
    //   - For TabPalette item we render an icon, a title, and some tab-related indicators like progress bar (as defined by TabItemTemplate)
    //   - All other items are currently rendered with icon, title and optional key-chord (as defined by GeneralItemTemplate)
    // Arguments:
    // - item - an instance of filtered command to render
    // Return Value:
    // - data template to use for rendering
    Windows::UI::Xaml::DataTemplate ExtensionPaletteMessageTemplateSelector2::SelectTemplateCore(const winrt::Windows::Foundation::IInspectable& item)
    {
        if (const auto message{ item.try_as<winrt::Microsoft::Terminal::Query::Extension::ChatMessage>() })
        {
            if (!message.IsQuery())
            {
                return RichResponseMessageTemplate();
            }
        }
        return RichQueryMessageTemplate();
    }

    Windows::UI::Xaml::DataTemplate ExtensionPaletteGroupedMessagesHeaderTemplateSelector::SelectTemplateCore(const winrt::Windows::Foundation::IInspectable& item, const winrt::Windows::UI::Xaml::DependencyObject& /*container*/)
    {
        return SelectTemplateCore(item);
    }

    // Method Description:
    // - This method is called once command palette decides how to render a filtered command.
    //   Currently we support two ways to render command, that depend on its palette item type:
    //   - For TabPalette item we render an icon, a title, and some tab-related indicators like progress bar (as defined by TabItemTemplate)
    //   - All other items are currently rendered with icon, title and optional key-chord (as defined by GeneralItemTemplate)
    // Arguments:
    // - item - an instance of filtered command to render
    // Return Value:
    // - data template to use for rendering
    Windows::UI::Xaml::DataTemplate ExtensionPaletteGroupedMessagesHeaderTemplateSelector::SelectTemplateCore(const winrt::Windows::Foundation::IInspectable& item)
    {
        if (const auto groupedMessage{ item.try_as<winrt::Microsoft::Terminal::Query::Extension::GroupedChatMessages>() })
        {
            if (!groupedMessage.IsQuery())
            {
                return ResponseGroupedMessageTemplate();
            }
        }
        return QueryGroupedMessageTemplate();
    }
}
