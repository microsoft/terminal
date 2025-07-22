// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "PaletteItemTemplateSelector.h"
#include "PaletteItemTemplateSelector.g.cpp"

#include "CommandPaletteItems.h"

namespace winrt::TerminalApp::implementation
{
    Windows::UI::Xaml::DataTemplate PaletteItemTemplateSelector::SelectTemplateCore(const winrt::Windows::Foundation::IInspectable& item, const winrt::Windows::UI::Xaml::DependencyObject& /*container*/)
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
    Windows::UI::Xaml::DataTemplate PaletteItemTemplateSelector::SelectTemplateCore(const winrt::Windows::Foundation::IInspectable& item)
    {
        if (const auto filteredCommand{ item.try_as<winrt::TerminalApp::FilteredCommand>() })
        {
            switch (filteredCommand.Item().Type())
            {
            case PaletteItemType::Tab:
                return TabItemTemplate();
            case PaletteItemType::Action:
            {
                const auto actionPaletteItem{ winrt::get_self<ActionPaletteItem>(filteredCommand.Item()) };
                if (actionPaletteItem->Command().HasNestedCommands())
                {
                    return NestedItemTemplate();
                }
                break; // Fall back to the general template
            }
            case PaletteItemType::CommandLine:
            default:
                break; // Fall back to the general template
            }
        }

        return GeneralItemTemplate();
    }
}
