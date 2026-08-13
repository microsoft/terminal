#pragma once

#include "IconPathConverter.g.h"

namespace winrt::Microsoft::Terminal::UI::implementation
{
    struct IconPathConverter
    {
        IconPathConverter() = default;

        // Returns an unsized IconElement — callers are responsible for setting
        // Width/Height to match their UI context.
        static Windows::UI::Xaml::Controls::IconElement IconWUX(const winrt::hstring& iconPath);
        static Windows::UI::Xaml::Controls::IconSource IconSourceWUX(const winrt::hstring& iconPath);
        static Microsoft::UI::Xaml::Controls::IconSource IconSourceMUX(const winrt::hstring& iconPath, bool convertToGrayscale);
    };
}

namespace winrt::Microsoft::Terminal::UI::factory_implementation
{
    BASIC_FACTORY(IconPathConverter);
}
