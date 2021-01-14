// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorLightenConverter.h"
#include "ColorLightenConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Text;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Foundation::IInspectable ColorLightenConverter::Convert(Foundation::IInspectable const& value,
                                                            Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                            Foundation::IInspectable const& /* parameter */,
                                                            hstring const& /* language */)
    {
        value;
        auto c = winrt::Windows::UI::Color{255, 255, 0, 255 }; // ARGB
        return winrt::box_value(c);
    }

    Foundation::IInspectable ColorLightenConverter::ConvertBack(Foundation::IInspectable const& /*value*/,
                                                                Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                                Foundation::IInspectable const& /*parameter*/,
                                                                hstring const& /* language */)
    {
        throw hresult_not_implemented();
    }
}
