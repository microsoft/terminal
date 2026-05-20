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
    ResourceDictionary StyleExtensions::_sharedImplicitStylesDictionary{ nullptr };

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

    // Lazy singleton: loads SettingsControlsImplicitStyles.xaml exactly once
    // for the process lifetime. We do NOT append this dictionary itself to any
    // element's MergedDictionaries — that triggers "Element is already the
    // child of another element" once a second element tries to merge it.
    // Instead, EnsureImplicitStylesMergedInto copies the dictionary's entries
    // (Style references) into the target's own Resources. Style instances are
    // reference types but not UIElements, so sharing them across multiple
    // elements' Resources collections is safe.
    ResourceDictionary StyleExtensions::_SharedImplicitStylesDictionary()
    {
        if (!_sharedImplicitStylesDictionary)
        {
            try
            {
                auto dict{ ResourceDictionary{} };
                dict.Source(winrt::Windows::Foundation::Uri{ L"ms-appx:///Microsoft.Terminal.Settings.Editor/SettingsControlsImplicitStyles.xaml" });
                _sharedImplicitStylesDictionary = dict;
            }
            CATCH_LOG();
        }
        return _sharedImplicitStylesDictionary;
    }

    void StyleExtensions::EnsureImplicitStylesMergedInto(const FrameworkElement& target)
    {
        if (!target)
        {
            return;
        }

        try
        {
            const auto resources{ target.Resources() };
            if (!resources)
            {
                return;
            }

            // Idempotency marker: if we've already populated this element's
            // Resources with the implicit styles, skip. Cheap to check
            // (one hash lookup), independent of MergedDictionaries.
            const auto markerKey{ box_value(hstring{ L"__SettingsControls_ImplicitStyles" }) };
            if (resources.HasKey(markerKey))
            {
                return;
            }

            const auto sharedDict{ _SharedImplicitStylesDictionary() };
            if (!sharedDict)
            {
                return;
            }

            // Copy each entry (Style or other resource) from the shared loaded
            // dictionary into the target's Resources. Style instances are
            // reference types that can safely be shared across multiple
            // element Resources collections (unlike UIElements, which have a
            // single-parent constraint). We deliberately do NOT
            // MergedDictionaries.Append(sharedDict) — that path throws
            // "Element is already the child of another element" once a second
            // element tries to merge the same shared dict.
            for (const auto& kv : sharedDict)
            {
                if (!resources.HasKey(kv.Key()))
                {
                    resources.Insert(kv.Key(), kv.Value());
                }
            }
            resources.Insert(markerKey, box_value(true));
        }
        CATCH_LOG();
    }
}
