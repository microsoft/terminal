#pragma once

#include "EmptyStringVisibilityConverter.g.h"

namespace winrt::TerminalApp::implementation
{
    struct EmptyStringVisibilityConverter : EmptyStringVisibilityConverterT<EmptyStringVisibilityConverter>
    {
        EmptyStringVisibilityConverter() = default;

        Windows::Foundation::IInspectable Convert(Windows::Foundation::IInspectable const& value,
                                                  Windows::UI::Xaml::Interop::TypeName const& targetType,
                                                  Windows::Foundation::IInspectable const& parameter,
                                                  hstring const& language);

        Windows::Foundation::IInspectable ConvertBack(Windows::Foundation::IInspectable const& value,
                                                      Windows::UI::Xaml::Interop::TypeName const& targetType,
                                                      Windows::Foundation::IInspectable const& parameter,
                                                      hstring const& language);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(EmptyStringVisibilityConverter);
}
