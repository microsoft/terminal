#include "pch.h"
#include "TabPaletteItem.h"
#include "PaletteItemTemplateSelector.h"
#include "PaletteItemTemplateSelector.g.cpp"

namespace winrt::TerminalApp::implementation
{
    Windows::UI::Xaml::DataTemplate PaletteItemTemplateSelector::SelectTemplateCore(winrt::Windows::Foundation::IInspectable const& item, winrt::Windows::UI::Xaml::DependencyObject const& /*container*/)
    {
        return SelectTemplateCore(item);
    }

    Windows::UI::Xaml::DataTemplate PaletteItemTemplateSelector::SelectTemplateCore(winrt::Windows::Foundation::IInspectable const& item)
    {
        if (const auto filteredCommand{ item.try_as<winrt::TerminalApp::FilteredCommand>() })
        {
            if (filteredCommand.Item().try_as<winrt::TerminalApp::TabPaletteItem>())
            {
                return TabItemTemplate();
            }
        }

        return GeneralItemTemplate();
    }
}
