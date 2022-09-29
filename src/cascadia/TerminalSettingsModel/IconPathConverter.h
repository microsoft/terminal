#pragma once

#include "IconPathConverter.g.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct IconPathConverter : IconPathConverterT<IconPathConverter>
    {
        IconPathConverter() = default;

        Windows::Foundation::IInspectable Convert(const Windows::Foundation::IInspectable& value,
                                                  const Windows::UI::Xaml::Interop::TypeName& targetType,
                                                  const Windows::Foundation::IInspectable& parameter,
                                                  const hstring& language);

        Windows::Foundation::IInspectable ConvertBack(const Windows::Foundation::IInspectable& value,
                                                      const Windows::UI::Xaml::Interop::TypeName& targetType,
                                                      const Windows::Foundation::IInspectable& parameter,
                                                      const hstring& language);

        static winrt::Windows::Foundation::IAsyncOperation<Microsoft::UI::Xaml::Controls::IconSource> IconSourceMUX(const winrt::hstring& iconPath);
        static winrt::Windows::Foundation::IAsyncOperation<Windows::UI::Xaml::Controls::IconElement> IconWUX(const winrt::hstring& iconPath);
        static winrt::Windows::Foundation::IAsyncOperation<Windows::UI::Xaml::Controls::IconSource> IconWUXTest(const winrt::hstring& iconPath);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(IconPathConverter);
}
