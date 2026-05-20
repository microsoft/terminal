// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "StyleExtensions.h"
#include "StyleExtensions.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    DependencyProperty StyleExtensions::_resourcesProperty{ nullptr };

    DependencyProperty StyleExtensions::ResourcesProperty()
    {
        _InitializeProperties();
        return _resourcesProperty;
    }

    void StyleExtensions::_InitializeProperties()
    {
        if (!_resourcesProperty)
        {
            _resourcesProperty = DependencyProperty::RegisterAttached(
                L"Resources",
                xaml_typename<ResourceDictionary>(),
                xaml_typename<Editor::StyleExtensions>(),
                PropertyMetadata{ nullptr, PropertyChangedCallback{ &StyleExtensions::_OnResourcesChanged } });
        }
    }

    ResourceDictionary StyleExtensions::GetResources(const DependencyObject& target)
    {
        return target.GetValue(ResourcesProperty()).try_as<ResourceDictionary>();
    }

    void StyleExtensions::SetResources(const DependencyObject& target, const ResourceDictionary& value)
    {
        target.SetValue(ResourcesProperty(), value);
    }

    void StyleExtensions::_OnResourcesChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& e)
    {
        const auto frameworkElement{ d.try_as<FrameworkElement>() };
        if (!frameworkElement)
        {
            return;
        }

        const auto elementResources{ frameworkElement.Resources() };
        if (!elementResources)
        {
            return;
        }
        const auto mergedDictionaries{ elementResources.MergedDictionaries() };
        if (!mergedDictionaries)
        {
            return;
        }

        // Remove the previously-merged dictionary (if any). Resource dictionaries
        // are reference types so IndexOf walks by identity, which is exactly what
        // we want: the same Setter.Value is shared across every element that uses
        // this Style, so the OldValue we see here is the exact instance we
        // appended last time the property changed.
        if (const auto oldDict{ e.OldValue().try_as<ResourceDictionary>() })
        {
            uint32_t index{ 0 };
            if (mergedDictionaries.IndexOf(oldDict, index))
            {
                mergedDictionaries.RemoveAt(index);
            }
        }

        // Add the new dictionary directly. We deliberately do NOT clone the way
        // the WCT C# port does: ResourceDictionary is sealed (so we can't tag a
        // private subclass like the toolkit), and a deep clone would have to
        // copy the inline dictionary's Source URI — which XAML may leave as a
        // relative string like "CommonResources.xaml" and which the runtime
        // then rejects with "is not a valid absolute URI". Sharing the same
        // dictionary across elements is fine: each element's MergedDictionaries
        // only holds a reference, and implicit styles are designed to be shared.
        if (const auto newDict{ e.NewValue().try_as<ResourceDictionary>() })
        {
            mergedDictionaries.Append(newDict);

            if (frameworkElement.IsLoaded())
            {
                _ForceControlToReloadThemeResources(frameworkElement);
            }
        }
    }

    void StyleExtensions::_ForceControlToReloadThemeResources(const FrameworkElement& element)
    {
        // Toggle RequestedTheme to force the framework to re-resolve all
        // {ThemeResource} bindings under this element. Required when the
        // style is applied to an already-loaded element. Matches the toolkit.
        const auto currentTheme{ element.RequestedTheme() };
        element.RequestedTheme(currentTheme == ElementTheme::Dark ? ElementTheme::Light : ElementTheme::Dark);
        element.RequestedTheme(currentTheme);
    }
}
