// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#pragma once

#include "StyleExtensions.g.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct StyleExtensions
    {
        StyleExtensions() = default;

        static Windows::UI::Xaml::DependencyProperty ResourcesProperty();

        static Windows::UI::Xaml::ResourceDictionary GetResources(const Windows::UI::Xaml::DependencyObject& target);
        static void SetResources(const Windows::UI::Xaml::DependencyObject& target, const Windows::UI::Xaml::ResourceDictionary& value);

        static void EnsureImplicitStylesMergedInto(const Windows::UI::Xaml::FrameworkElement& target);

    private:
        static void _InitializeProperties();
        static void _OnResourcesChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);
        static void _ForceControlToReloadThemeResources(const Windows::UI::Xaml::FrameworkElement& element);
        static Windows::UI::Xaml::ResourceDictionary _SharedImplicitStylesDictionary();

        static Windows::UI::Xaml::DependencyProperty _resourcesProperty;
        static Windows::UI::Xaml::ResourceDictionary _sharedImplicitStylesDictionary;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(StyleExtensions);
}
