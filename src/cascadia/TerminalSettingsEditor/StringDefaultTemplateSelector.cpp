// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "StringDefaultTemplateSelector.h"
#include "StringDefaultTemplateSelector.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    DataTemplate StringDefaultTemplateSelector::SelectTemplateCore(const IInspectable& item, const DependencyObject& /*container*/)
    {
        return SelectTemplateCore(item);
    }

    DataTemplate StringDefaultTemplateSelector::SelectTemplateCore(const IInspectable& item)
    {
        if (const auto pv{ item.try_as<IPropertyValue>() }; pv && pv.Type() == PropertyType::String)
        {
            return _StringTemplate;
        }
        return nullptr;
    }
}
