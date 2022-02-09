#pragma once

#include "IconPathConverter.g.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct IconPathConverter : IconPathConverterT<IconPathConverter>
    {
        IconPathConverter() = default;

        Windows::Foundation::IInspectable Convert(Windows::Foundation::IInspectable const& value,
                                                  Windows::UI::Xaml::Interop::TypeName const& targetType,
                                                  Windows::Foundation::IInspectable const& parameter,
                                                  hstring const& language);

        Windows::Foundation::IInspectable ConvertBack(Windows::Foundation::IInspectable const& value,
                                                      Windows::UI::Xaml::Interop::TypeName const& targetType,
                                                      Windows::Foundation::IInspectable const& parameter,
                                                      hstring const& language);

        static Windows::UI::Xaml::Controls::IconSource IconSourceWUX(hstring path);
        static Microsoft::UI::Xaml::Controls::IconSource IconSourceMUX(hstring path);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(IconPathConverter);
}
