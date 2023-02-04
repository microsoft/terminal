#pragma once

#include "IconPathConverter.g.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct IconPathConverter : IconPathConverterT<IconPathConverter>
    {
        IconPathConverter() = default;

        WF::IInspectable Convert(const WF::IInspectable& value,
                                                  const WUX::Interop::TypeName& targetType,
                                                  const WF::IInspectable& parameter,
                                                  const hstring& language);

        WF::IInspectable ConvertBack(const WF::IInspectable& value,
                                                      const WUX::Interop::TypeName& targetType,
                                                      const WF::IInspectable& parameter,
                                                      const hstring& language);

        static WUXC::IconElement IconWUX(const winrt::hstring& iconPath);
        static MUXC::IconSource IconSourceMUX(const winrt::hstring& iconPath);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(IconPathConverter);
}
