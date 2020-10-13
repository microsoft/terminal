#include "pch.h"
#include "MainPageTemplateSelector.h"
#include "MainPageTemplateSelector.g.cpp"

using namespace Windows::Foundation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Windows::UI::Xaml::DataTemplate MainPageTemplateSelector::SelectTemplateCore(const IInspectable& item, const Windows::UI::Xaml::DependencyObject& /*obj*/)
    {
        return SelectTemplateCore(item);
    }

    Windows::UI::Xaml::DataTemplate MainPageTemplateSelector::SelectTemplateCore(const IInspectable& item)
    {
        if (auto navItem = item.try_as<winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem>())
        {
            auto tag = unbox_value<hstring>(navItem.Tag());
            if (tag == L"Profiles_Nav")
            {
                return _ProfilesTemplate;
            }
        }

        return _ProfilesTemplate;
    }
}
