#pragma once

#include "EmptyStringVisibilityConverter.g.h"

namespace winrt::TerminalApp::implementation
{
    struct EmptyStringVisibilityConverter : EmptyStringVisibilityConverterT<EmptyStringVisibilityConverter>
    {
        EmptyStringVisibilityConverter() = default;

        Windows::Foundation::IInspectable Convert(const Windows::Foundation::IInspectable& value,
                                                  const Windows::UI::Xaml::Interop::TypeName& targetType,
                                                  const Windows::Foundation::IInspectable& parameter,
                                                  const hstring& language);

        Windows::Foundation::IInspectable ConvertBack(const Windows::Foundation::IInspectable& value,
                                                      const Windows::UI::Xaml::Interop::TypeName& targetType,
                                                      const Windows::Foundation::IInspectable& parameter,
                                                      const hstring& language);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(EmptyStringVisibilityConverter);
}
