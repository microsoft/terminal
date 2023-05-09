#pragma once

#include "Class.g.h"

namespace winrt::ExtensionComponent::implementation
{
    struct Class : ClassT<Class>
    {
        Class() = default;

        int32_t MyProperty();
        void MyProperty(int32_t value);
        int32_t DoTheThing();
        winrt::Windows::UI::Xaml::FrameworkElement PaneContent();
        winrt::Windows::UI::Xaml::Markup::IXamlMetadataProvider GetProvider();
    };
}

namespace winrt::ExtensionComponent::factory_implementation
{
    struct Class : ClassT<Class, implementation::Class>
    {
    };
}
