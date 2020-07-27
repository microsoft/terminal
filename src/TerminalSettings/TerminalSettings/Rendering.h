#pragma once

#include "Rendering.g.h"

namespace winrt::SettingsControl::implementation
{
    struct Rendering : RenderingT<Rendering>
    {
        Rendering();

        void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    struct Rendering : RenderingT<Rendering, implementation::Rendering>
    {
    };
}
