#pragma once

#include "EmptyStringVisibilityConverter.g.h"

namespace winrt::TerminalApp::implementation
{
    struct EmptyStringVisibilityConverter : EmptyStringVisibilityConverterT<EmptyStringVisibilityConverter>
    {
        EmptyStringVisibilityConverter() = default;

        WF::IInspectable Convert(const WF::IInspectable& value,
                                 const WUX::Interop::TypeName& targetType,
                                 const WF::IInspectable& parameter,
                                 const hstring& language);

        WF::IInspectable ConvertBack(const WF::IInspectable& value,
                                     const WUX::Interop::TypeName& targetType,
                                     const WF::IInspectable& parameter,
                                     const hstring& language);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(EmptyStringVisibilityConverter);
}
