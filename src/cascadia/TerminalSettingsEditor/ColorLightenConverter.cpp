// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorLightenConverter.h"
#include "ColorLightenConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Text;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Foundation::IInspectable ColorLightenConverter::Convert(Foundation::IInspectable const& value,
                                                            Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                            Foundation::IInspectable const& /* parameter */,
                                                            hstring const& /* language */)
    {
        auto original = winrt::unbox_value_or<Color>(value, Color{ 255, 0, 0, 0 });
        auto result = original;
        result.A = 128; // halfway transparent
        return winrt::box_value(result);
    }

    Foundation::IInspectable ColorLightenConverter::ConvertBack(Foundation::IInspectable const& /*value*/,
                                                                Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                                Foundation::IInspectable const& /*parameter*/,
                                                                hstring const& /* language */)
    {
        throw hresult_not_implemented();
    }
}
