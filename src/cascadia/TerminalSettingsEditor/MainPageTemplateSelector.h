#pragma once
#include "Utils.h"
#include "MainPageTemplateSelector.g.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct MainPageTemplateSelector : MainPageTemplateSelectorT<MainPageTemplateSelector>
    {
    public:
        MainPageTemplateSelector() = default;
        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const Windows::Foundation::IInspectable& item);
        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const Windows::Foundation::IInspectable& item, const Windows::UI::Xaml::DependencyObject& obj);

        GETSET_PROPERTY(Windows::UI::Xaml::DataTemplate, ProfilesTemplate, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(MainPageTemplateSelector);
}
